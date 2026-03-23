/**
 * @file BeliefSpaceOptimizer.cpp
 * @brief Implementation of belief-space optimizer
 */

#include "safe_regret/BeliefSpaceOptimizer.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace safe_regret {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Constructor/Destructor
// ============================================================================

BeliefSpaceOptimizer::BeliefSpaceOptimizer(int state_dim, int input_dim)
  : state_dim_(state_dim),
    input_dim_(input_dim),
    rng_(std::random_device{}()) {
}

BeliefSpaceOptimizer::~BeliefSpaceOptimizer() {
}

// ============================================================================
// Particle Filter Methods
// ============================================================================

std::vector<Particle> BeliefSpaceOptimizer::initializeParticles(
  const BeliefState& belief,
  int num_particles) const {

  std::vector<Particle> particles;
  particles.reserve(num_particles);

  // Use particles from belief if available
  if (!belief.particles.empty()) {
    for (const auto& p : belief.particles) {
      particles.emplace_back(p, 1.0 / num_particles);
    }
    return particles;
  }

  // Sample from Gaussian belief
  Eigen::LLT<Eigen::MatrixXd> chol(belief.covariance);

  if (chol.info() != Eigen::Success) {
    // Fallback: sample around mean with small noise
    std::normal_distribution<double> noise(0.0, 0.1);
    for (int i = 0; i < num_particles; ++i) {
      Eigen::VectorXd state = belief.mean;
      for (int j = 0; j < state_dim_; ++j) {
        state[j] += noise(rng_);
      }
      particles.emplace_back(state, 1.0 / num_particles);
    }
    return particles;
  }

  // Sample using Cholesky decomposition: x = μ + L·z
  std::normal_distribution<double> dist(0.0, 1.0);
  auto L = chol.matrixL();

  for (int i = 0; i < num_particles; ++i) {
    Eigen::VectorXd z(state_dim_);
    for (int j = 0; j < state_dim_; ++j) {
      z[j] = dist(rng_);
    }

    Eigen::VectorXd state = belief.mean + L * z;
    particles.emplace_back(state, 1.0 / num_particles);
  }

  return particles;
}

std::vector<Particle> BeliefSpaceOptimizer::propagateParticles(
  const std::vector<Particle>& particles,
  const Eigen::VectorXd& input,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func) const {

  std::vector<Particle> propagated;
  propagated.reserve(particles.size());

  for (const auto& particle : particles) {
    Eigen::VectorXd new_state = dynamics_func(particle.state, input);
    propagated.emplace_back(new_state, particle.weight);
  }

  return propagated;
}

std::vector<Particle> BeliefSpaceOptimizer::updateParticleWeights(
  const std::vector<Particle>& particles,
  const Eigen::VectorXd& measurement,
  std::function<double(const Eigen::VectorXd&, const Eigen::VectorXd&)> likelihood_func) const {

  std::vector<Particle> updated = particles;

  // Update weights
  double sum_weights = 0.0;
  for (auto& particle : updated) {
    particle.weight *= likelihood_func(particle.state, measurement);
    sum_weights += particle.weight;
  }

  // Normalize
  if (sum_weights > 1e-10) {
    for (auto& particle : updated) {
      particle.weight /= sum_weights;
    }
  }

  return updated;
}

std::vector<Particle> BeliefSpaceOptimizer::resampleParticles(
  const std::vector<Particle>& particles) const {

  if (particles.empty()) {
    return particles;
  }

  // Systematic resampling
  int n = particles.size();
  std::vector<Particle> resampled;
  resampled.reserve(n);

  // Compute cumulative weights
  std::vector<double> cumulative(n);
  cumulative[0] = particles[0].weight;
  for (int i = 1; i < n; ++i) {
    cumulative[i] = cumulative[i-1] + particles[i].weight;
  }

  // Systematic sampling
  double r = (std::uniform_real_distribution<double>(0.0, 1.0)(rng_)) / n;
  int j = 0;

  for (int i = 0; i < n; ++i) {
    double u = r + (double)i / n;
    while (j < n - 1 && u > cumulative[j]) {
      j++;
    }
    resampled.emplace_back(particles[j].state, 1.0 / n);
  }

  return resampled;
}

BeliefState BeliefSpaceOptimizer::estimateBeliefFromParticles(
  const std::vector<Particle>& particles) const {

  if (particles.empty()) {
    return BeliefState(state_dim_);
  }

  // Estimate mean
  Eigen::VectorXd mean = Eigen::VectorXd::Zero(state_dim_);
  for (const auto& particle : particles) {
    mean += particle.weight * particle.state;
  }

  // Estimate covariance
  Eigen::MatrixXd covariance = Eigen::MatrixXd::Zero(state_dim_, state_dim_);
  for (const auto& particle : particles) {
    Eigen::VectorXd diff = particle.state - mean;
    covariance += particle.weight * (diff * diff.transpose());
  }

  BeliefState belief(state_dim_);
  belief.mean = mean;
  belief.covariance = covariance;

  return belief;
}

// ============================================================================
// Unscented Transform Methods
// ============================================================================

std::vector<SigmaPoint> BeliefSpaceOptimizer::generateSigmaPoints(
  const Eigen::VectorXd& mean,
  const Eigen::MatrixXd& covariance,
  double alpha,
  double beta,
  double kappa) const {

  int n = mean.size();
  int num_sigma = 2 * n + 1;

  std::vector<SigmaPoint> sigma_points;
  sigma_points.reserve(num_sigma);

  // Compute scaling parameter
  double lambda = alpha * alpha * (n + kappa) - n;

  // Compute sqrt of (n + lambda) * covariance
  Eigen::MatrixXd scaled_cov = (n + lambda) * covariance;
  Eigen::LLT<Eigen::MatrixXd> chol(scaled_cov);

  if (chol.info() != Eigen::Success) {
    // Fallback: add small diagonal noise
    scaled_cov += 1e-6 * Eigen::MatrixXd::Identity(n, n);
    chol.compute(scaled_cov);
  }

  auto L_matrix = chol.matrixL().toDenseMatrix();

  // First sigma point: mean
  SigmaPoint pt0(n);
  pt0.state = mean;
  pt0.weight_mean = lambda / (n + lambda);
  pt0.weight_cov = lambda / (n + lambda) + (1.0 - alpha * alpha + beta);
  sigma_points.push_back(pt0);

  // Verify weights sum to 1 (for debugging)
  // mean weights should sum to: λ/(n+λ) + 2n * 1/(2(n+λ)) = 1
  // cov weights should sum to: [λ/(n+λ) + (1-α²+β)] + 2n * 1/(2(n+λ)) = 1 + (1-α²+β)
  // For standard UKF with α=1e-3, β=2, κ=0: (1-α²+β) ≈ 3, so sum is 4
  // This is intentional - the cov weights are scaled differently

  // Remaining sigma points: mean ± columns of L
  for (int i = 0; i < n; ++i) {
    SigmaPoint pt_plus(n), pt_minus(n);

    pt_plus.state = mean + L_matrix.col(i);
    pt_plus.weight_mean = 1.0 / (2.0 * (n + lambda));
    pt_plus.weight_cov = 1.0 / (2.0 * (n + lambda));

    pt_minus.state = mean - L_matrix.col(i);
    pt_minus.weight_mean = 1.0 / (2.0 * (n + lambda));
    pt_minus.weight_cov = 1.0 / (2.0 * (n + lambda));

    sigma_points.push_back(pt_plus);
    sigma_points.push_back(pt_minus);
  }

  return sigma_points;
}

std::pair<Eigen::VectorXd, Eigen::MatrixXd> BeliefSpaceOptimizer::unscentedTransform(
  const std::vector<SigmaPoint>& sigma_points,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&)> transform_func) const {

  if (sigma_points.empty()) {
    return {Eigen::VectorXd::Zero(state_dim_), Eigen::MatrixXd::Zero(state_dim_, state_dim_)};
  }

  int n = sigma_points[0].state.size();

  // Transform all sigma points
  std::vector<Eigen::VectorXd> transformed_points;
  transformed_points.reserve(sigma_points.size());

  for (const auto& sigma_pt : sigma_points) {
    transformed_points.push_back(transform_func(sigma_pt.state));
  }

  // Compute transformed mean
  Eigen::VectorXd mean = Eigen::VectorXd::Zero(n);
  for (size_t i = 0; i < sigma_points.size(); ++i) {
    mean += sigma_points[i].weight_mean * transformed_points[i];
  }

  // Compute transformed covariance
  Eigen::MatrixXd covariance = Eigen::MatrixXd::Zero(n, n);
  for (size_t i = 0; i < sigma_points.size(); ++i) {
    Eigen::VectorXd diff = transformed_points[i] - mean;
    covariance += sigma_points[i].weight_cov * (diff * diff.transpose());
  }

  return {mean, covariance};
}

BeliefState BeliefSpaceOptimizer::predictBeliefUKF(
  const BeliefState& belief,
  const Eigen::VectorXd& input,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise) const {

  // Generate sigma points
  auto sigma_points = generateSigmaPoints(belief.mean, belief.covariance);

  // Transform through dynamics
  auto transform_func = [&dynamics_func, &input](const Eigen::VectorXd& state) {
    return dynamics_func(state, input);
  };

  std::pair<Eigen::VectorXd, Eigen::MatrixXd> result = unscentedTransform(sigma_points, transform_func);
  Eigen::VectorXd predicted_mean = result.first;
  Eigen::MatrixXd predicted_cov = result.second;

  // Add process noise
  predicted_cov += process_noise;

  BeliefState predicted(belief.mean.size());
  predicted.mean = predicted_mean;
  predicted.covariance = predicted_cov;

  return predicted;
}

// ============================================================================
// STL Robustness Methods
// ============================================================================

double BeliefSpaceOptimizer::smoothMin(double a, double b, double tau) {
  // Smooth min: -τ * log(exp(-a/τ) + exp(-b/τ))
  if (tau < 1e-10) {
    return std::min(a, b);
  }
  return -tau * std::log(std::exp(-a/tau) + std::exp(-b/tau));
}

double BeliefSpaceOptimizer::smoothMax(double a, double b, double tau) {
  // Smooth max: τ * log(exp(a/τ) + exp(b/τ))
  if (tau < 1e-10) {
    return std::max(a, b);
  }
  return tau * std::log(std::exp(a/tau) + std::exp(b/tau));
}

double BeliefSpaceOptimizer::evaluatePredicate(
  const Eigen::VectorXd& state,
  const std::string& predicate) const {

  // Simple predicate evaluation (can be extended)
  // Format: "variable operator value"
  // Example: "x >= 0.0", "y <= 5.0"

  if (predicate.empty()) {
    return 0.0;
  }

  // Parse simple predicates
  if (predicate.find("x >= ") == 0 || predicate.find("x[0] >= ") == 0) {
    double value = std::stod(predicate.substr(predicate.find(">=") + 2));
    return state[0] - value;
  }

  if (predicate.find("y >= ") == 0 || predicate.find("x[1] >= ") == 0) {
    double value = std::stod(predicate.substr(predicate.find(">=") + 2));
    return state[1] - value;
  }

  if (predicate.find("x <= ") == 0 || predicate.find("x[0] <= ") == 0) {
    double value = std::stod(predicate.substr(predicate.find("<=") + 2));
    return value - state[0];
  }

  if (predicate.find("y <= ") == 0 || predicate.find("x[1] <= ") == 0) {
    double value = std::stod(predicate.substr(predicate.find("<=") + 2));
    return value - state[1];
  }

  // Default: assume satisfaction
  return 1.0;
}

STLRobustness BeliefSpaceOptimizer::computeSTLRobustness(
  const BeliefState& belief,
  const STLSpecification& stl_spec,
  const ReferenceTrajectory& trajectory,
  double smooth_tau) const {

  STLRobustness result;

  // Generate particles from belief
  auto particles = initializeParticles(belief, 100);

  // Compute expected robustness
  std::pair<double, double> robustness_result = computeExpectedRobustness(
    particles, stl_spec, trajectory, smooth_tau
  );

  result.expected_robustness = robustness_result.first;
  result.robustness_variance = robustness_result.second;
  result.is_satisfied = (robustness_result.first >= 0.0);

  // For raw robustness, use mean state
  result.robustness = robustness_result.first;  // Simplified

  return result;
}

std::pair<double, double> BeliefSpaceOptimizer::computeExpectedRobustness(
  const std::vector<Particle>& particles,
  const STLSpecification& stl_spec,
  const ReferenceTrajectory& trajectory,
  double smooth_tau) const {

  if (particles.empty() || trajectory.points.empty()) {
    return {0.0, 0.0};
  }

  std::vector<double> particle_robustness;
  particle_robustness.reserve(particles.size());

  // For each particle, compute robustness over trajectory
  for (const auto& particle : particles) {
    double min_robustness = std::numeric_limits<double>::infinity();

    // Compute robustness at each time step
    for (const auto& ref_point : trajectory.points) {
      double state_robustness = evaluatePredicate(ref_point.state, stl_spec.formula);
      min_robustness = smoothMin(min_robustness, state_robustness, smooth_tau);
    }

    particle_robustness.push_back(min_robustness);
  }

  // Compute mean and variance
  double sum = std::accumulate(particle_robustness.begin(), particle_robustness.end(), 0.0);
  double mean = sum / particle_robustness.size();

  double variance = 0.0;
  for (double r : particle_robustness) {
    variance += (r - mean) * (r - mean);
  }
  variance /= particle_robustness.size();

  return {mean, variance};
}

Eigen::VectorXd BeliefSpaceOptimizer::computeRobustnessGradient(
  const BeliefState& belief,
  const STLSpecification& stl_spec,
  const ReferenceTrajectory& trajectory,
  double smooth_tau) const {

  // Simplified gradient computation
  // In practice, would use automatic differentiation or analytical gradients

  int num_params = trajectory.points.size() * (state_dim_ + input_dim_);
  Eigen::VectorXd gradient = Eigen::VectorXd::Zero(num_params);

  // Finite differences (simplified)
  double epsilon = 1e-6;

  ReferenceTrajectory trajectory_perturbed = trajectory;
  STLRobustness robustness_base = computeSTLRobustness(
    belief, stl_spec, trajectory, smooth_tau
  );

  // Compute gradient for first few parameters (for demonstration)
  int num_grad_params = std::min(10, num_params);

  for (int i = 0; i < num_grad_params; ++i) {
    // Perturb parameter i
    trajectory_perturbed = trajectory;
    int pt_idx = i / (state_dim_ + input_dim_);
    int dim_idx = i % (state_dim_ + input_dim_);

    if (pt_idx < trajectory.points.size()) {
      if (dim_idx < state_dim_) {
        trajectory_perturbed.points[pt_idx].state[dim_idx] += epsilon;
      } else {
        trajectory_perturbed.points[pt_idx].input[dim_idx - state_dim_] += epsilon;
      }

      STLRobustness robustness_perturbed = computeSTLRobustness(
        belief, stl_spec, trajectory_perturbed, smooth_tau
      );

      gradient[i] = (robustness_perturbed.expected_robustness - robustness_base.expected_robustness) / epsilon;
    }
  }

  return gradient;
}

// ============================================================================
// Belief-Space Optimization
// ============================================================================

double BeliefSpaceOptimizer::computeStageCost(
  const ReferencePoint& ref_point) const {

  // Quadratic cost: ℓ(z, v) = ‖z‖² + ‖v‖²
  double state_cost = ref_point.state.squaredNorm();
  double input_cost = ref_point.input.squaredNorm();

  return state_cost + input_cost;
}

double BeliefSpaceOptimizer::computeBeliefSpaceCost(
  const BeliefState& belief,
  const ReferenceTrajectory& trajectory,
  const STLSpecification& stl_spec,
  const BeliefOptimizationOptions& options) const {

  double cost = 0.0;

  // Stage cost: E[Σ ℓ(z_t, v_t)]
  for (const auto& pt : trajectory.points) {
    cost += computeStageCost(pt);
  }

  // STL robustness term: -λ·E[ρ(φ)]
  STLRobustness robustness = computeSTLRobustness(
    belief, stl_spec, trajectory, 0.05
  );

  cost -= options.stl_weight * robustness.expected_robustness;

  // Regularization
  for (const auto& pt : trajectory.points) {
    cost += options.regularization * pt.state.squaredNorm();
  }

  return cost;
}

Eigen::VectorXd BeliefSpaceOptimizer::computeBeliefSpaceCostGradient(
  const BeliefState& belief,
  const ReferenceTrajectory& trajectory,
  const STLSpecification& stl_spec,
  const BeliefOptimizationOptions& options) const {

  int num_params = trajectory.points.size() * (state_dim_ + input_dim_);
  Eigen::VectorXd gradient = Eigen::VectorXd::Zero(num_params);

  // Finite difference gradient
  double epsilon = 1e-6;
  double base_cost = computeBeliefSpaceCost(belief, trajectory, stl_spec, options);

  for (int i = 0; i < num_params; ++i) {
    ReferenceTrajectory trajectory_perturbed = trajectory;
    int pt_idx = i / (state_dim_ + input_dim_);
    int dim_idx = i % (state_dim_ + input_dim_);

    if (pt_idx < trajectory.points.size()) {
      if (dim_idx < state_dim_) {
        trajectory_perturbed.points[pt_idx].state[dim_idx] += epsilon;
      } else {
        trajectory_perturbed.points[pt_idx].input[dim_idx - state_dim_] += epsilon;
      }

      double perturbed_cost = computeBeliefSpaceCost(
        belief, trajectory_perturbed, stl_spec, options
      );

      gradient[i] = (perturbed_cost - base_cost) / epsilon;
    }
  }

  return gradient;
}

ReferenceTrajectory BeliefSpaceOptimizer::lineSearch(
  const ReferenceTrajectory& current,
  const Eigen::VectorXd& gradient,
  const BeliefState& belief,
  const STLSpecification& stl_spec,
  const BeliefOptimizationOptions& options) const {

  double alpha = options.learning_rate;
  double rho = 0.5;  // Backtracking factor
  double c = 0.5;    // Sufficient decrease constant

  double current_cost = computeBeliefSpaceCost(belief, current, stl_spec, options);

  ReferenceTrajectory next = current;
  int num_params = current.points.size() * (state_dim_ + input_dim_);

  // Backtracking line search
  for (int iter = 0; iter < 10; ++iter) {
    // Take gradient step
    next = current;
    for (int i = 0; i < num_params; ++i) {
      int pt_idx = i / (state_dim_ + input_dim_);
      int dim_idx = i % (state_dim_ + input_dim_);

      if (pt_idx < next.points.size()) {
        double step = alpha * gradient[i];
        if (dim_idx < state_dim_) {
          next.points[pt_idx].state[dim_idx] -= step;
        } else {
          next.points[pt_idx].input[dim_idx - state_dim_] -= step;
        }
      }
    }

    double next_cost = computeBeliefSpaceCost(belief, next, stl_spec, options);

    // Armijo condition
    if (next_cost <= current_cost + c * alpha * gradient.dot(gradient)) {
      return next;
    }

    alpha *= rho;
  }

  return next;
}

ReferenceTrajectory BeliefSpaceOptimizer::optimizeReferenceTrajectory(
  const BeliefState& initial_belief,
  const STLSpecification& stl_spec,
  const ReferenceTrajectory& initial_guess,
  const BeliefOptimizationOptions& options) const {

  ReferenceTrajectory trajectory = initial_guess;
  double prev_cost = std::numeric_limits<double>::infinity();

  for (int iter = 0; iter < options.max_iterations; ++iter) {
    // Compute cost and gradient
    double cost = computeBeliefSpaceCost(
      initial_belief, trajectory, stl_spec, options
    );

    Eigen::VectorXd gradient = computeBeliefSpaceCostGradient(
      initial_belief, trajectory, stl_spec, options
    );

    // Check convergence
    if (std::abs(cost - prev_cost) < options.tolerance) {
      std::cout << "  Belief optimization converged in " << iter << " iterations" << std::endl;
      break;
    }

    prev_cost = cost;

    // Line search and update
    trajectory = lineSearch(trajectory, gradient, initial_belief, stl_spec, options);
  }

  return trajectory;
}

// ============================================================================
// Utility Methods
// ============================================================================

std::vector<BeliefState> BeliefSpaceOptimizer::simulateBeliefTrajectory(
  const BeliefState& initial_belief,
  const ReferenceTrajectory& trajectory,
  std::function<Eigen::VectorXd(const Eigen::VectorXd&, const Eigen::VectorXd&)> dynamics_func,
  const Eigen::MatrixXd& process_noise) const {

  std::vector<BeliefState> belief_trajectory;

  BeliefState current_belief = initial_belief;
  belief_trajectory.push_back(current_belief);

  // Default: use particle filter
  for (const auto& ref_point : trajectory.points) {
    // Particle filter propagation
    auto particles = initializeParticles(current_belief, 100);
    particles = propagateParticles(particles, ref_point.input, dynamics_func);
    current_belief = estimateBeliefFromParticles(particles);

    belief_trajectory.push_back(current_belief);
  }

  return belief_trajectory;
}

} // namespace safe_regret
