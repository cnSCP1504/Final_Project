/**
 * @file RegretAnalyzer.hpp
 * @brief Regret analysis for Safe-Regret MPC
 *
 * Implements:
 * - Safe regret (manuscript Eq. 254-257)
 * - Dynamic regret (manuscript Eq. 262-269)
 * - Regret transfer theorem (Theorem 4.8)
 * - Tracking error bound (Lemma 4.6)
 */

#pragma once

#include "safe_regret/RegretMetrics.hpp"
#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <fstream>

namespace safe_regret {

/**
 * @brief Oracle comparator for regret calculation
 *
 * Represents the best-in-class policy π* ∈ Π_safe
 * Used to compute both safe and dynamic regret
 */
class OracleComparator {
public:
  /**
   * @brief Constructor
   * @param state_dim State dimension
   * @param input_dim Input dimension
   */
  OracleComparator(int state_dim, int input_dim);

  /**
   * @brief Set oracle trajectory (offline optimal with full information)
   * @param trajectory Oracle trajectory {x_k*, u_k*}
   */
  void setOracleTrajectory(const ReferenceTrajectory& trajectory);

  /**
   * @brief Get optimal cost at step k
   * @param k Time step
   * @return ℓ(x_k*, u_k*)
   */
  double getOptimalCost(int k) const;

  /**
   * @brief Get oracle state/input at step k
   */
  std::pair<Eigen::VectorXd, Eigen::VectorXd> getOptimalPair(int k) const;

  /**
   * @brief Check if oracle is available at step k
   */
  bool hasOracleAt(int k) const;

private:
  ReferenceTrajectory oracle_trajectory_;
  std::vector<double> oracle_costs_;
};

/**
 * @brief Regret analyzer for tracking and computing regrets
 *
 * Implements manuscript Theorem 4.8 (Regret Transfer):
 * R_T^dyn = O(√T) + o(T) = o(T)
 */
class RegretAnalyzer {
public:
  /**
   * @brief Constructor
   * @param state_dim State dimension n
   * @param input_dim Input dimension m
   * @param L_lipschitz Lipschitz constant L_ℓ for cost function
   * @param K_gain LQR feedback gain K (for tracking error scaling)
   * @param infeasibility_penalty γ (penalty for constraint violation)
   */
  RegretAnalyzer(
    int state_dim,
    int input_dim,
    double L_lipschitz = 1.0,
    const Eigen::MatrixXd& K_gain = Eigen::MatrixXd(),
    double infeasibility_penalty = 1000.0);

  /**
   * @brief Update regret metrics for current time step
   *
   * Implements manuscript Eq. 1107-1113 (instantaneous cost decomposition):
   * ℓ(x_k,u_k) - ℓ(x_k*,u_k*)
   *   = ℓ(z_k+e_k, v_k+Ke_k) - ℓ(z_k,v_k) + ℓ(z_k,v_k) - ℓ(x_k*,u_k*)
   *   ≤ L_ℓ(‖e_k‖ + ‖Ke_k‖) + ℓ(z_k,v_k) - ℓ(x_k*,u_k*)
   *
   * @param actual_state x_k
   * @param actual_input u_k
   * @param nominal_state z_k
   * @param nominal_input v_k
   * @param tracking_error e_k
   * @param reference_state z_k^ref
   * @param reference_input v_k^ref
   * @param oracle Pointer to OracleComparator (can be nullptr if unavailable)
   * @param is_feasible Whether (x_k, u_k) ∈ 𝒳 × 𝒰
   * @param tightening_slack Δ_k^tight (cost from constraint tightening)
   * @return StepRegret for this time step
   */
  StepRegret updateStep(
    const Eigen::VectorXd& actual_state,
    const Eigen::VectorXd& actual_input,
    const Eigen::VectorXd& nominal_state,
    const Eigen::VectorXd& nominal_input,
    const Eigen::VectorXd& tracking_error,
    const Eigen::VectorXd& reference_state,
    const Eigen::VectorXd& reference_input,
    const OracleComparator* oracle,
    bool is_feasible,
    double tightening_slack = 0.0);

  /**
   * @brief Compute cumulative regrets (Theorem 4.8)
   *
   * Computes:
   * - Safe regret R_T^safe (Eq. 254-257)
   * - Dynamic regret R_T^dyn (Eq. 262-269)
   * - Decomposition into tracking + nominal + tightening + abstract
   *
   * @return RegretMetrics with all cumulative quantities
   */
  RegretMetrics computeCumulativeRegrets() const;

  /**
   * @brief Verify tracking error bound (Lemma 4.6)
   *
   * Checks: Σ‖e_k‖ ≤ C_e·√T
   *
   * @param C_e Tracking error constant (from Lyapunov analysis)
   * @return true if bound holds
   */
  bool verifyTrackingErrorBound(double C_e) const;

  /**
   * @brief Verify regret transfer (Theorem 4.8)
   *
   * Checks: R_T^dyn = O(√T) + o(T) = o(T)
   *
   * @return true if sublinear growth is verified
   */
  bool verifyRegretTransfer() const;

  /**
   * @brief Estimate Lipschitz constant from data
   *
   * If L_ℓ is unknown, estimate from trajectory data
   *
   * @return Estimated L_ℓ
   */
  double estimateLipschitzConstant() const;

  /**
   * @brief Get regret growth rate
   *
   * Analyzes R_T/T vs T to determine if o(T)
   *
   * @return Pair (growth_type, rate) where growth_type ∈ {"o(T)", "O(√T)", "O(T)"}
   */
  std::pair<std::string, double> analyzeGrowthRate() const;

  /**
   * @brief Enable/disable CSV logging
   */
  void enableLogging(const std::string& filename);
  void disableLogging();

  /**
   * @brief Reset analyzer (clear all history)
   */
  void reset();

  /**
   * @brief Get current time step
   */
  int getCurrentStep() const { return current_step_; }

  /**
   * @brief Get step history
   */
  const std::vector<StepRegret>& getStepHistory() const { return step_history_; }

private:
  // Parameters
  int state_dim_;
  int input_dim_;
  double L_lipschitz_;          ///< L_ℓ: Lipschitz constant (Eq. 1094)
  Eigen::MatrixXd K_gain_;       ///< K: LQR feedback gain
  double infeasibility_penalty_; ///< γ: penalty weight (Eq. 267)

  // History
  int current_step_;
  std::vector<StepRegret> step_history_;

  // Logging
  bool logging_enabled_;
  std::ofstream log_file_;

  // Helper functions
  double computeStageCost(
    const Eigen::VectorXd& state,
    const Eigen::VectorXd& input) const;

  /**
   * @brief Compute tracking contribution (Lemma 4.6)
   *
   * L_ℓ(1+‖K‖)·Σ‖e_k‖ ≤ C_e·√T
   */
  double computeTrackingContribution(const StepRegret& step) const;

  /**
   * @brief Compute nominal gap
   *
   * ℓ(z_k,v_k) - ℓ(x_k*,u_k*)
   */
  double computeNominalGap(
    const Eigen::VectorXd& nominal_state,
    const Eigen::VectorXd& nominal_input,
    const Eigen::VectorXd& oracle_state,
    const Eigen::VectorXd& oracle_input) const;
};

} // namespace safe_regret
