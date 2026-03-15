#ifndef ROBUSTNESS_BUDGET_H
#define ROBUSTNESS_BUDGET_H

#include <vector>
#include <deque>
#include <Eigen/Dense>

namespace stl_ros {

/**
 * @brief Robustness budget manager for Safe-Regret MPC
 *
 * From manuscript:
 * R_{k+1} ← UpdateBudget(R_k, ρ^soft(φ; x_{k:k+N}))
 *
 * Simple version:
 * R_{k+1} = R_k + ρ^soft(·) - r̲
 *
 * Purpose: Prevent erosion of satisfaction due to horizon receding.
 * Maintains R_k ≥ 0 ensures long-term satisfaction.
 */
class RobustnessBudget {
public:
    /**
     * @brief Constructor
     * @param initial_budget Initial budget R_0 (default: 0)
     * @param baseline_r Per-step robustness requirement r̲ > 0
     * @param budget_max Maximum budget cap (prevents unlimited accumulation)
     */
    RobustnessBudget(double initial_budget = 0.0,
                    double baseline_r = 0.1,
                    double budget_max = 10.0);

    /**
     * @brief Update budget with new robustness value
     * @param robustness Current robustness ρ^soft(φ; x_{k:k+N})
     * @return Updated budget R_{k+1}
     */
    double update(double robustness);

    /**
     * @brief Advanced update with adaptive baseline
     * @param robustness Current robustness
     * @param satisfaction_rate Historical satisfaction rate
     * @return Updated budget
     */
    double updateAdaptive(double robustness, double satisfaction_rate);

    /**
     * @brief Check if budget is sufficient for next step
     * @return true if R_k ≥ 0 (feasible), false otherwise
     */
    bool isFeasible() const { return budget_ >= 0.0; }

    /**
     * @brief Get current budget
     */
    double getBudget() const { return budget_; }

    /**
     * @brief Set budget manually (for initialization/reset)
     */
    void setBudget(double budget) { budget_ = budget; }

    /**
     * @brief Get budget history
     */
    const std::deque<double>& getHistory() const { return history_; }

    /**
     * @brief Get statistics
     */
    struct Stats {
        double min_budget;
        double max_budget;
        double mean_budget;
        int violation_count;
    };
    Stats getStats() const;

    /**
     * @brief Reset budget history
     */
    void reset();

    /**
     * @brief Set baseline robustness requirement
     */
    void setBaseline(double baseline_r) { baseline_r_ = baseline_r; }

    /**
     * @brief Get violation probability estimate
     * @return Estimated Pr(R_k < 0)
     */
    double getViolationProbability() const;

private:
    double budget_;           // Current budget R_k
    double baseline_r_;       // Per-step requirement r̲
    double budget_max_;       // Maximum budget cap
    std::deque<double> history_;  // Budget history (limited size)

    static constexpr int MAX_HISTORY_SIZE = 1000;
};

/**
 * @brief Budget-aware MPC constraint
 *
 * Incorporates budget constraint into MPC optimization:
 * R_{k+1} = R_k + ρ^soft(φ; x_{k:k+N}) - r̲ ≥ 0
 */
class BudgetConstraint {
public:
    /**
     * @brief Constructor
     * @param budget Budget manager reference
     */
    explicit BudgetConstraint(RobustnessBudget& budget);

    /**
     * @brief Evaluate constraint violation
     * @param robustness Current robustness value
     * @return Constraint value (negative = violation)
     */
    double evaluate(double robustness) const;

    /**
     * @brief Get constraint gradient w.r.t. robustness
     * @return Gradient (always 1.0 for linear budget)
     */
    double gradient() const { return 1.0; }

    /**
     * @brief Check if constraint is satisfied
     */
    bool isSatisfied(double robustness) const;

private:
    RobustnessBudget& budget_;
};

/**
 * @brief Composite budget manager for multiple STL formulas
 *
 * Handles multiple formulas with different budgets and priorities.
 */
class MultiFormulaBudget {
public:
    /**
     * @brief Add formula budget
     * @param formula_name Formula identifier
     * @param baseline_r Baseline requirement
     * @param priority Priority weight (higher = more important)
     */
    void addFormula(const std::string& formula_name,
                   double baseline_r,
                   double priority = 1.0);

    /**
     * @brief Update all budgets
     * @param robustness_values Map of formula_name -> robustness
     * @return Overall feasibility (all budgets ≥ 0)
     */
    bool updateAll(const std::map<std::string, double>& robustness_values);

    /**
     * @brief Get overall budget status
     * @return true if all budgets feasible
     */
    bool isFeasible() const;

    /**
     * @brief Get most critical formula (lowest budget)
     */
    std::string getCriticalFormula() const;

    /**
     * @brief Get budget by formula name
     */
    RobustnessBudget& getBudget(const std::string& formula_name);

private:
    struct FormulaInfo {
        RobustnessBudget budget;
        double priority;
    };
    std::map<std::string, FormulaInfo> formulas_;
};

} // namespace stl_ros

#endif // ROBUSTNESS_BUDGET_H
