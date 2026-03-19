/**
 * @file AmbiguityCalibrator.hpp
 * @brief Calibrates ambiguity sets P_k for distributionally robust optimization
 *
 * Based on manuscript Section III.A:
 * - Constructs Wasserstein ball B_ε_k(ℙ̂_k)
 * - Provides finite-sample coverage guarantee
 * - Computes radius ε_k via concentration bounds or quantiles
 */

#pragma once

#include <Eigen/Dense>
#include <vector>
#include <memory>

namespace dr_tightening {

class ResidualCollector;

/**
 * @brief Ambiguity set calibration strategies
 */
enum class AmbiguityStrategy {
  WATERSTEIN_BALL,    ///< Wasserstein ball (recommended)
  EMPIRICAL_GAUSSIAN, ///< Gaussian approximation (simple)
  KL_DIVERGENCE       ///< KL divergence set (optional)
};

class AmbiguityCalibrator {
public:
  /**
   * @brief Constructor
   * @param strategy Calibration strategy
   * @param confidence_level Confidence for coverage (1-α)
   */
  explicit AmbiguityCalibrator(
    AmbiguityStrategy strategy = AmbiguityStrategy::WATERSTEIN_BALL,
    double confidence_level = 0.95);

  /**
   * @brief Compute Wasserstein radius ε_k using concentration bound
   *
   * From manuscript: "radius selected to meet finite-sample coverage guarantee"
   *
   * @param residuals Sample residuals
   * @param num_samples Number of samples (window size)
   * @param dim Dimension of disturbance
   * @return Wasserstein radius ε_k
   */
  double computeWassersteinRadius(
    const std::vector<Eigen::VectorXd>& residuals,
    int num_samples,
    int dim);

  /**
   * @brief Compute radius using empirical quantile (data-driven)
   * @param distances Pairwise distances
   * @param quantile Quantile level (e.g., 0.95)
   * @return Quantile-based radius
   */
  double computeEmpiricalQuantile(
    const std::vector<double>& distances,
    double quantile);

  /**
   * @brief Get coverage guarantee ε_n → 0 as n → ∞
   * @param num_samples Current window size
   * @return Coverage error bound
   */
  double getCoverageError(int num_samples) const;

  /**
   * @brief Set strategy
   * @param strategy New calibration strategy
   */
  void setStrategy(AmbiguityStrategy strategy);

  /**
   * @brief Get current strategy
   * @return Current strategy
   */
  AmbiguityStrategy getStrategy() const;

private:
  AmbiguityStrategy strategy_;
  double confidence_level_;

  /**
   * @brief Compute Wasserstein distance between two samples
   * @param sample1 First sample
   * @param sample2 Second sample
   * @return W2 distance
   */
  double computeWassersteinDistance(
    const Eigen::VectorXd& sample1,
    const Eigen::VectorXd& sample2);

  /**
   * @brief Concentration bound for Wasserstein radius
   * Based on mass concentration inequality
   */
  double concentrationBound(int num_samples, int dim);
};

} // namespace dr_tightening
