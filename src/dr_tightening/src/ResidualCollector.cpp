/**
 * @file ResidualCollector.cpp
 * @brief Implementation of residual collection for DR chance constraints
 */

#include "dr_tightening/ResidualCollector.hpp"
#include <iostream>
#include <numeric>

namespace dr_tightening {

ResidualCollector::ResidualCollector(int window_size)
  : window_size_(window_size), stats_dirty_(true) {
  // Initialize with empty state (dimension 0)
  empirical_mean_ = Eigen::VectorXd();
  empirical_cov_ = Eigen::MatrixXd();
}

void ResidualCollector::addResidual(const Eigen::VectorXd& residual) {
  // Check dimension consistency
  if (!residual_window_.empty() &&
      residual.size() != residual_window_[0].size()) {
    std::cerr << "Warning: Residual dimension mismatch! "
              << "Expected " << residual_window_[0].size()
              << ", got " << residual.size() << std::endl;
    std::cerr << "Skipping this residual." << std::endl;
    return;  // Skip this residual
  }

  residual_window_.push_back(residual);
  stats_dirty_ = true;

  // Remove oldest if window is full
  if (static_cast<int>(residual_window_.size()) > window_size_) {
    residual_window_.pop_front();
    stats_dirty_ = true;  // Need to recompute
  }
}

Eigen::VectorXd ResidualCollector::getMean() const {
  if (residual_window_.empty()) {
    return Eigen::VectorXd();
  }
  if (stats_dirty_) {
    const_cast<ResidualCollector*>(this)->updateStatistics();
  }
  return empirical_mean_;
}

Eigen::MatrixXd ResidualCollector::getCovariance() const {
  if (residual_window_.empty()) {
    return Eigen::MatrixXd();
  }
  if (stats_dirty_) {
    const_cast<ResidualCollector*>(this)->updateStatistics();
  }
  return empirical_cov_;
}

int ResidualCollector::getWindowSize() const {
  return static_cast<int>(residual_window_.size());
}

bool ResidualCollector::isWindowFull() const {
  return static_cast<int>(residual_window_.size()) >= window_size_;
}

void ResidualCollector::clear() {
  residual_window_.clear();
  empirical_mean_ = Eigen::VectorXd::Zero(1);
  empirical_cov_ = Eigen::MatrixXd::Zero(1, 1);
  stats_dirty_ = true;
}

Eigen::MatrixXd ResidualCollector::getResidualsMatrix() const {
  if (residual_window_.empty()) {
    return Eigen::MatrixXd();
  }

  int dim = residual_window_[0].size();
  Eigen::MatrixXd matrix(dim, residual_window_.size());

  for (size_t i = 0; i < residual_window_.size(); ++i) {
    matrix.col(i) = residual_window_[i];
  }

  return matrix;
}

int ResidualCollector::getResidualDimension() const {
  if (residual_window_.empty()) {
    return 0;
  }
  return residual_window_[0].size();
}

void ResidualCollector::updateStatistics() {
  if (residual_window_.empty()) {
    return;
  }

  int n = residual_window_.size();
  int dim = residual_window_[0].size();

  // Initialize if first time or dimension changed
  if (empirical_mean_.size() != dim) {
    empirical_mean_ = Eigen::VectorXd::Zero(dim);
    empirical_cov_ = Eigen::MatrixXd::Zero(dim, dim);
  } else if (empirical_cov_.rows() != dim || empirical_cov_.cols() != dim) {
    empirical_cov_ = Eigen::MatrixXd::Zero(dim, dim);
  }

  // Compute mean
  empirical_mean_.setZero();
  for (const auto& residual : residual_window_) {
    empirical_mean_ += residual;
  }
  empirical_mean_ /= static_cast<double>(n);

  // Compute covariance
  empirical_cov_.setZero();
  for (const auto& residual : residual_window_) {
    Eigen::VectorXd centered = residual - empirical_mean_;
    empirical_cov_ += centered * centered.transpose();
  }

  if (n > 1) {
    empirical_cov_ /= static_cast<double>(n - 1);  // Sample covariance
  }

  stats_dirty_ = false;
}

} // namespace dr_tightening
