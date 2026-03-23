/**
 * @file test_ompl_planner.cpp
 * @brief Test OMPL integration with Safe-Regret framework
 */

#include <safe_regret/OMPLPathPlanner.hpp>
#include <iostream>
#include <cassert>
#include <cmath>

using namespace safe_regret;

void test_assert(bool condition, const std::string& test_name) {
  if (condition) {
    std::cout << "✓ PASS: " << test_name << std::endl;
  } else {
    std::cout << "✗ FAIL: " << test_name << std::endl;
    std::exit(1);
  }
}

void test_ompl_basic() {
  std::cout << "\n=== Test: OMPL Basic Functionality ===" << std::endl;

  // Create workspace
  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  OMPLPathPlanner planner(bounds, 1.0);

  // Add obstacle
  planner.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));

  // Plan path
  Point2D start(-5.0, 0.0);
  Point2D goal(5.0, 0.0);

  Path2D path = planner.planPath(start, goal, OMPLPlannerType::RRTSTAR);

  test_assert(!path.empty(), "OMPL found collision-free path");
  test_assert(path.waypoints[0].x == start.x, "Path starts at start");
  test_assert(path.waypoints.back().x == goal.x, "Path ends at goal");

  std::cout << "  Path length: " << path.length() << std::endl;
}

void test_multiple_paths() {
  std::cout << "\n=== Test: Multiple OMPL Paths ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  OMPLPathPlanner planner(bounds, 1.0);
  planner.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));

  Point2D start(-5.0, 0.0);
  Point2D goal(5.0, 0.0);

  auto paths = planner.planMultiplePaths(start, goal, OMPLPlannerType::RRTSTAR, 3);

  std::cout << "  Generated " << paths.size() << " paths" << std::endl;
  test_assert(paths.size() >= 1, "At least one path generated");

  for (size_t i = 0; i < paths.size(); ++i) {
    std::cout << "  Path " << i << ": " << paths[i].size()
              << " waypoints, length = " << paths[i].length() << std::endl;
  }
}

void test_path_simplification() {
  std::cout << "\n=== Test: Path Simplification ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  OMPLPathPlanner planner(bounds, 1.0);

  Point2D start(-5.0, -5.0);
  Point2D goal(5.0, 5.0);

  Path2D path = planner.planPath(start, goal, OMPLPlannerType::PRM);

  test_assert(!path.empty(), "Path found for simplification");

  size_t original_size = path.size();
  std::cout << "  Original waypoints: " << original_size << std::endl;

  Path2D simplified = planner.simplifyPath(path);

  std::cout << "  Simplified waypoints: " << simplified.size() << std::endl;
  test_assert(simplified.size() <= original_size, "Simplified path has fewer waypoints");
  test_assert(simplified.length() < path.length() * 1.1, "Simplified path is similar length");
}

void test_collision_checking() {
  std::cout << "\n=== Test: Collision Checking ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  OMPLPathPlanner planner(bounds, 1.0);
  planner.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.5));

  // Direct path through obstacle
  Path2D direct_path;
  direct_path.addWaypoint(Point2D(-5.0, 0.0));
  direct_path.addWaypoint(Point2D(5.0, 0.0));

  bool is_collision_free = planner.isPathCollisionFree(direct_path);

  std::cout << "  Direct path collision-free: " << (is_collision_free ? "YES" : "NO") << std::endl;

  // Path around obstacle
  Path2D around_path;
  around_path.addWaypoint(Point2D(-5.0, 0.0));
  around_path.addWaypoint(Point2D(0.0, 2.0));
  around_path.addWaypoint(Point2D(5.0, 0.0));

  is_collision_free = planner.isPathCollisionFree(around_path);

  std::cout <<  " Around path collision-free: " << (is_collision_free ? "YES" : "NO") << std::endl;
  test_assert(is_collision_free, "Path around obstacle is collision-free");
}

void test_topological_paths() {
  std::cout << "\n=== Test: Topological Paths ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  OMPLTopologicalPlanner planner(bounds, 2.0);
  planner.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));

  Point2D start(-5.0, 0.0);
  Point2D goal(5.0, 0.0);

  auto paths = planner.generateHomologyPaths(start, goal, 3);

  std::cout << "  Generated " << paths.size() << " topological paths" << std::endl;
  test_assert(paths.size() >= 1, "At least one topological path generated");

  for (size_t i = 0; i < std::min(paths.size(), size_t(3)); ++i) {
    std::cout << "  Path " << i << ": "
              << paths[i].size() << " waypoints, "
              << "length = " << paths[i].length() << std::endl;
  }
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "  OMPL Integration Tests" << std::endl;
  std::cout << "========================================" << std::endl;

  try {
    test_ompl_basic();
    test_multiple_paths();
    test_path_simplification();
    test_collision_checking();
    test_topological_paths();

    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ ALL OMPL TESTS PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "\n✗ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
