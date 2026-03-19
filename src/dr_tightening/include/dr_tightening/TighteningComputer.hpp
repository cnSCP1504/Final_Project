/**
 * @file TighteningComputer.hpp
 * @brief Computes deterministic margins σ_{k,t} for DR chance constraints
 *
 * Based on manuscript Lemma 4.1 (Eq. 698):
 * h(z_t) ≥ L_h·ē + μ_t + κ_{δ_t}·σ_t + σ_{k,t}
 *
 * where:
 * - L_h·ē = tube offset (from Lipschitz continuity, Eq. 712)
 * - μ_t + κ_{δ_t}·σ_t = distributional safety (Eq. 735)
 * - κ_{δ_t} = sqrt((1-δ_t)/δ_t) (Cantelli factor, Eq. 731)
 * - σ_t^2 = c_t^T Σ_t c_t (variance along sensitivity direction, Eq. 692)
 * - μ_t = c_t^T μ_t (mean along sensitivity direction, Eq. 691)
 */

#pragma once

#include <Eigen/Dense>
#include <vector>
#include <memory>

namespace dr_tightening {

class ResidualCollector;
class SafetyLinearization;

/**
 * @brief Risk allocation strategies for joint chance constraints
 */
enum class RiskAllocation {
  UNIFORM,       ///< δ_t = δ/(N+1) (simple)
  DEADLINE_WEIGHED, ///< More risk to earlier steps
  INVERSE_SQUARE ///< δ_t ∝ 1/t² (conservative early)
};

class TighteningComputer {
public:
  /**
   * @brief Constructor
   * @param total_risk Total risk level δ (e.g., 0.1 for 90% safety)
   * @param horizon MPC horizon length N
   * @param allocation Risk allocation strategy
   */
  explicit TighteningComputer(
    double total_risk = 0.1,
    int horizon = 20,
    RiskAllocation allocation = RiskAllocation::UNIFORM);

  /**
   * @brief Compute per-step margins σ_{k,t} using Chebyshev bound
   *
   * Implements manuscript Eq. 698 (Lemma 4.1):
   * σ_{k,t} = h(z_t) - [L_h·ē + μ_t + κ_{δ_t}·σ_t]
   *
   * Or equivalently for constraint tightening:
   * h(z_t) ≥ σ_{k,t} + L_h·ē + μ_t + κ_{δ_t}·σ_t
   *
   * @param nominal_state z_t (nominal state)
   * @param safety_value h(z_t) (safety function value)
   * @param gradient c_t = ∇h(z_t) (sensitivity direction)
   * @param residual_collector Contains μ, Σ from data
   * @param tube_radius ē (tube error bound)
   * @param lipschitz_const L_h (Lipschitz constant)
   * @param time_step t in {k, ..., k+N}
   * @return Total margin required: σ_{k,t} + L_h·ē + μ_t + κ_{δ_t}·σ_t
   */
  double computeChebyshevMargin(
    const Eigen::VectorXd& nominal_state,
    double safety_value,
    const Eigen::VectorXd& gradient,
    const ResidualCollector& residual_collector,
    double tube_radius,
    double lipschitz_const,
    int time_step);

  /**
   * @brief Compute Cantelli/Chebyshev factor κ_{δ_t}
   *
   * From manuscript Eq. 731:
   * κ_{δ_t} = sqrt((1-δ_t)/δ_t)
   *
   * @param delta_t Per-step risk allocation δ_t
   * @return Cantelli factor κ_{δ_t}
   */
  static double computeCantelliFactor(double delta_t);

  /**
   * @brief Allocate total risk δ into per-step risks {δ_t}
   *
   * From manuscript (below Eq. 700): Σ δ_t ≤ δ (Boole's inequality)
   *
   * @param total_risk Overall risk δ
   * @param horizon MPC horizon N
   * @param allocation Allocation strategy
   * @return Vector of δ_t for t = 0, ..., N
   */
  std::vector<double> allocateRisk(
    double total_risk,
    int horizon,
    RiskAllocation allocation) const;

  /**
   * @brief Compute mean μ_t along sensitivity direction
   *
   * From manuscript Eq. 691: μ_t = c_t^T μ_t (mean of ω)
   *
   * @param gradient c_t = ∇h(z_t)
   * @param mean_disturbance μ (disturbance mean)
   * @return μ_t
   */
  double computeMeanAlongSensitivity(
    const Eigen::VectorXd& gradient,
    const Eigen::VectorXd& mean_disturbance) const;

  /**
   * @brief Compute standard deviation σ_t along sensitivity direction
   *
   * From manuscript Eq. 692: σ_t² = c_t^T Σ_t c_t
   *
   * @param gradient c_t = ∇h(z_t)
   * @param covariance Σ (disturbance covariance)
   * @return σ_t
   */
  double computeStdAlongSensitivity(
    const Eigen::VectorXd& gradient,
    const Eigen::MatrixXd& covariance) const;

  /**
   * @brief Add tube error compensation η_ℰ
   *
   * From manuscript Eq. 698: L_h·ē (tube offset)
   *
   * @param lipschitz_const L_h
   * @param tube_radius ē
   * @return Tube offset
   */
  double computeTubeOffset(
    double lipschitz_const,
    double tube_radius) const;

  /**
   * @brief Set risk allocation strategy
   * @param allocation New strategy
   */
  void setRiskAllocation(RiskAllocation allocation);

  /**
   * @brief Set total risk level
   * @param risk Risk level δ ∈ (0, 1)
   */
  void setTotalRisk(double risk);

  /**
   * @brief Set MPC horizon
   * @param horizon Horizon length N
   */
  void setHorizon(int horizon);

private:
  double total_risk_;     // δ
  int horizon_;           // N
  RiskAllocation allocation_;
  std::vector<double> per_step_risks_;  // {δ_t}

  /**
   * @brief Update per-step risk allocations
   */
  void updateRiskAllocation();
};

} // namespace dr_tightening
