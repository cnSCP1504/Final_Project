/**
 * @file TopologicalAbstraction.hpp
 * @brief Topological abstraction for belief-space planning
 *
 * Implements topological path planning using homology classes to reason about
 * distinct routes in partially observable environments.
 */

#pragma once

#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <random>

namespace safe_regret {

/**
 * @brief 2D point for planning
 */
struct Point2D {
  double x, y;

  Point2D() : x(0.0), y(0.0) {}
  Point2D(double x_, double y_) : x(x_), y(y_) {}

  Eigen::VectorXd toVector() const {
    Eigen::VectorXd vec(2);
    vec << x, y;
    return vec;
  }

  double distanceTo(const Point2D& other) const {
    return std::sqrt((x - other.x) * (x - other.x) +
                     (y - other.y) * (y - other.y));
  }

  Point2D operator+(const Point2D& other) const {
    return Point2D(x + other.x, y + other.y);
  }

  Point2D operator*(double scalar) const {
    return Point2D(x * scalar, y * scalar);
  }
};

/**
 * @brief 2D obstacle representation
 */
struct Obstacle2D {
  Point2D center;
  double radius;

  Obstacle2D(Point2D c = Point2D(), double r = 0.0)
    : center(c), radius(r) {}

  bool contains(const Point2D& point) const {
    return center.distanceTo(point) <= radius;
  }
};

/**
 * @brief Simple 2D polygon (workspace boundary)
 */
struct Polygon2D {
  std::vector<Point2D> vertices;

  bool contains(const Point2D& point) const;
  Point2D getCenter() const;
  double getArea() const;
};

/**
 * @brief Path represented as sequence of waypoints
 */
struct Path2D {
  std::vector<Point2D> waypoints;
  double cost;

  Path2D() : cost(0.0) {}

  void addWaypoint(const Point2D& wp) {
    waypoints.push_back(wp);
  }

  size_t size() const { return waypoints.size(); }
  bool empty() const { return waypoints.empty(); }

  double length() const {
    double total = 0.0;
    for (size_t i = 1; i < waypoints.size(); ++i) {
      total += waypoints[i-1].distanceTo(waypoints[i]);
    }
    return total;
  }

  std::vector<Eigen::VectorXd> toEigenPath() const {
    std::vector<Eigen::VectorXd> path;
    for (const auto& wp : waypoints) {
      path.push_back(wp.toVector());
    }
    return path;
  }
};

/**
 * @brief Homology class identifier
 */
struct HomologyClass {
  std::vector<int> winding_numbers;
  std::string class_id;

  HomologyClass() {}

  HomologyClass(const std::vector<int>& winding)
    : winding_numbers(winding) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < winding.size(); ++i) {
      ss << winding[i];
      if (i < winding.size() - 1) ss << ",";
    }
    ss << "]";
    class_id = ss.str();
  }

  bool operator==(const HomologyClass& other) const {
    return winding_numbers == other.winding_numbers;
  }

  bool operator<(const HomologyClass& other) const {
    return class_id < other.class_id;
  }
};

/**
 * @brief Topological abstraction of the workspace
 */
class TopologicalAbstraction {
public:
  explicit TopologicalAbstraction(const Eigen::VectorXd& workspace_bounds);

  void addObstacle(const Obstacle2D& obstacle);

  std::vector<HomologyClass> computeHomologyClasses(
    const Point2D& start,
    const Point2D& goal) const;

  Path2D generateRepresentativePath(
    const Point2D& start,
    const Point2D& goal,
    const HomologyClass& homology_class) const;

  std::vector<Path2D> getAllTopologicalPaths(
    const Point2D& start,
    const Point2D& goal) const;

  size_t getObstacleCount() const { return obstacles_.size(); }
  void clearObstacles() { obstacles_.clear(); }

protected:
  int computeWindingNumber(const Point2D& point, const Obstacle2D& obstacle) const;
  std::vector<int> computeWindingNumbers(const Point2D& point) const;
  bool isPathCollisionFree(const Path2D& path) const;
  Path2D simplifyPath(const Path2D& path) const;
  Point2D findWaypoint(const Point2D& current, const Point2D& goal,
                      const HomologyClass& target_class) const;

private:
  Eigen::VectorXd workspace_bounds_;
  std::vector<Obstacle2D> obstacles_;
  Polygon2D workspace_polygon_;

  bool obstacleLineIntersects(const Point2D& start, const Point2D& goal,
                            const Obstacle2D& obstacle) const;
  bool lineIntersectsObstacle(const Point2D& p1, const Point2D& p2,
                            const Obstacle2D& obstacle) const;
  bool isEdgeCollisionFree(const Point2D& p1, const Point2D& p2) const;

  mutable std::mt19937 rng_;
  std::uniform_real_distribution<double> dist_;
};

} // namespace safe_regret
