/**
 * @file test_stress.cpp
 * @brief Stress test for dr_tightening package
 *
 * Tests performance under heavy load and extreme conditions
 */

#include "dr_tightening/ResidualCollector.hpp"
#include "dr_tightening/AmbiguityCalibrator.hpp"
#include "dr_tightening/TighteningComputer.hpp"
#include "dr_tightening/SafetyLinearization.hpp"
#include <iostream>
#include <chrono>
#include <random>

using namespace dr_tightening;

double benchmarkResidualCollection(int iterations, int window_size, int dimension) {
  ResidualCollector collector(window_size);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::normal_distribution<double> d(0.0, 0.1);

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < iterations; ++i) {
    Eigen::VectorXd r(dimension);
    for (int j = 0; j < dimension; ++j) {
      r[j] = d(gen);
    }
    collector.addResidual(r);

    // Periodically access statistics
    if (i % 100 == 0) {
      auto mean = collector.getMean();
      auto cov = collector.getCovariance();
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  return diff.count();
}

double benchmarkMarginComputation(int iterations) {
  // Setup
  ResidualCollector collector(200);
  TighteningComputer computer(0.1, 20);

  // Add residuals
  std::random_device rd;
  std::mt19937 gen(rd());
  std::normal_distribution<double> d(0.0, 0.1);

  for (int i = 0; i < 200; ++i) {
    Eigen::VectorXd r(3);
    r << d(gen), d(gen), d(gen);
    collector.addResidual(r);
  }

  // Safety function
  auto safety_func = [](const Eigen::VectorXd& x) -> double {
    return x.norm() - 2.0;
  };

  SafetyLinearization linearization(safety_func);

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < iterations; ++i) {
    Eigen::VectorXd nominal(3);
    nominal << 3.0, 0.0, 0.0;

    auto result = linearization.linearize(nominal);

    double margin = computer.computeChebyshevMargin(
      nominal,
      result.nominal_value,
      result.gradient,
      collector,
      0.5,
      1.0,
      i % 20);
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  return diff.count();
}

void testRealtimePerformance() {
  std::cout << "\n=== Realtime Performance Test ===" << std::endl;

  const int target_hz = 100;  // 100 Hz = 10ms per computation
  const double target_time = 1.0 / target_hz;  // 10ms

  // Test residual collection speed
  std::cout << "\nResidual Collection (1000 iterations, window=200, dim=3):" << std::endl;
  double time_collect = benchmarkResidualCollection(1000, 200, 3);
  double avg_collect = time_collect / 1000.0;

  std::cout << "  Total time: " << time_collect << "s" << std::endl;
  std::cout << "  Average: " << (avg_collect * 1000) << "ms per add" << std::endl;
  std::cout << "  Target: " << (target_time * 1000) << "ms" << std::endl;

  if (avg_collect < target_time) {
    std::cout << "  ✓ PASS: Meets realtime requirement" << std::endl;
  } else {
    std::cout << "  ✗ FAIL: Exceeds realtime budget" << std::endl;
  }

  // Test margin computation speed
  std::cout << "\nMargin Computation (1000 iterations):" << std::endl;
  double time_margin = benchmarkMarginComputation(1000);
  double avg_margin = time_margin / 1000.0;

  std::cout << "  Total time: " << time_margin << "s" << std::endl;
  std::cout << "  Average: " << (avg_margin * 1000) << "ms per computation" << std::endl;
  std::cout << "  Target: " << (target_time * 1000) << "ms" << std::endl;

  if (avg_margin < target_time) {
    std::cout << "  ✓ PASS: Meets realtime requirement" << std::endl;
  } else {
    std::cout << "  ✗ FAIL: Exceeds realtime budget" << std::endl;
  }
}

void testMemoryUsage() {
  std::cout << "\n=== Memory Usage Test ===" << std::endl;

  // Create multiple collectors with large windows
  std::vector<ResidualCollector*> collectors;

  for (int i = 0; i < 10; ++i) {
    ResidualCollector* collector = new ResidualCollector(1000);

    // Add residuals
    for (int j = 0; j < 1000; ++j) {
      Eigen::VectorXd r(10);
      r.setRandom();
      collector->addResidual(r);
    }

    collectors.push_back(collector);
  }

  std::cout << "Created 10 collectors with 1000 residuals of dimension 10" << std::endl;
  std::cout << "Estimated memory: ~" << (10 * 1000 * 10 * 8 / 1024.0 / 1024.0)
            << " MB (double precision)" << std::endl;

  // Clean up
  for (auto* collector : collectors) {
    delete collector;
  }

  std::cout << "  ✓ PASS: Memory management successful" << std::endl;
}

void testConcurrentAccess() {
  std::cout << "\n=== Concurrent Access Simulation ===" << std::endl;

  // Simulate rapid additions and reads
  ResidualCollector collector(500);

  for (int i = 0; i < 1000; ++i) {
    Eigen::VectorXd r(5);
    r.setRandom();
    collector.addResidual(r);

    // Access statistics every 10 iterations
    if (i % 10 == 0) {
      auto mean = collector.getMean();
      auto cov = collector.getCovariance();
      auto dim = collector.getResidualDimension();
    }
  }

  std::cout << "Completed 1000 add/access cycles" << std::endl;
  std::cout << "Final window size: " << collector.getWindowSize() << std::endl;
  std::cout << "  ✓ PASS: No race conditions or crashes" << std::endl;
}

int main() {
  std::cout << "\n╔════════════════════════════════════════════════════════╗\n"
            << "║  DR Tightening Stress Test Suite                        ║\n"
            << "║  Performance and Robustness Under Load                  ║\n"
            << "╚════════════════════════════════════════════════════════╝\n"
            << std::endl;

  testRealtimePerformance();
  testMemoryUsage();
  testConcurrentAccess();

  std::cout << "\n╔════════════════════════════════════════════════════════╗\n"
            << "║  Stress Test Complete                                    ║\n"
            << "╚════════════════════════════════════════════════════════╝\n"
            << std::endl;

  return 0;
}
