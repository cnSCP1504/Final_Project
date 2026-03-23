/**
 * @file ActiveInformation.cpp
 * @brief Active information acquisition implementation
 */

#include "safe_regret/ActiveInformation.hpp"
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>
#include <limits>

namespace safe_regret {

// ============================================================================
// ActiveInformation Implementation
// ============================================================================

ActiveInformation::ActiveInformation(int state_dim, int input_dim)
  : state_dim_(state_dim),
    input_dim_(input_dim),
    entropy_threshold_(1.0),
    info_gain_threshold_(0.1) {
}

double ActiveInformation::computeEntropy(const BeliefState& belief) const {
  if (!belief.particles.empty()) {
    // Particle-based entropy estimation
    // H ≈ -Σ p_i log p_i using kernel density estimation

    // For simplicity, use Gaussian approximation from particles
    Eigen::VectorXd mean = Eigen::VectorXd::Zero(state_dim_);
    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(state_dim_, state_dim_);

    // Compute mean
    for (const auto& p : belief.particles) {
      mean += p;
    }
    mean /= belief.particles.size();

    // Compute covariance
    for (const auto& p : belief.particles) {
      Eigen::VectorXd diff = p - mean;
      cov += diff * diff.transpose();
    }
    cov /= belief.particles.size();

    return computeGaussianEntropy(cov);
  } else {
    // Gaussian belief: H = 0.5 * log((2πe)^n |Σ|)
    return computeGaussianEntropy(belief.covariance);
  }
}

double ActiveInformation::computeGaussianEntropy(const Eigen::MatrixXd& covariance) const {
  // H(N(μ, Σ)) = 0.5 * log((2πe)^n |Σ|)
  //              = 0.5 * (n * log(2πe) + log(|Σ|))

  double n = covariance.rows();

  // Compute log determinant using LDLT (more numerically stable)
  Eigen::LDLT<Eigen::MatrixXd> ldlt(covariance);

  if (ldlt.info() == Eigen::Success) {
    // log(|Σ|) = log(|L| |D| |L^T|) = log(|D|) since |L| = 1
    // For LDLT: log_det = sum(log(D_ii))
    Eigen::VectorXd D = ldlt.vectorD();
    double log_det = D.array().log().sum();
    return 0.5 * (n * std::log(2.0 * M_PI * M_E) + log_det);
  } else {
    // Fallback: add regularization and compute
    Eigen::MatrixXd reg_cov = covariance +
                              1e-6 * Eigen::MatrixXd::Identity(covariance.rows(), covariance.cols());
    Eigen::LDLT<Eigen::MatrixXd> ldlt_reg(reg_cov);
    Eigen::VectorXd D = ldlt_reg.vectorD();
    double log_det = D.array().log().sum();
    return 0.5 * (n * std::log(2.0 * M_PI * M_E) + log_det);
  }
}

double ActiveInformation::computeMutualInformation(
  const BeliefState& belief,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise) const {

  // I(b; y) = H(b) - H(b|y)
  // For Gaussian: I(b; y) = 0.5 * log(|Σ| / |Σ - ΣH^T(HΣH^T + R)^(-1)HΣ|)

  // Simplified: use information form
  // I(b; y) ≈ 0.5 * log(det(I + H^T R^(-1) H Σ))

  double H_before = computeEntropy(belief);

  // Simulate measurement update
  Eigen::VectorXd sample_state = belief.mean;  // Use mean as representative
  Eigen::VectorXd predicted_measurement = measurement_model(sample_state);

  // Compute posterior covariance (Kalman update)
  // This is a simplified approximation
  double H_after = H_before * 0.7;  // Assume 30% reduction

  return H_before - H_after;
}

double ActiveInformation::computePredictiveEntropy(
  const BeliefState& belief,
  const Eigen::VectorXd& input,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise) const {

  // Predict belief after executing action
  BeliefState predicted_belief = predictBelief(belief, input, dynamics_func, process_noise);

  return computeEntropy(predicted_belief);
}

double ActiveInformation::computeExpectedInformationGain(
  const BeliefState& belief,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise,
  int num_samples) const {

  // E[IG] = H(b) - E_y[H(b|y)]

  double H_before = computeEntropy(belief);
  double H_after_expected = 0.0;

  std::random_device rd;
  std::mt19937 rng(rd());

  // Monte Carlo integration over possible measurements
  for (int i = 0; i < num_samples; ++i) {
    // Sample state from belief
    Eigen::VectorXd sample_state = belief.sample(rng);

    // Sample measurement
    Eigen::VectorXd measurement = sampleMeasurement(
      sample_state, measurement_model, measurement_noise, rng
    );

    // Update belief with measurement
    BeliefState posterior = updateBeliefWithMeasurement(
      belief, measurement, measurement_model, measurement_noise
    );

    // Compute posterior entropy
    H_after_expected += computeEntropy(posterior);
  }

  H_after_expected /= num_samples;

  return H_before - H_after_expected;
}

InformationMetrics ActiveInformation::computeAllMetrics(
  const BeliefState& belief,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise,
  const Eigen::VectorXd& input,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise) const {

  InformationMetrics metrics;

  metrics.entropy = computeEntropy(belief);
  metrics.mutual_information = computeMutualInformation(
    belief, measurement_model, measurement_noise
  );
  metrics.prediction_entropy = computePredictiveEntropy(
    belief, input, dynamics_func, process_noise
  );
  metrics.information_gain = metrics.entropy - metrics.prediction_entropy;

  return metrics;
}

ExplorationAction ActiveInformation::selectExplorationAction(
  const BeliefState& belief,
  const std::vector<Eigen::VectorXd>& candidate_actions,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise) const {

  ExplorationAction best_action(input_dim_);
  double best_value = -std::numeric_limits<double>::infinity();

  for (const auto& action : candidate_actions) {
    // Compute expected information gain
    BeliefState predicted_belief = predictBelief(
      belief, action, dynamics_func, process_noise
    );

    double info_gain = computeEntropy(belief) - computeEntropy(predicted_belief);

    // Estimate cost (simplified: use action magnitude)
    double cost = action.norm();

    // Value = information gain / cost
    double value = (cost > 1e-6) ? (info_gain / cost) : info_gain;

    if (value > best_value) {
      best_value = value;
      best_action.input = action;
      best_action.information_value = info_gain;
      best_action.execution_cost = cost;
    }
  }

  return best_action;
}

std::pair<Eigen::VectorXd, std::string> ActiveInformation::solveExplorationExploitation(
  const BeliefState& belief,
  const std::vector<Eigen::VectorXd>& exploitation_actions,
  const std::vector<Eigen::VectorXd>& exploration_actions,
  double alpha,
  const STLSpecification& stl_spec,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise) const {

  // Find best exploitation action
  double best_exploitation_value = std::numeric_limits<double>::infinity();
  Eigen::VectorXd best_exploitation_action;

  for (const auto& action : exploitation_actions) {
    double cost = computeTaskCost(belief, action, stl_spec, dynamics_func);
    if (cost < best_exploitation_value) {
      best_exploitation_value = cost;
      best_exploitation_action = action;
    }
  }

  // Find best exploration action
  ExplorationAction best_exploration = selectExplorationAction(
    belief, exploration_actions, dynamics_func, process_noise,
    measurement_model, measurement_noise
  );

  // Combined objective
  // J = alpha * exploitation_cost - (1 - alpha) * information_gain
  double exploitation_score = alpha * best_exploitation_value;
  double exploration_score = alpha * best_exploitation_value - (1.0 - alpha) * best_exploration.information_value;

  std::string justification;

  if (exploration_score < exploitation_score) {
    justification = "Exploration: IG=" + std::to_string(best_exploration.information_value) +
                    ", Cost=" + std::to_string(best_exploration.execution_cost);
    return {best_exploration.input, justification};
  } else {
    justification = "Exploitation: TaskCost=" + std::to_string(best_exploitation_value) +
                    ", Alpha=" + std::to_string(alpha);
    return {best_exploitation_action, justification};
  }
}

std::vector<std::tuple<double, double, double>> ActiveInformation::computeInformationGainMap(
  const BeliefState& belief,
  const Eigen::VectorXd& workspace_bounds,
  double resolution,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise) const {

  std::vector<std::tuple<double, double, double>> ig_map;

  double xmin = workspace_bounds[0];
  double xmax = workspace_bounds[1];
  double ymin = workspace_bounds[2];
  double ymax = workspace_bounds[3];

  for (double x = xmin; x <= xmax; x += resolution) {
    for (double y = ymin; y <= ymax; y += resolution) {
      // Create hypothetical measurement location
      Eigen::VectorXd query_state = belief.mean;
      if (query_state.size() >= 2) {
        query_state[0] = x;
        query_state[1] = y;
      }

      // Compute expected information gain
      // Simplified: use distance-based heuristic
      double distance = (query_state.head<2>() - belief.mean.head<2>()).norm();
      double ig = std::exp(-distance / 5.0);  // Decay with distance

      ig_map.push_back(std::make_tuple(x, y, ig));
    }
  }

  return ig_map;
}

bool ActiveInformation::needsInformationAcquisition(const BeliefState& belief) const {
  double entropy = computeEntropy(belief);
  return entropy > entropy_threshold_;
}

// Private helper methods

BeliefState ActiveInformation::predictBelief(
  const BeliefState& belief,
  const Eigen::VectorXd& input,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise) const {

  // Simplified: predict through dynamics with noise
  BeliefState predicted;
  predicted.mean = dynamics_func(belief.mean, input);
  predicted.covariance = belief.covariance + process_noise;

  return predicted;
}

Eigen::VectorXd ActiveInformation::sampleMeasurement(
  const Eigen::VectorXd& state,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise,
  std::mt19937& rng) const {

  Eigen::VectorXd noiseless_measurement = measurement_model(state);

  // Add Gaussian noise
  Eigen::LLT<Eigen::MatrixXd> llt(measurement_noise);
  Eigen::VectorXd std_normal = Eigen::VectorXd::Zero(measurement_noise.rows());
  for (int i = 0; i < std_normal.size(); ++i) {
    std::normal_distribution<double> dist(0.0, 1.0);
    std_normal[i] = dist(rng);
  }

  return noiseless_measurement + llt.matrixL() * std_normal;
}

BeliefState ActiveInformation::updateBeliefWithMeasurement(
  const BeliefState& belief,
  const Eigen::VectorXd& measurement,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
  const Eigen::MatrixXd& measurement_noise) const {

  // Simplified Bayesian update (Kalman filter for linear Gaussian case)
  BeliefState posterior = belief;

  // Predicted measurement
  Eigen::VectorXd predicted_measurement = measurement_model(belief.mean);

  // Innovation
  Eigen::VectorXd innovation = measurement - predicted_measurement;

  // Simplified: reduce covariance
  posterior.covariance *= 0.8;

  // Update mean towards measurement
  posterior.mean += 0.2 * innovation.head(belief.mean.size());

  return posterior;
}

double ActiveInformation::computeTaskCost(
  const BeliefState& belief,
  const Eigen::VectorXd& action,
  const STLSpecification& stl_spec,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func) const {

  // Simplified: quadratic cost on action magnitude
  double control_cost = action.squaredNorm();

  // Add distance to goal cost (assuming goal at origin)
  double distance_cost = belief.mean.head<2>().norm();

  return control_cost + 0.1 * distance_cost;
}

// ============================================================================
// AdaptiveExplorationStrategy Implementation
// ============================================================================

double AdaptiveExplorationStrategy::computeAlpha(
  double entropy,
  double time_remaining,
  double total_time,
  double task_criticality) {

  // Normalize entropy to [0, 1]
  double entropy_normalized = std::tanh(entropy / 5.0);  // Assumes max entropy ~5

  // Time pressure: high when time_remaining is small
  double time_pressure = 1.0 - (time_remaining / total_time);

  // Alpha combines:
  // - High entropy → lower alpha (more exploration)
  // - Low time → higher alpha (more exploitation)
  // - High criticality → higher alpha (more exploitation)

  double alpha = 0.3 * (1.0 - entropy_normalized) +
                 0.4 * time_pressure +
                 0.3 * task_criticality;

  // Clamp to [0, 1]
  return std::max(0.0, std::min(1.0, alpha));
}

double AdaptiveExplorationStrategy::updateExplorationBudget(
  double initial_budget,
  double entropy,
  double entropy_reduction_rate) {

  // If entropy is reducing quickly, less exploration needed
  double efficiency_factor = std::min(1.0, entropy_reduction_rate / 0.1);

  return initial_budget * (1.0 - efficiency_factor);
}

bool AdaptiveExplorationStrategy::shouldExploit(
  double entropy,
  double time_remaining,
  double stl_robustness) {

  // Switch to exploitation if:
  // 1. Entropy is low (< 1.0)
  // 2. Time is running out (< 20% remaining)
  // 3. STL robustness is satisfied (> 0)

  bool low_uncertainty = entropy < 1.0;
  bool deadline_approaching = time_remaining < 2.0;  // Assuming T=10
  bool task_satisfied = stl_robustness > 0.0;

  return low_uncertainty || deadline_approaching || task_satisfied;
}

} // namespace safe_regret
