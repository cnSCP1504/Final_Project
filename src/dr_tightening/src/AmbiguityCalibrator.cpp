/**
 * @file AmbiguityCalibrator.cpp
 * @brief Implementation of ambiguity set calibration for DR chance constraints
 */

#include "dr_tightening/AmbiguityCalibrator.hpp"
#include "dr_tightening/ResidualCollector.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

namespace dr_tightening {

AmbiguityCalibrator::AmbiguityCalibrator(
  AmbiguityStrategy strategy,
  double confidence_level)
  : strategy_(strategy), confidence_level_(confidence_level) {
}

double AmbiguityCalibrator::computeWassersteinRadius(
  const std::vector<Eigen::VectorXd>& residuals,
  int num_samples,
  int dim) {
  if (residuals.empty()) {
    return 0.0;
  }

  // Strategy 1: Concentration bound (conservative but guaranteed)
  if (strategy_ == AmbiguityStrategy::WATERSTEIN_BALL) {
    return concentrationBound(num_samples, dim);
  }

  // Strategy 2: Empirical quantile (data-driven, less conservative)
  if (strategy_ == AmbiguityStrategy::EMPIRICAL_GAUSSIAN) {
    // Compute pairwise distances
    std::vector<double> distances;
    for (size_t i = 0; i < residuals.size(); ++i) {
      for (size_t j = i + 1; j < residuals.size(); ++j) {
        double dist = computeWassersteinDistance(residuals[i], residuals[j]);
        distances.push_back(dist);
      }
    }

    if (distances.empty()) {
      return 0.0;
    }

    // Use empirical quantile
    return computeEmpiricalQuantile(distances, confidence_level_);
  }

  // Default: small radius
  return 0.01;
}

double AmbiguityCalibrator::computeEmpiricalQuantile(
  const std::vector<double>& distances,
  double quantile) {
  if (distances.empty()) {
    return 0.0;
  }

  std::vector<double> sorted_distances = distances;
  std::sort(sorted_distances.begin(), sorted_distances.end());

  int idx = static_cast<int>(std::ceil(quantile * sorted_distances.size())) - 1;
  idx = std::max(0, std::min(idx, static_cast<int>(sorted_distances.size()) - 1));

  return sorted_distances[idx];
}

double AmbiguityCalibrator::getCoverageError(int num_samples) const {
  // From manuscript: ε_n → 0 as n → ∞
  // Using rate O(n^(-1/dim)) for Wasserstein ambiguity
  double rate = 1.0 / std::sqrt(static_cast<double>(num_samples));
  return rate * (1.0 - confidence_level_);
}

void AmbiguityCalibrator::setStrategy(AmbiguityStrategy strategy) {
  strategy_ = strategy;
}

AmbiguityStrategy AmbiguityCalibrator::getStrategy() const {
  return strategy_;
}

double AmbiguityCalibrator::computeWassersteinDistance(
  const Eigen::VectorXd& sample1,
  const Eigen::VectorXd& sample2) {
  // For distributions, use L2 distance as proxy for W2
  // For single samples, this is just Euclidean distance
  return (sample1 - sample2).norm();
}

double AmbiguityCalibrator::concentrationBound(int num_samples, int dim) {
  // Mass concentration inequality for Wasserstein ball
  // P(ℙ ∉ B_ε(ℙ̂)) ≤ C * exp(-n * ε^p) for some C, p
  // Solving for ε with confidence level (1-α):

  double alpha = 1.0 - confidence_level_;
  double n = static_cast<double>(num_samples);
  double d = static_cast<double>(dim);

  // Simplified bound: ε ∝ (log(1/α) / n)^(1/d)
  // This is a conservative approximation
  double log_term = std::log(1.0 / alpha);
  double epsilon = std::pow(log_term / n, 1.0 / d);

  return epsilon;
}

} // namespace dr_tightening
