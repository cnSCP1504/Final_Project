/**
 * @file ResidualCollector.hpp
 * @brief Collects and manages residuals for distributionally robust chance constraints
 *
 * Based on manuscript Section III.A: Ambiguity calibration
 * - Sliding window W_k = {r_{k-M+1}, ..., r_k}
 * - Computes empirical mean μ̂ and covariance Σ̂
 * - Supports online updates
 */

#pragma once

#include <Eigen/Dense>
#include <deque>
#include <vector>

namespace dr_tightening {

class ResidualCollector {
public:
  /**
   * @brief Constructor
   * @param window_size Size of sliding window M (default: 200 from manuscript)
   */
  explicit ResidualCollector(int window_size = 200);

  /**
   * @brief Add a new residual to the window
   * @param residual New residual vector r_t
   */
  void addResidual(const Eigen::VectorXd& residual);

  /**
   * @brief Get empirical mean μ̂_k
   * @return Mean vector
   */
  Eigen::VectorXd getMean() const;

  /**
   * @brief Get empirical covariance Σ̂_k
   * @return Covariance matrix
   */
  Eigen::MatrixXd getCovariance() const;

  /**
   * @brief Get number of residuals in window
   * @return Current window size
   */
  int getWindowSize() const;

  /**
   * @brief Check if window is full
   * @return True if window has M samples
   */
  bool isWindowFull() const;

  /**
   * @brief Clear all residuals
   */
  void clear();

  /**
   * @brief Get residuals as matrix (for external processing)
   * @return Matrix where each column is a residual
   */
  Eigen::MatrixXd getResidualsMatrix() const;

  /**
   * @brief Get the dimension of residuals
   * @return Dimension of residual vectors, or 0 if empty
   */
  int getResidualDimension() const;

private:
  int window_size_;
  std::deque<Eigen::VectorXd> residual_window_;

  // Cached statistics (updated incrementally)
  Eigen::VectorXd empirical_mean_;
  Eigen::MatrixXd empirical_cov_;
  bool stats_dirty_;

  /**
   * @brief Update empirical statistics from scratch
   */
  void updateStatistics();
};

} // namespace dr_tightening
