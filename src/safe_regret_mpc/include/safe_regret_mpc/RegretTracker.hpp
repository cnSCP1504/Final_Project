#ifndef REGRET_TRACKER_HPP
#define REGRET_TRACKER_HPP

#include <Eigen/Dense>
#include <vector>
#include <deque>

namespace safe_regret_mpc {

/**
 * @brief Regret Tracker for Safe-Regret MPC
 *
 * Tracks and analyzes regret metrics:
 * - Dynamic regret R_T^dyn = Σ ℓ(x_t,u_t) - ℓ(x_t^⋆,u_t^⋆)
 * - Safe regret R_T^safe = min_{π∈Π_safe} R_T^dyn
 * - Reference regret R_T^ref (from abstract planner)
 * - Regret bound verification
 *
 * Key results (Theorem 4.8):
 * - If R_T^ref = o(T) and Σ‖e_k‖ ≤ C_e·√T,
 * - Then R_T^dyn = o(T) and R_T^safe = o(T)
 */
class RegretTracker {
public:
    using VectorXd = Eigen::VectorXd;

    /**
     * @brief Regret metrics structure
     */
    struct RegretMetrics {
        double cumulative_regret;          ///< Σ ℓ(x,u) - ℓ(x^⋆,u^⋆)
        double average_regret;             ///< Cumulative / T
        double instantaneous_regret;       ///< Current step regret
        double reference_regret;           ///< From reference planner
        double safe_regret;                ///< Min over safe policies
        double regret_bound;               ///< Theoretical bound o(T)
        double regret_ratio;               ///< Actual / theoretical
        double tracking_error_bound;       ///< Σ ‖e_k‖

        int time_step;                     ///< Current time step
    };

    /**
     * @brief Cost history entry
     */
    struct CostEntry {
        double actual_cost;                ///< ℓ(x_t,u_t)
        double reference_cost;             ///< ℓ(x_t^⋆,u_t^⋆)
        double oracle_cost;                ///< Optimal cost in hindsight
        VectorXd state;                    ///< State x_t
        VectorXd control;                  ///< Control u_t
        double time_stamp;                 ///< Time stamp
    };

    /**
     * @brief Constructor
     */
    RegretTracker();

    /**
     * @brief Destructor
     */
    ~RegretTracker();

    /**
     * @brief Initialize regret tracker
     * @param max_history Maximum history length
     * @return True if successful
     */
    bool initialize(size_t max_history = 10000);

    /**
     * @brief Update regret with current costs
     * @param actual_cost Actual cost incurred
     * @param reference_cost Reference planner cost
     * @param state Current state
     * @param control Current control
     */
    void update(double actual_cost,
               double reference_cost,
               const VectorXd& state,
               const VectorXd& control);

    /**
     * @brief Update tracking error bound
     * @param tracking_error Current tracking error ‖e_k‖
     */
    void updateTrackingError(double tracking_error);

    /**
     * @brief Get current regret metrics
     * @return Regret metrics
     */
    RegretMetrics getMetrics() const;

    /**
     * @brief Get cumulative regret
     * @return R_T^dyn
     */
    double getCumulativeRegret() const { return metrics_.cumulative_regret; }

    /**
     * @brief Get safe regret
     * @return R_T^safe
     */
    double getSafeRegret() const { return metrics_.safe_regret; }

    /**
     * @brief Get regret bound
     * @return Theoretical bound
     */
    double getRegretBound() const;

    /**
     * @brief Verify regret bound
     * @return True if actual regret ≤ bound
     */
    bool verifyRegretBound() const;

    /**
     * @brief Reset regret tracker
     */
    void reset();

    /**
     * @brief Get cost history
     * @return Cost history
     */
    const std::deque<CostEntry>& getHistory() const { return cost_history_; }

    /**
     * @brief Compute regret statistics
     * @return Mean, std dev, min, max of regret
     */
    std::tuple<double, double, double, double> computeRegretStatistics() const;

    /**
     * @brief Set oracle costs (for evaluation)
     * @param oracle_costs Optimal costs in hindsight
     */
    void setOracleCosts(const std::vector<double>& oracle_costs);

    /**
     * @brief Export regret data
     * @param filename Output filename
     * @return True if successful
     */
    bool exportData(const std::string& filename) const;

private:
    /**
     * @brief Compute safe regret (minimum over safe policies)
     * @return Safe regret value
     */
    double computeSafeRegret() const;

    /**
     * @brief Compute theoretical regret bound
     * @return Bound o(T)
     */
    double computeTheoreticalBound() const;

    /**
     * @brief Check if policy is safe
     * @param costs Cost history of policy
     * @return True if policy satisfies safety constraints
     */
    bool isSafePolicy(const std::vector<double>& costs) const;

    // Member variables
    std::deque<CostEntry> cost_history_;
    std::vector<double> tracking_errors_;
    std::vector<double> oracle_costs_;
    size_t max_history_;

    RegretMetrics metrics_;
    double cumulative_tracking_error_;
    bool has_oracle_costs_;
};

} // namespace safe_regret_mpc

#endif // REGRET_TRACKER_HPP
