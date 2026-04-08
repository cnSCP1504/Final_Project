/**
 * @file SafetyLinearization.cpp
 * @brief Implementation of safety function linearization for DR chance constraints
 *
 * Based on manuscript Eq. 448-452
 */

#include "dr_tightening/SafetyLinearization.hpp"
#include <cmath>
#include <random>
#include <iostream>

namespace dr_tightening {

SafetyLinearization::SafetyLinearization(
  SafetyFunction safety_func,
  SafetyGradient gradient_func)
  : safety_func_(safety_func), gradient_func_(gradient_func) {
}

LinearizationResult SafetyLinearization::linearize(
  const Eigen::VectorXd& nominal_state) {
  LinearizationResult result;
  result.nominal_value = safety_func_(nominal_state);
  result.offset = nominal_state;
  result.is_valid = true;

  // Compute gradient
  if (gradient_func_) {
    result.gradient = gradient_func_(nominal_state);
  } else {
    result.gradient = numericalGradient(nominal_state);
  }

  return result;
}

Eigen::VectorXd SafetyLinearization::computeSensitivityDirection(
  const Eigen::VectorXd& nominal_state,
  const Eigen::MatrixXd& disturbance_matrix) const {
  // From manuscript Eq. 452:
  // c_t^T = ∇h(z̄_t)^T G_t
  //
  // If G_t is not provided (default), assume G_t = I
  // (state directly affected by disturbance)

  Eigen::VectorXd gradient;
  if (gradient_func_) {
    gradient = gradient_func_(nominal_state);
  } else {
    gradient = numericalGradient(nominal_state);
  }

  if (disturbance_matrix.size() == 0) {
    // G_t = I (identity)
    return gradient;
  } else {
    // c_t = G_t^T ∇h(z̄_t)
    return disturbance_matrix.transpose() * gradient;
  }
}

double SafetyLinearization::evaluateLinearized(
  const LinearizationResult& linearization,
  const Eigen::VectorXd& perturbed_state) const {
  // h(x) ≈ h(z̄) + c^T (x - z̄)
  // where c = ∇h(z̄)

  Eigen::VectorXd delta = perturbed_state - linearization.offset;
  return linearization.nominal_value + linearization.gradient.dot(delta);
}

Eigen::VectorXd SafetyLinearization::numericalGradient(
  const Eigen::VectorXd& state,
  double epsilon) const {
  return centralDifference(state, epsilon);
}

double SafetyLinearization::estimateLipschitzConstant(
  const Eigen::VectorXd& center,
  double radius,
  int num_samples) const {
  // Estimate L_h by sampling pairs within the region
  // L_h ≈ max |h(x1) - h(x2)| / ‖x1 - x2‖

  std::random_device rd;
  std::mt19937 gen(rd());
  std::normal_distribution<double> d(0.0, 1.0);

  double max_lipschitz = 0.0;
  int dim = center.size();

  for (int i = 0; i < num_samples; ++i) {
    // Sample two random points in the ball
    Eigen::VectorXd x1 = center;
    Eigen::VectorXd x2 = center;

    for (int j = 0; j < dim; ++j) {
      double r1 = d(gen);
      double r2 = d(gen);
      // Normalize to unit ball and scale
      x1[j] += radius * r1 / std::sqrt(dim);
      x2[j] += radius * r2 / std::sqrt(dim);
    }

    // Compute function values
    double h1 = safety_func_(x1);
    double h2 = safety_func_(x2);

    // Compute Lipschitz estimate
    double dist = (x1 - x2).norm();
    if (dist > 1e-8) {
      double lipschitz = std::abs(h1 - h2) / dist;
      max_lipschitz = std::max(max_lipschitz, lipschitz);
    }
  }

  return max_lipschitz;
}

void SafetyLinearization::setGradientFunction(SafetyGradient gradient_func) {
  gradient_func_ = gradient_func;
}

SafetyGradient SafetyLinearization::getGradientFunction() const {
  return gradient_func_;
}

bool SafetyLinearization::hasAnalyticGradient() const {
  return gradient_func_ != nullptr;
}

Eigen::VectorXd SafetyLinearization::centralDifference(
  const Eigen::VectorXd& state,
  double epsilon) const {
  int dim = state.size();
  Eigen::VectorXd gradient(dim);

  // ✅ DEBUG: 打印前几次梯度计算
  static int debug_count = 0;
  bool should_debug = (debug_count < 3);
  debug_count++;

  if (should_debug) {
    std::cout << "[DEBUG] centralDifference:" << std::endl;
    std::cout << "  state: [" << state.transpose() << "]" << std::endl;
    std::cout << "  dim: " << dim << std::endl;
    std::cout << "  epsilon: " << epsilon << std::endl;
  }

  Eigen::VectorXd state_plus = state;
  Eigen::VectorXd state_minus = state;

  for (int i = 0; i < dim; ++i) {
    state_plus[i] += epsilon;
    state_minus[i] -= epsilon;

    double f_plus = safety_func_(state_plus);
    double f_minus = safety_func_(state_minus);

    gradient[i] = (f_plus - f_minus) / (2.0 * epsilon);

    if (should_debug) {
      std::cout << "  dim[" << i << "]:" << std::endl;
      std::cout << "    state_plus[" << i << "]: " << state_plus[i] << std::endl;
      std::cout << "    state_minus[" << i << "]: " << state_minus[i] << std::endl;
      std::cout << "    f_plus: " << f_plus << std::endl;
      std::cout << "    f_minus: " << f_minus << std::endl;
      std::cout << "    gradient[" << i << "]: " << gradient[i] << std::endl;
    }

    // Reset
    state_plus[i] = state[i];
    state_minus[i] = state[i];
  }

  if (should_debug) {
    std::cout << "  gradient: [" << gradient.transpose() << "]" << std::endl;
    std::cout << std::endl;
  }

  return gradient;
}

} // namespace dr_tightening
