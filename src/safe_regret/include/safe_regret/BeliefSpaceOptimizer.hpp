/**
 * @file BeliefSpaceOptimizer.hpp
 * @brief Belief-space optimizer for Safe-Regret MPC
 *
 * Implements belief-state optimization under uncertainty with:
 * - Particle filter for belief propagation
 * - Unscented transform for nonlinear uncertainty propagation
 * - STL robustness integration in belief space
 * - Gradient-based optimization for reference planning
 *
 * Based on manuscript Section 4.2: Belief-Space STL Robustness
 */

#pragma once

#include "safe_regret/ReferencePlanner.hpp"
#include "safe_regret/RegretMetrics.hpp"
#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <random>
#include <functional>

namespace safe_regret {

/**
 * @brief Particle filter state representation
 */
struct Particle {
  Eigen::VectorXd state;     ///< Particle state
  double weight;             ///< Particle weight (normalized)

  Particle(int dim = 0)
    : state(dim), weight(1.0) {}

  Particle(const Eigen::VectorXd& s, double w = 1.0)
    : state(s), weight(w) {}
};

/**
 * @brief Sigma point for unscented transform
 */
struct SigmaPoint {
  Eigen::VectorXd state;
  double weight_mean;
  double weight_cov;

  SigmaPoint(int dim = 0)
    : state(dim), weight_mean(0.0), weight_cov(0.0) {}
};

/**
 * @brief STL robustness evaluation result
 */
struct STLRobustness {
  double robustness;         ///< ρ(φ; x) - raw robustness value
  double expected_robustness; ///< E[ρ(φ)] - expected robustness over belief
  double robustness_variance; ///< Var[ρ(φ)] - variance over belief
  bool is_satisfied;         ///< ρ(φ) ≥ 0

  STLRobustness()
    : robustness(0.0), expected_robustness(0.0),
      robustness_variance(0.0), is_satisfied(false) {}
};

/**
 * @brief Optimization options for belief-space optimizer
 */
struct BeliefOptimizationOptions {
  int num_particles;          ///< Number of particles for belief approximation
  int max_iterations;         ///< Maximum optimization iterations
  double tolerance;           ///< Convergence tolerance
  double learning_rate;       ///< Gradient descent learning rate
  double stl_weight;          ///< Weight λ for STL robustness term
  double regularization;      ///< L2 regularization weight
  bool use_unscented_transform;  ///< Use UKF instead of particle filter

  BeliefOptimizationOptions()
    : num_particles(100),
      max_iterations(50),
      tolerance(1e-4),
      learning_rate(0.01),
      stl_weight(1.0),
      regularization(0.01),
      use_unscented_transform(false) {}
};

/**
 * @brief Belief-space optimizer for Safe-Regret MPC
 *
 * Solves: min E[Σ ℓ(x_t, u_t)] - λ·E[ρ(φ; x_{k:k+N})]
 * subject to: belief dynamics, chance constraints
 */
class BeliefSpaceOptimizer {
public:
  /**
   * @brief Constructor
   * @param state_dim State dimension
   * @param input_dim Input dimension
   */
  BeliefSpaceOptimizer(int state_dim, int input_dim);

  /**
   * @brief Destructor
   */
  ~BeliefSpaceOptimizer();

  // ==========================================================================
  // Particle Filter Methods
  // ==========================================================================

  /**
   * @brief Initialize particle filter from belief
   * @param belief Initial belief state
   * @param num_particles Number of particles to generate
   * @return Vector of particles
   */
  std::vector<Particle> initializeParticles(
    const BeliefState& belief,
    int num_particles) const;

  /**
   * @brief Propagate particles through dynamics
   * @param particles Current particles
   * @param input Control input
   * @param dynamics_func Dynamics function f(x, u)
   * @return Propagated particles
   */
  std::vector<Particle> propagateParticles(
    const std::vector<Particle>& particles,
    const Eigen::VectorXd& input,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func) const;

  /**
   * @brief Update particle weights from measurement
   * @param particles Current particles
   * @param measurement Measurement y
   * @param likelihood_func Likelihood function p(y|x)
   * @return Updated particles (weights normalized)
   */
  std::vector<Particle> updateParticleWeights(
    const std::vector<Particle>& particles,
    const Eigen::VectorXd& measurement,
    std::function<double(const Eigen::VectorXd&, const Eigen::VectorXd&)> likelihood_func) const;

  /**
   * @brief Resample particles using systematic resampling
   * @param particles Current particles
   * @return Resampled particles
   */
  std::vector<Particle> resampleParticles(
    const std::vector<Particle>& particles) const;

  /**
   * @brief Estimate belief from particles
   * @param particles Particle set
   * @return Estimated belief state
   */
  BeliefState estimateBeliefFromParticles(
    const std::vector<Particle>& particles) const;

  // ==========================================================================
  // Unscented Transform Methods
  // ==========================================================================

  /**
   * @brief Generate sigma points for unscented transform
   * @param mean Mean state
   * @param covariance Covariance matrix
   * @param alpha UKF alpha parameter
   * @param beta UKF beta parameter
   * @param kappa UKF kappa parameter
   * @return Vector of sigma points
   */
  std::vector<SigmaPoint> generateSigmaPoints(
    const Eigen::VectorXd& mean,
    const Eigen::MatrixXd& covariance,
    double alpha = 1e-3,
    double beta = 2.0,
    double kappa = 0.0) const;

  /**
   * @brief Unscented transform for nonlinear function
   * @param sigma_points Input sigma points
   * @param transform_func Nonlinear transformation
   * @return Transformed mean and covariance
   */
  std::pair<Eigen::VectorXd, Eigen::MatrixXd> unscentedTransform(
    const std::vector<SigmaPoint>& sigma_points,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&)> transform_func) const;

  /**
   * @brief Predict belief through nonlinear dynamics using UKF
   * @param belief Current belief
   * @param input Control input
   * @param dynamics_func Dynamics function
   * @param process_noise Process noise covariance
   * @return Predicted belief
   */
  BeliefState predictBeliefUKF(
    const BeliefState& belief,
    const Eigen::VectorXd& input,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise) const;

  // ==========================================================================
  // STL Robustness Methods
  // ==========================================================================

  /**
   * @brief Compute STL robustness over belief space
   * @param belief Current belief
   * @param stl_spec STL specification
   * @param trajectory Candidate trajectory
   * @param smooth_tau Temperature for smooth robustness (0 = exact)
   * @return STL robustness evaluation
   */
  STLRobustness computeSTLRobustness(
    const BeliefState& belief,
    const STLSpecification& stl_spec,
    const ReferenceTrajectory& trajectory,
    double smooth_tau = 0.05) const;

  /**
   * @brief Compute expected robustness E[ρ(φ)] over particles
   * @param particles Particle set representing belief
   * @param stl_spec STL specification
   * @param trajectory Candidate trajectory
   * @return Expected robustness and variance
   */
  std::pair<double, double> computeExpectedRobustness(
    const std::vector<Particle>& particles,
    const STLSpecification& stl_spec,
    const ReferenceTrajectory& trajectory,
    double smooth_tau = 0.05) const;

  /**
   * @brief Compute robustness gradient for optimization
   * @param belief Current belief
   * @param stl_spec STL specification
   * @param trajectory Candidate trajectory
   * @param smooth_tau Temperature for smooth robustness
   * @return Gradient w.r.t. trajectory parameters
   */
  Eigen::VectorXd computeRobustnessGradient(
    const BeliefState& belief,
    const STLSpecification& stl_spec,
    const ReferenceTrajectory& trajectory,
    double smooth_tau = 0.05) const;

  // ==========================================================================
  // Belief-Space Optimization
  // ==========================================================================

  /**
   * @brief Optimize reference trajectory in belief space
   *
   * Solves: min J_ref = E[Σ ℓ(z_t, v_t)] - λ·E[ρ(φ; z_{k:k+N})]
   *
   * @param initial_belief Current belief state
   * @param stl_spec STL specification
   * @param initial_guess Initial reference trajectory
   * @param options Optimization options
   * @return Optimized reference trajectory
   */
  ReferenceTrajectory optimizeReferenceTrajectory(
    const BeliefState& initial_belief,
    const STLSpecification& stl_spec,
    const ReferenceTrajectory& initial_guess,
    const BeliefOptimizationOptions& options = BeliefOptimizationOptions()) const;

  /**
   * @brief Compute cost of reference trajectory in belief space
   * @param belief Current belief
   * @param trajectory Reference trajectory
   * @param stl_spec STL specification
   * @param options Optimization options
   * @return Cost value
   */
  double computeBeliefSpaceCost(
    const BeliefState& belief,
    const ReferenceTrajectory& trajectory,
    const STLSpecification& stl_spec,
    const BeliefOptimizationOptions& options) const;

  /**
   * @brief Compute cost gradient for optimization
   * @param belief Current belief
   * @param trajectory Reference trajectory
   * @param stl_spec STL specification
   * @param options Optimization options
   * @return Gradient w.r.t. trajectory parameters
   */
  Eigen::VectorXd computeBeliefSpaceCostGradient(
    const BeliefState& belief,
    const ReferenceTrajectory& trajectory,
    const STLSpecification& stl_spec,
    const BeliefOptimizationOptions& options) const;

  // ==========================================================================
  // Utility Methods
  // ==========================================================================

  /**
   * @brief Simulate belief propagation over trajectory
   * @param initial_belief Starting belief
   * @param trajectory Reference trajectory
   * @param dynamics_func Dynamics function
   * @param process_noise Process noise covariance
   * @return Belief trajectory (belief at each time step)
   */
  std::vector<BeliefState> simulateBeliefTrajectory(
    const BeliefState& initial_belief,
    const ReferenceTrajectory& trajectory,
    std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
    const Eigen::MatrixXd& process_noise) const;

  /**
   * @brief Set random seed for reproducibility
   * @param seed Random seed
   */
  void setRandomSeed(unsigned int seed) {
    rng_.seed(seed);
  }

private:
  // Dimensions
  int state_dim_;
  int input_dim_;

  // Random number generation
  mutable std::mt19937 rng_;

  // Helper functions
  /**
   * @brief Compute smooth min/max for robustness
   */
  static double smoothMin(double a, double b, double tau);
  static double smoothMax(double a, double b, double tau);

  /**
   * @brief Evaluate STL predicate on state
   */
  double evaluatePredicate(
    const Eigen::VectorXd& state,
    const std::string& predicate) const;

  /**
   * @brief Compute stage cost
   */
  double computeStageCost(
    const ReferencePoint& ref_point) const;

  /**
   * @brief Line search for gradient descent
   */
  ReferenceTrajectory lineSearch(
    const ReferenceTrajectory& current,
    const Eigen::VectorXd& gradient,
    const BeliefState& belief,
    const STLSpecification& stl_spec,
    const BeliefOptimizationOptions& options) const;
};

} // namespace safe_regret
