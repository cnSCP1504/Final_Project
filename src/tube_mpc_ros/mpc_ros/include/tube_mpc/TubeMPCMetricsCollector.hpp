#ifndef TUBE_MPC_METRICS_COLLECTOR_HPP
#define TUBE_MPC_METRICS_COLLECTOR_HPP

#include <ros/ros.h>
#include <safe_regret_mpc/SafeRegretMetrics.h>
#include <Eigen/Dense>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <cstdint>

namespace tube_mpc {

/**
 * @brief Metrics collector specifically for Tube MPC mode
 *
 * Collects all available metrics when running in standalone tube_mpc mode:
 * - Tracking performance (CTE, eθ, tracking error norm)
 * - MPC computation (solve time, feasibility)
 * - Control inputs (linear/angular velocity)
 * - Tube occupancy and violations
 * - Stage cost
 *
 * For STL/DR metrics, uses placeholder values or simple proxies.
 */
class TubeMPCMetricsCollector {
public:
    using VectorXd = Eigen::VectorXd;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    /**
     * @brief Snapshot of metrics at a single time step
     */
    struct MetricsSnapshot {
        double timestamp;
        unsigned int time_step;

        // State and tracking
        double x, y, theta;          // Robot position
        double linear_vel, angular_vel;  // Control inputs
        double cte;                   // Cross-track error
        double etheta;                // Heading error (radians)
        double tracking_error_norm;   // ‖e_t‖_2

        // MPC metrics
        bool mpc_feasible;
        double mpc_solve_time;
        double mpc_objective_value;
        unsigned int mpc_iterations;

        // Tube metrics
        double tube_radius;
        bool tube_occupied;
        unsigned int tube_violation_count;

        // Stage cost
        double stage_cost;
        double tracking_cost;
        double control_cost;

        // In-place rotation
        bool in_place_rotation;

        // Placeholder for STL/DR (used in safe_regret_mpc mode)
        double stl_robustness_proxy;    // Simple proxy (e.g., -tracking_error)
        double safety_margin_proxy;     // Simple proxy (e.g., distance to obstacle)
    };

    /**
     * @brief Summary statistics over an episode
     */
    struct EpisodeSummary {
        // Tracking performance
        double cte_mean;
        double cte_std;
        double cte_max;
        double etheta_mean;
        double etheta_std;

        // MPC performance
        double feasibility_rate;
        double solve_time_median;
        double solve_time_p90;
        double solve_time_mean;
        unsigned int solve_failures;

        // Tube performance
        double tracking_error_mean;
        double tracking_error_std;
        double tracking_error_max;
        unsigned int tube_violations;
        double tube_violation_rate;

        // Control effort
        double control_effort_mean;
        double linear_vel_mean;
        double angular_vel_mean;

        // Episode info
        double total_time;
        unsigned int total_steps;
        std::string task_name;
        std::string scenario_id;
    };

    /**
     * @brief Constructor
     */
    TubeMPCMetricsCollector();

    /**
     * @brief Initialize collector
     */
    bool initialize(const std::string& task_name,
                   const std::string& scenario_id,
                   const std::string& method_name);

    /**
     * @brief Record metrics at current time step
     */
    void recordSnapshot(const MetricsSnapshot& snapshot);

    /**
     * @brief Get ROS metrics message (compatible with SafeRegretMetrics)
     */
    safe_regret_mpc::SafeRegretMetrics getMetricsMessage() const;

    /**
     * @brief Get episode summary
     */
    EpisodeSummary getEpisodeSummary() const;

    /**
     * @brief Save to CSV files
     */
    bool saveTimeSeriesCSV(const std::string& filepath);
    bool saveSummaryCSV(const std::string& filepath);
    bool saveMetadataJSON(const std::string& filepath);

    /**
     * @brief Reset all metrics
     */
    void reset();

    /**
     * @brief Get snapshot count
     */
    size_t getSnapshotCount() const { return snapshots_.size(); }

private:
    std::string task_name_;
    std::string scenario_id_;
    std::string method_name_;

    std::vector<MetricsSnapshot> snapshots_;
    TimePoint start_time_;

    unsigned int total_steps_;
    unsigned int feasible_solves_;
    unsigned int total_solves_;
    unsigned int tube_violations_;

    // Running statistics
    double cte_mean_;
    double cte_m2_;
    double etheta_mean_;
    double etheta_m2_;
    double tracking_error_mean_;
    double tracking_error_m2_;
    double control_effort_sum_;

    // Solve times
    std::vector<double> solve_times_;

    double getCurrentTime() const;
    double computePercentile(const std::vector<double>& data, double p) const;
};

} // namespace tube_mpc

#endif // TUBE_MPC_METRICS_COLLECTOR_HPP
