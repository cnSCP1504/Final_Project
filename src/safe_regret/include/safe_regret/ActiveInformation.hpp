/**
 * @file ActiveInformation.hpp
 * @brief Active information acquisition for Safe-Regret MPC
 *
 * Implements mutual information computation, entropy-based exploration,
 * and exploration-exploitation tradeoff for belief-space planning.
 *
 * Based on manuscript Section 4.2: Information Acquisition
 */

#pragma once

#include "safe_regret/ReferencePlanner.hpp"
#include <Eigen/Dense>
#include <vector>
#include <functional>
#include <memory>

namespace safe_regret {

/**
 * @brief Information metrics for belief states
 */
struct InformationMetrics {
  double entropy;              ///< H(b): Shannon entropy
  double mutual_information;   ///< I(b; y): Mutual information with measurement
  double prediction_entropy;   ///< H(b'): Predictive entropy after action
  double information_gain;     ///< IG = H(b) - H(b'|y)

  InformationMetrics()
    : entropy(0.0), mutual_information(0.0),
      prediction_entropy(0.0), information_gain(0.0) {}
};

/**
 * @brief Exploration action with information value
 */
struct ExplorationAction {
  Eigen::VectorXd input;        ///< Control input to explore
  double information_value;    ///< Expected information gain
  double execution_cost;       ///< Cost of exploration (time, energy)

  ExplorationAction(int input_dim = 0)
    : input(input_dim), information_value(0.0), execution_cost(0.0) {}
};

/**
 * @brief Active information acquisition for belief-space planning
 *
 * Computes information-theoretic quantities to guide exploration:
 * - Mutual information I(b; y) for measurement selection
 * - Entropy H(b) for uncertainty quantification
 * - Information gain for exploration actions
 * - Exploration-exploitation tradeoff
 */
class ActiveInformation {
public:
  /**
   * @brief Constructor
   * @param state_dim State dimension
   * @param input_dim Input dimension
   */
  ActiveInformation(int state_dim, int input_dim);

  /**
   * @brief Compute Shannon entropy of belief
   *
   * H(b) = -∫ p(x) log p(x) dx
   *
   * For Gaussian: H(b) = 0.5 * log((2πe)^n |Σ|)
   *
   * @param belief Belief state
   * @return Entropy in nats
   */
  double computeEntropy(const BeliefState& belief) const;

  /**
   * @brief Compute mutual information with measurement
   *
   * I(b; y) = H(b) - H(b|y)
   *
   * Reduction in uncertainty after measurement
   *
   * @param belief Current belief
   * @param measurement_model Measurement function y = g(x, ν)
   * @param measurement_noise Measurement noise covariance
   * @return Mutual information in nats
   */
  double computeMutualInformation(
    const BeliefState& belief,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise) const;

  /**
   * @brief Compute predictive entropy after action
   *
   * H(b') = H(b') where b' is predicted belief after executing action
   *
   * @param belief Current belief
   * @param input Control input to execute
   * @param dynamics_func Dynamics function x' = f(x, u)
   * @param process_noise Process noise covariance
   * @return Predictive entropy in nats
   */
  double computePredictiveEntropy(
    const BeliefState& belief,
    const Eigen::VectorXd& input,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise) const;

  /**
   * @brief Compute expected information gain
   *
   * E[IG] = H(b) - E_y[H(b|y)]
   *
   * Expected reduction in entropy from taking measurement
   *
   * @param belief Current belief
   * @param measurement_model Measurement function
   * @param measurement_noise Measurement noise
   * @param num_samples Number of samples for expectation
   * @return Expected information gain
   */
  double computeExpectedInformationGain(
    const BeliefState& belief,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise,
    int num_samples = 100) const;

  /**
   * @brief Compute all information metrics
   *
   * Computes entropy, mutual information, predictive entropy, and gain
   *
   * @param belief Current belief
   * @param measurement_model Measurement function
   * @param measurement_noise Measurement noise
   * @param input Action to evaluate (for predictive entropy)
   * @param dynamics_func Dynamics function
   * @param process_noise Process noise
   * @return InformationMetrics struct with all values
   */
  InformationMetrics computeAllMetrics(
    const BeliefState& belief,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise,
    const Eigen::VectorXd& input,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise) const;

  /**
   * @brief Select best exploration action
   *
   * Maximizes information gain per unit cost:
   * max_u (IG(u) / cost(u))
   *
   * @param belief Current belief
   * @param candidate_actions Candidate exploration actions
   * @param dynamics_func Dynamics function
   * @param process_noise Process noise
   * @param measurement_model Measurement function
   * @param measurement_noise Measurement noise
   * @return Best exploration action
   */
  ExplorationAction selectExplorationAction(
    const BeliefState& belief,
    const std::vector<Eigen::VectorXd>& candidate_actions,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise) const;

  /**
   * @brief Solve exploration-exploitation tradeoff
   *
   * Balances information gain (exploration) vs task performance (exploitation):
   * J = α * E[ℓ(z,v)] - (1-α) * E[IG]
   *
   * @param belief Current belief
   * @param exploitation_actions Task-oriented actions
   * @param exploration_actions Information-gathering actions
   * @param alpha Tradeoff parameter (0=pure exploration, 1=pure exploitation)
   * @param stl_spec STL specification for task cost
   * @param dynamics_func Dynamics function
   * @param process_noise Process noise
   * @param measurement_model Measurement function
   * @param measurement_noise Measurement noise
   * @return Selected action with justification
   */
  std::pair<Eigen::VectorXd, std::string> solveExplorationExploitation(
    const BeliefState& belief,
    const std::vector<Eigen::VectorXd>& exploitation_actions,
    const std::vector<Eigen::VectorXd>& exploration_actions,
    double alpha,
    const STLSpecification& stl_spec,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise) const;

  /**
   * @brief Compute information gain map over workspace
   *
   * For visualization and analysis: computes expected information gain
   * at each location in workspace
   *
   * @param belief Current belief
   * @param workspace_bounds [xmin, xmax, ymin, ymax]
   * @param resolution Grid resolution
   * @param measurement_model Measurement function
   * @param measurement_noise Measurement noise
   * @return Vector of (x, y, information_gain) tuples
   */
  std::vector<std::tuple<double, double, double>> computeInformationGainMap(
    const BeliefState& belief,
    const Eigen::VectorXd& workspace_bounds,
    double resolution,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise) const;

  /**
   * @brief Set entropy threshold for active information
   *
   * Only gather information when H(b) > threshold
   */
  void setEntropyThreshold(double threshold) { entropy_threshold_ = threshold; }

  /**
   * @brief Set information gain threshold
   *
   * Only take exploration actions if expected IG > threshold
   */
  void setInformationGainThreshold(double threshold) { info_gain_threshold_ = threshold; }

  /**
   * @brief Check if information acquisition is needed
   *
   * Returns true if entropy is above threshold
   *
   * @param belief Current belief
   * @return true if information acquisition is beneficial
   */
  bool needsInformationAcquisition(const BeliefState& belief) const;

private:
  int state_dim_;
  int input_dim_;

  // Thresholds
  double entropy_threshold_;
  double info_gain_threshold_;

  // Helper functions

  /**
   * @brief Compute Gaussian entropy
   *
   * H(N(μ, Σ)) = 0.5 * log((2πe)^n |Σ|)
   */
  double computeGaussianEntropy(const Eigen::MatrixXd& covariance) const;

  /**
   * @brief Predict belief after action
   *
   * Uses unscented transform for nonlinear dynamics
   */
  BeliefState predictBelief(
    const BeliefState& belief,
    const Eigen::VectorXd& input,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise) const;

  /**
   * @brief Sample measurement for Monte Carlo integration
   */
  Eigen::VectorXd sampleMeasurement(
    const Eigen::VectorXd& state,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise,
    std::mt19937& rng) const;

  /**
   * @brief Update belief with measurement (Bayesian update)
   */
  BeliefState updateBeliefWithMeasurement(
    const BeliefState& belief,
    const Eigen::VectorXd& measurement,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> measurement_model,
    const Eigen::MatrixXd& measurement_noise) const;

  /**
   * @brief Compute task cost for exploitation action
   *
   * Expected stage cost + STL robustness penalty
   */
  double computeTaskCost(
    const BeliefState& belief,
    const Eigen::VectorXd& action,
    const STLSpecification& stl_spec,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func) const;
};

/**
 * @brief Adaptive exploration strategy
 *
 * Adjusts exploration-exploitation tradeoff based on:
 * - Current uncertainty (entropy)
 * - Time remaining (deadline)
 * - Task criticality
 */
class AdaptiveExplorationStrategy {
public:
  /**
   * @brief Compute adaptive alpha for exploration-exploitation
   *
   * alpha = f(H(b), t_remaining, task_importance)
   *
   * - High entropy → more exploration (lower alpha)
   * - Low time remaining → more exploitation (higher alpha)
   * - Critical task → more exploitation (higher alpha)
   *
   * @param entropy Current belief entropy
   * @param time_remaining Time until deadline
   * @param total_time Total task time
   * @param task_criticality Task importance [0, 1]
   * @return alpha in [0, 1]
   */
  static double computeAlpha(
    double entropy,
    double time_remaining,
    double total_time,
    double task_criticality = 0.5);

  /**
   * @brief Update exploration budget
   *
   * Dynamically adjust how much time to spend exploring
   *
   * @param initial_budget Initial exploration budget
   * @param entropy Current entropy
   * @param entropy_reduction_rate How fast entropy is decreasing
   * @return Updated exploration budget
   */
  static double updateExplorationBudget(
    double initial_budget,
    double entropy,
    double entropy_reduction_rate);

  /**
   * @brief Check if should switch to exploitation mode
   *
   * Switch conditions:
   * - Entropy is low enough
   * - Deadline approaching
   * - Task nearly complete
   *
   * @param entropy Current entropy
   * @param time_remaining Time until deadline
   * @param stl_robustness Current STL robustness
   * @return true if should exploit
   */
  static bool shouldExploit(
    double entropy,
    double time_remaining,
    double stl_robustness);
};

} // namespace safe_regret
