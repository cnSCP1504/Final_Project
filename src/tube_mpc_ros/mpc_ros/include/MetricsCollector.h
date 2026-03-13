#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <ros/ros.h>
#include <vector>
#include <deque>
#include <fstream>
#include <Eigen/Eigen>
#include <std_msgs/Float64.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/Twist.h>

// Use Metrics namespace to avoid conflict with TubeMPC class
namespace Metrics {

/**
 * @brief Comprehensive metrics collector for Safe-Regret MPC evaluation
 *
 * Collects metrics according to paper acceptance criteria:
 * 1. Satisfaction probability (p̂_sat)
 * 2. Empirical risk (δ̂)
 * 3. Dynamic and safe regret
 * 4. Recursive feasibility rate
 * 5. Computation time statistics
 * 6. Tracking error and tube occupancy
 * 7. Calibration accuracy
 */
class MetricsCollector {
public:
    struct MetricsSnapshot {
        double timestamp;
        double cte;                  // Cross-track error
        double etheta;              // Heading error
        double vel;                 // Velocity
        double angvel;              // Angular velocity
        double tracking_error_norm; // ||e_t||_2
        double tube_radius;         // Current tube radius
        double tube_violation;      // Tube constraint violation (0 or magnitude)
        double safety_margin;       // h(x_t) value
        double safety_violation;    // Safety constraint violation (0 or 1)
        double mpc_solve_time;      // MPC solve time in ms
        bool mpc_feasible;          // MPC feasibility status
        double cost;                // Instantaneous cost ℓ(x,u)
        double robustness;          // STL robustness ρ(φ) (placeholder for Phase 2)
        double regret_dynamic;      // Dynamic regret contribution
        double regret_safe;         // Safe regret contribution
    };

    struct AggregatedMetrics {
        // Safety and satisfaction
        double empirical_risk;           // δ̂
        double satisfaction_probability;  // p̂_sat
        double safety_violation_rate;     // h(x_t) < 0 rate

        // Feasibility
        double feasibility_rate;          // Recursive feasibility rate
        int total_mpc_calls;
        int successful_mpc_solves;
        int failed_mpc_solves;

        // Tracking performance
        double mean_tracking_error;
        double max_tracking_error;
        double std_tracking_error;
        double tube_violation_rate;
        double tube_occupancy_rate;       // % time within tube

        // Computation
        double mean_solve_time_ms;
        double median_solve_time_ms;
        double p90_solve_time_ms;
        double max_solve_time_ms;

        // Regret (cumulative)
        double cumulative_dynamic_regret;
        double cumulative_safe_regret;
        double average_dynamic_regret;
        double average_safe_regret;

        // Calibration
        double calibration_error;         // |δ̂ - δ_target|
    };

    MetricsCollector(const std::string& output_csv = "/tmp/tube_mpc_metrics.csv");

    // === Core Metrics Recording ===

    /**
     * @brief Record metrics at each control step
     */
    void recordMetrics(const MetricsSnapshot& snapshot);

    /**
     * @brief Record MPC solve result
     */
    void recordMPCSolve(double solve_time_ms, bool feasible, const Eigen::VectorXd& solution);

    /**
     * @brief Update regret calculations
     */
    void updateRegret(double current_cost, double reference_cost,
                     double safe_reference_cost = std::numeric_limits<double>::infinity());

    /**
     * @brief Update STL robustness (placeholder for Phase 2)
     */
    void updateSTLRobustness(double robustness_value);

    // === Safety Constraint Monitoring ===

    /**
     * @brief Check and record safety constraint h(x) >= 0
     * @param h_value Current value of safety function h(x)
     * @param h_threshold Safety threshold (typically 0)
     */
    void recordSafetyConstraint(double h_value, double h_threshold = 0.0);

    /**
     * @brief Set target risk level δ for calibration monitoring
     */
    void setTargetRisk(double delta) { _target_delta = delta; }

    // === Tube Monitoring ===

    /**
     * @brief Record tube constraint status
     * @param error_norm Current tracking error norm ||e_t||
     */
    void recordTubeStatus(const Eigen::VectorXd& error_vector);

    // === Aggregated Metrics ===

    /**
     * @brief Get current aggregated metrics
     */
    AggregatedMetrics getAggregatedMetrics() const;

    /**
     * @brief Print metrics summary to console
     */
    void printSummary() const;

    /**
     * @brief Reset all counters (for new experiment run)
     */
    void reset();

    // === ROS Integration ===

    /**
     * @brief Setup ROS publishers for real-time metrics
     */
    void setupROSPublishers(ros::NodeHandle& nh);

    /**
     * @brief Publish metrics via ROS topics
     */
    void publishMetrics();

    // === File I/O ===

    /**
     * @brief Flush accumulated data to CSV file
     */
    void flushToCSV();

    /**
     * @brief Close CSV file and write summary
     */
    void closeCSV();

    // === Statistical Analysis ===

    /**
     * @brief Compute confidence interval for satisfaction probability
     * @param confidence_level Confidence level (e.g., 0.95 for 95% CI)
     * @return Pair of (lower_bound, upper_bound)
     */
    std::pair<double, double> computeSatisfactionCI(double confidence_level = 0.95) const;

    /**
     * @brief Compute percentile of solve time
     * @param percentile Percentile (0-100)
     */
    double computeSolveTimePercentile(double percentile) const;

private:
    // Data storage
    std::deque<MetricsSnapshot> _metrics_history;
    std::deque<double> _solve_times;
    std::deque<double> _tracking_errors;
    std::deque<bool> _feasibility_history;
    std::deque<double> _safety_margins;

    // Aggregators
    int _total_steps;
    int _safety_violations;
    int _tube_violations;
    int _tube_occupancy;
    int _mpc_successes;
    int _mpc_failures;

    // Regret tracking
    double _cumulative_dynamic_regret;
    double _cumulative_safe_regret;
    double _reference_cost_sum;
    double _safe_reference_cost_sum;

    // STL robustness (for Phase 2)
    double _current_stl_robustness;
    int _stl_violations;

    // Parameters
    double _target_delta;
    double _tube_radius_threshold;
    size_t _max_history_size;

    // File I/O
    std::string _output_csv;
    std::ofstream _csv_file;
    bool _csv_initialized;

    // ROS publishers
    ros::Publisher _pub_empirical_risk;
    ros::Publisher _pub_feasibility_rate;
    ros::Publisher _pub_tracking_error;
    ros::Publisher _pub_solve_time;
    ros::Publisher _pub_regret_dynamic;
    ros::Publisher _pub_regret_safe;

    // === Helper functions ===

    double computeMean(const std::deque<double>& data) const;
    double computeStd(const std::deque<double>& data, double mean) const;
    double computeMedian(std::deque<double> data) const;
    void initializeCSV();
    void writeCSVHeader();
};

} // namespace Metrics

#endif // METRICS_COLLECTOR_H
