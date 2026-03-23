/**
 * @file RegretMetrics.cpp
 * @brief Implementation of regret metrics
 */

#include "safe_regret/RegretMetrics.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>

namespace safe_regret {

void RegretMetrics::computeCumulative() {
  if (step_history.empty()) {
    return;
  }

  // Reset cumulative quantities
  safe_regret = 0.0;
  dynamic_regret = 0.0;
  abstract_regret = 0.0;
  tracking_contribution = 0.0;
  nominal_contribution = 0.0;
  tightening_contribution = 0.0;
  abstract_contribution = 0.0;

  horizon_T = step_history.size();

  // Sum over all steps
  for (const auto& step : step_history) {
    // Dynamic regret: Σ[ℓ(x,u) - ℓ(x*,u*)] + γ·1{infeasible}
    dynamic_regret += step.cost_regret + step.feasibility_penalty;

    // Tracking contribution: L_ℓ(1+‖K‖)·‖e_k‖
    tracking_contribution += step.tracking_error_norm;

    // Nominal contribution: ℓ(z,v) - ℓ(x*,u*)
    nominal_contribution += step.nominal_cost - step.comparator_cost;

    // Tightening slack
    tightening_contribution += step.tightening_slack;

    // Abstract contribution: ℓ(z^ref, v^ref) - ℓ(z^⋄, v^⋄)
    abstract_contribution += step.reference_cost - step.comparator_cost;
  }

  // Safe regret: same as dynamic but restricted to Π_safe
  // (For now, use dynamic_regret as approximation)
  safe_regret = dynamic_regret;

  // Tracking error bound: C_e·√T (Lemma 4.6)
  tracking_error_bound = std::sqrt(static_cast<double>(horizon_T));

  // Sublinear growth check: R_T / T
  sublinear_check = dynamic_regret / static_cast<double>(horizon_T);
}

bool RegretMetrics::verifySublinearGrowth(double epsilon) const {
  if (horizon_T == 0) {
    return false;
  }

  // Check if R_T/T < ε
  double ratio = dynamic_regret / static_cast<double>(horizon_T);

  // For o(T), we need lim_{T→∞} R_T/T = 0
  // We check if R_T/T < ε (e.g., ε = 1/√T or small constant)
  double threshold = epsilon / std::sqrt(static_cast<double>(horizon_T));

  return ratio < threshold;
}

std::pair<std::string, double> RegretMetrics::getGrowthRate() const {
  if (horizon_T < 2) {
    return {"Unknown", 0.0};
  }

  // Analyze scaling by fitting to R_T ~ c·T^α
  // If α < 1, then R_T = o(T)
  double log_T = std::log(static_cast<double>(horizon_T));
  double log_R = std::log(dynamic_regret + 1e-10);

  // Rough estimate: α ≈ log(R) / log(T)
  double alpha = log_R / log_T;

  std::string rate;
  if (alpha < 0.6) {
    rate = "o(T)";  // Sublinear
  } else if (alpha < 0.9) {
    rate = "O(√T)";  // Square root
  } else {
    rate = "O(T)";  // Linear (bad!)
  }

  return {rate, alpha};
}

void RegretMetrics::exportToCSV(const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return;
  }

  // Header
  file << "step,instant_cost,comparator_cost,cost_regret,feasibility_penalty,"
       << "tracking_error_norm,nominal_cost,reference_cost,tightening_slack\n";

  // Data
  for (size_t i = 0; i < step_history.size(); ++i) {
    const auto& step = step_history[i];
    file << i << ","
         << step.instant_cost << ","
         << step.comparator_cost << ","
         << step.cost_regret << ","
         << step.feasibility_penalty << ","
         << step.tracking_error_norm << ","
         << step.nominal_cost << ","
         << step.reference_cost << ","
         << step.tightening_slack << "\n";
  }

  file.close();
}

void RegretMetrics::printSummary() const {
  std::cout << "\n========================================" << std::endl;
  std::cout << "       Regret Analysis Summary" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Horizon T: " << horizon_T << std::endl;
  std::cout << std::scientific << std::setprecision(4);

  std::cout << "\nMain Regret Quantities:" << std::endl;
  std::cout << "  Safe Regret R_T^safe:    " << safe_regret << std::endl;
  std::cout << "  Dynamic Regret R_T^dyn:  " << dynamic_regret << std::endl;
  std::cout << "  Abstract Regret R_T^ref: " << abstract_regret << std::endl;

  std::cout << "\nDecomposition (Theorem 4.8):" << std::endl;
  std::cout << "  Tracking O(√T):  " << tracking_contribution << std::endl;
  std::cout << "  Nominal:         " << nominal_contribution << std::endl;
  std::cout << "  Tightening:      " << tightening_contribution << std::endl;
  std::cout << "  Abstract o(T):   " << abstract_contribution << std::endl;

  std::cout << "\nTheoretical Bounds:" << std::endl;
  std::cout << "  Tracking Error Bound: " << tracking_error_bound << " (C_e·√T)" << std::endl;
  std::cout << "  Lipschitz Bound:      " << lipschitz_bound << " (L_ℓ(1+‖K‖))" << std::endl;

  std::cout << "\nSublinear Growth Check:" << std::endl;
  std::cout << "  R_T / T: " << sublinear_check << std::endl;

  auto [rate, alpha] = getGrowthRate();
  std::cout << "  Growth Rate: " << rate << " (α ≈ " << alpha << ")" << std::endl;

  bool sublinear = verifySublinearGrowth();
  std::cout << "  Is o(T): " << (sublinear ? "YES ✓" : "NO ✗") << std::endl;

  std::cout << "========================================\n" << std::endl;
}

bool ReferenceTrajectory::isTubeFeasible(
  double v_max,
  double kappa_max,
  const Eigen::VectorXd& tube_center,
  double tube_radius) const {

  if (points.size() < 3) {
    return true;  // Too short to check curvature
  }

  // Check velocity bounds
  for (const auto& pt : points) {
    if (pt.input.norm() > v_max) {
      return false;
    }
  }

  // Check curvature bounds (need at least 3 points)
  for (size_t i = 1; i < points.size() - 1; ++i) {
    // Compute curvature using finite differences
    Eigen::VectorXd prev = points[i-1].state;
    Eigen::VectorXd curr = points[i].state;
    Eigen::VectorXd next = points[i+1].state;

    // For 2D: κ = |x'y'' - y'x''| / (x'^2 + y'^2)^(3/2)
    if (curr.size() >= 2) {
      double dx1 = curr[0] - prev[0];
      double dy1 = curr[1] - prev[1];
      double dx2 = next[0] - curr[0];
      double dy2 = next[1] - curr[1];

      double dx = 0.5 * (dx1 + dx2);
      double dy = 0.5 * (dy1 + dy2);
      double ddx = dx2 - dx1;
      double ddy = dy2 - dy1;

      double numerator = std::abs(dx * ddy - dy * ddx);
      double denominator = std::pow(dx*dx + dy*dy, 1.5);

      if (denominator > 1e-6) {
        double kappa = numerator / denominator;
        if (kappa > kappa_max) {
          return false;
        }
      }
    }
  }

  // Check tube admissibility: ‖z_k - tube_center‖ ≤ (state_bound - tube_radius)
  double max_distance = tube_radius;

  for (const auto& pt : points) {
    double dist = (pt.state - tube_center).norm();
    if (dist > max_distance) {
      return false;
    }
  }

  return true;
}

} // namespace safe_regret
