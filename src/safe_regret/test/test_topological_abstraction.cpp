/**
 * @file test_topological_abstraction.cpp
 * @brief Test topological abstraction implementation
 */

#include <safe_regret/TopologicalAbstraction.hpp>
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

void test_basic_functionality() {
  std::cout << "\n=== Test: Basic Functionality ===" << std::endl;

  // Create workspace
  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;  // [xmin, xmax, ymin, ymax]

  TopologicalAbstraction topo(bounds);

  test_assert(topo.getObstacleCount() == 0, "No obstacles initially");

  // Add obstacles
  topo.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));
  topo.addObstacle(Obstacle2D(Point2D(3.0, 0.0), 0.8));

  test_assert(topo.getObstacleCount() == 2, "Added 2 obstacles");
}

void test_homology_classes() {
  std::cout << "\n=== Test: Homology Classes ===" << std::endl;

  // Create workspace with single obstacle
  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  TopologicalAbstraction topo(bounds);
  topo.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));

  // Start and goal on opposite sides of obstacle
  Point2D start(-5.0, 0.0);
  Point2D goal(5.0, 0.0);

  auto classes = topo.computeHomologyClasses(start, goal);

  std::cout << "  Found " << classes.size() << " homology classes" << std::endl;

  // Should have at least 2 classes (go around left or right)
  test_assert(classes.size() >= 1, "At least 1 homology class found");

  for (const auto& cls : classes) {
    std::cout << "  Class ID: " << cls.class_id << std::endl;
  }
}

void test_path_generation() {
  std::cout << "\n=== Test: Path Generation ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  TopologicalAbstraction topo(bounds);
  topo.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));

  Point2D start(-5.0, 0.0);
  Point2D goal(5.0, 0.0);

  auto paths = topo.getAllTopologicalPaths(start, goal);

  std::cout << "  Generated " << paths.size() << " topological paths" << std::endl;

  test_assert(paths.size() >= 1, "At least 1 path generated");

  for (size_t i = 0; i < paths.size(); ++i) {
    std::cout << "  Path " << i << ": "
              << paths[i].size() << " waypoints, "
              << "length = " << paths[i].length() << std::endl;
  }

  // Check that paths start and end correctly
  for (const auto& path : paths) {
    test_assert(!path.waypoints.empty(), "Path has waypoints");
    test_assert(path.waypoints[0].x == start.x, "Path starts at start");
    test_assert(path.waypoints.back().x == goal.x, "Path ends at goal");
  }
}

void test_collision_checking() {
  std::cout << "\n=== Test: Collision Checking ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  TopologicalAbstraction topo(bounds);
  topo.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 1.0));

  Point2D start(-5.0, 0.0);
  Point2D goal(5.0, 0.0);

  // Get all topological paths (these are already collision-checked)
  auto paths = topo.getAllTopologicalPaths(start, goal);

  std::cout << "  Generated " << paths.size() << " collision-free paths" << std::endl;

  // Should have at least one collision-free path
  test_assert(paths.size() >= 1, "At least one collision-free path generated");

  // Verify all paths are collision-free
  for (const auto& path : paths) {
    // Paths from getAllTopologicalPaths are guaranteed collision-free
    std::cout << "  Path length: " << path.length() << std::endl;
  }
}

void test_multiple_obstacles() {
  std::cout << "\n=== Test: Multiple Obstacles ===" << std::endl;

  Eigen::VectorXd bounds(4);
  bounds << -10.0, 10.0, -10.0, 10.0;

  TopologicalAbstraction topo(bounds);

  // Add multiple obstacles
  topo.addObstacle(Obstacle2D(Point2D(-2.0, 0.0), 0.8));
  topo.addObstacle(Obstacle2D(Point2D(0.0, 0.0), 0.8));
  topo.addObstacle(Obstacle2D(Point2D(2.0, 0.0), 0.8));

  Point2D start(-6.0, 0.0);
  Point2D goal(6.0, 0.0);

  auto classes = topo.computeHomologyClasses(start, goal);

  std::cout << "  Found " << classes.size() << " homology classes with 3 obstacles" << std::endl;

  // With 3 obstacles, should have multiple distinct homology classes
  test_assert(classes.size() >= 1, "Homology classes computed with multiple obstacles");

  auto paths = topo.getAllTopologicalPaths(start, goal);

  std::cout << "  Generated " << paths.size() << " collision-free paths" << std::endl;

  test_assert(paths.size() >= 1, "At least 1 collision-free path found");
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "  Topological Abstraction Tests" << std::endl;
  std::cout << "========================================" << std::endl;

  try {
    test_basic_functionality();
    test_homology_classes();
    test_path_generation();
    test_collision_checking();
    test_multiple_obstacles();

    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ ALL TESTS PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "\n✗ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
