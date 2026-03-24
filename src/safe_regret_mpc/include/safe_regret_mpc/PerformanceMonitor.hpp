#ifndef PERFORMANCE_MONITOR_HPP
#define PERFORMANCE_MONITOR_HPP

#include <Eigen/Dense>
#include <vector>
#include <deque>
#include <chrono>
#include <map>

namespace safe_regret_mpc {

/**
 * @brief Performance Monitor for Safe-Regret MPC
 *
 * Monitors and tracks system performance:
 * - Computational metrics (solve times, frequencies)
 * - Tracking performance (errors, deviations)
 * - Safety metrics (margins, violations)
 * - Feasibility metrics
 * - Overall performance score
 */
class PerformanceMonitor {
public:
    using VectorXd = Eigen::VectorXd;

    /**
     * @brief Performance metrics structure
     */
    struct PerformanceMetrics {
        // Computational metrics
        double solve_time_mean;
        double solve_time_max;
        double solve_time_std;
        double frequency;
        size_t total_solves;
        size_t failed_solves;

        // Tracking performance
        double cross_track_error;
        double heading_error;
        double velocity_error;
        double lateral_deviation;
        double control_effort;
        double control_smoothness;

        // Safety metrics
        double safety_margin;
        int constraint_violations;
        double probability_violation;
        double tube_radius;

        // Feasibility metrics
        double feasibility_rate;
        bool recursive_feasibility;
        double terminal_feasibility;

        // Overall performance
        double performance_score;
        std::string performance_grade;

        int time_step;
    };

    /**
     * @brief Timing data
     */
    struct TimingData {
        double mpc_solve_time;
        double stl_eval_time;
        double dr_compute_time;
        double total_time;
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::high_resolution_clock::time_point end_time;
    };

    /**
     * @brief Constructor
     */
    PerformanceMonitor();

    /**
     * @brief Destructor
     */
    ~PerformanceMonitor();

    /**
     * @brief Initialize performance monitor
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Start timing measurement
     */
    void startTiming();

    /**
     * @brief Stop timing measurement
     * @return Timing data
     */
    TimingData stopTiming();

    /**
     * @brief Record solve time
     * @param solve_time Solve time in ms
     * @param success Success flag
     */
    void recordSolveTime(double solve_time, bool success);

    /**
     * @brief Update tracking performance
     * @param cte Cross-track error
     * @param etheta Heading error
     * @param evel Velocity error
     * @param lateral_dev Lateral deviation
     */
    void updateTrackingPerformance(double cte, double etheta,
                                   double evel, double lateral_dev);

    /**
     * @brief Update control performance
     * @param control_effort Control effort norm
     * @param control_smoothness Control smoothness metric
     */
    void updateControlPerformance(double control_effort,
                                  double control_smoothness);

    /**
     * @brief Update safety metrics
     * @param safety_margin Minimum safety margin
     * @param prob_violation Estimated violation probability
     * @param tube_radius Current tube radius
     */
    void updateSafetyMetrics(double safety_margin,
                            double prob_violation,
                            double tube_radius);

    /**
     * @brief Record constraint violation
     */
    void recordViolation();

    /**
     * @brief Update feasibility metrics
     * @param feasible MPC feasibility status
     * @param terminal_feasible Terminal feasibility
     */
    void updateFeasibility(bool feasible, bool terminal_feasible);

    /**
     * @brief Get current performance metrics
     * @return Performance metrics
     */
    PerformanceMetrics getMetrics() const;

    /**
     * @brief Compute overall performance score
     * @return Score in [0, 1]
     */
    double computePerformanceScore();

    /**
     * @brief Get performance grade
     * @return Letter grade (A-F)
     */
    std::string getPerformanceGrade();

    /**
     * @brief Reset performance monitoring
     */
    void reset();

    /**
     * @brief Export performance data
     * @param filename Output filename
     * @return True if successful
     */
    bool exportData(const std::string& filename) const;

    /**
     * @brief Get statistics for a metric
     * @param values Vector of values
     * @return Mean, std dev, min, max
     */
    static std::tuple<double, double, double, double>
    computeStatistics(const std::vector<double>& values);

private:
    /**
     * @brief Update rolling statistics
     */
    void updateStatistics();

    /**
     * @brief Compute control frequency
     * @return Frequency in Hz
     */
    double computeFrequency();

    // Member variables
    std::deque<double> solve_times_;
    std::deque<bool> solve_success_;
    size_t failed_solves_;

    std::deque<double> cte_history_;
    std::deque<double> etheta_history_;
    std::deque<double> evel_history_;
    std::deque<double> lateral_dev_history_;

    std::deque<double> control_effort_history_;
    std::deque<double> control_smoothness_history_;

    double safety_margin_;
    int constraint_violations_;
    double probability_violation_;
    double tube_radius_;

    size_t feasible_solves_;
    size_t total_solves_;
    size_t terminal_feasible_solves_;
    bool recursive_feasibility_;

    std::deque<TimingData> timing_history_;
    std::chrono::high_resolution_clock::time_point current_start_;

    PerformanceMetrics metrics_;
    size_t max_history_size_;

    bool initialized_;
};

} // namespace safe_regret_mpc

#endif // PERFORMANCE_MONITOR_HPP
