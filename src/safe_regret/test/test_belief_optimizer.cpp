/**
 * @file test_belief_optimizer.cpp
 * @brief Test BeliefSpaceOptimizer functionality
 */

#include <safe_regret/BeliefSpaceOptimizer.hpp>
#include <safe_regret/ReferencePlanner.hpp>
#include <iostream>
#include <cassert>
#include <cmath>

using namespace safe_regret;

void test_assert(bool condition, const std::string& test_name) {
  if (condition) {
    std::cout << "✓ PASS: " << test_name << std::endl;
  } else {
    std::cout << "✗ FAIL: " << test_name << std::endl;
    std::exit(1);
  }
}

// Simple dynamics: x_{k+1} = x_k + u_k (only first 2 dimensions affected by input)
Eigen::VectorXd simpleDynamics(const Eigen::VectorXd& state, const Eigen::VectorXd& input) {
  Eigen::VectorXd next_state = state;
  next_state(0) += input(0);  // x += vx
  next_state(1) += input(1);  // y += vy
  // Other dimensions unchanged
  return next_state;
}

// Simple likelihood: Gaussian
double simpleLikelihood(const Eigen::VectorXd& state, const Eigen::VectorXd& measurement) {
  double diff = (state - measurement).norm();
  return std::exp(-0.5 * diff * diff);
}

void test_particle_filter_initialization() {
  std::cout << "\n=== Test: Particle Filter Initialization ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Create Gaussian belief
  BeliefState belief(3);
  belief.mean << 1.0, 2.0, 0.0;
  belief.covariance << 1.0, 0.0, 0.0,
                      0.0, 1.0, 0.0,
                      0.0, 0.0, 1.0;

  // Initialize particles
  auto particles = optimizer.initializeParticles(belief, 100);

  test_assert(particles.size() == 100, "Generated 100 particles");

  // Check weights are normalized
  double sum_weights = 0.0;
  for (const auto& p : particles) {
    sum_weights += p.weight;
  }
  test_assert(std::abs(sum_weights - 1.0) < 1e-10, "Particle weights normalized");

  // Check mean of particles is close to belief mean
  Eigen::VectorXd particle_mean = Eigen::VectorXd::Zero(3);
  for (const auto& p : particles) {
    particle_mean += p.weight * p.state;
  }

  double mean_error = (particle_mean - belief.mean).norm();
  test_assert(mean_error < 0.5, "Particle mean close to belief mean");
}

void test_particle_propagation() {
  std::cout << "\n=== Test: Particle Propagation ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Initialize particles
  BeliefState belief(3);
  belief.mean << 0.0, 0.0, 0.0;
  belief.covariance.setIdentity();

  auto particles = optimizer.initializeParticles(belief, 50);

  // Propagate with simple dynamics
  Eigen::VectorXd input(2);
  input << 0.1, 0.2;

  auto propagated = optimizer.propagateParticles(particles, input, simpleDynamics);

  test_assert(propagated.size() == particles.size(), "Particle count preserved");

  // Check particles moved (first two dimensions should change)
  for (size_t i = 0; i < particles.size(); ++i) {
    Eigen::VectorXd expected(3);
    expected << particles[i].state(0) + input(0),
                particles[i].state(1) + input(1),
                particles[i].state(2);

    double diff = (propagated[i].state - expected).norm();
    test_assert(diff < 1e-10, "Particle propagated correctly");
  }
}

void test_particle_weights_update() {
  std::cout << "\n=== Test: Particle Weight Update ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Initialize particles
  BeliefState belief(3);
  belief.mean << 0.0, 0.0, 0.0;
  belief.covariance.setIdentity();

  auto particles = optimizer.initializeParticles(belief, 50);

  // Update weights with measurement
  Eigen::VectorXd measurement(3);
  measurement << 0.5, 0.5, 0.0;

  auto updated = optimizer.updateParticleWeights(particles, measurement, simpleLikelihood);

  // Check weights still normalized
  double sum_weights = 0.0;
  for (const auto& p : updated) {
    sum_weights += p.weight;
  }
  test_assert(std::abs(sum_weights - 1.0) < 1e-10, "Weights normalized after update");

  // Check weights changed
  bool weights_changed = false;
  for (size_t i = 0; i < particles.size(); ++i) {
    if (std::abs(particles[i].weight - updated[i].weight) > 1e-10) {
      weights_changed = true;
      break;
    }
  }
  test_assert(weights_changed, "Particle weights updated");
}

void test_belief_estimation() {
  std::cout << "\n=== Test: Belief Estimation from Particles ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Create known belief
  BeliefState belief(3);
  belief.mean << 1.0, 2.0, 0.0;
  belief.covariance << 0.5, 0.0, 0.0,
                      0.0, 0.5, 0.0,
                      0.0, 0.0, 0.5;

  // Generate particles
  auto particles = optimizer.initializeParticles(belief, 500);

  // Estimate belief
  BeliefState estimated = optimizer.estimateBeliefFromParticles(particles);

  // Check mean estimate
  double mean_error = (estimated.mean - belief.mean).norm();
  test_assert(mean_error < 0.3, "Estimated mean close to true mean");

  // Check covariance estimate (rough check)
  double cov_error = (estimated.covariance - belief.covariance).norm();
  test_assert(cov_error < 1.0, "Estimated covariance reasonable");
}

void test_unscented_transform() {
  std::cout << "\n=== Test: Unscented Transform ===" << std::endl;

  BeliefSpaceOptimizer optimizer(2, 2);

  // Create Gaussian
  Eigen::VectorXd mean(2);
  mean << 1.0, 2.0;

  Eigen::MatrixXd cov(2, 2);
  cov << 1.0, 0.5,
        0.5, 1.0;

  // Generate sigma points
  auto sigma_points = optimizer.generateSigmaPoints(mean, cov);

  test_assert(sigma_points.size() == 2 * 2 + 1, "Generated 2n+1 sigma points");

  // Check first sigma point is the mean
  double diff_first = (sigma_points[0].state - mean).norm();
  test_assert(diff_first < 1e-10, "First sigma point is the mean");

  // Check weights sum to 1 for mean weights
  double sum_mean_weights = 0.0;
  for (const auto& sp : sigma_points) {
    sum_mean_weights += sp.weight_mean;
  }
  test_assert(std::abs(sum_mean_weights - 1.0) < 1e-10, "Mean weights sum to 1");

  // Cov weights have different scaling (standard UKF property)
  double sum_cov_weights = 0.0;
  for (const auto& sp : sigma_points) {
    sum_cov_weights += sp.weight_cov;
  }
  // For standard UKF parameters, sum_cov_weights > 1 is normal
  test_assert(sum_cov_weights > 0.0, "Cov weights are positive");
}

void test_stl_robustness_computation() {
  std::cout << "\n=== Test: STL Robustness Computation ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Create belief
  BeliefState belief(3);
  belief.mean << 0.0, 0.0, 0.0;
  belief.covariance.setIdentity();

  // Create STL spec
  STLSpecification stl_spec = STLSpecification::createReachability("x >= 5.0", 10.0);

  // Create trajectory moving towards goal
  ReferenceTrajectory trajectory;
  for (int t = 0; t < 10; ++t) {
    ReferencePoint pt(3, 2);
    pt.state << static_cast<double>(t), 0.0, 0.0;
    pt.input << 1.0, 0.0;
    pt.timestamp = t;
    trajectory.addPoint(pt);
  }

  // Compute robustness
  STLRobustness robustness = optimizer.computeSTLRobustness(
    belief, stl_spec, trajectory, 0.05
  );

  std::cout << "  Expected robustness: " << robustness.expected_robustness << std::endl;
  std::cout << "  Robustness variance: " << robustness.robustness_variance << std::endl;
  std::cout << "  STL satisfied: " << (robustness.is_satisfied ? "YES" : "NO") << std::endl;

  test_assert(!std::isnan(robustness.expected_robustness), "Robustness is valid number");
  test_assert(robustness.robustness_variance >= 0.0, "Variance is non-negative");
}

void test_belief_space_optimization() {
  std::cout << "\n=== Test: Belief-Space Optimization ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Create initial belief
  BeliefState belief(3);
  belief.mean << -5.0, 0.0, 0.0;
  belief.covariance.setIdentity();

  // Create STL spec
  STLSpecification stl_spec = STLSpecification::createReachability("x >= 5.0", 10.0);

  // Create initial guess (straight line)
  ReferenceTrajectory initial_guess;
  for (int t = 0; t < 10; ++t) {
    ReferencePoint pt(3, 2);
    pt.state << -5.0 + t, 0.0, 0.0;
    pt.input << 1.0, 0.0;
    pt.timestamp = t;
    initial_guess.addPoint(pt);
  }

  // Set optimization options
  BeliefOptimizationOptions options;
  options.max_iterations = 10;
  options.num_particles = 50;
  options.stl_weight = 0.5;

  // Optimize
  ReferenceTrajectory optimized = optimizer.optimizeReferenceTrajectory(
    belief, stl_spec, initial_guess, options
  );

  test_assert(!optimized.points.empty(), "Optimized trajectory generated");
  test_assert(optimized.points.size() == initial_guess.points.size(), "Trajectory size preserved");

  // Check that cost improved (or stayed same)
  double initial_cost = optimizer.computeBeliefSpaceCost(
    belief, initial_guess, stl_spec, options
  );
  double optimized_cost = optimizer.computeBeliefSpaceCost(
    belief, optimized, stl_spec, options
  );

  std::cout << "  Initial cost: " << initial_cost << std::endl;
  std::cout << "  Optimized cost: " << optimized_cost << std::endl;
  std::cout << "  Cost improvement: " << (initial_cost - optimized_cost) << std::endl;

  test_assert(optimized_cost <= initial_cost + 1e-6, "Optimization did not increase cost");
}

void test_belief_trajectory_simulation() {
  std::cout << "\n=== Test: Belief Trajectory Simulation ===" << std::endl;

  BeliefSpaceOptimizer optimizer(3, 2);

  // Initial belief
  BeliefState initial_belief(3);
  initial_belief.mean << 0.0, 0.0, 0.0;
  initial_belief.covariance.setIdentity();

  // Create trajectory
  ReferenceTrajectory trajectory;
  for (int t = 0; t < 5; ++t) {
    ReferencePoint pt(3, 2);
    pt.state << static_cast<double>(t), 0.0, 0.0;
    pt.input << 1.0, 0.0;
    pt.timestamp = t;
    trajectory.addPoint(pt);
  }

  // Process noise
  Eigen::MatrixXd process_noise = 0.1 * Eigen::MatrixXd::Identity(3, 3);

  // Simulate
  auto belief_trajectory = optimizer.simulateBeliefTrajectory(
    initial_belief, trajectory, simpleDynamics, process_noise
  );

  test_assert(belief_trajectory.size() == trajectory.points.size() + 1, "Belief trajectory length correct");

  // Check belief evolution
  for (size_t i = 1; i < belief_trajectory.size(); ++i) {
    // Mean should progress forward
    test_assert(belief_trajectory[i].mean[0] >= belief_trajectory[i-1].mean[0] - 0.5,
                "Belief progresses forward");
  }
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "  BeliefSpaceOptimizer Tests" << std::endl;
  std::cout << "========================================" << std::endl;

  try {
    test_particle_filter_initialization();
    test_particle_propagation();
    test_particle_weights_update();
    test_belief_estimation();
    test_unscented_transform();
    test_stl_robustness_computation();
    test_belief_space_optimization();
    test_belief_trajectory_simulation();

    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ ALL BELIEF OPTIMIZER TESTS PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "\n✗ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
