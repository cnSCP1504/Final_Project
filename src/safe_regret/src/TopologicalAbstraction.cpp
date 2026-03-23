/**
 * @file TopologicalAbstraction.cpp
 * @brief Implementation of topological abstraction
 */

#include "safe_regret/TopologicalAbstraction.hpp"
#include <algorithm>
#include <queue>
#include <cmath>
#include <limits>
#include <sstream>

namespace safe_regret {

// ============================================================================
// Polygon2D Implementation
// ============================================================================

bool Polygon2D::contains(const Point2D& point) const {
  // Ray casting algorithm
  int crossings = 0;
  size_t n = vertices.size();

  for (size_t i = 0; i < n; ++i) {
    Point2D p1 = vertices[i];
    Point2D p2 = vertices[(i + 1) % n];

    // Check if edge straddles horizontal line at point.y
    if ((p1.y > point.y) != (p2.y > point.y)) {
      // Compute x intersection
      double x_intersect = (p2.x - p1.x) * (point.y - p1.y) / (p2.y - p1.y) + p1.x;

      if (point.x < x_intersect) {
        crossings++;
      }
    }
  }

  return (crossings % 2) == 1;
}

Point2D Polygon2D::getCenter() const {
  if (vertices.empty()) {
    return Point2D();
  }

  double cx = 0.0, cy = 0.0;
  double signed_area = 0.0;
  size_t n = vertices.size();

  for (size_t i = 0; i < n; ++i) {
    size_t j = (i + 1) % n;
    double cross = vertices[i].x * vertices[j].y -
                   vertices[j].x * vertices[i].y;
    signed_area += cross;
    cx += (vertices[i].x + vertices[j].x) * cross;
    cy += (vertices[i].y + vertices[j].y) * cross;
  }

  signed_area *= 0.5;
  cx /= (6.0 * signed_area);
  cy /= (6.0 * signed_area);

  return Point2D(cx, cy);
}

double Polygon2D::getArea() const {
  double area = 0.0;
  size_t n = vertices.size();

  for (size_t i = 0; i < n; ++i) {
    size_t j = (i + 1) % n;
    area += vertices[i].x * vertices[j].y;
    area -= vertices[j].x * vertices[i].y;
  }

  return std::abs(area) / 2.0;
}

// ============================================================================
// TopologicalAbstraction Implementation
// ============================================================================

TopologicalAbstraction::TopologicalAbstraction(const Eigen::VectorXd& workspace_bounds)
  : workspace_bounds_(workspace_bounds),
    rng_(std::random_device{}()),
    dist_(0.0, 1.0) {

  // Create workspace polygon from bounds
  if (workspace_bounds.size() >= 4) {
    workspace_polygon_.vertices.push_back(
      Point2D(workspace_bounds[0], workspace_bounds[2]));  // min_x, min_y
    workspace_polygon_.vertices.push_back(
      Point2D(workspace_bounds[1], workspace_bounds[2]));  // max_x, min_y
    workspace_polygon_.vertices.push_back(
      Point2D(workspace_bounds[1], workspace_bounds[3]));  // max_x, max_y
    workspace_polygon_.vertices.push_back(
      Point2D(workspace_bounds[0], workspace_bounds[3]));  // min_x, max_y
  }
}

void TopologicalAbstraction::addObstacle(const Obstacle2D& obstacle) {
  obstacles_.push_back(obstacle);
}

std::vector<HomologyClass> TopologicalAbstraction::computeHomologyClasses(
  const Point2D& start,
  const Point2D& goal) const {

  if (obstacles_.empty()) {
    // No obstacles: only direct path
    std::vector<HomologyClass> classes;
    std::vector<int> empty_winding;
    classes.push_back(HomologyClass(empty_winding));
    return classes;
  }

  std::vector<HomologyClass> classes;

  // For each obstacle, path can go around left (winding = +1) or right (winding = -1)
  // This creates 2^n possible homology classes for n obstacles
  // However, in practice, many are equivalent or infeasible

  // Simplified approach: consider relevant obstacles only
  std::vector<int> relevant_obstacles;

  for (size_t i = 0; i < obstacles_.size(); ++i) {
    // Check if obstacle blocks direct path
    if (obstacleLineIntersects(start, goal, obstacles_[i])) {
      relevant_obstacles.push_back(i);
    }
  }

  if (relevant_obstacles.empty()) {
    // Direct path is collision-free
    return {HomologyClass(std::vector<int>(obstacles_.size(), 0))};
  }

  // Generate homology classes for relevant obstacles
  // Each relevant obstacle can be bypassed left (+1) or right (-1)
  size_t n_relevant = relevant_obstacles.size();

  for (size_t mask = 0; mask < (1u << n_relevant); ++mask) {
    std::vector<int> winding_numbers(obstacles_.size(), 0);

    for (size_t i = 0; i < n_relevant; ++i) {
      int obs_idx = relevant_obstacles[i];
      // Check bit i in mask
      if (mask & (1u << i)) {
        winding_numbers[obs_idx] = 1;   // Go around left
      } else {
        winding_numbers[obs_idx] = -1;  // Go around right
      }
    }

    classes.push_back(HomologyClass(winding_numbers));
  }

  return classes;
}

Path2D TopologicalAbstraction::generateRepresentativePath(
  const Point2D& start,
  const Point2D& goal,
  const HomologyClass& homology_class) const {

  Path2D path;
  path.addWaypoint(start);

  Point2D current = start;

  // Generate intermediate waypoints to satisfy homology class
  // For each obstacle, add waypoint on appropriate side

  for (size_t i = 0; i < obstacles_.size(); ++i) {
    if (homology_class.winding_numbers[i] != 0) {
      // Need to go around this obstacle
      Point2D waypoint = findWaypoint(current, goal, homology_class);
      path.addWaypoint(waypoint);
      current = waypoint;
    }
  }

  path.addWaypoint(goal);

  // Simplify path (remove redundant waypoints)
  path = simplifyPath(path);

  return path;
}

std::vector<Path2D> TopologicalAbstraction::getAllTopologicalPaths(
  const Point2D& start,
  const Point2D& goal) const {

  std::vector<Path2D> all_paths;
  auto classes = computeHomologyClasses(start, goal);

  for (const auto& homology_class : classes) {
    Path2D path = generateRepresentativePath(start, goal, homology_class);
    path.cost = path.length();  // Use path length as cost

    // Only add if collision-free
    if (isPathCollisionFree(path)) {
      all_paths.push_back(path);
    }
  }

  // Sort by cost
  std::sort(all_paths.begin(), all_paths.end(),
    [](const Path2D& a, const Path2D& b) {
      return a.cost < b.cost;
    });

  return all_paths;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

int TopologicalAbstraction::computeWindingNumber(
  const Point2D& point,
  const Obstacle2D& obstacle) const {

  // Compute winding number using angle summation
  double total_angle = 0.0;

  // Sample points on obstacle circumference
  int n_samples = 16;
  for (int i = 0; i < n_samples; ++i) {
    double theta1 = 2.0 * M_PI * i / n_samples;
    double theta2 = 2.0 * M_PI * (i + 1) / n_samples;

    Point2D p1(
      obstacle.center.x + obstacle.radius * std::cos(theta1),
      obstacle.center.y + obstacle.radius * std::sin(theta1)
    );

    Point2D p2(
      obstacle.center.x + obstacle.radius * std::cos(theta2),
      obstacle.center.y + obstacle.radius * std::sin(theta2)
    );

    // Compute angle change
    double v1 = std::atan2(p1.y - point.y, p1.x - point.x);
    double v2 = std::atan2(p2.y - point.y, p2.x - point.x);

    double angle_diff = v2 - v1;

    // Normalize to [-pi, pi]
    while (angle_diff <= -M_PI) angle_diff += 2.0 * M_PI;
    while (angle_diff > M_PI) angle_diff -= 2.0 * M_PI;

    total_angle += angle_diff;
  }

  // Winding number is total angle divided by 2*pi
  return static_cast<int>(std::round(total_angle / (2.0 * M_PI)));
}

std::vector<int> TopologicalAbstraction::computeWindingNumbers(
  const Point2D& point) const {

  std::vector<int> winding_numbers;

  for (const auto& obstacle : obstacles_) {
    winding_numbers.push_back(computeWindingNumber(point, obstacle));
  }

  return winding_numbers;
}

bool TopologicalAbstraction::isPathCollisionFree(const Path2D& path) const {
  // Check each edge for collision
  for (size_t i = 0; i < path.waypoints.size() - 1; ++i) {
    const Point2D& p1 = path.waypoints[i];
    const Point2D& p2 = path.waypoints[i + 1];

    // Check collision with each obstacle
    for (const auto& obstacle : obstacles_) {
      if (lineIntersectsObstacle(p1, p2, obstacle)) {
        return false;
      }
    }
  }

  return true;
}

Path2D TopologicalAbstraction::simplifyPath(const Path2D& path) const {
  if (path.waypoints.size() <= 2) {
    return path;
  }

  Path2D simplified;
  simplified.addWaypoint(path.waypoints[0]);

  size_t i = 0;
  while (i < path.waypoints.size() - 1) {
    // Try to skip waypoints
    size_t j = path.waypoints.size() - 1;

    // Find farthest waypoint visible from current
    for (; j > i + 1; --j) {
      if (isEdgeCollisionFree(path.waypoints[i], path.waypoints[j])) {
        break;
      }
    }

    simplified.addWaypoint(path.waypoints[j]);
    i = j;
  }

  return simplified;
}

Point2D TopologicalAbstraction::findWaypoint(
  const Point2D& current,
  const Point2D& goal,
  const HomologyClass& target_class) const {

  // Find intermediate waypoint that achieves target homology class
  // Strategy: go around obstacles on appropriate side

  Point2D waypoint = current;

  for (size_t i = 0; i < obstacles_.size(); ++i) {
    int winding = target_class.winding_numbers[i];

    if (winding != 0) {
      const Obstacle2D& obs = obstacles_[i];

      // Compute direction from obstacle to line (current -> goal)
      double dx = goal.x - current.x;
      double dy = goal.y - current.y;

      // Perpendicular direction
      double perp_x = -dy;
      double perp_y = dx;
      double perp_norm = std::sqrt(perp_x * perp_x + perp_y * perp_y);

      if (perp_norm > 1e-6) {
        perp_x /= perp_norm;
        perp_y /= perp_norm;
      }

      // Offset to side based on winding direction
      double offset = obs.radius * 1.2;  // 20% margin

      if (winding > 0) {
        // Go around left (counter-clockwise)
        waypoint.x = obs.center.x + perp_x * offset;
        waypoint.y = obs.center.y + perp_y * offset;
      } else {
        // Go around right (clockwise)
        waypoint.x = obs.center.x - perp_x * offset;
        waypoint.y = obs.center.y - perp_y * offset;
      }

      break;  // Found waypoint for most relevant obstacle
    }
  }

  return waypoint;
}

bool TopologicalAbstraction::obstacleLineIntersects(
  const Point2D& start,
  const Point2D& goal,
  const Obstacle2D& obstacle) const {

  // Check if line segment from start to goal intersects obstacle
  return lineIntersectsObstacle(start, goal, obstacle);
}

bool TopologicalAbstraction::lineIntersectsObstacle(
  const Point2D& p1,
  const Point2D& p2,
  const Obstacle2D& obstacle) const {

  // Compute closest point on line segment to obstacle center
  double dx = p2.x - p1.x;
  double dy = p2.y - p1.y;
  double length_sq = dx * dx + dy * dy;

  double t = ((obstacle.center.x - p1.x) * dx +
              (obstacle.center.y - p1.y) * dy) / length_sq;

  // Clamp t to [0, 1]
  t = std::max(0.0, std::min(1.0, t));

  // Closest point on segment
  double closest_x = p1.x + t * dx;
  double closest_y = p1.y + t * dy;

  // Distance from obstacle center to closest point
  double dist = std::sqrt(
    (closest_x - obstacle.center.x) * (closest_x - obstacle.center.x) +
    (closest_y - obstacle.center.y) * (closest_y - obstacle.center.y)
  );

  return dist <= obstacle.radius;
}

bool TopologicalAbstraction::isEdgeCollisionFree(
  const Point2D& p1,
  const Point2D& p2) const {

  for (const auto& obstacle : obstacles_) {
    if (lineIntersectsObstacle(p1, p2, obstacle)) {
      return false;
    }
  }
  return true;
}

// ============================================================================
// VisibilityGraph Implementation (TODO: Add to header if needed)
// ============================================================================

/*
std::vector<Point2D> VisibilityGraph::buildGraph(
  const std::vector<Obstacle2D>& obstacles,
  const std::vector<Point2D>& waypoints) {

  // For now, just return waypoints
  // In full implementation, would filter to only visible connections
  return waypoints;
}

Path2D VisibilityGraph::findShortestPath(
  const std::vector<Point2D>& graph,
  const Point2D& start,
  const Point2D& goal,
  const std::vector<Obstacle2D>& obstacles) {

  // Simple implementation: use all waypoints in order
  Path2D path;
  path.addWaypoint(start);

  for (const auto& wp : graph) {
    path.addWaypoint(wp);
  }

  path.addWaypoint(goal);

  return path;
}

bool VisibilityGraph::isVisible(
  const Point2D& from,
  const Point2D& to,
  const std::vector<Obstacle2D>& obstacles) {

  for (const auto& obs : obstacles) {
    // Simple line-obstacle intersection test
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double length_sq = dx * dx + dy * dy;

    double t = ((obs.center.x - from.x) * dx +
                (obs.center.y - from.y) * dy) / length_sq;
    t = std::max(0.0, std::min(1.0, t));

    double closest_x = from.x + t * dx;
    double closest_y = from.y + t * dy;

    double dist = std::sqrt(
      (closest_x - obs.center.x) * (closest_x - obs.center.x) +
      (closest_y - obs.center.y) * (closest_y - obs.center.y)
    );

    if (dist <= obs.radius) {
      return false;
    }
  }

  return true;
}
*/

} // namespace safe_regret
