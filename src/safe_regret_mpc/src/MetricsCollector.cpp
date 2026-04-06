#include "safe_regret_mpc/MetricsCollector.hpp"
#include <ros/ros.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace safe_regret_mpc {

MetricsCollector::MetricsCollector()
    : target_risk_delta_(0.1),
      horizon_n_(20),
      total_steps_(0),
      safety_violations_(0),
      tube_violations_(0),
      feasible_solves_(0),
      total_solves_(0),
      empirical_risk_running_(0.0),
      tracking_error_running_mean_(0.0),
      tracking_error_running_m2_(0.0),
      calibration_error_running_mean_(0.0),
      calibration_error_running_m2_(0.0),
      dynamic_regret_cumulative_(0.0),
      safe_regret_cumulative_(0.0),
      reference_cost_cumulative_(0.0),
      actual_cost_cumulative_(0.0) {
    start_time_ = Clock::now();
}

bool MetricsCollector::initialize(const std::string& task_name,
                                  const std::string& scenario_id,
                                  const std::string& method_name,
                                  double target_risk_delta,
                                  double horizon_n) {
    task_name_ = task_name;
    scenario_id_ = scenario_id;
    method_name_ = method_name;
    target_risk_delta_ = target_risk_delta;
    horizon_n_ = horizon_n;

    start_time_ = Clock::now();

    ROS_INFO("MetricsCollector initialized:");
    ROS_INFO("  Task: %s", task_name.c_str());
    ROS_INFO("  Scenario: %s", scenario_id.c_str());
    ROS_INFO("  Method: %s", method_name.c_str());
    ROS_INFO("  Target risk: %.3f", target_risk_delta);
    ROS_INFO("  Horizon: %.0f", horizon_n);

    return true;
}

void MetricsCollector::recordSnapshot(const MetricsSnapshot& snapshot) {
    snapshots_.push_back(snapshot);
    total_steps_++;

    // Update cumulative statistics
    if (snapshot.safety_violation) {
        safety_violations_++;
    }

    if (!snapshot.tube_occupied) {
        tube_violations_++;
    }

    if (snapshot.mpc_feasible) {
        feasible_solves_++;
    }
    total_solves_++;

    // Update empirical risk running average
    double current_empirical_risk = snapshot.safety_violation ? 1.0 : 0.0;
    empirical_risk_running_ += (current_empirical_risk - empirical_risk_running_) / total_steps_;

    // Update tracking error statistics (Welford's algorithm)
    tracking_error_running_mean_ += (snapshot.tracking_error_norm - tracking_error_running_mean_) / total_steps_;
    tracking_error_running_m2_ += (snapshot.tracking_error_norm - tracking_error_running_mean_) *
                                   (snapshot.tracking_error_norm - tracking_error_running_mean_);

    // Update calibration error statistics
    calibration_error_running_mean_ += (snapshot.calibration_error - calibration_error_running_mean_) / total_steps_;
    calibration_error_running_m2_ += (snapshot.calibration_error - calibration_error_running_mean_) *
                                      (snapshot.calibration_error - calibration_error_running_mean_);

    // Update regret
    dynamic_regret_cumulative_ += snapshot.dynamic_regret;
    safe_regret_cumulative_ += snapshot.safe_regret;
    reference_cost_cumulative_ += snapshot.reference_cost;
    actual_cost_cumulative_ += snapshot.actual_cost;

    // Track STL satisfaction
    stl_satisfaction_history_.push_back(snapshot.stl_robustness >= 0.0);

    // Track solve times
    if (snapshot.mpc_solve_time > 0) {
        solve_times_.push_back(snapshot.mpc_solve_time);
    }
}

void MetricsCollector::updateStatistics() {
    // Statistics are updated incrementally in recordSnapshot()
    // This method can be used for batch updates if needed
}

safe_regret_mpc::SafeRegretMetrics MetricsCollector::getMetricsMessage() const {
    safe_regret_mpc::SafeRegretMetrics msg;

    // Header
    msg.header.stamp = ros::Time::now();

    // Task information
    msg.task_name = task_name_;
    msg.scenario_id = scenario_id_;
    msg.method_name = method_name_;

    // Time information
    msg.experiment_time = getCurrentTime();
    msg.time_step = total_steps_;
    msg.total_steps = total_steps_;

    // Configuration
    msg.horizon_n = horizon_n_;
    msg.risk_delta = target_risk_delta_;

    if (!snapshots_.empty()) {
        const auto& latest = snapshots_.back();

        // STL metrics
        msg.stl_robustness = latest.stl_robustness;
        msg.stl_robustness_budget = latest.stl_budget;
        msg.stl_robustness_baseline = latest.stl_baseline;

        // Safety metrics
        msg.empirical_risk = empirical_risk_running_;
        msg.target_risk = target_risk_delta_;
        msg.safety_violation_count = safety_violations_;

        // Regret metrics
        msg.dynamic_regret_cumulative = dynamic_regret_cumulative_;
        msg.safe_regret_cumulative = safe_regret_cumulative_;

        if (total_steps_ > 0) {
            msg.dynamic_regret_normalized = dynamic_regret_cumulative_ / total_steps_;
            msg.safe_regret_normalized = safe_regret_cumulative_ / total_steps_;
        }

        // MPC metrics
        msg.mpc_feasible = latest.mpc_feasible;
        msg.mpc_solve_time = latest.mpc_solve_time;
        msg.mpc_objective_value = latest.mpc_objective_value;

        msg.feasibility_rate = total_solves_ > 0 ?
            static_cast<double>(feasible_solves_) / total_solves_ : 0.0;
        msg.feasibility_count = feasible_solves_;
        msg.total_solve_count = total_solves_;

        // Tracking metrics
        msg.tracking_error_norm = latest.tracking_error_norm;
        msg.cte = latest.cte;
        msg.heading_error = latest.etheta;
        msg.tube_occupied = latest.tube_occupied;
        msg.tube_violation_count = tube_violations_;

        // Control metrics
        msg.cmd_linear_vel = latest.linear_vel;
        msg.cmd_angular_vel = latest.angular_vel;
        msg.stage_cost = latest.stage_cost;
        msg.in_place_rotation = latest.in_place_rotation;
        msg.heading_error_degrees = std::abs(latest.etheta * 180.0 / M_PI);

        // Calibration metrics
        msg.calibration_error = latest.calibration_error;
        msg.well_calibrated = latest.well_calibrated;
    }

    return msg;
}

MetricsCollector::EpisodeSummary MetricsCollector::getEpisodeSummary() const {
    EpisodeSummary summary;

    // Basic info
    summary.task_name = task_name_;
    summary.scenario_id = scenario_id_;
    summary.total_steps = total_steps_;
    summary.total_time = getCurrentTime();

    // Satisfaction probability with Wilson CI
    if (!stl_satisfaction_history_.empty()) {
        int satisfied_count = std::count(stl_satisfaction_history_.begin(),
                                         stl_satisfaction_history_.end(), true);
        summary.satisfaction_prob = static_cast<double>(satisfied_count) / total_steps_;

        auto ci = computeWilsonCI(summary.satisfaction_prob, total_steps_);
        summary.satisfaction_ci_lower = ci.first;
        summary.satisfaction_ci_upper = ci.second;
    }

    // Empirical risk
    summary.empirical_risk = empirical_risk_running_;
    summary.target_risk = target_risk_delta_;

    // Regret (normalized)
    if (total_steps_ > 0) {
        summary.dynamic_regret_normalized = dynamic_regret_cumulative_ / total_steps_;
        summary.safe_regret_normalized = safe_regret_cumulative_ / total_steps_;
    }

    // Feasibility
    summary.feasibility_rate = total_solves_ > 0 ?
        static_cast<double>(feasible_solves_) / total_solves_ : 0.0;
    summary.total_solves = total_solves_;
    summary.feasible_solves = feasible_solves_;
    summary.solve_failures = total_solves_ - feasible_solves_;

    // Computation
    if (!solve_times_.empty()) {
        std::vector<double> sorted_times = solve_times_;
        std::sort(sorted_times.begin(), sorted_times.end());

        summary.solve_time_median = sorted_times[sorted_times.size() / 2];
        summary.solve_time_p90 = computePercentile(sorted_times, 0.90);
        summary.solve_time_mean = std::accumulate(solve_times_.begin(),
                                                   solve_times_.end(), 0.0) / solve_times_.size();
    }

    // Tracking error
    summary.tracking_error_mean = tracking_error_running_mean_;
    summary.tracking_error_std = total_steps_ > 1 ?
        std::sqrt(tracking_error_running_m2_ / (total_steps_ - 1)) : 0.0;

    if (!snapshots_.empty()) {
        auto max_it = std::max_element(snapshots_.begin(), snapshots_.end(),
            [](const MetricsSnapshot& a, const MetricsSnapshot& b) {
                return a.tracking_error_norm < b.tracking_error_norm;
            });
        summary.tracking_error_max = max_it->tracking_error_norm;
    }

    summary.tube_violations = tube_violations_;

    // Calibration
    summary.calibration_error_mean = calibration_error_running_mean_;
    summary.calibration_error_std = total_steps_ > 1 ?
        std::sqrt(calibration_error_running_m2_ / (total_steps_ - 1)) : 0.0;

    int well_calibrated_count = std::count_if(snapshots_.begin(), snapshots_.end(),
        [](const MetricsSnapshot& s) { return s.well_calibrated; });
    summary.well_calibrated_ratio = static_cast<double>(well_calibrated_count) / total_steps_;

    return summary;
}

bool MetricsCollector::saveTimeSeriesCSV(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        ROS_ERROR("Failed to open file for writing: %s", filepath.c_str());
        return false;
    }

    // Write header
    file << "timestamp,time_step,stl_robustness,stl_budget,stl_baseline,"
         << "empirical_risk,safety_margin,safety_violation,"
         << "dynamic_regret,safe_regret,reference_cost,actual_cost,"
         << "mpc_feasible,mpc_solve_time,mpc_objective_value,"
         << "tracking_error_norm,cte,etheta,tube_occupied,tube_violation_count,"
         << "linear_vel,angular_vel,stage_cost,in_place_rotation,"
         << "calibration_error,well_calibrated\n";

    // Write data
    for (const auto& snapshot : snapshots_) {
        file << std::fixed << std::setprecision(6)
             << snapshot.timestamp << ","
             << snapshot.time_step << ","
             << snapshot.stl_robustness << ","
             << snapshot.stl_budget << ","
             << snapshot.stl_baseline << ","
             << snapshot.empirical_risk << ","
             << snapshot.safety_margin << ","
             << (snapshot.safety_violation ? 1 : 0) << ","
             << snapshot.dynamic_regret << ","
             << snapshot.safe_regret << ","
             << snapshot.reference_cost << ","
             << snapshot.actual_cost << ","
             << (snapshot.mpc_feasible ? 1 : 0) << ","
             << snapshot.mpc_solve_time << ","
             << snapshot.mpc_objective_value << ","
             << snapshot.tracking_error_norm << ","
             << snapshot.cte << ","
             << snapshot.etheta << ","
             << (snapshot.tube_occupied ? 1 : 0) << ","
             << snapshot.tube_violation_count << ","
             << snapshot.linear_vel << ","
             << snapshot.angular_vel << ","
             << snapshot.stage_cost << ","
             << (snapshot.in_place_rotation ? 1 : 0) << ","
             << snapshot.calibration_error << ","
             << (snapshot.well_calibrated ? 1 : 0) << "\n";
    }

    file.close();
    ROS_INFO("Saved time series data to: %s (%zu records)", filepath.c_str(), snapshots_.size());
    return true;
}

bool MetricsCollector::saveSummaryCSV(const std::string& filepath) {
    EpisodeSummary summary = getEpisodeSummary();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        ROS_ERROR("Failed to open file for writing: %s", filepath.c_str());
        return false;
    }

    // Write header
    file << "metric,value,unit\n";

    // Satisfaction probability
    file << "satisfaction_prob," << summary.satisfaction_prob << ",ratio\n";
    file << "satisfaction_ci_lower," << summary.satisfaction_ci_lower << ",ratio\n";
    file << "satisfaction_ci_upper," << summary.satisfaction_ci_upper << ",ratio\n";

    // Empirical risk
    file << "empirical_risk," << summary.empirical_risk << ",ratio\n";
    file << "target_risk," << summary.target_risk << ",ratio\n";

    // Regret
    file << "dynamic_regret_normalized," << summary.dynamic_regret_normalized << ",cost_per_step\n";
    file << "safe_regret_normalized," << summary.safe_regret_normalized << ",cost_per_step\n";

    // Feasibility
    file << "feasibility_rate," << summary.feasibility_rate << ",ratio\n";
    file << "feasible_solves," << summary.feasible_solves << ",count\n";
    file << "total_solves," << summary.total_solves << ",count\n";
    file << "solve_failures," << summary.solve_failures << ",count\n";

    // Computation
    file << "solve_time_median," << summary.solve_time_median << ",ms\n";
    file << "solve_time_p90," << summary.solve_time_p90 << ",ms\n";
    file << "solve_time_mean," << summary.solve_time_mean << ",ms\n";

    // Tracking
    file << "tracking_error_mean," << summary.tracking_error_mean << ",m\n";
    file << "tracking_error_std," << summary.tracking_error_std << ",m\n";
    file << "tracking_error_max," << summary.tracking_error_max << ",m\n";
    file << "tube_violations," << summary.tube_violations << ",count\n";

    // Calibration
    file << "calibration_error_mean," << summary.calibration_error_mean << ",ratio\n";
    file << "calibration_error_std," << summary.calibration_error_std << ",ratio\n";
    file << "well_calibrated_ratio," << summary.well_calibrated_ratio << ",ratio\n";

    // Episode info
    file << "total_time," << summary.total_time << ",s\n";
    file << "total_steps," << summary.total_steps << ",steps\n";
    file << "task_name," << summary.task_name << ",string\n";
    file << "scenario_id," << summary.scenario_id << ",string\n";

    file.close();
    ROS_INFO("Saved summary statistics to: %s", filepath.c_str());
    return true;
}

bool MetricsCollector::saveMetadataJSON(const std::string& filepath) {
    EpisodeSummary summary = getEpisodeSummary();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        ROS_ERROR("Failed to open file for writing: %s", filepath.c_str());
        return false;
    }

    file << "{\n";
    file << "  \"task_name\": \"" << summary.task_name << "\",\n";
    file << "  \"scenario_id\": \"" << summary.scenario_id << "\",\n";
    file << "  \"method_name\": \"" << method_name_ << "\",\n";
    file << "  \"total_steps\": " << summary.total_steps << ",\n";
    file << "  \"total_time\": " << summary.total_time << ",\n";
    file << "  \"configuration\": {\n";
    file << "    \"horizon_n\": " << horizon_n_ << ",\n";
    file << "    \"target_risk_delta\": " << target_risk_delta_ << "\n";
    file << "  },\n";
    file << "  \"results\": {\n";
    file << "    \"satisfaction_prob\": " << summary.satisfaction_prob << ",\n";
    file << "    \"satisfaction_ci\": [" << summary.satisfaction_ci_lower
         << ", " << summary.satisfaction_ci_upper << "],\n";
    file << "    \"empirical_risk\": " << summary.empirical_risk << ",\n";
    file << "    \"dynamic_regret_normalized\": " << summary.dynamic_regret_normalized << ",\n";
    file << "    \"safe_regret_normalized\": " << summary.safe_regret_normalized << ",\n";
    file << "    \"feasibility_rate\": " << summary.feasibility_rate << ",\n";
    file << "    \"solve_time_median\": " << summary.solve_time_median << ",\n";
    file << "    \"solve_time_p90\": " << summary.solve_time_p90 << "\n";
    file << "  }\n";
    file << "}\n";

    file.close();
    ROS_INFO("Saved metadata to: %s", filepath.c_str());
    return true;
}

void MetricsCollector::reset() {
    snapshots_.clear();
    start_time_ = Clock::now();

    total_steps_ = 0;
    safety_violations_ = 0;
    tube_violations_ = 0;
    feasible_solves_ = 0;
    total_solves_ = 0;

    empirical_risk_running_ = 0.0;
    tracking_error_running_mean_ = 0.0;
    tracking_error_running_m2_ = 0.0;
    calibration_error_running_mean_ = 0.0;
    calibration_error_running_m2_ = 0.0;

    dynamic_regret_cumulative_ = 0.0;
    safe_regret_cumulative_ = 0.0;
    reference_cost_cumulative_ = 0.0;
    actual_cost_cumulative_ = 0.0;

    stl_satisfaction_history_.clear();
    solve_times_.clear();

    ROS_INFO("MetricsCollector reset");
}

std::pair<double, double> MetricsCollector::computeWilsonCI(double proportion, size_t n) const {
    if (n == 0) return {0.0, 0.0};

    double z = 1.96;  // 95% confidence
    double denominator = 1.0 + z * z / n;

    double center = (proportion + z * z / (2.0 * n)) / denominator;
    double margin = z * std::sqrt((proportion * (1.0 - proportion) + z * z / (4.0 * n)) / n) / denominator;

    return {center - margin, center + margin};
}

double MetricsCollector::computePercentile(const std::vector<double>& data, double percentile) const {
    if (data.empty()) return 0.0;

    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(percentile * (sorted.size() - 1));
    return sorted[index];
}

double MetricsCollector::getCurrentTime() const {
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    return duration.count() / 1000.0;
}

} // namespace safe_regret_mpc
