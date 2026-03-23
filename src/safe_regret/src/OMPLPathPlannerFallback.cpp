/**
 * @file OMPLPathPlannerFallback.cpp
 * @brief Fallback implementation of path planner when OMPL is not available
 *
 * Provides simple geometric planning algorithms as fallback when OMPL is not installed.
 */

#include "safe_regret/OMPLPathPlanner.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace safe_regret {

// ============================================================================
// OMPLPathPlanner Fallback Implementation
// ============================================================================

OMPLPathPlanner::OMPLPathPlanner(const Eigen::VectorXd& workspace_bounds,
                               double planning_time)
  : workspace_bounds_(workspace_bounds),
    planning_time_(planning_time) {
  // Fallback constructor - no OMPL initialization needed
}

OMPLPathPlanner::~OMPLPathPlanner() {
  // Cleanup handled automatically
}

void OMPLPathPlanner::addObstacle(const Obstacle2D& obstacle) {
  circular_obstacles_.push_back(obstacle);
}

void OMPLPathPlanner::addPolygonObstacle(const std::vector<Point2D>& vertices) {
  polygon_obstacles_.push_back(vertices);
}

void OMPLPathPlanner::clearObstacles() {
  circular_obstacles_.clear();
  polygon_obstacles_.clear();
}

bool OMPLPathPlanner::isPathCollisionFree(const Path2D& path) const {
  // Check each edge for collision
  for (size_t i = 0; i < path.waypoints.size() - 1; ++i) {
    const Point2D& p1 = path.waypoints[i];
    const Point2D& p2 = path.waypoints[i + 1];

    // Sample points along edge
    int num_samples = 10;
    for (int j = 0; j <= num_samples; ++j) {
      double t = static_cast<double>(j) / num_samples;

      Point2D sampled_point(
        p1.x + t * (p2.x - p1.x),
        p1.y + t * (p2.y - p1.y)
      );

      // Check circular obstacles
      for (const auto& obs : circular_obstacles_) {
        if (obs.contains(sampled_point)) {
          return false;
        }
      }

      // Check polygon obstacles
      for (const auto& poly : polygon_obstacles_) {
        Polygon2D polygon;
        polygon.vertices = poly;

        if (polygon.contains(sampled_point)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool OMPLPathPlanner::arePathsTopologicallyEquivalent(const Path2D& path1,
                                                     const Path2D& path2) const {
  // Simplified check: compare path lengths and general shape
  double length_diff = std::abs(path1.length() - path2.length());
  return length_diff < 0.5;  // Consider similar if lengths differ by < 0.5m
}

Path2D OMPLPathPlanner::planGeometricPath(const Point2D& start,
                                          const Point2D& goal) {
  // Simple straight-line path with collision checking
  Path2D path;

  // Check if direct path is collision-free
  Path2D direct_path;
  direct_path.addWaypoint(start);
  direct_path.addWaypoint(goal);

  if (isPathCollisionFree(direct_path)) {
    return direct_path;
  }

  // Otherwise, create a simple waypoint-based path around obstacles
  path.addWaypoint(start);

  // Try to navigate around obstacles using a simple approach
  Point2D current = start;
  Point2D next = goal;

  // Add intermediate waypoints to avoid obstacles
  for (const auto& obs : circular_obstacles_) {
    // Check if obstacle is in the way
    double dist_to_obstacle = current.distanceTo(obs.center);

    if (dist_to_obstacle < obs.radius + 1.0) {
      // Go around the obstacle
      double angle = std::atan2(goal.y - obs.center.y, goal.x - obs.center.x);
      double avoid_radius = obs.radius + 0.5;

      // Try going clockwise
      Point2D waypoint1(
        obs.center.x + avoid_radius * std::cos(angle + M_PI/2),
        obs.center.y + avoid_radius * std::sin(angle + M_PI/2)
      );

      // Try going counter-clockwise
      Point2D waypoint2(
        obs.center.x + avoid_radius * std::cos(angle - M_PI/2),
        obs.center.y + avoid_radius * std::sin(angle - M_PI/2)
      );

      // Choose the closer waypoint
      if (current.distanceTo(waypoint1) < current.distanceTo(waypoint2)) {
        path.addWaypoint(waypoint1);
      } else {
        path.addWaypoint(waypoint2);
      }
    }
  }

  path.addWaypoint(goal);

  // Verify the path is collision-free
  if (!isPathCollisionFree(path)) {
    std::cerr << "Warning: Could not find collision-free path in fallback planner"
              << std::endl;
    return Path2D();  // Return empty path
  }

  return path;
}

std::vector<Path2D> OMPLPathPlanner::planMultipleGeometricPaths(
  const Point2D& start,
  const Point2D& goal,
  int num_paths) {

  std::vector<Path2D> paths;

  // Plan first path (direct or simple avoidance)
  Path2D first_path = planGeometricPath(start, goal);

  if (first_path.empty()) {
    return paths;  // Even first path failed
  }

  paths.push_back(first_path);

  // Generate additional paths with variations
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(-1.0, 1.0);

  for (int i = 1; i < num_paths; ++i) {
    Path2D path;
    path.addWaypoint(start);

    // Add random intermediate waypoints
    for (const auto& obs : circular_obstacles_) {
      double angle = dis(gen) * M_PI;
      double avoid_radius = obs.radius + 0.5 + dis(gen) * 0.5;

      Point2D waypoint(
        obs.center.x + avoid_radius * std::cos(angle),
        obs.center.y + avoid_radius * std::sin(angle)
      );

      path.addWaypoint(waypoint);
    }

    path.addWaypoint(goal);

    // Check if collision-free and topologically distinct
    if (isPathCollisionFree(path)) {
      bool is_distinct = true;

      for (const auto& existing_path : paths) {
        if (arePathsTopologicallyEquivalent(path, existing_path)) {
          is_distinct = false;
          break;
        }
      }

      if (is_distinct) {
        paths.push_back(path);
      }
    }

    // Stop if we have enough paths
    if ((int)paths.size() >= num_paths) {
      break;
    }
  }

  // Sort by length
  std::sort(paths.begin(), paths.end(),
    [](const Path2D& a, const Path2D& b) {
      return a.length() < b.length();
    });

  return paths;
}

Path2D OMPLPathPlanner::planPath(const Point2D& start,
                                const Point2D& goal,
                                OMPLPlannerType planner_type) {
  // Fallback: ignore planner_type, use simple geometric planning
  return planGeometricPath(start, goal);
}

std::vector<Path2D> OMPLPathPlanner::planMultiplePaths(
  const Point2D& start,
  const Point2D& goal,
  OMPLPlannerType planner_type,
  int num_paths) {

  // Fallback: use simple geometric planning
  return planMultipleGeometricPaths(start, goal, num_paths);
}

Path2D OMPLPathPlanner::simplifyPath(const Path2D& path) {
  if (path.waypoints.size() < 3) {
    return path;
  }

  Path2D simplified;
  simplified.addWaypoint(path.waypoints[0]);

  // Try to skip waypoints while maintaining collision-free path
  size_t i = 0;
  while (i < path.waypoints.size() - 1) {
    // Try to connect to furthest visible waypoint
    size_t j = path.waypoints.size() - 1;

    for (; j > i + 1; --j) {
      // Check if edge from i to j is collision-free
      Path2D test_path;
      test_path.addWaypoint(path.waypoints[i]);
      test_path.addWaypoint(path.waypoints[j]);

      if (isPathCollisionFree(test_path)) {
        simplified.addWaypoint(path.waypoints[j]);
        i = j;
        break;
      }
    }

    // If no shortcut found, move to next waypoint
    if (j == i + 1) {
      i++;
      if (i < path.waypoints.size()) {
        simplified.addWaypoint(path.waypoints[i]);
      }
    }
  }

  return simplified;
}

// ============================================================================
// OMPLTopologicalPlanner Fallback Implementation
// ============================================================================

std::string OMPLTopologicalPlanner::computePathSignature(const Path2D& path) const {
  // Compute signature based on path waypoints
  std::stringstream ss;
  ss << path.size();

  for (const auto& wp : path.waypoints) {
    ss << "_" << static_cast<int>(wp.x * 10) << static_cast<int>(wp.y * 10);
  }

  return ss.str();
}

bool OMPLTopologicalPlanner::areTopologicallyEquivalent(const Path2D& path1,
                                                       const Path2D& path2) const {
  std::string sig1 = computePathSignature(path1);
  std::string sig2 = computePathSignature(path2);

  return sig1 == sig2;
}

std::vector<Path2D> OMPLTopologicalPlanner::generateHomologyPaths(
  const Point2D& start,
  const Point2D& goal,
  int num_homology_classes) {

  // Use fallback geometric planning with variations
  std::vector<Path2D> all_paths;

  // Generate multiple paths with different strategies
  for (int i = 0; i < num_homology_classes * 2; ++i) {
    Path2D path = planPath(start, goal, OMPLPlannerType::RRTSTAR);

    if (!path.empty()) {
      // Check if topologically distinct
      bool is_distinct = true;

      for (const auto& existing_path : all_paths) {
        if (areTopologicallyEquivalent(path, existing_path)) {
          is_distinct = false;
          break;
        }
      }

      if (is_distinct) {
        all_paths.push_back(path);
      }

      if ((int)all_paths.size() >= num_homology_classes) {
        break;
      }
    }
  }

  // Sort by cost (length)
  std::sort(all_paths.begin(), all_paths.end(),
    [](const Path2D& a, const Path2D& b) {
      return a.length() < b.length();
    });

  return all_paths;
}

// ============================================================================
// OMPLPathOptimizer Fallback Implementation
// ============================================================================

Path2D OMPLPathOptimizer::optimizePath(const Path2D& path) {
  // Combined optimization
  Path2D optimized = shortcutPath(path);
  optimized = smoothPath(optimized, 0.1);

  return optimized;
}

Path2D OMPLPathOptimizer::shortcutPath(const Path2D& path) {
  if (path.waypoints.size() < 3) {
    return path;
  }

  Path2D shortcut;
  shortcut.addWaypoint(path.waypoints[0]);

  size_t i = 0;
  while (i < path.waypoints.size() - 1) {
    // Try to connect to furthest visible waypoint
    size_t j = path.waypoints.size() - 1;

    for (; j > i + 1; --j) {
      // For fallback, assume visibility if distance is reasonable
      const Point2D& p1 = path.waypoints[i];
      const Point2D& p2 = path.waypoints[j];

      double dist = p1.distanceTo(p2);

      // Simple heuristic: if direct connection is not too long, it's probably visible
      if (dist < 5.0) {
        shortcut.addWaypoint(path.waypoints[j]);
        i = j;
        break;
      }
    }

    if (j == i + 1) {
      i++;
      if (i < path.waypoints.size()) {
        shortcut.addWaypoint(path.waypoints[i]);
      }
    }
  }

  return shortcut;
}

Path2D OMPLPathOptimizer::smoothPath(const Path2D& path, double resolution) {
  if (path.waypoints.size() < 2) {
    return path;
  }

  Path2D smoothed;

  // Add first waypoint
  smoothed.addWaypoint(path.waypoints[0]);

  // Interpolate between waypoints
  for (size_t i = 0; i < path.waypoints.size() - 1; ++i) {
    const Point2D& p1 = path.waypoints[i];
    const Point2D& p2 = path.waypoints[i + 1];

    double dist = p1.distanceTo(p2);
    int num_segments = static_cast<int>(dist / resolution);

    for (int j = 1; j < num_segments; ++j) {
      double t = static_cast<double>(j) / num_segments;
      Point2D interpolated(
        p1.x + t * (p2.x - p1.x),
        p1.y + t * (p2.y - p1.y)
      );

      smoothed.addWaypoint(interpolated);
    }
  }

  // Add last waypoint
  smoothed.addWaypoint(path.waypoints.back());

  return smoothed;
}

double OMPLPathOptimizer::computeCurvature(const Point2D& p1,
                                           const Point2D& p2,
                                           const Point2D& p3) {
  // Compute curvature at p2
  double dx1 = p2.x - p1.x;
  double dy1 = p2.y - p1.y;
  double dx2 = p3.x - p2.x;
  double dy2 = p3.y - p2.y;

  double num = std::abs(dx1 * dy2 - dy1 * dx2);
  double den = std::pow(dx1 * dx1 + dy1 * dy1, 1.5);

  return (den > 1e-6) ? (num / den) : 0.0;
}

} // namespace safe_regret
