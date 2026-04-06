#ifndef SAFE_REGRET_MPC_METRICS_COLLECTOR_HPP
#define SAFE_REGRET_MPC_METRICS_COLLECTOR_HPP

#include <ros/ros.h>
#include <safe_regret_mpc/SafeRegretMetrics.h>
#include <Eigen/Dense>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <map>
#include <numeric>
#include <algorithm>

namespace safe_regret_mpc {

/**
 * @brief Comprehensive metrics collector for Safe-Regret MPC
 *
 * Collects all metrics specified in manuscript Section VI.B.1:
 * - Satisfaction probability
 * - Empirical risk
 * - Dynamic/Safe regret
 * - Recursive feasibility rate
 * - Computation metrics
 * - Tracking error and tube occupancy
 * - Calibration accuracy
 */
class MetricsCollector {
public:
    // Type definitions
    using VectorXd = Eigen::VectorXd;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    /**
     * @brief Snapshot of metrics at a single time step
     */
    struct MetricsSnapshot {
        double timestamp;
        uint32_t time_step;

        // STL metrics
        double stl_robustness;
        double stl_budget;
        double stl_baseline;

        // Safety metrics
        double empirical_risk;
        double safety_margin;
        bool safety_violation;

        // Regret metrics
        double dynamic_regret;
        double safe_regret;
        double reference_cost;
        double actual_cost;

        // MPC metrics
        bool mpc_feasible;
        double mpc_solve_time;
        double mpc_objective_value;

        // Tracking metrics
        double tracking_error_norm;
        double cte;
        double etheta;
        bool tube_occupied;
        uint32_t tube_violation_count;

        // Control metrics
        double linear_vel;
        double angular_vel;
        double stage_cost;

        // Calibration metrics
        double calibration_error;
        bool well_calibrated;

        // Metadata
        bool in_place_rotation;
    };

    /**
     * @brief Summary statistics over an episode
     */
    struct EpisodeSummary {
        // Satisfaction probability
        double satisfaction_prob;
        double satisfaction_ci_lower;
        double satisfaction_ci_upper;

        // Empirical risk
        double empirical_risk;
        double target_risk;

        // Regret (normalized)
        double dynamic_regret_normalized;
        double safe_regret_normalized;

        // Feasibility
        double feasibility_rate;
        uint32_t total_solves;
        uint32_t feasible_solves;
        uint32_t solve_failures;

        // Computation
        double solve_time_median;
        double solve_time_p90;
        double solve_time_mean;

        // Tracking
        double tracking_error_mean;
        double tracking_error_std;
        double tracking_error_max;
        uint32_t tube_violations;

        // Calibration
        double calibration_error_mean;
        double calibration_error_std;
        double well_calibrated_ratio;

        // Episode info
        double total_time;
        uint32_t total_steps;
        std::string task_name;
        std::string scenario_id;
    };

    /**
     * @brief Constructor
     */
    MetricsCollector();

    /**
     * @brief Initialize collector with task configuration
     */
    bool initialize(const std::string& task_name,
                   const std::string& scenario_id,
                   const std::string& method_name,
                   double target_risk_delta,
                   double horizon_n);

    /**
     * @brief Record metrics at current time step
     */
    void recordSnapshot(const MetricsSnapshot& snapshot);

    /**
     * @brief Update cumulative statistics
     */
    void updateStatistics();

    /**
     * @brief Get current ROS metrics message
     */
    safe_regret_mpc::SafeRegretMetrics getMetricsMessage() const;

    /**
     * @brief Get episode summary
     */
    EpisodeSummary getEpisodeSummary() const;

    /**
     * @brief Save time series data to CSV
     */
    bool saveTimeSeriesCSV(const std::string& filepath);

    /**
     * @brief Save summary statistics to CSV
     */
    bool saveSummaryCSV(const std::string& filepath);

    /**
     * @brief Save episode metadata to JSON
     */
    bool saveMetadataJSON(const std::string& filepath);

    /**
     * @brief Reset all metrics (for new episode)
     */
    void reset();

    /**
     * @brief Get number of snapshots recorded
     */
    size_t getSnapshotCount() const { return snapshots_.size(); }

private:
    // Configuration
    std::string task_name_;
    std::string scenario_id_;
    std::string method_name_;
    double target_risk_delta_;
    double horizon_n_;

    // Time series data
    std::vector<MetricsSnapshot> snapshots_;
    TimePoint start_time_;

    // Cumulative statistics
    uint32_t total_steps_;
    uint32_t safety_violations_;
    uint32_t tube_violations_;
    uint32_t feasible_solves_;
    uint32_t total_solves_;

    // Running statistics (for online mean/std)
    double empirical_risk_running_;
    double tracking_error_running_mean_;
    double tracking_error_running_m2_;  // For Welford's algorithm
    double calibration_error_running_mean_;
    double calibration_error_running_m2_;

    // Regret tracking
    double dynamic_regret_cumulative_;
    double safe_regret_cumulative_;
    double reference_cost_cumulative_;
    double actual_cost_cumulative_;

    // STL satisfaction tracking
    std::vector<bool> stl_satisfaction_history_;

    // Solve times for percentile computation
    std::vector<double> solve_times_;

    /**
     * @brief Compute Wilson 95% confidence interval for proportion
     */
    std::pair<double, double> computeWilsonCI(double proportion, size_t n) const;

    /**
     * @brief Compute percentile of data
     */
    double computePercentile(const std::vector<double>& data, double percentile) const;

    /**
     * @brief Get current timestamp in seconds
     */
    double getCurrentTime() const;
};

} // namespace safe_regret_mpc

#endif // SAFE_REGRET_MPC_METRICS_COLLECTOR_HPP
