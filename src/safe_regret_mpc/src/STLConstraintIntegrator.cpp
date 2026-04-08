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

    // ✅ CRITICAL FIX: Calculate distance from robot to path (not from origin to first point!)
    // belief_mean: current robot position [x, y, theta, ...]
    // trajectory: reference trajectory [ [x0, y0, theta0, ...], [x1, y1, theta1, ...], ... ]

    double robot_x = belief_mean(0);
    double robot_y = belief_mean(1);

    // Find minimum distance from robot to any segment in the trajectory
    double min_dist = std::numeric_limits<double>::max();

    for (size_t i = 0; i < trajectory.size() - 1; ++i) {
        double x1 = trajectory[i](0);
        double y1 = trajectory[i](1);
        double x2 = trajectory[i+1](0);
        double y2 = trajectory[i+1](1);

        // Distance from point to line segment
        double dx = x2 - x1;
        double dy = y2 - y1;
        double len_sq = dx*dx + dy*dy;

        double dist;
        if (len_sq < 1e-9) {
            // Degenerate segment: distance to point
            dist = std::sqrt((robot_x - x1)*(robot_x - x1) + (robot_y - y1)*(robot_y - y1));
        } else {
            // Projection parameter t (clamped to [0, 1])
            double t = std::max(0.0, std::min(1.0,
                ((robot_x - x1) * dx + (robot_y - y1) * dy) / len_sq));

            // Closest point on segment
            double nearest_x = x1 + t * dx;
            double nearest_y = y1 + t * dy;

            // Distance to closest point
            dist = std::sqrt((robot_x - nearest_x)*(robot_x - nearest_x) +
                             (robot_y - nearest_y)*(robot_y - nearest_y));
        }

        if (dist < min_dist) {
            min_dist = dist;
        }
    }

    // Robustness: threshold - distance
    return baseline_ - min_dist;
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
