/**
 * @file test_dimension_mismatch.cpp
 * @brief Test dimension mismatch handling
 */

#include "dr_tightening/ResidualCollector.hpp"
#include <iostream>
#include <cassert>

using namespace dr_tightening;

int main() {
  std::cout << "Testing dimension mismatch handling..." << std::endl;

  ResidualCollector collector(10);

  // Add residuals of dimension 3
  for (int i = 0; i < 5; ++i) {
    Eigen::VectorXd r(3);
    r << i, i*2, i*3;
    collector.addResidual(r);
  }

  std::cout << "Added 5 residuals of dimension 3" << std::endl;
  std::cout << "Window size: " << collector.getWindowSize() << std::endl;
  std::cout << "Residual dimension: " << collector.getResidualDimension() << std::endl;

  // Try to add residual of different dimension
  std::cout << "\nAttempting to add residual of dimension 2..." << std::endl;
  Eigen::VectorXd r_wrong(2);
  r_wrong << 1, 2;
  collector.addResidual(r_wrong);

  std::cout << "Window size after wrong dim: " << collector.getWindowSize() << std::endl;
  std::cout << "Residual dimension: " << collector.getResidualDimension() << std::endl;

  // Check that wrong dimension was rejected
  if (collector.getWindowSize() == 5) {
    std::cout << "\n✓ PASS: Wrong dimension residual was rejected" << std::endl;
    return 0;
  } else {
    std::cout << "\n✗ FAIL: Wrong dimension residual was accepted!" << std::endl;
    return 1;
  }
}
