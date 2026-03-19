/**
 * @file SafetyLinearization.hpp
 * @brief Linearizes safety function h(x) for DR chance constraints
 *
 * Based on manuscript Eq. 448-452:
 * - h(x_t) ≈ h(z̄_t) + ∇h(z̄_t)^T (x_t - z̄_t)
 * - x_t = z̄_t + G_t ω_t (affine in disturbance proxy)
 * - Defines sensitivity direction c_t = ∇h(z̄_t)^T G_t
 */

#pragma once

#include <Eigen/Dense>
#include <functional>
#include <string>

namespace dr_tightening {

/**
 * @brief Safety function h: 𝒳 → ℝ with safe set 𝒞 = {x: h(x) ≥ 0}
 */
using SafetyFunction = std::function<double(const Eigen::VectorXd&)>;

/**
 * @brief Gradient of safety function ∇h: 𝒳 → ℝ^n
 */
using SafetyGradient = std::function<Eigen::VectorXd(const Eigen::VectorXd&)>;

/**
 * @brief Linearization result
 */
struct LinearizationResult {
  double nominal_value;      ///< h(z̄_t)
  Eigen::VectorXd gradient;  ///< c_t = ∇h(z̄_t)
  Eigen::VectorXd offset;    ///< z̄_t (for reconstruction)
  bool is_valid;             ///< Whether linearization is valid
};

class SafetyLinearization {
public:
  /**
   * @brief Constructor
   * @param safety_func Safety function h(x)
   * @param gradient_func Gradient function ∇h(x) (optional, uses numerical gradient if null)
   */
  SafetyLinearization(
    SafetyFunction safety_func,
    SafetyGradient gradient_func = nullptr);

  /**
   * @brief Linearize safety function at nominal state
   *
   * Implements manuscript Eq. 448:
   * h(x_t) ≈ h(z̄_t) + ∇h(z̄_t)^T (x_t - z̄_t)
   *
   * @param nominal_state z̄_t (nominal predicted state)
   * @return LinearizationResult containing h(z̄_t) and ∇h(z̄_t)
   */
  LinearizationResult linearize(const Eigen::VectorXd& nominal_state);

  /**
   * @brief Compute sensitivity direction c_t
   *
   * From manuscript Eq. 452:
   * c_t^T = ∇h(z̄_t)^T G_t
   *
   * If G_t = I (state directly affected by disturbance),
   * then c_t = ∇h(z̄_t)
   *
   * @param nominal_state z̄_t
   * @param disturbance_matrix G_t (default: identity)
   * @return Sensitivity direction c_t
   */
  Eigen::VectorXd computeSensitivityDirection(
    const Eigen::VectorXd& nominal_state,
    const Eigen::MatrixXd& disturbance_matrix = Eigen::MatrixXd()) const;

  /**
   * @brief Evaluate linearized safety at perturbed state
   *
   * h(x) ≈ h(z̄) + c^T (x - z̄)
   *
   * @param linearization Result from linearize()
   * @param perturbed_state x = z̄ + Δx
   * @return Linearized safety value
   */
  double evaluateLinearized(
    const LinearizationResult& linearization,
    const Eigen::VectorXd& perturbed_state) const;

  /**
   * @brief Compute numerical gradient (finite differences)
   * @param state Point to compute gradient at
   * @param epsilon Step size for finite differences
   * @return Numerical gradient approximation
   */
  Eigen::VectorXd numericalGradient(
    const Eigen::VectorXd& state,
    double epsilon = 1e-6) const;

  /**
   * @brief Estimate Lipschitz constant L_h on a region
   *
   * From manuscript Eq. 712: h(x) ≥ h(z) - L_h‖e‖
   *
   * @param center Center of region
   * @param radius Radius of region
   * @param num_samples Number of samples to estimate
   * @return Estimated Lipschitz constant
   */
  double estimateLipschitzConstant(
    const Eigen::VectorXd& center,
    double radius,
    int num_samples = 100) const;

  /**
   * @brief Set gradient function
   * @param gradient_func New gradient function
   */
  void setGradientFunction(SafetyGradient gradient_func);

  /**
   * @brief Get gradient function
   * @return Current gradient function
   */
  SafetyGradient getGradientFunction() const;

  /**
   * @brief Check if gradient is available
   * @return True if analytic gradient is set
   */
  bool hasAnalyticGradient() const;

private:
  SafetyFunction safety_func_;
  SafetyGradient gradient_func_;

  /**
   * @brief Compute numerical gradient using central differences
   */
  Eigen::VectorXd centralDifference(
    const Eigen::VectorXd& state,
    double epsilon) const;
};

} // namespace dr_tightening
