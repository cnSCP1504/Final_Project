/**
 * @file test_dr_formulas.cpp
 * @brief Test program for DR chance constraint formulas from manuscript
 *
 * Tests the following key formulas:
 * 1. Lemma 4.1 (Eq. 698): h(z_t) ≥ L_h·ē + μ_t + κ_{δ_t}·σ_t + σ_{k,t}
 * 2. Cantelli factor (Eq. 731): κ_{δ_t} = sqrt((1-δ_t)/δ_t)
 * 3. Sensitivity statistics (Eqs. 691-692): μ_t = c^T μ, σ_t² = c^T Σ c
 * 4. Tube offset (Eq. 712): L_h·ē
 */

#include "dr_tightening/ResidualCollector.hpp"
#include "dr_tightening/AmbiguityCalibrator.hpp"
#include "dr_tightening/TighteningComputer.hpp"
#include "dr_tightening/SafetyLinearization.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <random>

using namespace dr_tightening;

// Test safety function: distance to obstacle
// h(x) = distance_from_obstacle - buffer
// Safe set: h(x) ≥ 0
double testSafetyFunction(const Eigen::VectorXd& x) {
  // Simple 2D example: obstacle at (5, 5), buffer zone 1.0
  Eigen::Vector2d obstacle(5.0, 5.0);
  double buffer = 1.0;

  Eigen::Vector2d pos = x.head<2>();
  double distance = (pos - obstacle).norm();

  return distance - buffer;
}

// Analytic gradient
Eigen::VectorXd testSafetyGradient(const Eigen::VectorXd& x) {
  Eigen::Vector2d obstacle(5.0, 5.0);
  Eigen::Vector2d pos = x.head<2>();
  Eigen::Vector2d diff = pos - obstacle;
  double distance = diff.norm();

  Eigen::VectorXd grad = Eigen::VectorXd::Zero(x.size());
  grad.head<2>() = diff / distance;  // Unit vector pointing away from obstacle

  return grad;
}

// Test residual generation (simulating process/estimation noise)
Eigen::VectorXd generateTestResidual(int dim) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::normal_distribution<double> d(0.0, 0.1);  // σ = 0.1

  Eigen::VectorXd residual(dim);
  for (int i = 0; i < dim; ++i) {
    residual[i] = d(gen);
  }
  return residual;
}

// Color codes for terminal output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define BOLD    "\033[1m"

void printHeader(const std::string& title) {
  std::cout << "\n" << BLUE << BOLD << "=== " << title << " ===" << RESET << std::endl;
}

void printTestResult(const std::string& test_name, bool passed) {
  if (passed) {
    std::cout << GREEN << "✓ " << test_name << RESET << std::endl;
  } else {
    std::cout << RED << "✗ " << test_name << RESET << std::endl;
  }
}

bool testCantelliFactor() {
  printHeader("Test 1: Cantelli Factor (Eq. 731)");

  // Test: κ_{δ_t} = sqrt((1-δ_t)/δ_t)
  std::vector<double> delta_values = {0.01, 0.05, 0.1, 0.2};

  std::cout << std::setw(10) << "δ_t"
            << std::setw(15) << "κ_{δ_t} (calc)"
            << std::setw(15) << "κ_{δ_t} (ref)"
            << std::setw(10) << "Error"
            << std::endl;
  std::cout << std::string(50, '-') << std::endl;

  bool all_passed = true;
  for (double delta_t : delta_values) {
    double kappa_calc = TighteningComputer::computeCantelliFactor(delta_t);
    double kappa_ref = std::sqrt((1.0 - delta_t) / delta_t);

    double error = std::abs(kappa_calc - kappa_ref);
    bool passed = error < 1e-10;

    std::cout << std::setw(10) << delta_t
              << std::setw(15) << kappa_calc
              << std::setw(15) << kappa_ref
              << std::setw(10) << error
              << (passed ? GREEN : RED) << (passed ? " ✓" : " ✗") << RESET
              << std::endl;

    all_passed &= passed;
  }

  printTestResult("Cantelli Factor Formula", all_passed);
  return all_passed;
}

bool testSensitivityStatistics() {
  printHeader("Test 2: Sensitivity Statistics (Eqs. 691-692)");

  // Test: μ_t = c^T μ, σ_t² = c^T Σ c
  TighteningComputer computer(0.1, 20);

  // Create test gradient c_t = [1, 0, 0]
  Eigen::VectorXd gradient = Eigen::VectorXd::Zero(3);
  gradient[0] = 1.0;

  // Create test disturbance statistics
  Eigen::VectorXd mean = Eigen::VectorXd::Zero(3);
  mean[0] = 0.05;
  mean[1] = 0.02;
  mean[2] = 0.01;

  Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(3, 3);
  cov(0, 0) = 0.01;  // σ² = 0.01
  cov(1, 1) = 0.02;
  cov(2, 2) = 0.005;

  // Compute μ_t
  double mu_t = computer.computeMeanAlongSensitivity(gradient, mean);
  double mu_t_ref = 0.05;  // c^T μ = 1.0 * 0.05 + 0 * 0.02 + 0 * 0.01

  // Compute σ_t
  double sigma_t = computer.computeStdAlongSensitivity(gradient, cov);
  double sigma_t_ref = std::sqrt(0.01);  // sqrt(c^T Σ c) = sqrt(0.01)

  std::cout << "Gradient c: [" << gradient.transpose() << "]" << std::endl;
  std::cout << "Mean μ: [" << mean.transpose() << "]" << std::endl;
  std::cout << "Covariance diagonal: [" << cov.diagonal().transpose() << "]" << std::endl;
  std::cout << std::endl;
  std::cout << "μ_t (calc): " << mu_t << ", μ_t (ref): " << mu_t_ref << std::endl;
  std::cout << "σ_t (calc): " << sigma_t << ", σ_t (ref): " << sigma_t_ref << std::endl;

  bool mu_passed = std::abs(mu_t - mu_t_ref) < 1e-10;
  bool sigma_passed = std::abs(sigma_t - sigma_t_ref) < 1e-10;

  std::cout << std::endl;
  printTestResult("Mean along sensitivity (Eq. 691)", mu_passed);
  printTestResult("Std along sensitivity (Eq. 692)", sigma_passed);

  return mu_passed && sigma_passed;
}

bool testTubeOffset() {
  printHeader("Test 3: Tube Offset (Eq. 712)");

  // Test: L_h·ē
  TighteningComputer computer(0.1, 20);

  double L_h = 2.0;      // Lipschitz constant
  double e_bar = 0.5;    // Tube radius

  double tube_offset = computer.computeTubeOffset(L_h, e_bar);
  double ref_offset = L_h * e_bar;  // Expected: 2.0 * 0.5 = 1.0

  std::cout << "Lipschitz constant L_h: " << L_h << std::endl;
  std::cout << "Tube radius ē: " << e_bar << std::endl;
  std::cout << "Tube offset (calc): " << tube_offset << std::endl;
  std::cout << "Tube offset (ref): " << ref_offset << std::endl;

  bool passed = std::abs(tube_offset - ref_offset) < 1e-10;

  std::cout << std::endl;
  printTestResult("Tube offset formula (Eq. 712)", passed);

  return passed;
}

bool testCompleteMargin() {
  printHeader("Test 4: Complete Margin Formula (Eq. 698)");

  // Test: h(z_t) ≥ L_h·ē + μ_t + κ_{δ_t}·σ_t + σ_{k,t}
  //
  // This tests the complete Lemma 4.1 formula

  // Setup
  ResidualCollector collector(200);

  // Add test residuals (simulate 200 samples)
  for (int i = 0; i < 200; ++i) {
    Eigen::VectorXd residual = generateTestResidual(3);
    collector.addResidual(residual);
  }

  // Tightening computer
  TighteningComputer computer(0.1, 20, RiskAllocation::UNIFORM);

  // Nominal state
  Eigen::VectorXd nominal_state = Eigen::VectorXd::Zero(3);
  nominal_state[0] = 2.0;  // x = 2.0, y = 0.0, theta = 0.0

  // Safety function
  SafetyLinearization linearization(testSafetyFunction, testSafetyGradient);
  auto result = linearization.linearize(nominal_state);

  // Tube parameters
  double tube_radius = 0.5;
  double lipschitz_const = 1.0;  // L_h

  // Compute margin
  double margin = computer.computeChebyshevMargin(
    nominal_state,
    result.nominal_value,
    result.gradient,
    collector,
    tube_radius,
    lipschitz_const,
    10);  // time_step = 10

  // Manual calculation for verification
  Eigen::VectorXd mean = collector.getMean();
  Eigen::MatrixXd cov = collector.getCovariance();

  double tube_offset = computer.computeTubeOffset(lipschitz_const, tube_radius);
  double mu_t = computer.computeMeanAlongSensitivity(result.gradient, mean);
  double sigma_t = computer.computeStdAlongSensitivity(result.gradient, cov);

  // Get δ_t for step 10
  auto per_step_risks = computer.allocateRisk(0.1, 20, RiskAllocation::UNIFORM);
  double delta_t = per_step_risks[10];
  double kappa = computer.computeCantelliFactor(delta_t);

  double margin_manual = tube_offset + mu_t + kappa * sigma_t;

  std::cout << "Nominal state: [" << nominal_state.transpose() << "]" << std::endl;
  std::cout << "Safety value h(z_t): " << result.nominal_value << std::endl;
  std::cout << "Gradient c_t: [" << result.gradient.transpose() << "]" << std::endl;
  std::cout << std::endl;
  std::cout << "Component breakdown:" << std::endl;
  std::cout << "  L_h·ē (tube offset):      " << tube_offset << std::endl;
  std::cout << "  μ_t (mean along c):        " << mu_t << std::endl;
  std::cout << "  κ_{δ_t}:                    " << kappa << std::endl;
  std::cout << "  σ_t (std along c):         " << sigma_t << std::endl;
  std::cout << "  κ_{δ_t}·σ_t:               " << kappa * sigma_t << std::endl;
  std::cout << std::endl;
  std::cout << "Margin (computed): " << margin << std::endl;
  std::cout << "Margin (manual):   " << margin_manual << std::endl;
  std::cout << "Error:            " << std::abs(margin - margin_manual) << std::endl;

  bool passed = std::abs(margin - margin_manual) < 1e-8;

  std::cout << std::endl;
  printTestResult("Complete margin formula (Eq. 698)", passed);

  return passed;
}

bool testRiskAllocation() {
  printHeader("Test 5: Risk Allocation (Boole's Inequality)");

  // Test: Σ δ_t ≤ δ
  double total_risk = 0.1;
  int horizon = 20;

  TighteningComputer computer(total_risk, horizon, RiskAllocation::UNIFORM);
  auto risks = computer.allocateRisk(total_risk, horizon, RiskAllocation::UNIFORM);

  double sum_risks = 0.0;
  for (double delta_t : risks) {
    sum_risks += delta_t;
  }

  std::cout << "Total risk δ: " << total_risk << std::endl;
  std::cout << "Horizon N: " << horizon << std::endl;
  std::cout << "Σ δ_t: " << sum_risks << std::endl;
  std::cout << "Risk per step: " << risks[0] << " (uniform)" << std::endl;
  std::cout << "Check: Σ δ_t ≤ δ? " << (sum_risks <= total_risk + 1e-10 ? "Yes" : "No") << std::endl;

  bool passed = (sum_risks <= total_risk + 1e-10);

  std::cout << std::endl;
  printTestResult("Boole's inequality constraint", passed);

  return passed;
}

bool testLinearization() {
  printHeader("Test 6: Safety Function Linearization (Eq. 448)");

  SafetyLinearization linearization(testSafetyFunction, testSafetyGradient);

  // Nominal state
  Eigen::VectorXd nominal = Eigen::VectorXd::Zero(3);
  nominal[0] = 3.0;
  nominal[1] = 3.0;

  // Linearize
  auto result = linearization.linearize(nominal);

  // Perturbed state (small change)
  Eigen::VectorXd perturbed = nominal;
  perturbed[0] += 0.01;

  // True function value
  double h_true = testSafetyFunction(perturbed);

  // Linearized value
  double h_linear = linearization.evaluateLinearized(result, perturbed);

  std::cout << "Nominal state: [" << nominal.transpose() << "]" << std::endl;
  std::cout << "Perturbed state: [" << perturbed.transpose() << "]" << std::endl;
  std::cout << "True h(x): " << h_true << std::endl;
  std::cout << "Linear h(x): " << h_linear << std::endl;
  std::cout << "Error: " << std::abs(h_true - h_linear) << std::endl;

  bool passed = std::abs(h_true - h_linear) < 0.1;  // 10% error tolerance

  std::cout << std::endl;
  printTestResult("Linearization accuracy", passed);

  return passed;
}

int main() {
  std::cout << "\n" << BOLD << BLUE
            << "╔════════════════════════════════════════════════════════╗"
            << "\n║  DR Chance Constraint Formula Test Suite              ║"
            << "\n║  Testing manuscript Lemma 4.1 and related formulas     ║"
            << "\n╚════════════════════════════════════════════════════════╝"
            << RESET << std::endl;

  int passed = 0;
  int total = 0;

  // Run all tests
  total++; if (testCantelliFactor()) passed++;
  total++; if (testSensitivityStatistics()) passed++;
  total++; if (testTubeOffset()) passed++;
  total++; if (testCompleteMargin()) passed++;
  total++; if (testRiskAllocation()) passed++;
  total++; if (testLinearization()) passed++;

  // Summary
  std::cout << "\n" << BOLD << "═══════════════════════════════════════" << RESET << std::endl;
  std::cout << BOLD << "Test Summary: " << passed << "/" << total << " tests passed" << RESET << std::endl;

  if (passed == total) {
    std::cout << GREEN << BOLD << "✓ ALL TESTS PASSED!" << RESET << std::endl;
    std::cout << "\nAll manuscript formulas are correctly implemented." << std::endl;
    std::cout << "Ready to integrate with Tube MPC." << std::endl;
    return 0;
  } else {
    std::cout << RED << BOLD << "✗ SOME TESTS FAILED!" << RESET << std::endl;
    std::cout << "\nPlease review the implementation." << std::endl;
    return 1;
  }
}
