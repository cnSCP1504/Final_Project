/**
 * @file TighteningComputer.cpp
 * @brief Implementation of deterministic margin computation for DR chance constraints
 *
 * Implements manuscript Lemma 4.1 (Eq. 698):
 * h(z_t) ≥ L_h·ē + μ_t + κ_{δ_t}·σ_t + σ_{k,t}
 */

#include "dr_tightening/TighteningComputer.hpp"
#include "dr_tightening/ResidualCollector.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace dr_tightening {

TighteningComputer::TighteningComputer(
  double total_risk,
  int horizon,
  RiskAllocation allocation)
  : total_risk_(total_risk), horizon_(horizon), allocation_(allocation) {
  updateRiskAllocation();
}

double TighteningComputer::computeChebyshevMargin(
  const Eigen::VectorXd& nominal_state,
  double safety_value,
  const Eigen::VectorXd& gradient,
  const ResidualCollector& residual_collector,
  double tube_radius,
  double lipschitz_const,
  int time_step) {
  // Get disturbance statistics
  Eigen::VectorXd mean = residual_collector.getMean();
  Eigen::MatrixXd covariance = residual_collector.getCovariance();

  // Check time_step bounds
  if (time_step < 0 || time_step > horizon_) {
    time_step = std::min(std::max(time_step, 0), horizon_);
  }

  double delta_t = per_step_risks_[time_step];

  // From manuscript Eq. 698 (Lemma 4.1):
  // h(z_t) ≥ L_h·ē + μ_t + κ_{δ_t}·σ_t + σ_{k,t}
  //
  // We return the total required margin:
  // total_margin = L_h·ē + μ_t + κ_{δ_t}·σ_t
  // Then σ_{k,t} = h(z_t) - total_margin (for constraint tightening)

  // Component 1: Tube offset (L_h·ē)
  double tube_offset = computeTubeOffset(lipschitz_const, tube_radius);

  // Component 2: Mean along sensitivity (μ_t = c_t^T μ)
  double mean_along_sensitivity = computeMeanAlongSensitivity(gradient, mean);

  // Component 3: Std along sensitivity (σ_t = sqrt(c_t^T Σ c_t))
  double std_along_sensitivity = computeStdAlongSensitivity(gradient, covariance);

  // Component 4: Cantelli factor (κ_{δ_t} = sqrt((1-δ_t)/δ_t))
  double cantelli_factor = computeCantelliFactor(delta_t);

  // Total deterministic margin
  double total_margin = tube_offset + mean_along_sensitivity +
                       cantelli_factor * std_along_sensitivity;

  return total_margin;
}

double TighteningComputer::computeCantelliFactor(double delta_t) {
  // From manuscript Eq. 731:
  // κ_{δ_t} = sqrt((1-δ_t)/δ_t)
  //
  // This is the Cantelli/Chebyshev one-sided factor
  // that ensures Pr(Y < μ - κσ) ≤ δ_t

  if (delta_t <= 0.0 || delta_t >= 1.0) {
    std::cerr << "Warning: delta_t = " << delta_t
              << " is outside (0,1), using clamped value" << std::endl;
    delta_t = std::max(0.01, std::min(0.99, delta_t));
  }

  return std::sqrt((1.0 - delta_t) / delta_t);
}

std::vector<double> TighteningComputer::allocateRisk(
  double total_risk,
  int horizon,
  RiskAllocation allocation) const {
  std::vector<double> delta_t(horizon + 1);

  if (allocation == RiskAllocation::UNIFORM) {
    // δ_t = δ / (N+1) for all t
    double uniform_risk = total_risk / static_cast<double>(horizon + 1);
    std::fill(delta_t.begin(), delta_t.end(), uniform_risk);
  }
  else if (allocation == RiskAllocation::DEADLINE_WEIGHED) {
    // More risk to earlier steps, less to later steps
    // δ_t ∝ (t+1) / Σ(t+1)
    double sum = 0.0;
    for (int t = 0; t <= horizon; ++t) {
      sum += static_cast<double>(t + 1);
    }
    for (int t = 0; t <= horizon; ++t) {
      delta_t[t] = total_risk * static_cast<double>(t + 1) / sum;
    }
  }
  else if (allocation == RiskAllocation::INVERSE_SQUARE) {
    // Conservative early: δ_t ∝ 1/(t+1)²
    double sum = 0.0;
    for (int t = 0; t <= horizon; ++t) {
      sum += 1.0 / static_cast<double>((t + 1) * (t + 1));
    }
    for (int t = 0; t <= horizon; ++t) {
      double weight = 1.0 / static_cast<double>((t + 1) * (t + 1));
      delta_t[t] = total_risk * weight / sum;
    }
  }

  // Verify Boole's inequality: Σ δ_t ≤ δ
  double sum_risk = std::accumulate(delta_t.begin(), delta_t.end(), 0.0);
  if (sum_risk > total_risk + 1e-6) {
    std::cerr << "Warning: Risk allocation exceeds total risk: "
              << sum_risk << " > " << total_risk << std::endl;
  }

  return delta_t;
}

double TighteningComputer::computeMeanAlongSensitivity(
  const Eigen::VectorXd& gradient,
  const Eigen::VectorXd& mean_disturbance) const {
  // From manuscript Eq. 691:
  // μ_t = c_t^T μ_t (mean of disturbance proxy)
  // where c_t = ∇h(z_t)

  if (gradient.size() != mean_disturbance.size()) {
    std::cerr << "Warning: Gradient and disturbance mean dimension mismatch"
              << std::endl;
    return 0.0;
  }

  return gradient.dot(mean_disturbance);
}

double TighteningComputer::computeStdAlongSensitivity(
  const Eigen::VectorXd& gradient,
  const Eigen::MatrixXd& covariance) const {
  // From manuscript Eq. 692:
  // σ_t² = c_t^T Σ_t c_t
  // where c_t = ∇h(z_t) and Σ_t is disturbance covariance

  if (gradient.size() != covariance.rows() ||
      gradient.size() != covariance.cols()) {
    std::cerr << "Warning: Gradient and covariance dimension mismatch"
              << std::endl;
    return 0.0;
  }

  double variance = gradient.transpose() * covariance * gradient;
  return std::sqrt(std::max(0.0, variance));
}

double TighteningComputer::computeTubeOffset(
  double lipschitz_const,
  double tube_radius) const {
  // From manuscript Eq. 712:
  // L_h·ē (tube offset from Lipschitz continuity)
  //
  // Since h(x) ≥ h(z) - L_h‖e‖ and ‖e‖ ≤ ē,
  // we have h(x) ≥ h(z) - L_h·ē

  return lipschitz_const * tube_radius;
}

void TighteningComputer::setRiskAllocation(RiskAllocation allocation) {
  allocation_ = allocation;
  updateRiskAllocation();
}

void TighteningComputer::setTotalRisk(double risk) {
  total_risk_ = risk;
  updateRiskAllocation();
}

void TighteningComputer::setHorizon(int horizon) {
  horizon_ = horizon;
  updateRiskAllocation();
}

void TighteningComputer::updateRiskAllocation() {
  per_step_risks_ = allocateRisk(total_risk_, horizon_, allocation_);
}

} // namespace dr_tightening
