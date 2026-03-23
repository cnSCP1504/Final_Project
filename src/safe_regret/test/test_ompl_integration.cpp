/**
 * @file test_ompl_integration.cpp
 * @brief Test OMPL integration with ReferencePlanner
 */

#include <safe_regret/ReferencePlanner.hpp>
#include <safe_regret/TopologicalAbstraction.hpp>
#include <iostream>
#include <cassert>

using namespace safe_regret;

void test_assert(bool condition, const std::string& test_name) {
  if (condition) {
    std::cout << "✓ PASS: " << test_name << std::endl;
  } else {
    std::cout << "✗ FAIL: " << test_name << std::endl;
    std::exit(1);
  }
}

void test_ompl_integration() {
  std::cout << "\n=== Test: OMPL Integration with ReferencePlanner ===" << std::endl;

#ifdef HAVE_OMPL
  std::cout << "  OMPL is available - testing full integration" << std::endl;

  // Create planner
  ReferencePlanner planner(3, 2, NoRegretAlgorithm::ONLINE_MIRROR_DESCENT);

  // Set workspace bounds
  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;
  planner.setWorkspaceBounds(bounds);

  // Add obstacle
  planner.addObstacle(Obstacle2D(Point2D(2.0, 0.0), 1.0));

  // Check OMPL availability
  test_assert(planner.isOMPLAvailable(), "OMPL planner is available");

  // Create belief state
  BeliefState belief(3);
  belief.mean << -5.0, 0.0, 0.0;  // Start at (-5, 0)
  belief.covariance.setIdentity();

  // Create STL specification
  STLSpecification stl_spec = STLSpecification::createReachability("Goal", 10.0);

  // Plan reference trajectory
  ReferenceTrajectory reference = planner.planReference(belief, stl_spec, 20);

  test_assert(!reference.points.empty(), "Reference trajectory generated with OMPL");
  test_assert(reference.points.size() == 20, "Reference trajectory has correct horizon");

  // Check that path avoids obstacle
  bool avoids_obstacle = true;
  for (const auto& pt : reference.points) {
    double dist_to_obstacle = std::sqrt(
      std::pow(pt.state[0] - 2.0, 2) + std::pow(pt.state[1] - 0.0, 2)
    );
    if (dist_to_obstacle < 1.0) {  // Inside obstacle
      avoids_obstacle = false;
      break;
    }
  }
  test_assert(avoids_obstacle, "OMPL path avoids obstacles");

#else
  std::cout << "  OMPL not available - testing fallback behavior" << std::endl;

  // Create planner
  ReferencePlanner planner(3, 2, NoRegretAlgorithm::ONLINE_MIRROR_DESCENT);

  // Create belief state
  BeliefState belief(3);
  belief.mean << -5.0, 0.0, 0.0;

  // Create STL specification
  STLSpecification stl_spec = STLSpecification::createReachability("Goal", 10.0);

  // Plan reference trajectory (should use fallback)
  ReferenceTrajectory reference = planner.planReference(belief, stl_spec, 20);

  test_assert(!reference.points.empty(), "Fallback planner generates trajectory");
  test_assert(reference.points.size() == 20, "Fallback trajectory has correct horizon");
#endif
}

void test_ompl_toggle() {
  std::cout << "\n=== Test: OMPL Enable/Disable ===" << std::endl;

#ifdef HAVE_OMPL
  ReferencePlanner planner(3, 2);

  // Initially enabled
  test_assert(planner.isOMPLAvailable(), "OMPL initially available");

  // Disable OMPL
  planner.setOMPLEnabled(false);
  test_assert(!planner.isOMPLAvailable(), "OMPL disabled");

  // Re-enable OMPL
  planner.setOMPLEnabled(true);
  test_assert(planner.isOMPLAvailable(), "OMPL re-enabled");
#else
  std::cout << "  OMPL not available - skipping toggle test" << std::endl;
#endif
}

void test_workspace_obstacles() {
  std::cout << "\n=== Test: Workspace and Obstacle Management ===" << std::endl;

#ifdef HAVE_OMPL
  ReferencePlanner planner(3, 2);

  // Set workspace bounds
  Eigen::VectorXd bounds(4);
  bounds << -5.0, 5.0, -3.0, 3.0;
  planner.setWorkspaceBounds(bounds);

  // Add multiple obstacles
  planner.addObstacle(Obstacle2D(Point2D(1.0, 1.0), 0.5));
  planner.addObstacle(Obstacle2D(Point2D(-1.0, -1.0), 0.5));

  // Plan with obstacles
  BeliefState belief(3);
  belief.mean << -3.0, 0.0, 0.0;

  STLSpecification stl_spec = STLSpecification::createReachability("Goal", 10.0);
  ReferenceTrajectory reference = planner.planReference(belief, stl_spec, 15);

  test_assert(!reference.points.empty(), "Path planned with multiple obstacles");
#else
  std::cout << "  OMPL not available - skipping workspace/obstacle test" << std::endl;
#endif
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "  OMPL Integration Tests" << std::endl;
  std::cout << "========================================" << std::endl;

  try {
    test_ompl_integration();
    test_ompl_toggle();
    test_workspace_obstacles();

    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ ALL OMPL INTEGRATION TESTS PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "\n✗ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
