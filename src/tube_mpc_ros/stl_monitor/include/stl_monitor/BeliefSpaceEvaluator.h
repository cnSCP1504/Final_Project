#ifndef STL_MONITOR_BELIEFSPACEEVALUATOR_H
#define STL_MONITOR_BELIEFSPACEEVALUATOR_H

#include "stl_monitor/STLTypes.h"
#include "stl_monitor/SmoothRobustness.h"
#include <random>

namespace stl_monitor {

/**
 * @brief Belief-space STL robustness evaluation
 *
 * Computes expected robustness under belief state:
 * ρ̃_k = E_{x∼β_k}[ρ(φ; x_{k:k+N})]
 *
 * Supports two methods:
 * 1. Particle-based Monte Carlo integration
 * 2. Unscented Transform (sigma-point sampling)
 */
class BeliefSpaceEvaluator {
public:
    enum class Method {
        PARTICLE,       // Monte Carlo with particle cloud
        UNSCENTED       // Unscented Transform
    };

    BeliefSpaceEvaluator(Method method = Method::PARTICLE);
    ~BeliefSpaceEvaluator();

    /**
     * @brief Compute expected robustness in belief space
     * @param formula STL formula
     * @param belief Belief state distribution
     * @param time_horizon Prediction horizon
     * @return Expected robustness value
     */
    RobustnessValue computeExpectedRobustness(
        const STLFormula& formula,
        const BeliefState& belief,
        double time_horizon
    );

    /**
     * @brief Set evaluation method
     */
    void setMethod(Method method);

    /**
     * @brief Particle-based methods
     */
    void setNumParticles(size_t n);
    size_t getNumParticles() const;

    /**
     * @brief Generate particle samples from belief
     * @param belief Belief state
     * @param num_samples Number of samples
     * @return Sampled states
     */
    std::vector<Eigen::VectorXd> sampleParticles(
        const BeliefState& belief,
        size_t num_samples
    ) const;

    /**
     * @brief Unscented Transform parameters
     */
    void setUnscentedParams(double alpha = 1e-3, double beta = 2.0, double kappa = 0.0);

    /**
     * @brief Generate sigma points for Unscented Transform
     */
    std::vector<Eigen::VectorXd> generateSigmaPoints(
        const BeliefState& belief
    ) const;

    /**
     * @brief Compute sigma point weights
     */
    struct SigmaWeights {
        Eigen::VectorXd mean_weights;
        Eigen::VectorXd cov_weights;
    };
    SigmaWeights computeSigmaWeights(size_t n_dim) const;

    /**
     * @brief Generate predicted trajectory from belief
     * Used for propagating particles forward in time
     */
    Trajectory predictTrajectory(
        const BeliefState& belief,
        double time_horizon,
        double dt = 0.1
    ) const;

    /**
     * @brief Set dynamics model for prediction
     * using simple motion model by default
     */
    using DynamicsFunc = std::function<Eigen::VectorXd(const Eigen::VectorXd&, double)>;
    void setDynamicsModel(DynamicsFunc dynamics);

private:
    Method method_;
    size_t num_particles_;

    // Unscented Transform parameters
    double alpha_;
    double beta_;
    double kappa_;
    mutable double lambda_;  // mutable: computed in const functions

    // Dynamics model
    DynamicsFunc dynamics_model_;

    // Random number generator
    mutable std::mt19937 rng_;

    // Helper functions
    RobustnessValue computeParticleBased_(
        const STLFormula& formula,
        const BeliefState& belief,
        double time_horizon
    );

    RobustnessValue computeUnscented_(
        const STLFormula& formula,
        const BeliefState& belief,
        double time_horizon
    );

    // Default dynamics model (constant velocity)
    Eigen::VectorXd defaultDynamics_(
        const Eigen::VectorXd& state,
        double dt
    ) const;
};

} // namespace stl_monitor

#endif // STL_MONITOR_BELIEFSPACEEVALUATOR_H
