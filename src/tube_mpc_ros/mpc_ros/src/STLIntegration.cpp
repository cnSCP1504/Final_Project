#include "tube_mpc_ros/STLIntegration.h"
#include <algorithm>

namespace tube_mpc_ros {

STLIntegration::STLIntegration(const Config& config)
    : config_(config),
      stl_enabled_(true),
      current_robustness_(0.0),
      current_budget_(config.initial_budget),
      expected_robustness_(0.0),
      violation_count_(0),
      total_robustness_(0.0),
      robustness_samples_(0) {
}

double STLIntegration::modifyCost(double base_cost, double robustness) const {
    if (!stl_enabled_) {
        return base_cost;
    }

    // J_k = E[ℓ(x,u)] - λ * ρ^soft(φ; x_{k:k+N})
    // From manuscript Eq. 7
    double stl_term = config_.stl_weight_lambda * robustness;
    return base_cost - stl_term;
}

double STLIntegration::computeBudgetPenalty(double current_budget, double robustness) const {
    if (!config_.enable_budget_constraints) {
        return 0.0;
    }

    // Budget constraint: R_{k+1} = R_k + ρ^soft(·) - r̲ ≥ 0
    // From manuscript Eq. 8
    double next_budget = updateBudget(current_budget, robustness);

    if (next_budget < 0) {
        // Penalty proportional to violation
        double violation = -next_budget;
        return config_.budget_constraint_weight * violation * violation;
    }

    return 0.0;
}

bool STLIntegration::isFeasible(double robustness, double budget) const {
    // Check minimum robustness threshold
    if (robustness < config_.min_robustness_threshold) {
        return false;
    }

    // Check budget constraint
    if (config_.enable_budget_constraints) {
        double next_budget = updateBudget(budget, robustness);
        if (next_budget < 0) {
            return false;
        }
    }

    return true;
}

double STLIntegration::updateBudget(double current_budget, double robustness) const {
    // R_{k+1} = R_k + ρ^soft(·) - r̲
    // From manuscript Eq. 8
    double next_budget = current_budget + robustness - config_.baseline_robustness_r;

    // Clamp to prevent unlimited accumulation
    const double MAX_BUDGET = 10.0;
    return std::max(0.0, std::min(next_budget, MAX_BUDGET));
}

Eigen::VectorXd STLIntegration::getCostGradient(const Eigen::VectorXd& robustness_gradient) const {
    if (!stl_enabled_) {
        return Eigen::VectorXd::Zero(robustness_gradient.size());
    }

    // ∇J = ∇ℓ - λ * ∇ρ
    // The STL term subtracts from the cost, so we subtract λ * ∇ρ
    return -config_.stl_weight_lambda * robustness_gradient;
}

void STLIntegration::processRobustnessMsg(const stl_ros::STLRobustness& msg) {
    current_robustness_ = msg.robustness;
    expected_robustness_ = msg.expected_robustness;

    // Update statistics
    total_robustness_ += msg.robustness;
    robustness_samples_++;

    if (!msg.satisfied) {
        violation_count_++;
    }

    ROS_DEBUG("STL Robustness: %.3f (expected: %.3f, satisfied: %s)",
              msg.robustness, msg.expected_robustness,
              msg.satisfied ? "true" : "false");
}

void STLIntegration::processBudgetMsg(const stl_ros::STLBudget& msg) {
    current_budget_ = msg.current_budget;

    if (!msg.feasible) {
        ROS_WARN("STL Budget violation detected! Budget: %.3f", msg.current_budget);
    }

    ROS_DEBUG("STL Budget: %.3f (feasible: %s)",
              msg.current_budget, msg.feasible ? "true" : "false");
}

} // namespace tube_mpc_ros
