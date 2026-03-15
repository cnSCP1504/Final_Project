#ifndef SMOOTH_ROBUSTNESS_H
#define SMOOTH_ROBUSTNESS_H

#include <vector>
#include <cmath>
#include <Eigen/Dense>

namespace stl_ros {

/**
 * @brief Smooth robustness calculator using log-sum-exp approximation
 *
 * From manuscript equations:
 * - smax_τ(z₁,...,zq) = τ * log(∑ᵢ e^(zᵢ/τ))
 * - smin_τ(z₁,...,zq) = -τ * log(∑ᵢ e^(-zᵢ/τ))
 *
 * Properties:
 * - Preserves ordering: if zᵢ ≤ zⱼ then smooth_min(z) ≤ smooth_max(z)
 * - Approximation error: |ρ^soft - ρ| ≤ τ * log(C(φ))
 * - Differentiable (gradients available for MPC)
 */
class SmoothRobustness {
public:
    /**
     * @brief Constructor with temperature parameter
     * @param tau Temperature parameter (smaller = more accurate but sharper gradients)
     *            Typical values: 0.01 - 0.1
     */
    explicit SmoothRobustness(double tau = 0.05);

    /**
     * @brief Smooth maximum (log-sum-exp)
     * @param values Vector of values
     * @return smax_τ(values) = τ * log(∑ᵢ e^(values[i]/τ))
     */
    double smoothMax(const std::vector<double>& values) const;

    /**
     * @brief Smooth minimum (negative log-sum-exp)
     * @param values Vector of values
     * @return smin_τ(values) = -τ * log(∑ᵢ e^(-values[i]/τ))
     */
    double smoothMin(const std::vector<double>& values) const;

    /**
     * @brief Smooth maximum over time interval
     * @param trajectory Robustness values over time
     * @param times Time points
     * @param t_start Interval start
     * @param t_end Interval end
     * @return Maximum robustness in [t_start, t_end]
     */
    double smoothMaxInterval(const std::vector<double>& trajectory,
                            const std::vector<double>& times,
                            double t_start, double t_end) const;

    /**
     * @brief Smooth minimum over time interval
     * @param trajectory Robustness values over time
     * @param times Time points
     * @param t_start Interval start
     * @param t_end Interval end
     * @return Minimum robustness in [t_start, t_end]
     */
    double smoothMinInterval(const std::vector<double>& trajectory,
                            const std::vector<double>& times,
                            double t_start, double t_end) const;

    /**
     * @brief Compute gradient of smooth max w.r.t. input values
     * @param values Vector of values
     * @return Gradient vector (same size as values)
     */
    std::vector<double> smoothMaxGradient(const std::vector<double>& values) const;

    /**
     * @brief Compute gradient of smooth min w.r.t. input values
     * @param values Vector of values
     * @return Gradient vector (same size as values)
     */
    std::vector<double> smoothMinGradient(const std::vector<double>& values) const;

    /**
     * @brief Get temperature parameter
     */
    double getTau() const { return tau_; }

    /**
     * @brief Set temperature parameter
     * @param tau New temperature (must be positive)
     */
    void setTau(double tau);

    /**
     * @brief Compute approximation error bound
     * @param num_terms Number of terms in aggregation
     * @return Upper bound on |ρ^soft - ρ|
     */
    double errorBound(int num_terms) const;

private:
    double tau_;  // Temperature parameter

    /**
     * @brief Numerically stable log-sum-exp computation
     * @param values Vector of values
     * @return log(∑ᵢ e^(values[i])) computed in stable way
     */
    double logSumExp(const std::vector<double>& values) const;
};

/**
 * @brief STL semantics using smooth operators
 *
 * Implements smooth versions of:
 * - NOT: ¬φ  →  -ρ(φ)
 * - AND: φ₁ ∧ φ₂  →  min(ρ(φ₁), ρ(φ₂))  →  smin(ρ(φ₁), ρ(φ₂))
 * - OR:  φ₁ ∨ φ₂  →  max(ρ(φ₁), ρ(φ₂))  →  smax(ρ(φ₁), ρ(φ₂))
 * - UNTIL: φ₁ U_[a,b] φ₂
 * - EVENTUALLY: ◇_[a,b] φ  →  sup_{t∈[a,b]} ρ(φ, t)  →  smax over interval
 * - GLOBALLY: □_[a,b] φ  →  inf_{t∈[a,b]} ρ(φ, t)  →  smin over interval
 */
class SmoothSTLSemantics {
public:
    explicit SmoothSTLSemantics(double tau = 0.05) : calc_(tau) {}

    /**
     * @brief Evaluate STL formula with smooth semantics
     * @param formula STL formula
     * @param trajectory State trajectory
     * @param times Time points
     * @return Smooth robustness value
     */
    double evaluate(const STLFormula::Ptr& formula,
                   const std::vector<Eigen::VectorXd>& trajectory,
                   const std::vector<double>& times) const;

    /**
     * @brief Compute gradient of robustness w.r.t. trajectory states
     * @param formula STL formula
     * @param trajectory State trajectory
     * @param times Time points
     * @return Gradient vectors for each time step
     */
    std::vector<Eigen::VectorXd> gradient(const STLFormula::Ptr& formula,
                                         const std::vector<Eigen::VectorXd>& trajectory,
                                         const std::vector<double>& times) const;

private:
    SmoothRobustness calc_;
};

} // namespace stl_ros

#endif // SMOOTH_ROBUSTNESS_H
