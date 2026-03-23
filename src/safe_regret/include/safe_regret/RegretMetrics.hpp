/**
 * @file RegretMetrics.hpp
 * @brief Regret metrics data structures for Safe-Regret MPC
 *
 * Based on manuscript definitions:
 * - Safe regret (Eq. 254-257): R_T^safe(π) := J_T(π) - inf_{π*∈Π_safe} J_T(π*)
 * - Dynamic regret (Eq. 262-269): R_T^dyn(π) := Σ[ℓ(x_k,u_k) - ℓ(x_k*,u_k*)] + γ·Σ[1{(x_k,u_k)∉𝒳×𝒰}]
 */

#pragma once

#include <Eigen/Dense>
#include <vector>
#include <string>
#include <cmath>

namespace safe_regret {

/**
 * @brief Regret metrics at a single time step
 */
struct StepRegret {
  double instant_cost;           ///< ℓ(x_k, u_k)
  double comparator_cost;        ///< ℓ(x_k*, u_k*)
  double cost_regret;            ///< ℓ(x_k, u_k) - ℓ(x_k*, u_k*)
  double feasibility_penalty;    ///< γ·1{(x_k,u_k)∉𝒳×𝒰}
  double tracking_error_norm;    ///< ‖e_k‖
  double nominal_cost;           ///< ℓ(z_k, v_k)
  double reference_cost;         ///< ℓ(z_k^ref, v_k^ref)
  double tightening_slack;       ///< Δ_k^tight

  StepRegret()
    : instant_cost(0.0), comparator_cost(0.0), cost_regret(0.0),
      feasibility_penalty(0.0), tracking_error_norm(0.0),
      nominal_cost(0.0), reference_cost(0.0), tightening_slack(0.0) {}
};

/**
 * @brief Cumulative regret metrics over horizon T
 *
 * Implements manuscript Eq. 254-257 (Safe Regret) and Eq. 262-269 (Dynamic Regret)
 */
struct RegretMetrics {
  // Main regret quantities
  double safe_regret;             ///< R_T^safe (Eq. 254-257)
  double dynamic_regret;          ///< R_T^dyn (Eq. 262-269)
  double abstract_regret;         ///< R_T^ref (abstract layer)

  // Decomposition components (from Theorem 4.8 proof)
  double tracking_contribution;   ///< O(√T) term: L_ℓ(1+‖K‖)·Σ‖e_k‖
  double nominal_contribution;    ///< Σ[ℓ(z_k,v_k) - ℓ(x_k*,u_k*)]
  double tightening_contribution; ///< ΣΔ_k^tight
  double abstract_contribution;   ///< R_T^ref = o(T)

  // Bounds and theoretical guarantees
  double tracking_error_bound;    ///< C_e·√T (Lemma 4.6)
  double lipschitz_bound;         ///< L_ℓ(1+‖K‖)
  double sublinear_check;         ///< R_T/T ratio (should → 0)

  // Statistics
  int horizon_T;                  ///< Time horizon
  std::vector<StepRegret> step_history;  ///< Per-step data

  RegretMetrics()
    : safe_regret(0.0), dynamic_regret(0.0), abstract_regret(0.0),
      tracking_contribution(0.0), nominal_contribution(0.0),
      tightening_contribution(0.0), abstract_contribution(0.0),
      tracking_error_bound(0.0), lipschitz_bound(0.0),
      sublinear_check(0.0), horizon_T(0) {}

  /**
   * @brief Compute cumulative regrets from step history
   *
   * Implements Theorem 4.8 decomposition:
   * R_T^dyn = L_ℓ(1+‖K‖)·Σ‖e_k‖ + Σ[ℓ(z_k,v_k) - ℓ(z_k^ref,v_k^ref)]
   *          + R_T^ref + ΣΔ_k^tight
   */
  void computeCumulative();

  /**
   * @brief Verify sublinear growth: R_T/T → 0 as T → ∞
   * @return true if R_T/T < ε (e.g., ε = 1/√T)
   */
  bool verifySublinearGrowth(double epsilon = 0.1) const;

  /**
   * @brief Get regret growth rate
   * @return Pair (rate, description) where rate is "o(T)", "O(√T)", etc.
   */
  std::pair<std::string, double> getGrowthRate() const;

  /**
   * @brief Export to CSV for analysis
   */
  void exportToCSV(const std::string& filename) const;

  /**
   * @brief Print summary statistics
   */
  void printSummary() const;
};

/**
 * @brief Reference trajectory point
 */
struct ReferencePoint {
  Eigen::VectorXd state;     ///< z_k (nominal state)
  Eigen::VectorXd input;     ///< v_k (nominal input)
  double timestamp;          ///< Time stamp

  ReferencePoint(int state_dim, int input_dim)
    : state(state_dim), input(input_dim), timestamp(0.0) {}
};

/**
 * @brief Reference trajectory sequence
 */
struct ReferenceTrajectory {
  std::vector<ReferencePoint> points;

  void addPoint(const ReferencePoint& point) { points.push_back(point); }
  size_t size() const { return points.size(); }
  bool empty() const { return points.empty(); }

  /**
   * @brief Check if trajectory is feasible for Tube MPC
   *
   * Feasibility conditions from Theorem 4.8:
   * - Velocity bounds: ‖v_k‖ ≤ v_max
   * - Curvature bounds: κ(z_k) ≤ κ_max
   * - Tube admissibility: z_k ∈ 𝒳 ⊖ ℰ
   */
  bool isTubeFeasible(
    double v_max,
    double kappa_max,
    const Eigen::VectorXd& tube_center,
    double tube_radius) const;
};

} // namespace safe_regret
