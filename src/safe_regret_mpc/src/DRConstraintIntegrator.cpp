#include "safe_regret_mpc/DRConstraintIntegrator.hpp"
#include <cmath>
#include <numeric>
#include <iostream>

using namespace safe_regret_mpc;
using namespace Eigen;

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

DRConstraintIntegrator::DRConstraintIntegrator()
    : window_size_(200),
      residual_mean_(VectorXd::Zero(6)),
      residual_cov_(MatrixXd::Identity(6, 6)),
      delta_(0.1),
      allocation_strategy_(RiskAllocation::UNIFORM),
      ambiguity_type_(AmbiguityType::CHEBYSHEV),
      ambiguity_radius_(0.1),
      statistics_updated_(false),
      initialized_(false)
{
}

DRConstraintIntegrator::~DRConstraintIntegrator() {
}

// ========== Initialization ==========

bool DRConstraintIntegrator::initialize() {
    initialized_ = true;
    return true;
}

// ========== Residual Collection ==========

void DRConstraintIntegrator::addResidual(const VectorXd& residual) {
    residuals_window_.push_back(residual);

    // Maintain window size
    if (residuals_window_.size() > window_size_) {
        residuals_window_.pop_front();
    }

    statistics_updated_ = false;
}

// ========== Statistics ==========

void DRConstraintIntegrator::updateStatistics() {
    if (statistics_updated_ || residuals_window_.empty()) return;

    // Compute mean
    residual_mean_ = VectorXd::Zero(residuals_window_[0].size());
    for (const auto& residual : residuals_window_) {
        residual_mean_ += residual;
    }
    residual_mean_ /= residuals_window_.size();

    // Compute covariance
    residual_cov_ = MatrixXd::Zero(residual_mean_.size(), residual_mean_.size());
    for (const auto& residual : residuals_window_) {
        VectorXd diff = residual - residual_mean_;
        residual_cov_ += diff * diff.transpose();
    }
    residual_cov_ /= (residuals_window_.size() - 1);

    statistics_updated_ = true;
}

std::pair<VectorXd, MatrixXd> DRConstraintIntegrator::getResidualStatistics() const {
    return {residual_mean_, residual_cov_};
}

// ========== Margin Computation ==========

std::vector<double> DRConstraintIntegrator::computeMargins(
    size_t horizon,
    double tube_radius,
    double lipschitz_const) {
    updateStatistics();

    // Allocate risk across horizon
    std::vector<double> risk_allocation = allocateRisk(horizon);

    // Compute tube offset
    double tube_offset = lipschitz_const * tube_radius;

    // Compute margins for each time step
    std::vector<double> margins(horizon, 0.0);

    for (size_t t = 0; t < horizon; ++t) {
        // Simple: use mean and std along first dimension
        double mean = residual_mean_(0);
        double std = std::sqrt(residual_cov_(0, 0));

        margins[t] = computeSingleMargin(mean, std, risk_allocation[t], tube_offset);
    }

    margins_ = margins;
    return margins;
}

double DRConstraintIntegrator::computeSingleMargin(
    double mean, double std, double delta, double tube_offset) {
    // Cantelli factor
    double cantelli = computeCantelliFactor(delta);

    // Total margin: μ + κ·σ + L_h·ē
    return mean + cantelli * std + tube_offset;
}

double DRConstraintIntegrator::computeCantelliFactor(double delta) {
    // κ_δ = sqrt((1-δ)/δ)
    return std::sqrt((1.0 - delta) / delta);
}

// ========== Risk Allocation ==========

std::vector<double> DRConstraintIntegrator::allocateRisk(size_t horizon) {
    std::vector<double> allocation(horizon, 0.0);

    switch (allocation_strategy_) {
        case RiskAllocation::UNIFORM: {
            double delta_t = delta_ / horizon;
            std::fill(allocation.begin(), allocation.end(), delta_t);
            break;
        }

        case RiskAllocation::DEADLINE_WEIGHTED: {
            // Weighted by 1/t (more risk early, less later)
            double sum_weights = 0.0;
            for (size_t t = 0; t < horizon; ++t) {
                sum_weights += 1.0 / (t + 1);
            }
            for (size_t t = 0; t < horizon; ++t) {
                allocation[t] = delta_ * (1.0 / (t + 1)) / sum_weights;
            }
            break;
        }

        case RiskAllocation::ADAPTIVE: {
            // TODO: Implement adaptive allocation based on variance
            // For now, use uniform
            double delta_t_adapt = delta_ / horizon;
            std::fill(allocation.begin(), allocation.end(), delta_t_adapt);
            break;
        }
    }

    return allocation;
}

// ========== Reset ==========

void DRConstraintIntegrator::resetResiduals() {
    residuals_window_.clear();
    statistics_updated_ = false;
}

} // namespace safe_regret_mpc
