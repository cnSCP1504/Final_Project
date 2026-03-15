#ifndef BELIEF_SPACE_EVALUATOR_H
#define BELIEF_SPACE_EVALUATOR_H

#include <vector>
#include <memory>
#include <Eigen/Dense>
#include "STL_ros/STLFormula.h"
#include "STL_ros/SmoothRobustness.h"

namespace stl_ros {

/**
 * @brief Belief state representation (Gaussian distribution)
 *
 * The belief β_k = 𝓝(x̂_k, Σ_k) represents the robot's state estimate:
 * - x̂_k: Mean state estimate
 * - Σ_k: Covariance matrix (uncertainty)
 */
struct BeliefState {
    Eigen::VectorXd mean;     // State mean (n x 1)
    Eigen::MatrixXd cov;      // State covariance (n x n)

    BeliefState(int state_dim = 0) {
        mean = Eigen::VectorXd::Zero(state_dim);
        cov = Eigen::MatrixXd::Zero(state_dim, state_dim);
    }

    /**
     * @brief Sample from belief distribution
     * @param num_samples Number of samples to draw
     * @return Sampled states
     */
    std::vector<Eigen::VectorXd> sample(int num_samples) const;
};

/**
 * @brief Belief-space STL robustness evaluator
 *
 * From manuscript:
 * ρ̃_k = E_{x∼β_k}[ρ^soft(φ; x_{k:k+N})]
 *
 * Computes expected robustness over belief distribution using Monte Carlo sampling.
 */
class BeliefSpaceEvaluator {
public:
    /**
     * @brief Constructor
     * @param num_samples Number of Monte Carlo samples
     * @param tau Temperature for smooth robustness
     */
    BeliefSpaceEvaluator(int num_samples = 100, double tau = 0.05);

    /**
     * @brief Evaluate expected robustness over belief
     * @param formula STL formula
     * @param current_belief Current belief state β_k
     * @param prediction_horizon Prediction horizon N
     * @param dt Time step
     * @return Expected robustness E[ρ^soft(φ; x_{k:k+N})]
     */
    double expectedRobustness(const STLFormula::Ptr& formula,
                             const BeliefState& current_belief,
                             int prediction_horizon,
                             double dt = 0.1) const;

    /**
     * @brief Evaluate with belief propagation
     * @param formula STL formula
     * @param belief_trajectory Belief trajectory over horizon
     * @param times Time points
     * @return Expected robustness over belief evolution
     */
    double expectedRobustnessTrajectory(const STLFormula::Ptr& formula,
                                       const std::vector<BeliefState>& belief_trajectory,
                                       const std::vector<double>& times) const;

    /**
     * @brief Compute gradient of expected robustness w.r.t. belief mean
     * @param formula STL formula
     * @param current_belief Current belief state
     * @param prediction_horizon Prediction horizon
     * @param dt Time step
     * @return Gradient vector
     */
    Eigen::VectorXd gradientMean(const STLFormula::Ptr& formula,
                                const BeliefState& current_belief,
                                int prediction_horizon,
                                double dt = 0.1) const;

    /**
     * @brief Get Monte Carlo samples for debugging
     */
    std::vector<double> getSampleValues() const { return sample_values_; }

    /**
     * @brief Get semantics for external access
     */
    const SmoothSTLSemantics& getSemantics() const { return semantics_; }

private:
    int num_samples_;  // Monte Carlo sample count
    SmoothSTLSemantics semantics_;
    mutable std::vector<double> sample_values_;  // Store samples for analysis

    /**
     * @brief Propagate belief forward using simple dynamics model
     * @param belief Current belief
     * @param control Input sequence
     * @param dt Time step
     * @return Predicted belief trajectory
     */
    std::vector<BeliefState> propagateBelief(const BeliefState& belief,
                                            const std::vector<Eigen::VectorXd>& control,
                                            double dt) const;

    /**
     * @brief Simple uncertainty propagation (Kalman prediction step)
     */
    BeliefState predictBelief(const BeliefState& belief,
                             const Eigen::VectorXd& control,
                             double dt) const;
};

/**
 * @brief Efficient evaluator using importance sampling
 *
 * For better performance with high-dimensional beliefs or complex formulas.
 */
class ImportanceSamplingEvaluator {
public:
    /**
     * @brief Constructor
     * @param num_samples Number of samples
     * @param tau Temperature parameter
     */
    ImportanceSamplingEvaluator(int num_samples = 50, double tau = 0.05);

    /**
     * @brief Evaluate using importance sampling
     * @param formula STL formula
     * @param current_belief Current belief
     * @param prediction_horizon Prediction horizon
     * @param dt Time step
     * @return Expected robustness
     */
    double evaluate(const STLFormula::Ptr& formula,
                   const BeliefState& current_belief,
                   int prediction_horizon,
                   double dt = 0.1) const;

private:
    int num_samples_;
    SmoothSTLSemantics semantics_;
};

} // namespace stl_ros

#endif // BELIEF_SPACE_EVALUATOR_H
