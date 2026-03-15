#include "STL_ros/BeliefSpaceEvaluator.h"
#include <random>
#include <cmath>

namespace stl_ros {

// ==================== BeliefState Implementation ====================

std::vector<Eigen::VectorXd> BeliefState::sample(int num_samples) const {
    std::vector<Eigen::VectorXd> samples;
    samples.reserve(num_samples);

    // Use Cholesky decomposition for sampling from multivariate Gaussian
    Eigen::LLT<Eigen::MatrixXd> llt(cov);
    if (llt.info() != Eigen::Success) {
        // Fallback to diagonal covariance if Cholesky fails
        Eigen::MatrixXd diag_cov = cov.diagonal().asDiagonal();
        std::vector<Eigen::VectorXd> fallback_samples = sample(num_samples);
        return fallback_samples;
    }

    Eigen::MatrixXd L = llt.matrixL();

    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < num_samples; ++i) {
        Eigen::VectorXd z(mean.size());
        for (int j = 0; j < mean.size(); ++j) {
            z(j) = dist(gen);
        }

        Eigen::VectorXd sample = mean + L * z;
        samples.push_back(sample);
    }

    return samples;
}

// ==================== BeliefSpaceEvaluator Implementation ====================

BeliefSpaceEvaluator::BeliefSpaceEvaluator(int num_samples, double tau)
    : num_samples_(num_samples), semantics_(tau) {
    if (num_samples <= 0) {
        throw std::invalid_argument("Number of samples must be positive");
    }
}

double BeliefSpaceEvaluator::expectedRobustness(
    const STLFormula::Ptr& formula,
    const BeliefState& current_belief,
    int prediction_horizon,
    double dt) const {

    // Sample from belief
    std::vector<Eigen::VectorXd> samples = current_belief.sample(num_samples_);

    // Evaluate robustness for each sample
    double sum_robustness = 0.0;
    sample_values_.clear();
    sample_values_.reserve(num_samples_);

    for (const auto& sample : samples) {
        // Create simple trajectory (constant state for now)
        std::vector<Eigen::VectorXd> trajectory(prediction_horizon, sample);

        // Create time vector
        std::vector<double> times;
        for (int t = 0; t < prediction_horizon; ++t) {
            times.push_back(t * dt);
        }

        // Evaluate robustness
        double robustness = semantics_.evaluate(formula, trajectory, times);
        sum_robustness += robustness;
        sample_values_.push_back(robustness);
    }

    // Return expected value
    return sum_robustness / num_samples_;
}

double BeliefSpaceEvaluator::expectedRobustnessTrajectory(
    const STLFormula::Ptr& formula,
    const std::vector<BeliefState>& belief_trajectory,
    const std::vector<double>& times) const {

    if (belief_trajectory.size() != times.size()) {
        throw std::invalid_argument("Belief trajectory and times must have same size");
    }

    double total_robustness = 0.0;
    int num_timesteps = belief_trajectory.size();

    for (size_t t = 0; t < belief_trajectory.size(); ++t) {
        // Sample from belief at this timestep
        std::vector<Eigen::VectorXd> samples = belief_trajectory[t].sample(num_samples_);

        // Average robustness over samples
        double timestep_robustness = 0.0;
        for (const auto& sample : samples) {
            std::vector<Eigen::VectorXd> single_state(1, sample);
            std::vector<double> single_time(1, times[t]);

            double robustness = semantics_.evaluate(formula, single_state, single_time);
            timestep_robustness += robustness;
        }
        timestep_robustness /= num_samples_;

        total_robustness += timestep_robustness;
    }

    return total_robustness / num_timesteps;
}

Eigen::VectorXd BeliefSpaceEvaluator::gradientMean(
    const STLFormula::Ptr& formula,
    const BeliefState& current_belief,
    int prediction_horizon,
    double dt) const {

    // Compute gradient using finite differences
    const double eps = 1e-6;
    int state_dim = current_belief.mean.size();

    Eigen::VectorXd gradient = Eigen::VectorXd::Zero(state_dim);

    for (int d = 0; d < state_dim; ++d) {
        // Perturb mean forward
        BeliefState belief_plus = current_belief;
        belief_plus.mean(d) += eps;

        double robustness_plus = expectedRobustness(formula, belief_plus, prediction_horizon, dt);

        // Perturb mean backward
        BeliefState belief_minus = current_belief;
        belief_minus.mean(d) -= eps;

        double robustness_minus = expectedRobustness(formula, belief_minus, prediction_horizon, dt);

        // Central difference
        gradient(d) = (robustness_plus - robustness_minus) / (2.0 * eps);
    }

    return gradient;
}

std::vector<BeliefState> BeliefSpaceEvaluator::propagateBelief(
    const BeliefState& belief,
    const std::vector<Eigen::VectorXd>& control,
    double dt) const {

    std::vector<BeliefState> belief_trajectory;
    belief_trajectory.push_back(belief);

    BeliefState current_belief = belief;

    for (const auto& u : control) {
        BeliefState next_belief = predictBelief(current_belief, u, dt);
        belief_trajectory.push_back(next_belief);
        current_belief = next_belief;
    }

    return belief_trajectory;
}

BeliefState BeliefSpaceEvaluator::predictBelief(
    const BeliefState& belief,
    const Eigen::VectorXd& control,
    double dt) const {

    // Simple Kalman prediction step
    // x_{k+1} = f(x_k, u_k)
    // Σ_{k+1} = A Σ_k A^T + Q

    // For simplicity, assume linear dynamics with identity transition
    BeliefState predicted;
    predicted.mean = belief.mean;  // No dynamics for now
    predicted.cov = belief.cov + Eigen::MatrixXd::Identity(belief.cov.rows(), belief.cov.cols()) * 0.01;  // Add process noise

    return predicted;
}

// ==================== ImportanceSamplingEvaluator Implementation ====================

ImportanceSamplingEvaluator::ImportanceSamplingEvaluator(int num_samples, double tau)
    : num_samples_(num_samples), semantics_(tau) {
}

double ImportanceSamplingEvaluator::evaluate(
    const STLFormula::Ptr& formula,
    const BeliefState& current_belief,
    int prediction_horizon,
    double dt) const {

    // Simplified importance sampling
    // In full implementation, would use proposal distribution and importance weights

    std::vector<Eigen::VectorXd> samples = current_belief.sample(num_samples_);

    double weighted_sum = 0.0;
    double total_weight = 0.0;

    for (const auto& sample : samples) {
        std::vector<Eigen::VectorXd> trajectory(prediction_horizon, sample);
        std::vector<double> times;
        for (int t = 0; t < prediction_horizon; ++t) {
            times.push_back(t * dt);
        }

        double robustness = semantics_.evaluate(formula, trajectory, times);
        double weight = std::exp(-robustness);  // Importance weight

        weighted_sum += weight * robustness;
        total_weight += weight;
    }

    return weighted_sum / total_weight;
}

} // namespace stl_ros
