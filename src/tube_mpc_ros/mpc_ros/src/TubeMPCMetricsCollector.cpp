#include "tube_mpc/TubeMPCMetricsCollector.hpp"
#include <ros/ros.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace tube_mpc {

TubeMPCMetricsCollector::TubeMPCMetricsCollector()
    : total_steps_(0),
      feasible_solves_(0),
      total_solves_(0),
      tube_violations_(0),
      cte_mean_(0.0),
      cte_m2_(0.0),
      etheta_mean_(0.0),
      etheta_m2_(0.0),
      tracking_error_mean_(0.0),
      tracking_error_m2_(0.0),
      control_effort_sum_(0.0) {
    start_time_ = Clock::now();
}

bool TubeMPCMetricsCollector::initialize(const std::string& task_name,
                                        const std::string& scenario_id,
                                        const std::string& method_name) {
    task_name_ = task_name;
    scenario_id_ = scenario_id;
    method_name_ = method_name;

    start_time_ = Clock::now();

    ROS_INFO("TubeMPCMetricsCollector initialized:");
    ROS_INFO("  Task: %s", task_name.c_str());
    ROS_INFO("  Scenario: %s", scenario_id.c_str());
    ROS_INFO("  Method: %s", method_name.c_str());

    return true;
}

void TubeMPCMetricsCollector::recordSnapshot(const MetricsSnapshot& snapshot) {
    snapshots_.push_back(snapshot);
    total_steps_++;

    // Update feasibility
    if (snapshot.mpc_feasible) {
        feasible_solves_++;
    }
    total_solves_++;

    // Update tube violations
    if (!snapshot.tube_occupied) {
        tube_violations_++;
    }

    // Update CTE statistics (Welford's algorithm)
    cte_mean_ += (snapshot.cte - cte_mean_) / total_steps_;
    cte_m2_ += (snapshot.cte - cte_mean_) * (snapshot.cte - cte_mean_);

    // Update etheta statistics
    etheta_mean_ += (snapshot.etheta - etheta_mean_) / total_steps_;
    etheta_m2_ += (snapshot.etheta - etheta_mean_) * (snapshot.etheta - etheta_mean_);

    // Update tracking error statistics
    tracking_error_mean_ += (snapshot.tracking_error_norm - tracking_error_mean_) / total_steps_;
    tracking_error_m2_ += (snapshot.tracking_error_norm - tracking_error_mean_) *
                           (snapshot.tracking_error_norm - tracking_error_mean_);

    // Update control effort
    control_effort_sum_ += std::abs(snapshot.linear_vel) + std::abs(snapshot.angular_vel);

    // Track solve times
    if (snapshot.mpc_solve_time > 0) {
        solve_times_.push_back(snapshot.mpc_solve_time);
    }
}

safe_regret_mpc::SafeRegretMetrics TubeMPCMetricsCollector::getMetricsMessage() const {
    safe_regret_mpc::SafeRegretMetrics msg;

    msg.header.stamp = ros::Time::now();

    // Task information
    msg.task_name = task_name_;
    msg.scenario_id = scenario_id_;
    msg.method_name = method_name_;

    // Time
    msg.experiment_time = getCurrentTime();
    msg.time_step = total_steps_;
    msg.total_steps = total_steps_;

    if (!snapshots_.empty()) {
        const auto& latest = snapshots_.back();

        // State and tracking
        msg.cte = latest.cte;
        msg.heading_error = latest.etheta;
        msg.tracking_error_norm = latest.tracking_error_norm;

        // Control
        msg.cmd_linear_vel = latest.linear_vel;
        msg.cmd_angular_vel = latest.angular_vel;

        // MPC
        msg.mpc_feasible = latest.mpc_feasible;
        msg.mpc_solve_time = latest.mpc_solve_time;
        msg.mpc_objective_value = latest.mpc_objective_value;

        // Tube
        msg.tube_radius = latest.tube_radius;
        msg.tube_occupied = latest.tube_occupied;
        msg.tube_violation_count = tube_violations_;

        // Stage cost
        msg.stage_cost = latest.stage_cost;

        // In-place rotation
        msg.in_place_rotation = latest.in_place_rotation;
        msg.heading_error_degrees = std::abs(latest.etheta * 180.0 / M_PI);

        // Feasibility rate
        msg.feasibility_rate = total_solves_ > 0 ?
            static_cast<double>(feasible_solves_) / total_solves_ : 0.0;
        msg.feasibility_count = feasible_solves_;
        msg.total_solve_count = total_solves_;

        // Placeholder values for STL/DR (not available in tube_mpc mode)
        msg.stl_robustness = latest.stl_robustness_proxy;
        msg.stl_robustness_budget = 1.0;  // Placeholder
        msg.empirical_risk = latest.safety_margin_proxy > 0 ? 0.0 : 1.0;
        msg.safety_violation_count = latest.safety_margin_proxy < 0 ? 1 : 0;

        // Regret (use stage cost as proxy)
        msg.dynamic_regret_instant = latest.stage_cost;
        msg.safe_regret_instant = latest.stage_cost;
    }

    return msg;
}

TubeMPCMetricsCollector::EpisodeSummary TubeMPCMetricsCollector::getEpisodeSummary() const {
    EpisodeSummary summary;

    // Basic info
    summary.task_name = task_name_;
    summary.scenario_id = scenario_id_;
    summary.total_steps = total_steps_;
    summary.total_time = getCurrentTime();

    // CTE statistics
    summary.cte_mean = cte_mean_;
    summary.cte_std = total_steps_ > 1 ? std::sqrt(cte_m2_ / (total_steps_ - 1)) : 0.0;

    if (!snapshots_.empty()) {
        auto max_cte = std::max_element(snapshots_.begin(), snapshots_.end(),
            [](const MetricsSnapshot& a, const MetricsSnapshot& b) {
                return std::abs(a.cte) < std::abs(b.cte);
            });
        summary.cte_max = std::abs(max_cte->cte);
    }

    // Heading error statistics
    summary.etheta_mean = etheta_mean_;
    summary.etheta_std = total_steps_ > 1 ? std::sqrt(etheta_m2_ / (total_steps_ - 1)) : 0.0;

    // Feasibility
    summary.feasibility_rate = total_solves_ > 0 ?
        static_cast<double>(feasible_solves_) / total_solves_ : 0.0;
    summary.solve_failures = total_solves_ - feasible_solves_;

    // Solve time statistics
    if (!solve_times_.empty()) {
        std::vector<double> sorted = solve_times_;
        std::sort(sorted.begin(), sorted.end());

        summary.solve_time_median = sorted[sorted.size() / 2];
        summary.solve_time_p90 = computePercentile(sorted, 0.90);
        summary.solve_time_mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
    }

    // Tracking error
    summary.tracking_error_mean = tracking_error_mean_;
    summary.tracking_error_std = total_steps_ > 1 ?
        std::sqrt(tracking_error_m2_ / (total_steps_ - 1)) : 0.0;

    if (!snapshots_.empty()) {
        auto max_error = std::max_element(snapshots_.begin(), snapshots_.end(),
            [](const MetricsSnapshot& a, const MetricsSnapshot& b) {
                return a.tracking_error_norm < b.tracking_error_norm;
            });
        summary.tracking_error_max = max_error->tracking_error_norm;
    }

    summary.tube_violations = tube_violations_;
    summary.tube_violation_rate = static_cast<double>(tube_violations_) / total_steps_;

    // Control effort
    summary.control_effort_mean = control_effort_sum_ / total_steps_;

    return summary;
}

bool TubeMPCMetricsCollector::saveTimeSeriesCSV(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        ROS_ERROR("Failed to open file: %s", filepath.c_str());
        return false;
    }

    // Write header
    file << "timestamp,time_step,x,y,theta,linear_vel,angular_vel,"
         << "cte,etheta,tracking_error_norm,tube_radius,tube_occupied,"
         << "mpc_feasible,mpc_solve_time,stage_cost,in_place_rotation\n";

    // Write data
    for (const auto& snapshot : snapshots_) {
        file << std::fixed << std::setprecision(6)
             << snapshot.timestamp << ","
             << snapshot.time_step << ","
             << snapshot.x << ","
             << snapshot.y << ","
             << snapshot.theta << ","
             << snapshot.linear_vel << ","
             << snapshot.angular_vel << ","
             << snapshot.cte << ","
             << snapshot.etheta << ","
             << snapshot.tracking_error_norm << ","
             << snapshot.tube_radius << ","
             << (snapshot.tube_occupied ? 1 : 0) << ","
             << (snapshot.mpc_feasible ? 1 : 0) << ","
             << snapshot.mpc_solve_time << ","
             << snapshot.stage_cost << ","
             << (snapshot.in_place_rotation ? 1 : 0) << "\n";
    }

    file.close();
    ROS_INFO("Saved time series to: %s (%zu records)", filepath.c_str(), snapshots_.size());
    return true;
}

bool TubeMPCMetricsCollector::saveSummaryCSV(const std::string& filepath) {
    EpisodeSummary summary = getEpisodeSummary();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        ROS_ERROR("Failed to open file: %s", filepath.c_str());
        return false;
    }

    file << "metric,value,unit\n";

    // Tracking performance
    file << "cte_mean," << summary.cte_mean << ",m\n";
    file << "cte_std," << summary.cte_std << ",m\n";
    file << "cte_max," << summary.cte_max << ",m\n";
    file << "etheta_mean," << summary.etheta_mean << ",rad\n";
    file << "etheta_std," << summary.etheta_std << ",rad\n";

    // MPC performance
    file << "feasibility_rate," << summary.feasibility_rate << ",ratio\n";
    file << "solve_time_median," << summary.solve_time_median << ",ms\n";
    file << "solve_time_p90," << summary.solve_time_p90 << ",ms\n";
    file << "solve_time_mean," << summary.solve_time_mean << ",ms\n";
    file << "solve_failures," << summary.solve_failures << ",count\n";

    // Tube performance
    file << "tracking_error_mean," << summary.tracking_error_mean << ",m\n";
    file << "tracking_error_std," << summary.tracking_error_std << ",m\n";
    file << "tracking_error_max," << summary.tracking_error_max << ",m\n";
    file << "tube_violations," << summary.tube_violations << ",count\n";
    file << "tube_violation_rate," << summary.tube_violation_rate << ",ratio\n";

    // Control effort
    file << "control_effort_mean," << summary.control_effort_mean << ",units\n";

    // Episode info
    file << "total_time," << summary.total_time << ",s\n";
    file << "total_steps," << summary.total_steps << ",steps\n";
    file << "task_name," << summary.task_name << ",string\n";
    file << "scenario_id," << summary.scenario_id << ",string\n";

    file.close();
    ROS_INFO("Saved summary to: %s", filepath.c_str());
    return true;
}

bool TubeMPCMetricsCollector::saveMetadataJSON(const std::string& filepath) {
    EpisodeSummary summary = getEpisodeSummary();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        ROS_ERROR("Failed to open file: %s", filepath.c_str());
        return false;
    }

    file << "{\n";
    file << "  \"task_name\": \"" << summary.task_name << "\",\n";
    file << "  \"scenario_id\": \"" << summary.scenario_id << "\",\n";
    file << "  \"method_name\": \"" << method_name_ << "\",\n";
    file << "  \"total_steps\": " << summary.total_steps << ",\n";
    file << "  \"total_time\": " << summary.total_time << ",\n";
    file << "  \"mode\": \"tube_mpc_standalone\",\n";
    file << "  \"results\": {\n";
    file << "    \"cte_mean\": " << summary.cte_mean << ",\n";
    file << "    \"etheta_mean\": " << summary.etheta_mean << ",\n";
    file << "    \"feasibility_rate\": " << summary.feasibility_rate << ",\n";
    file << "    \"solve_time_median\": " << summary.solve_time_median << ",\n";
    file << "    \"tracking_error_mean\": " << summary.tracking_error_mean << ",\n";
    file << "    \"tube_violations\": " << summary.tube_violations << "\n";
    file << "  }\n";
    file << "}\n";

    file.close();
    ROS_INFO("Saved metadata to: %s", filepath.c_str());
    return true;
}

void TubeMPCMetricsCollector::reset() {
    snapshots_.clear();
    start_time_ = Clock::now();

    total_steps_ = 0;
    feasible_solves_ = 0;
    total_solves_ = 0;
    tube_violations_ = 0;

    cte_mean_ = 0.0;
    cte_m2_ = 0.0;
    etheta_mean_ = 0.0;
    etheta_m2_ = 0.0;
    tracking_error_mean_ = 0.0;
    tracking_error_m2_ = 0.0;
    control_effort_sum_ = 0.0;

    solve_times_.clear();

    ROS_INFO("TubeMPCMetricsCollector reset");
}

double TubeMPCMetricsCollector::getCurrentTime() const {
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    return duration.count() / 1000.0;
}

double TubeMPCMetricsCollector::computePercentile(const std::vector<double>& data, double p) const {
    if (data.empty()) return 0.0;

    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(p * (sorted.size() - 1));
    return sorted[index];
}

} // namespace tube_mpc
