#ifndef STL_MONITOR_SMOOTHROBUSTNESS_H
#define STL_MONITOR_SMOOTHROBUSTNESS_H

#include "stl_monitor/STLTypes.h"
#include <cmath>

namespace stl_monitor {

// Forward declaration for computeNode_ method
class SmoothRobustness;

/**
 * @brief Smooth (differentiable) STL robustness computation
 *
 * Uses log-sum-exp approximation for min/max operators:
 * - smin(x) = -τ * log(Σ exp(-x_i/τ))
 * - smax(x) = τ * log(Σ exp(x_i/τ))
 *
 * This enables gradient-based optimization in MPC.
 */
class SmoothRobustness {
public:
    SmoothRobustness(double temperature = 0.1);
    ~SmoothRobustness();

    /**
     * @brief Compute robustness for a formula over a trajectory
     * @param formula STL formula
     * @param traj State trajectory
     * @return Robustness value with gradient information
     */
    RobustnessValue compute(
        const STLFormula& formula,
        const Trajectory& traj
    );

    /**
     * @brief Compute robustness at specific time
     */
    RobustnessValue computeAtTime(
        const STLFormula& formula,
        const Trajectory& traj,
        double query_time
    );

    /**
     * @brief Set temperature parameter τ
     * Lower τ = more accurate, Higher τ = smoother
     */
    void setTemperature(double tau);
    double getTemperature() const;

    /**
     * @brief Enable/disable gradient computation
     */
    void setComputeGradients(bool enable);

    /**
     * @brief Smooth min approximation
     */
    double smin(const Eigen::VectorXd& x) const;
    double smin(double a, double b) const;

    /**
     * @brief Smooth max approximation
     */
    double smax(const Eigen::VectorXd& x) const;
    double smax(double a, double b) const;

    /**
     * @brief Gradients for smooth min/max
     */
    Eigen::VectorXd smin_grad(const Eigen::VectorXd& x) const;
    Eigen::VectorXd smax_grad(const Eigen::VectorXd& x) const;

private:
    double temperature_;
    bool compute_gradients_;

    // Robustness computation for each operator type
    RobustnessValue computeAtom_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeNode_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeNot_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeAnd_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeOr_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeEventually_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeAlways_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    RobustnessValue computeUntil_(
        const STLFormulaNode* node,
        const Trajectory& traj,
        size_t time_idx
    ) const;

    // Helper: find time indices within interval
    std::vector<size_t> findIndicesInInterval_(
        const Trajectory& traj,
        size_t start_idx,
        const TimeInterval& interval
    ) const;

    // Helper: evaluate atomic predicate
    double evaluatePredicate_(
        const AtomicPredicate& pred,
        const Eigen::VectorXd& state
    ) const;

    // Friend declaration for internal access
    friend class SmoothRobustnessTest;
};

} // namespace stl_monitor

#endif // STL_MONITOR_SMOOTHROBUSTNESS_H
