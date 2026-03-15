#ifndef TUBE_MPC_STL_INTEGRATION_H
#define TUBE_MPC_STL_INTEGRATION_H

#include <ros/ros.h>
#include <stl_ros/STLRobustness.h>
#include <stl_ros/STLBudget.h>
#include <Eigen/Dense>

namespace tube_mpc_ros {

/**
 * @brief STL Integration for Tube MPC
 *
 * Integrates STL monitoring feedback into Tube MPC cost function
 * according to Safe-Regret MPC manuscript:
 *
 * J_k = E[ℓ(x,u)] - λ * ρ^soft(φ; x_{k:k+N})
 *
 * where:
 * - ρ^soft is the smooth STL robustness
 * - λ is the STL weight parameter
 * - Budget constraints: R_{k+1} = R_k + ρ^soft(·) - r̲ ≥ 0
 */
class STLIntegration {
public:
    struct Config {
        double stl_weight_lambda;      // STL weight λ
        double budget_constraint_weight;  // Budget penalty weight
        bool enable_budget_constraints;     // Enforce budget constraints
        double min_robustness_threshold;    // Minimum acceptable robustness

        // Budget parameters
        double initial_budget;
        double baseline_robustness_r;

        // Constructor with default values
        Config()
            : stl_weight_lambda(1.0)
            , budget_constraint_weight(10.0)
            , enable_budget_constraints(true)
            , min_robustness_threshold(-1.0)
            , initial_budget(0.0)
            , baseline_robustness_r(0.1) {}
    };

    STLIntegration(const Config& config = Config());

    /**
     * @brief Modify MPC cost with STL robustness
     * @param base_cost Base tracking cost from MPC
     * @param robustness STL robustness value
     * @return Modified cost: J = base_cost - λ * ρ^soft
     */
    double modifyCost(double base_cost, double robustness) const;

    /**
     * @brief Compute budget constraint penalty
     * @param current_budget Current budget R_k
     * @param robustness Current robustness ρ^soft
     * @return Penalty if constraint violated, 0 otherwise
     */
    double computeBudgetPenalty(double current_budget, double robustness) const;

    /**
     * @brief Check if STL constraints are satisfied
     * @param robustness Current robustness
     * @param budget Current budget
     * @return true if all constraints satisfied
     */
    bool isFeasible(double robustness, double budget) const;

    /**
     * @brief Update budget with new robustness value
     * @param current_budget Current budget R_k
     * @param robustness New robustness ρ^soft
     * @return Updated budget R_{k+1}
     */
    double updateBudget(double current_budget, double robustness) const;

    /**
     * @brief Get gradient of cost w.r.t. state
     * @param robustness_gradient Gradient of robustness w.r.t. state
     * @return Cost gradient: ∇J = ∇ℓ - λ * ∇ρ
     */
    Eigen::VectorXd getCostGradient(const Eigen::VectorXd& robustness_gradient) const;

    /**
     * @brief Process STL robustness message
     */
    void processRobustnessMsg(const stl_ros::STLRobustness& msg);

    /**
     * @brief Process STL budget message
     */
    void processBudgetMsg(const stl_ros::STLBudget& msg);

    // Getters
    double getCurrentRobustness() const { return current_robustness_; }
    double getCurrentBudget() const { return current_budget_; }
    bool isSTLEnabled() const { return stl_enabled_; }
    const Config& getConfig() const { return config_; }

    // Setters
    void setSTLEnabled(bool enabled) { stl_enabled_ = enabled; }
    void setConfig(const Config& config) { config_ = config; }

private:
    Config config_;
    bool stl_enabled_;

    // Current STL status
    double current_robustness_;
    double current_budget_;
    double expected_robustness_;

    // Statistics
    int violation_count_;
    double total_robustness_;
    int robustness_samples_;
};

} // namespace tube_mpc_ros

#endif // TUBE_MPC_STL_INTEGRATION_H
