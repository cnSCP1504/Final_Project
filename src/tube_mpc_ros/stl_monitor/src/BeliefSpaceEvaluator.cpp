#include "stl_monitor/BeliefSpaceEvaluator.h"
#include <cmath>
#include <random>

namespace stl_monitor {

BeliefSpaceEvaluator::BeliefSpaceEvaluator(Method method)
    : method_(method),
      num_particles_(100),
      alpha_(1e-3),
      beta_(2.0),
      kappa_(0.0),
      lambda_(0.0),
      rng_(std::random_device{}())
{
    // Default: constant velocity motion model
    dynamics_model_ = [this](const Eigen::VectorXd& state, double dt) -> Eigen::VectorXd {
        return this->defaultDynamics_(state, dt);
    };
}

BeliefSpaceEvaluator::~BeliefSpaceEvaluator() {}

void BeliefSpaceEvaluator::setMethod(Method method) {
    method_ = method;
}

void BeliefSpaceEvaluator::setNumParticles(size_t n) {
    if (n > 0) {
        num_particles_ = n;
    }
}

size_t BeliefSpaceEvaluator::getNumParticles() const {
    return num_particles_;
}

RobustnessValue BeliefSpaceEvaluator::computeExpectedRobustness(
    const STLFormula& formula,
    const BeliefState& belief,
    double time_horizon)
{
    if (method_ == Method::PARTICLE) {
        return computeParticleBased_(formula, belief, time_horizon);
    } else {
        return computeUnscented_(formula, belief, time_horizon);
    }
}

std::vector<Eigen::VectorXd> BeliefSpaceEvaluator::sampleParticles(
    const BeliefState& belief,
    size_t num_samples) const
{
    int n_dim = belief.mean.size();

    // Check if covariance is positive definite
    Eigen::MatrixXd L = belief.covariance.llt().matrixL();

    std::vector<Eigen::VectorXd> particles;
    particles.reserve(num_samples);

    std::normal_distribution<double> dist(0.0, 1.0);

    for (size_t i = 0; i < num_samples; ++i) {
        // Sample from standard normal
        Eigen::VectorXd z(n_dim);
        for (int j = 0; j < n_dim; ++j) {
            z(j) = dist(rng_);
        }

        // Transform to belief distribution: x = μ + L*z
        Eigen::VectorXd sample = belief.mean + L * z;
        particles.push_back(sample);
    }

    return particles;
}

void BeliefSpaceEvaluator::setUnscentedParams(double alpha, double beta, double kappa) {
    alpha_ = alpha;
    beta_ = beta;
    kappa_ = kappa;
}

std::vector<Eigen::VectorXd> BeliefSpaceEvaluator::generateSigmaPoints(
    const BeliefState& belief) const
{
    int n_dim = belief.mean.size();
    lambda_ = alpha_ * alpha_ * (n_dim + kappa_) - n_dim;

    // Number of sigma points: 2n + 1
    size_t n_points = 2 * n_dim + 1;
    std::vector<Eigen::VectorXd> sigma_points;
    sigma_points.reserve(n_points);

    // Compute sqrt of (n + λ) * P
    Eigen::MatrixXd sqrt_P = ((n_dim + lambda_) * belief.covariance).llt().matrixL();

    // First sigma point: mean
    sigma_points.push_back(belief.mean);

    // Remaining sigma points: mean ± columns of sqrt_P
    for (int i = 0; i < n_dim; ++i) {
        Eigen::VectorXd offset = sqrt_P.col(i);

        sigma_points.push_back(belief.mean + offset);
        sigma_points.push_back(belief.mean - offset);
    }

    return sigma_points;
}

BeliefSpaceEvaluator::SigmaWeights BeliefSpaceEvaluator::computeSigmaWeights(size_t n_dim) const {
    lambda_ = alpha_ * alpha_ * (n_dim + kappa_) - n_dim;

    SigmaWeights weights;
    size_t n_points = 2 * n_dim + 1;

    weights.mean_weights = Eigen::VectorXd(n_points);
    weights.cov_weights = Eigen::VectorXd(n_points);

    // Weight for mean
    weights.mean_weights(0) = lambda_ / (n_dim + lambda_);
    weights.cov_weights(0) = weights.mean_weights(0) + (1 - alpha_ * alpha_ + beta_);

    // Weights for sigma points
    double w_common = 1.0 / (2 * (n_dim + lambda_));
    for (size_t i = 1; i < n_points; ++i) {
        weights.mean_weights(i) = w_common;
        weights.cov_weights(i) = w_common;
    }

    return weights;
}

Trajectory BeliefSpaceEvaluator::predictTrajectory(
    const BeliefState& belief,
    double time_horizon,
    double dt) const
{
    Trajectory traj;
    size_t n_steps = static_cast<size_t>(time_horizon / dt);

    traj.reserve(n_steps + 1);

    // Initial state
    Eigen::VectorXd state = belief.mean;
    traj.push_back(0.0, state);

    // Propagate forward
    for (size_t i = 0; i < n_steps; ++i) {
        state = dynamics_model_(state, dt);
        traj.push_back((i + 1) * dt, state);
    }

    return traj;
}

void BeliefSpaceEvaluator::setDynamicsModel(DynamicsFunc dynamics) {
    dynamics_model_ = dynamics;
}

RobustnessValue BeliefSpaceEvaluator::computeParticleBased_(
    const STLFormula& formula,
    const BeliefState& belief,
    double time_horizon)
{
    // Sample particles from belief
    std::vector<Eigen::VectorXd> particles = sampleParticles(belief, num_particles_);

    // Evaluate robustness for each particle
    std::vector<double> robustness_values;
    robustness_values.reserve(num_particles_);

    for (const auto& particle : particles) {
        // Create trajectory from particle (simple prediction)
        Trajectory traj = predictTrajectory(
            BeliefState(particle.size()),  // dummy belief, just for mean initialization
            time_horizon,
            0.1
        );

        // Override initial state
        traj.states[0] = particle;

        // Compute robustness
        SmoothRobustness smooth_rob(0.1);
        RobustnessValue rob = smooth_rob.compute(formula, traj);
        robustness_values.push_back(rob.value);
    }

    // Expected robustness = mean over particles
    double sum = 0.0;
    for (double val : robustness_values) {
        sum += val;
    }
    double expected_rob = sum / num_particles_;

    // Compute variance
    double variance = 0.0;
    for (double val : robustness_values) {
        variance += (val - expected_rob) * (val - expected_rob);
    }
    variance /= num_particles_;

    return RobustnessValue(expected_rob);
}

RobustnessValue BeliefSpaceEvaluator::computeUnscented_(
    const STLFormula& formula,
    const BeliefState& belief,
    double time_horizon)
{
    // Generate sigma points
    std::vector<Eigen::VectorXd> sigma_points = generateSigmaPoints(belief);
    SigmaWeights weights = computeSigmaWeights(belief.mean.size());

    // Evaluate robustness for each sigma point
    std::vector<double> robustness_values;
    for (const auto& sigma_point : sigma_points) {
        // Create trajectory from sigma point
        Trajectory traj = predictTrajectory(
            BeliefState(sigma_point.size()),
            time_horizon,
            0.1
        );
        traj.states[0] = sigma_point;

        // Compute robustness
        SmoothRobustness smooth_rob(0.1);
        RobustnessValue rob = smooth_rob.compute(formula, traj);
        robustness_values.push_back(rob.value);
    }

    // Expected robustness = weighted sum
    double expected_rob = 0.0;
    for (size_t i = 0; i < sigma_points.size(); ++i) {
        expected_rob += weights.mean_weights(i) * robustness_values[i];
    }

    return RobustnessValue(expected_rob);
}

Eigen::VectorXd BeliefSpaceEvaluator::defaultDynamics_(
    const Eigen::VectorXd& state,
    double dt) const
{
    // Simple constant velocity model
    // state: [x, y, theta, v, omega]
    Eigen::VectorXd next_state = state;

    if (state.size() >= 5) {
        double x = state[0];
        double y = state[1];
        double theta = state[2];
        double v = state[3];
        double omega = state[4];

        // Bicycle model
        next_state[0] = x + v * std::cos(theta) * dt;
        next_state[1] = y + v * std::sin(theta) * dt;
        next_state[2] = theta + omega * dt;
        // velocity and angular velocity remain constant
    }

    return next_state;
}

} // namespace stl_monitor
