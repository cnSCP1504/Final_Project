#include "safe_regret_mpc/STLConstraintIntegrator.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace safe_regret_mpc;
using namespace Eigen;

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

STLConstraintIntegrator::STLConstraintIntegrator()
    : formula_tree_(nullptr),
      baseline_(0.1),
      budget_(1.0),
      initial_budget_(1.0),
      temperature_(0.05),
      violation_count_(0),
      initialized_(false),
      num_particles_(100)
{
}

STLConstraintIntegrator::~STLConstraintIntegrator() {
    if (formula_tree_) {
        delete formula_tree_;
    }
}

// ========== Initialization ==========

bool STLConstraintIntegrator::initialize() {
    initialized_ = true;
    return true;
}

// ========== Formula Parsing ==========

bool STLConstraintIntegrator::setFormula(const std::string& formula) {
    // TODO: Implement full formula parser
    // For now, just store the formula string
    std::cout << "STL formula set: " << formula << std::endl;
    return true;
}

STLConstraintIntegrator::STLNode* STLConstraintIntegrator::parseFormula(
    const std::string& formula) {
    // TODO: Implement full parser
    return nullptr;
}

// ========== Robustness Computation ==========

double STLConstraintIntegrator::computeSmoothRobustness(
    const std::vector<VectorXd>& trajectory,
    const std::vector<double>& time_stamps) {
    // TODO: Implement full robustness computation
    // For now, return simple baseline
    return baseline_;
}

double STLConstraintIntegrator::computeBeliefRobustness(
    const VectorXd& belief_mean,
    const MatrixXd& belief_cov,
    const std::vector<VectorXd>& trajectory) {
    // TODO: Implement belief-space robustness
    // For now, use simple distance-based metric
    if (trajectory.empty()) return baseline_;

    // Simple robustness: negative distance to goal
    double distance = trajectory[0].norm();
    return baseline_ - distance;
}

std::vector<VectorXd> STLConstraintIntegrator::computeRobustnessGradient(
    const std::vector<VectorXd>& trajectory) {
    // TODO: Implement gradient computation
    return std::vector<VectorXd>(trajectory.size(), VectorXd::Zero(6));
}

// ========== Budget Management ==========

double STLConstraintIntegrator::updateBudget(double current_robustness) {
    // Budget update: R_{k+1} = max{0, R_k + ρ̃_k - r̲}
    budget_ = std::max(0.0, budget_ + current_robustness - baseline_);

    if (budget_ < 0.0) {
        violation_count_++;
    }

    return budget_;
}

// ========== Smooth Approximation Functions ==========

double STLConstraintIntegrator::smoothMin(const std::vector<double>& values, double tau) {
    // smin_τ(z) = -τ·log(Σ exp(-z_i/τ))
    if (values.empty()) return 0.0;

    double sum_exp = 0.0;
    for (double val : values) {
        sum_exp += std::exp(-val / tau);
    }

    return -tau * std::log(sum_exp);
}

double STLConstraintIntegrator::smoothMax(const std::vector<double>& values, double tau) {
    // smax_τ(z) = τ·log(Σ exp(z_i/τ))
    if (values.empty()) return 0.0;

    double sum_exp = 0.0;
    for (double val : values) {
        sum_exp += std::exp(val / tau);
    }

    return tau * std::log(sum_exp);
}

std::vector<double> STLConstraintIntegrator::smoothMinGradient(
    const std::vector<double>& values, double tau) {
    // Gradient of smooth min
    std::vector<double> grad(values.size(), 0.0);

    if (values.empty()) return grad;

    double sum_exp = 0.0;
    for (double val : values) {
        sum_exp += std::exp(-val / tau);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        double exp_val = std::exp(-values[i] / tau);
        grad[i] = -exp_val / (sum_exp * tau);
    }

    return grad;
}

std::vector<double> STLConstraintIntegrator::smoothMaxGradient(
    const std::vector<double>& values, double tau) {
    // Gradient of smooth max
    std::vector<double> grad(values.size(), 0.0);

    if (values.empty()) return grad;

    double sum_exp = 0.0;
    for (double val : values) {
        sum_exp += std::exp(val / tau);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        double exp_val = std::exp(values[i] / tau);
        grad[i] = exp_val / (sum_exp * tau);
    }

    return grad;
}

// ========== Reset ==========

void STLConstraintIntegrator::reset() {
    budget_ = initial_budget_;
    violation_count_ = 0;
}

} // namespace safe_regret_mpc
