/**
 * @file RegretAnalyzer.cpp
 * @brief Implementation of regret analyzer
 */

#include "safe_regret/RegretAnalyzer.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <numeric>

namespace safe_regret {

// ============================================================================
// OracleComparator Implementation
// ============================================================================

OracleComparator::OracleComparator(int state_dim, int input_dim)
  : oracle_trajectory_() {
}

void OracleComparator::setOracleTrajectory(const ReferenceTrajectory& trajectory) {
  oracle_trajectory_ = trajectory;
  oracle_costs_.clear();

  // Compute costs for all steps
  for (const auto& pt : trajectory.points) {
    // Simple quadratic cost: ℓ(x,u) = ‖x‖^2 + ‖u‖^2
    double cost = pt.state.squaredNorm() + pt.input.squaredNorm();
    oracle_costs_.push_back(cost);
  }
}

double OracleComparator::getOptimalCost(int k) const {
  if (k >= 0 && k < static_cast<int>(oracle_costs_.size())) {
    return oracle_costs_[k];
  }
  return std::numeric_limits<double>::infinity();
}

std::pair<Eigen::VectorXd, Eigen::VectorXd>
OracleComparator::getOptimalPair(int k) const {
  if (k >= 0 && k < static_cast<int>(oracle_trajectory_.points.size())) {
    const auto& pt = oracle_trajectory_.points[k];
    return {pt.state, pt.input};
  }
  return {Eigen::VectorXd(), Eigen::VectorXd()};
}

bool OracleComparator::hasOracleAt(int k) const {
  return (k >= 0 && k < static_cast<int>(oracle_trajectory_.points.size()));
}

// ============================================================================
// RegretAnalyzer Implementation
// ============================================================================

RegretAnalyzer::RegretAnalyzer(
  int state_dim,
  int input_dim,
  double L_lipschitz,
  const Eigen::MatrixXd& K_gain,
  double infeasibility_penalty)
  : state_dim_(state_dim),
    input_dim_(input_dim),
    L_lipschitz_(L_lipschitz),
    K_gain_(K_gain),
    infeasibility_penalty_(infeasibility_penalty),
    current_step_(0),
    logging_enabled_(false) {

  // If K_gain not provided, use zero matrix
  if (K_gain_.rows() == 0) {
    K_gain_ = Eigen::MatrixXd::Zero(input_dim, state_dim);
  }
}

StepRegret RegretAnalyzer::updateStep(
  const Eigen::VectorXd& actual_state,
  const Eigen::VectorXd& actual_input,
  const Eigen::VectorXd& nominal_state,
  const Eigen::VectorXd& nominal_input,
  const Eigen::VectorXd& tracking_error,
  const Eigen::VectorXd& reference_state,
  const Eigen::VectorXd& reference_input,
  const OracleComparator* oracle,
  bool is_feasible,
  double tightening_slack) {

  StepRegret step;

  // Compute costs
  step.instant_cost = computeStageCost(actual_state, actual_input);
  step.nominal_cost = computeStageCost(nominal_state, nominal_input);
  step.reference_cost = computeStageCost(reference_state, reference_input);

  // Get oracle cost if available
  if (oracle && oracle->hasOracleAt(current_step_)) {
    step.comparator_cost = oracle->getOptimalCost(current_step_);
  } else {
    // No oracle: use nominal cost as baseline
    step.comparator_cost = step.nominal_cost;
  }

  // Cost regret: ℓ(x,u) - ℓ(x*,u*)
  step.cost_regret = step.instant_cost - step.comparator_cost;

  // Feasibility penalty: γ·1{(x,u)∉𝒳×𝒰}
  step.feasibility_penalty = is_feasible ? 0.0 : infeasibility_penalty_;

  // Tracking error norm
  step.tracking_error_norm = tracking_error.norm();

  // Tightening slack
  step.tightening_slack = tightening_slack;

  // Store in history
  step_history_.push_back(step);

  // Log if enabled
  if (logging_enabled_ && log_file_.is_open()) {
    log_file_ << current_step_ << ","
              << step.instant_cost << ","
              << step.comparator_cost << ","
              << step.cost_regret << ","
              << step.feasibility_penalty << ","
              << step.tracking_error_norm << ","
              << step.nominal_cost << ","
              << step.reference_cost << ","
              << step.tightening_slack << "\n";
  }

  current_step_++;

  return step;
}

RegretMetrics RegretAnalyzer::computeCumulativeRegrets() const {
  RegretMetrics metrics;

  // Copy step history
  metrics.step_history = step_history_;
  metrics.horizon_T = current_step_;

  // Set Lipschitz bound: L_ℓ(1+‖K‖)
  metrics.lipschitz_bound = L_lipschitz_ * (1.0 + K_gain_.norm());

  // Compute cumulative quantities
  metrics.computeCumulative();

  return metrics;
}

bool RegretAnalyzer::verifyTrackingErrorBound(double C_e) const {
  if (step_history_.empty()) {
    return true;
  }

  // Compute Σ‖e_k‖
  double sum_error_norm = 0.0;
  for (const auto& step : step_history_) {
    sum_error_norm += step.tracking_error_norm;
  }

  // Check Lemma 4.6: Σ‖e_k‖ ≤ C_e·√T
  int T = current_step_;
  double bound = C_e * std::sqrt(static_cast<double>(T));

  return sum_error_norm <= bound;
}

bool RegretAnalyzer::verifyRegretTransfer() const {
  if (step_history_.empty()) {
    return true;
  }

  // Theorem 4.8: R_T^dyn = O(√T) + o(T) = o(T)
  // We verify by checking if R_T/T → 0

  RegretMetrics metrics = computeCumulativeRegrets();
  return metrics.verifySublinearGrowth();
}

double RegretAnalyzer::estimateLipschitzConstant() const {
  if (step_history_.size() < 2) {
    return L_lipschitz_;
  }

  // Estimate L_ℓ from data: max |ℓ(x1,u1) - ℓ(x2,u2)| / (‖x1-x2‖ + ‖u1-u2‖)

  double max_ratio = 0.0;

  for (size_t i = 0; i < step_history_.size(); ++i) {
    for (size_t j = i + 1; j < step_history_.size(); ++j) {
      // We don't have full state history in StepRegret, so use cost difference only
      // This is a simplified estimate
      double cost_diff = std::abs(step_history_[i].instant_cost -
                                   step_history_[j].instant_cost);

      // Approximate state difference using tracking error
      double state_diff = std::abs(step_history_[i].tracking_error_norm -
                                   step_history_[j].tracking_error_norm);

      if (state_diff > 1e-6) {
        double ratio = cost_diff / state_diff;
        max_ratio = std::max(max_ratio, ratio);
      }
    }
  }

  return max_ratio > 0 ? max_ratio : L_lipschitz_;
}

std::pair<std::string, double> RegretAnalyzer::analyzeGrowthRate() const {
  RegretMetrics metrics = computeCumulativeRegrets();
  return metrics.getGrowthRate();
}

void RegretAnalyzer::enableLogging(const std::string& filename) {
  log_file_.open(filename);
  if (log_file_.is_open()) {
    logging_enabled_ = true;

    // Write header
    log_file_ << "step,instant_cost,comparator_cost,cost_regret,feasibility_penalty,"
              << "tracking_error_norm,nominal_cost,reference_cost,tightening_slack\n";
  }
}

void RegretAnalyzer::disableLogging() {
  if (log_file_.is_open()) {
    log_file_.close();
  }
  logging_enabled_ = false;
}

void RegretAnalyzer::reset() {
  current_step_ = 0;
  step_history_.clear();

  if (log_file_.is_open()) {
    log_file_.close();
    logging_enabled_ = false;
  }
}

double RegretAnalyzer::computeStageCost(
  const Eigen::VectorXd& state,
  const Eigen::VectorXd& input) const {

  // Standard quadratic cost: ℓ(x,u) = ‖x‖^2_Q + ‖u‖^2_R
  // For simplicity, use Q = I, R = I
  return state.squaredNorm() + input.squaredNorm();
}

double RegretAnalyzer::computeTrackingContribution(const StepRegret& step) const {
  // L_ℓ(1+‖K‖)·‖e_k‖
  return L_lipschitz_ * (1.0 + K_gain_.norm()) * step.tracking_error_norm;
}

double RegretAnalyzer::computeNominalGap(
  const Eigen::VectorXd& nominal_state,
  const Eigen::VectorXd& nominal_input,
  const Eigen::VectorXd& oracle_state,
  const Eigen::VectorXd& oracle_input) const {

  // ℓ(z,v) - ℓ(x*,u*)
  double nominal_cost = computeStageCost(nominal_state, nominal_input);
  double oracle_cost = computeStageCost(oracle_state, oracle_input);

  return nominal_cost - oracle_cost;
}

} // namespace safe_regret
