#ifndef STL_MONITOR_H
#define STL_MONITOR_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <Eigen/Dense>
#include "STL_ros/STLFormula.h"
#include "STL_ros/SmoothRobustness.h"
#include "STL_ros/BeliefSpaceEvaluator.h"
#include "STL_ros/RobustnessBudget.h"

namespace stl_ros {

/**
 * @brief STL Monitoring result
 */
struct MonitorResult {
    double robustness;              // Current robustness value
    double expected_robustness;     // Belief-space expected robustness
    double budget;                  // Current budget
    bool satisfied;                 // Satisfaction status (robustness > 0)
    double violation_probability;   // Estimated Pr(robustness < 0)
    std::vector<double> samples;    // Monte Carlo samples (for debugging)

    // Timing information
    double computation_time_ms;     // Computation time in milliseconds

    // Per-timestep robustness values (for visualization)
    std::vector<double> timestep_robustness;
};

/**
 * @brief Main STL Monitor class
 *
 * Coordinates:
 * 1. STL formula parsing and evaluation
 * 2. Smooth robustness computation
 * 3. Belief-space evaluation
 * 4. Budget management
 * 5. Integration with MPC
 */
class STLMonitor {
public:
    /**
     * @brief Configuration
     */
    struct Config {
        int prediction_horizon = 20;      // MPC horizon N
        double dt = 0.1;                  // Time step
        double tau = 0.05;                // Temperature parameter
        int num_samples = 100;            // Monte Carlo samples
        double baseline_r = 0.1;          // Budget baseline
        double lambda = 1.0;              // STL objective weight

        // Belief state configuration
        int state_dim = 5;                // State dimension [x, y, theta, v, w]
        double initial_cov = 0.1;         // Initial covariance
    };

    /**
     * @brief Constructor
     */
    explicit STLMonitor(const Config& config = Config());

    /**
     * @brief Add STL formula to monitor
     * @param name Formula identifier
     * @param formula STL formula
     * @param priority Priority weight
     */
    void addFormula(const std::string& name, STLFormula::Ptr formula, double priority = 1.0);

    /**
     * @brief Remove formula
     */
    void removeFormula(const std::string& name);

    /**
     * @brief Evaluate all formulas for current belief state
     * @param current_belief Current belief β_k
     * @param prediction_state Predicted state trajectory (from MPC)
     * @return Map of formula_name -> MonitorResult
     */
    std::map<std::string, MonitorResult> evaluate(const BeliefState& current_belief,
                                                   const std::vector<Eigen::VectorXd>& prediction_state);

    /**
     * @brief Evaluate single formula
     */
    MonitorResult evaluateFormula(const std::string& formula_name,
                                  const BeliefState& current_belief,
                                  const std::vector<Eigen::VectorXd>& prediction_state);

    /**
     * @brief Get overall MPC objective cost
     * From manuscript: J_k = E[ℓ(x,u)] - λ * ρ^soft(φ)
     * @param stage_cost Stage cost from MPC
     * @param results STL evaluation results
     * @return Total objective including STL term
     */
    double getMPCObjective(double stage_cost,
                          const std::map<std::string, MonitorResult>& results) const;

    /**
     * @brief Check if all budget constraints are satisfied
     */
    bool isFeasible(const std::map<std::string, MonitorResult>& results) const;

    /**
     * @brief Update all budgets
     */
    void updateBudgets(const std::map<std::string, MonitorResult>& results);

    /**
     * @brief Get budget by formula name
     */
    RobustnessBudget& getBudget(const std::string& name);

    /**
     * @brief Get configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Set configuration (runtime updates)
     */
    void setConfig(const Config& config) { config_ = config; }

    /**
     * @brief Get formulas
     */
    const std::map<std::string, STLFormula::Ptr>& getFormulas() const { return formulas_; }

    /**
     * @brief Load formulas from file (YAML format)
     */
    bool loadFormulasFromFile(const std::string& filename);

    /**
     * @brief Export formulas to file
     */
    bool saveFormulasToFile(const std::string& filename) const;

    /**
     * @brief Reset all budgets
     */
    void reset();

private:
    Config config_;
    std::map<std::string, STLFormula::Ptr> formulas_;
    std::map<std::string, double> priorities_;
    std::map<std::string, RobustnessBudget> budgets_;
    std::map<std::string, BeliefSpaceEvaluator> evaluators_;

    /**
     * @brief Create evaluator for formula
     */
    BeliefSpaceEvaluator createEvaluator() const;
};

/**
 * @brief Real-time STL monitor for online control
 *
 * Optimized for performance with caching and incremental updates.
 */
class RealtimeSTLMonitor : public STLMonitor {
public:
    explicit RealtimeSTLMonitor(const Config& config = Config());

    /**
     * @brief Fast evaluation with caching
     */
    std::map<std::string, MonitorResult> evaluateFast(
        const BeliefState& current_belief,
        const std::vector<Eigen::VectorXd>& prediction_state);

    /**
     * @brief Enable/disable caching
     */
    void setCacheEnabled(bool enabled) { cache_enabled_ = enabled; }

    /**
     * @brief Clear cache
     */
    void clearCache();

private:
    bool cache_enabled_;
    struct CacheEntry {
        BeliefState belief;
        std::vector<Eigen::VectorXd> trajectory;
        std::map<std::string, MonitorResult> results;
    };
    std::vector<CacheEntry> cache_;
    static constexpr int MAX_CACHE_SIZE = 10;
};

} // namespace stl_ros

#endif // STL_MONITOR_H
