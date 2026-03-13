#include "MetricsCollector.h"
using namespace Metrics;  // Use Metrics namespace instead of TubeMPC
#include <std_msgs/Float64.h>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace Metrics {

MetricsCollector::MetricsCollector(const std::string& output_csv)
    : _output_csv(output_csv),
      _csv_initialized(false),
      _target_delta(0.1),  // Default 10% risk
      _tube_radius_threshold(1.0),
      _max_history_size(10000),
      _total_steps(0),
      _safety_violations(0),
      _tube_violations(0),
      _tube_occupancy(0),
      _mpc_successes(0),
      _mpc_failures(0),
      _cumulative_dynamic_regret(0.0),
      _cumulative_safe_regret(0.0),
      _reference_cost_sum(0.0),
      _safe_reference_cost_sum(0.0),
      _current_stl_robustness(0.0),
      _stl_violations(0)
{
    initializeCSV();
}

void MetricsCollector::initializeCSV()
{
    _csv_file.open(_output_csv);
    if (_csv_file.is_open()) {
        writeCSVHeader();
        _csv_initialized = true;
        ROS_INFO("MetricsCollector: Initialized CSV logging to %s", _output_csv.c_str());
    } else {
        ROS_ERROR("MetricsCollector: Failed to open CSV file %s", _output_csv.c_str());
    }
}

void MetricsCollector::writeCSVHeader()
{
    if (!_csv_file.is_open()) return;

    _csv_file << "timestamp,cte,etheta,vel,angvel,tracking_error_norm,"
              << "tube_radius,tube_violation,safety_margin,safety_violation,"
              << "mpc_solve_time,mpc_feasible,cost,robustness,"
              << "regret_dynamic,regret_safe\n";

    _csv_file.flush();
}

void MetricsCollector::recordMetrics(const MetricsSnapshot& snapshot)
{
    // Store in history (with size limit)
    _metrics_history.push_back(snapshot);
    if (_metrics_history.size() > _max_history_size) {
        _metrics_history.pop_front();
    }

    // Update tracking error statistics
    _tracking_errors.push_back(snapshot.tracking_error_norm);
    if (_tracking_errors.size() > _max_history_size) {
        _tracking_errors.pop_front();
    }

    // Update safety margins
    _safety_margins.push_back(snapshot.safety_margin);
    if (_safety_margins.size() > _max_history_size) {
        _safety_margins.pop_front();
    }

    // Count safety violations
    if (snapshot.safety_violation > 0.5) {  // Binary check
        _safety_violations++;
    }

    // Count tube violations
    if (snapshot.tube_violation > 0.0) {
        _tube_violations++;
    } else {
        _tube_occupancy++;
    }

    // Count feasibility
    if (snapshot.mpc_feasible) {
        _mpc_successes++;
    } else {
        _mpc_failures++;
        _feasibility_history.push_back(false);
    }

    // Record solve time
    if (snapshot.mpc_solve_time > 0) {
        _solve_times.push_back(snapshot.mpc_solve_time);
        if (_solve_times.size() > _max_history_size) {
            _solve_times.pop_front();
        }
    }

    _total_steps++;

    // Write to CSV
    if (_csv_initialized && _csv_file.is_open()) {
        _csv_file << std::fixed << std::setprecision(6)
                  << snapshot.timestamp << ","
                  << snapshot.cte << ","
                  << snapshot.etheta << ","
                  << snapshot.vel << ","
                  << snapshot.angvel << ","
                  << snapshot.tracking_error_norm << ","
                  << snapshot.tube_radius << ","
                  << snapshot.tube_violation << ","
                  << snapshot.safety_margin << ","
                  << snapshot.safety_violation << ","
                  << snapshot.mpc_solve_time << ","
                  << (snapshot.mpc_feasible ? 1 : 0) << ","
                  << snapshot.cost << ","
                  << snapshot.robustness << ","
                  << snapshot.regret_dynamic << ","
                  << snapshot.regret_safe << "\n";
        _csv_file.flush();
    }
}

void MetricsCollector::recordMPCSolve(double solve_time_ms, bool feasible,
                                      const Eigen::VectorXd& solution)
{
    // This is called separately if needed
    // Most data comes through recordMetrics()
}

void MetricsCollector::updateRegret(double current_cost, double reference_cost,
                                   double safe_reference_cost)
{
    // Dynamic regret: J(π) - J(π*_dynamic)
    double dynamic_regret = current_cost - reference_cost;
    _cumulative_dynamic_regret += dynamic_regret;
    _reference_cost_sum += reference_cost;

    // Safe regret: J(π) - J(π*_safe)
    if (safe_reference_cost < std::numeric_limits<double>::infinity()) {
        double safe_regret = current_cost - safe_reference_cost;
        _cumulative_safe_regret += safe_regret;
        _safe_reference_cost_sum += safe_reference_cost;
    }
}

void MetricsCollector::updateSTLRobustness(double robustness_value)
{
    _current_stl_robustness = robustness_value;
    if (robustness_value < 0.0) {
        _stl_violations++;
    }
}

void MetricsCollector::recordSafetyConstraint(double h_value, double h_threshold)
{
    double violation = (h_value < h_threshold) ? 1.0 : 0.0;
    // This will be recorded in the next snapshot
}

void MetricsCollector::recordTubeStatus(const Eigen::VectorXd& error_vector)
{
    double error_norm = error_vector.norm();
    // This will be recorded in the next snapshot
}

MetricsCollector::AggregatedMetrics MetricsCollector::getAggregatedMetrics() const
{
    AggregatedMetrics metrics;

    // === Safety and Satisfaction ===
    metrics.safety_violation_rate = (_total_steps > 0) ?
        static_cast<double>(_safety_violations) / _total_steps : 0.0;

    metrics.empirical_risk = metrics.safety_violation_rate;

    metrics.satisfaction_probability = 1.0 - metrics.empirical_risk;

    // === Feasibility ===
    metrics.total_mpc_calls = _mpc_successes + _mpc_failures;
    metrics.feasibility_rate = (metrics.total_mpc_calls > 0) ?
        static_cast<double>(_mpc_successes) / metrics.total_mpc_calls : 0.0;
    metrics.successful_mpc_solves = _mpc_successes;
    metrics.failed_mpc_solves = _mpc_failures;

    // === Tracking Performance ===
    if (!_tracking_errors.empty()) {
        metrics.mean_tracking_error = computeMean(_tracking_errors);
        metrics.max_tracking_error = *std::max_element(_tracking_errors.begin(),
                                                       _tracking_errors.end());
        metrics.std_tracking_error = computeStd(_tracking_errors,
                                                metrics.mean_tracking_error);
    } else {
        metrics.mean_tracking_error = 0.0;
        metrics.max_tracking_error = 0.0;
        metrics.std_tracking_error = 0.0;
    }

    metrics.tube_violation_rate = (_total_steps > 0) ?
        static_cast<double>(_tube_violations) / _total_steps : 0.0;

    metrics.tube_occupancy_rate = (_total_steps > 0) ?
        static_cast<double>(_tube_occupancy) / _total_steps : 0.0;

    // === Computation ===
    if (!_solve_times.empty()) {
        metrics.mean_solve_time_ms = computeMean(_solve_times);
        metrics.median_solve_time_ms = computeMedian(_solve_times);
        metrics.p90_solve_time_ms = computeSolveTimePercentile(90.0);
        metrics.max_solve_time_ms = *std::max_element(_solve_times.begin(),
                                                       _solve_times.end());
    } else {
        metrics.mean_solve_time_ms = 0.0;
        metrics.median_solve_time_ms = 0.0;
        metrics.p90_solve_time_ms = 0.0;
        metrics.max_solve_time_ms = 0.0;
    }

    // === Regret ===
    metrics.cumulative_dynamic_regret = _cumulative_dynamic_regret;
    metrics.cumulative_safe_regret = _cumulative_safe_regret;

    if (_total_steps > 0) {
        metrics.average_dynamic_regret = _cumulative_dynamic_regret / _total_steps;
        metrics.average_safe_regret = _cumulative_safe_regret / _total_steps;
    } else {
        metrics.average_dynamic_regret = 0.0;
        metrics.average_safe_regret = 0.0;
    }

    // === Calibration ===
    metrics.calibration_error = std::abs(metrics.empirical_risk - _target_delta);

    return metrics;
}

void MetricsCollector::printSummary() const
{
    AggregatedMetrics m = getAggregatedMetrics();

    std::cout << "\n========================================" << std::endl;
    std::cout << "     Tube MPC Metrics Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << "\n--- Safety and Satisfaction ---" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Empirical risk (δ̂):          " << m.empirical_risk << std::endl;
    std::cout << "Satisfaction probability:    " << m.satisfaction_probability << std::endl;
    std::cout << "Safety violation rate:       " << m.safety_violation_rate << std::endl;
    std::cout << "Target risk (δ):             " << _target_delta << std::endl;
    std::cout << "Calibration error:           " << m.calibration_error << std::endl;

    std::cout << "\n--- Feasibility ---" << std::endl;
    std::cout << "Total MPC calls:             " << m.total_mpc_calls << std::endl;
    std::cout << "Successful solves:           " << m.successful_mpc_solves << std::endl;
    std::cout << "Failed solves:               " << m.failed_mpc_solves << std::endl;
    std::cout << "Feasibility rate:            " << m.feasibility_rate * 100 << "%" << std::endl;

    std::cout << "\n--- Tracking Performance ---" << std::endl;
    std::cout << std::setprecision(6);
    std::cout << "Mean tracking error:         " << m.mean_tracking_error << std::endl;
    std::cout << "Max tracking error:          " << m.max_tracking_error << std::endl;
    std::cout << "Std tracking error:          " << m.std_tracking_error << std::endl;
    std::cout << "Tube violation rate:         " << m.tube_violation_rate * 100 << "%" << std::endl;
    std::cout << "Tube occupancy rate:         " << m.tube_occupancy_rate * 100 << "%" << std::endl;

    std::cout << "\n--- Computation Time ---" << std::endl;
    std::cout << std::setprecision(3);
    std::cout << "Mean solve time:             " << m.mean_solve_time_ms << " ms" << std::endl;
    std::cout << "Median solve time:           " << m.median_solve_time_ms << " ms" << std::endl;
    std::cout << "P90 solve time:              " << m.p90_solve_time_ms << " ms" << std::endl;
    std::cout << "Max solve time:              " << m.max_solve_time_ms << " ms" << std::endl;

    std::cout << "\n--- Regret Analysis ---" << std::endl;
    std::cout << std::setprecision(6);
    std::cout << "Cumulative dynamic regret:   " << m.cumulative_dynamic_regret << std::endl;
    std::cout << "Average dynamic regret:      " << m.average_dynamic_regret << std::endl;
    std::cout << "Cumulative safe regret:      " << m.cumulative_safe_regret << std::endl;
    std::cout << "Average safe regret:         " << m.average_safe_regret << std::endl;

    // Compute o(T) regret growth rate check
    if (_total_steps > 100) {
        double regret_per_step_sqrt = m.average_dynamic_regret / std::sqrt(_total_steps);
        std::cout << "Regret/√T ratio:            " << regret_per_step_sqrt;
        if (regret_per_step_sqrt < 1.0) {
            std::cout << " (o(√T) growth - good!)" << std::endl;
        } else {
            std::cout << " (check for linear growth)" << std::endl;
        }
    }

    std::cout << "\n========================================" << std::endl;
}

void MetricsCollector::reset()
{
    _metrics_history.clear();
    _solve_times.clear();
    _tracking_errors.clear();
    _feasibility_history.clear();
    _safety_margins.clear();

    _total_steps = 0;
    _safety_violations = 0;
    _tube_violations = 0;
    _tube_occupancy = 0;
    _mpc_successes = 0;
    _mpc_failures = 0;

    _cumulative_dynamic_regret = 0.0;
    _cumulative_safe_regret = 0.0;
    _reference_cost_sum = 0.0;
    _safe_reference_cost_sum = 0.0;

    _current_stl_robustness = 0.0;
    _stl_violations = 0;

    ROS_INFO("MetricsCollector: All metrics reset");
}

void MetricsCollector::setupROSPublishers(ros::NodeHandle& nh)
{
    _pub_empirical_risk = nh.advertise<std_msgs::Float64>("/mpc_metrics/empirical_risk", 1);
    _pub_feasibility_rate = nh.advertise<std_msgs::Float64>("/mpc_metrics/feasibility_rate", 1);
    _pub_tracking_error = nh.advertise<std_msgs::Float64>("/mpc_metrics/tracking_error", 1);
    _pub_solve_time = nh.advertise<std_msgs::Float64>("/mpc_metrics/solve_time_ms", 1);
    _pub_regret_dynamic = nh.advertise<std_msgs::Float64>("/mpc_metrics/regret_dynamic", 1);
    _pub_regret_safe = nh.advertise<std_msgs::Float64>("/mpc_metrics/regret_safe", 1);

    ROS_INFO("MetricsCollector: ROS publishers initialized");
}

void MetricsCollector::publishMetrics()
{
    AggregatedMetrics m = getAggregatedMetrics();

    std_msgs::Float64 msg;

    msg.data = m.empirical_risk;
    _pub_empirical_risk.publish(msg);

    msg.data = m.feasibility_rate;
    _pub_feasibility_rate.publish(msg);

    msg.data = m.mean_tracking_error;
    _pub_tracking_error.publish(msg);

    msg.data = m.median_solve_time_ms;
    _pub_solve_time.publish(msg);

    msg.data = m.average_dynamic_regret;
    _pub_regret_dynamic.publish(msg);

    msg.data = m.average_safe_regret;
    _pub_regret_safe.publish(msg);
}

void MetricsCollector::flushToCSV()
{
    if (_csv_file.is_open()) {
        _csv_file.flush();
    }
}

void MetricsCollector::closeCSV()
{
    if (_csv_file.is_open()) {
        // Write summary
        _csv_file << "\n# Summary Statistics\n";
        _csv_file << "# total_steps," << _total_steps << "\n";
        _csv_file << "# empirical_risk," << getAggregatedMetrics().empirical_risk << "\n";
        _csv_file << "# feasibility_rate," << getAggregatedMetrics().feasibility_rate << "\n";
        _csv_file << "# mean_solve_time_ms," << getAggregatedMetrics().mean_solve_time_ms << "\n";

        _csv_file.close();
        _csv_initialized = false;
        ROS_INFO("MetricsCollector: CSV file closed");
    }
}

std::pair<double, double> MetricsCollector::computeSatisfactionCI(double confidence_level) const
{
    // Wilson score interval for binomial proportion
    if (_total_steps == 0) {
        return {0.0, 0.0};
    }

    double p = getAggregatedMetrics().satisfaction_probability;
    double n = static_cast<double>(_total_steps);
    double z = 1.96;  // For 95% CI (normal approximation)

    if (confidence_level != 0.95) {
        // Approximate z-score for other confidence levels
        // 0.90 -> 1.645, 0.99 -> 2.576
        if (confidence_level >= 0.99) z = 2.576;
        else if (confidence_level >= 0.90) z = 1.645;
    }

    double center = (p + z*z / (2*n)) / (1 + z*z/n);
    double margin = z * std::sqrt((p*(1-p) + z*z/(4*n)) / n) / (1 + z*z/n);

    return {center - margin, center + margin};
}

double MetricsCollector::computeSolveTimePercentile(double percentile) const
{
    if (_solve_times.empty()) {
        return 0.0;
    }

    std::vector<double> sorted_times(_solve_times.begin(), _solve_times.end());
    std::sort(sorted_times.begin(), sorted_times.end());

    size_t index = static_cast<size_t>(percentile / 100.0 * sorted_times.size());
    if (index >= sorted_times.size()) {
        index = sorted_times.size() - 1;
    }

    return sorted_times[index];
}

double MetricsCollector::computeMean(const std::deque<double>& data) const
{
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double MetricsCollector::computeStd(const std::deque<double>& data, double mean) const
{
    if (data.size() < 2) return 0.0;

    double sum_sq_diff = 0.0;
    for (double val : data) {
        double diff = val - mean;
        sum_sq_diff += diff * diff;
    }

    return std::sqrt(sum_sq_diff / (data.size() - 1));
}

double MetricsCollector::computeMedian(std::deque<double> data) const
{
    if (data.empty()) return 0.0;

    std::sort(data.begin(), data.end());
    size_t n = data.size();

    if (n % 2 == 0) {
        return (data[n/2 - 1] + data[n/2]) / 2.0;
    } else {
        return data[n/2];
    }
}

} // namespace Metrics
