#ifndef STL_MONITOR_ROBUSTNESSBUDGET_H
#define STL_MONITOR_ROBUSTNESSBUDGET_H

#include "stl_monitor/STLTypes.h"
#include <deque>

namespace stl_monitor {

/**
 * @brief Rolling robustness budget mechanism
 *
 * Maintains budget R_k that tracks cumulative robustness:
 * R_{k+1} = max{0, R_k + ρ̃_k - r̲}
 *
 * Where:
 * - R_k: Budget at time k
 * - ρ̃_k: Expected robustness at time k
 * - r̲: Baseline requirement per step
 *
 * Purpose: Prevent STL satisfaction from eroding over rolling horizons
 */
class RobustnessBudget {
public:
    RobustnessBudget();
    ~RobustnessBudget();

    /**
     * @brief Initialize budget
     * @param initial_value Initial budget R_0
     * @param baseline_requirement Per-step requirement r̲
     */
    void initialize(double initial_value = 1.0, double baseline_requirement = 0.1);

    /**
     * @brief Update budget with new robustness value
     * @param robustness Current robustness ρ_k
     * @return New budget value
     */
    double update(double robustness);

    /**
     * @brief Get current budget
     */
    double getBudget() const;

    /**
     * @brief Get budget state
     */
    const BudgetState& getState() const;

    /**
     * @brief Reset budget to initial value
     */
    void reset();

    /**
     * @brief Check if budget is healthy (above threshold)
     */
    bool isHealthy(double threshold = 0.0) const;

    /**
     * @brief Set baseline requirement
     */
    void setBaselineRequirement(double r_baseline);

    /**
     * @brief Enable/disable budget decay
     * @param decay_rate Decay rate (0.0 = no decay)
     */
    void setDecayRate(double decay_rate);

    /**
     * @brief Get budget history
     */
    const std::deque<double>& getHistory() const;
    void clearHistory();
    void setHistorySize(size_t size);

    /**
     * @brief Get budget statistics
     */
    struct BudgetStats {
        double mean;
        double min;
        double max;
        double std;
        size_t violation_count;
    };
    BudgetStats getStatistics() const;

    /**
     * @brief Predict future budget (for MPC lookahead)
     * @param horizon Prediction steps
     * @param expected_robustness Expected future robustness
     * @return Predicted budget values
     */
    std::vector<double> predictFuture(
        size_t horizon,
        double expected_robustness
    ) const;

    /**
     * @brief Check if budget will be sufficient for horizon
     */
    bool checkSufficiency(
        size_t horizon,
        double expected_robustness,
        double threshold = 0.0
    ) const;

private:
    BudgetState state_;
    std::deque<double> history_;
    size_t history_size_;
    double decay_rate_;

    // Statistics helper
    mutable bool stats_cached_;
    mutable BudgetStats cached_stats_;

    void invalidateCache_();
    void updateStatistics_() const;
};

} // namespace stl_monitor

#endif // STL_MONITOR_ROBUSTNESSBUDGET_H
