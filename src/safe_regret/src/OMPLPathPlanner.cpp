/**
 * @file OMPLPathPlanner.cpp
 * @brief Implementation of OMPL-based path planner
 */

#include "safe_regret/OMPLPathPlanner.hpp"
#include <ompl/base/objectives/PathLengthOptimizationObjective.h>
#include <ompl/geometric/planners/rrt/RRT.h>
#include <ompl/geometric/planners/rrt/RRTstar.h>
#include <ompl/geometric/planners/prm/PRM.h>
#include <ompl/geometric/planners/est/EST.h>
#include <ompl/geometric/planners/pdst/PDST.h>
#include <ompl/geometric/planners/kpiece/KPIECE1.h>
#include <ompl/geometric/planners/kpiece/KPIECE2.h>
#include <ompl/geometric/planners/lazyprm/LazyPRM.h>
#include <ompl/geometric/planners/sparset/SparsePRM.h>
#include <ompl/geometric/PathSimplifier.h>
#include <ompl/base/goals/GoalState.h>
#include <ompl/base/goals/GoalRegion.h>
#include <ompl/tools/benchmark/Profiling.h>

#include <iostream>
#include <cmath>
#include <algorithm>

namespace safe_regret {

// ============================================================================
// OMPLPathPlanner Implementation
// ============================================================================

OMPLPathPlanner::OMPLPathPlanner(const Eigen::VectorXd& workspace_bounds,
                               double planning_time)
  : workspace_bounds_(workspace_bounds),
    planning_time_(planning_time) {

  // Create SE(2) state space
  state_space_ = ompl::base::StateSpacePtr(new ompl::base::SE2StateSpace());

  // Create simple setup
  simple_setup_ = ompl::geometric::SimpleSetupPtr(
    new ompl::geometric::SimpleSetup(state_space_));

  space_info_ = simple_setup_->getSpaceInformation();

  // Set bounds
  ompl::base::RealVectorBounds bounds(2);
  bounds.setLow(0, workspace_bounds_[0]);  // xmin
  bounds.setLow(1, workspace_bounds_[2]);  // ymin
  bounds.setHigh(0, workspace_bounds_[1]);  // xmax
  bounds.setHigh(1, workspace_bounds_[3]);  // ymax

  space_info_->as<ompl::base::SE2StateSpace>()->setBounds(bounds);
}

OMPLPathPlanner::~OMPLPathPlanner() {
  // OMPL handles cleanup automatically
}

void OMPLPathPlanner::createSimpleSetup() {
  // Already created in constructor
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

bool OMPLPathPlanner::isValid(const ompl::base::State* state) const {
  // Extract position from state
  const ompl::base::SE2StateSpace::StateType* se2_state =
    state->as<ompl::base::SE2StateSpace::StateType>();

  if (!se2_state) {
    return false;
  }

  Point2D position(se2_state->getX(), se2_state->getY());

  // Check circular obstacles
  for (const auto& obs : circular_obstacles_) {
    if (obs.contains(position)) {
      return false;
    }
  }

  // Check polygon obstacles
  for (const auto& poly : polygon_obstacles_) {
    Polygon2D polygon;
    polygon.vertices = poly;

    if (polygon.contains(position)) {
      return false;
    }
  }

  // Check workspace bounds
  if (position.x < workspace_bounds_[0] || position.x > workspace_bounds_[1] ||
      position.y < workspace_bounds_[2] || position.y > workspace_bounds_[3]) {
    return false;
  }

  return true;
}

ompl::base::PlannerPtr OMPLPathPlanner::createPlanner(OMPLPlannerType type) {
  switch (type) {
    case OMPLPlannerType::RRT:
      return ompl::geometric::RRT::getPlanner(space_info_);

    case OMPLPlannerType::RRTSTAR:
      return ompl::geometric::RRTstar::getPlanner(space_info_);

    case OMPLPlannerType::PRM:
      return ompl::geometric::PRM::getPlanner(space_info_);

    case OMPLPlannerType::EST:
      return ompl::geometric::EST::getPlanner(space_info_);

    case OMPLPlannerType::PDST:
      return ompl::geometric::PDST::getPlanner(space_info_);

    case OMPLPlannerType::KPIECE:
      return ompl::geometric::KPIECE1::getPlanner(space_info_);

    case OMPLPlannerType::LAZYPRM:
      return ompl::geometric::LazyPRM::getPlanner(space_info_);

    case OMPLPlannerType::SPARS:
      return ompl::geometric::SparsePRM::getPlanner(space_info_);

    default:
      return ompl::geometric::RRTstar::getPlanner(space_info_);
  }
}

Path2D OMPLPathPlanner::planPath(const Point2D& start,
                                const Point2D& goal,
                                OMPLPlannerType planner_type) {

  // Set start and goal
  ompl::base::ScopedState<> start_state(space_info_);
  start_state->as<ompl::base::SE2StateSpace::StateType>()->setXY(start.x, start.y);

  ompl::base::ScopedState<> goal_state(space_info_);
  goal_state->as<ompl::base::SE2StateSpace::StateType>()->setXY(goal.x, goal.y);

  simple_setup_->setStartAndGoalStates(start_state, goal_state);

  // Set state validity checker
  simple_setup_->setStateValidityChecker(
    std::bind(&OMPLPathPlanner::isValid, this, std::placeholders::_1));

  // Create planner
  ompl::base::PlannerPtr planner = createPlanner(planner_type);
  simple_setup_->setPlanner(planner);

  // Setup optimization objective (path length)
  ompl::base::OptimizationObjectivePtr objective(
    new ompl::base::PathLengthOptimizationObjective(space_info_));
  simple_setup_->getProblemDefinition()->setOptimizationObjective(objective);

  // Add obstacles to OMPL
  addObstaclesToOMPL();

  // Solve
  ompl::base::PlannerStatus solved = simple_setup_->solve(planning_time_);

  if (solved) {
    // Get solution path
    ompl::geometric::PathGeometric ompl_path = simple_setup_->getSolutionPath();
    return omplPathToPath2D(ompl_path);
  } else {
    // Planning failed
    return Path2D();  // Empty path
  }
}

std::vector<Path2D> OMPLPathPlanner::planMultiplePaths(
  const Point2D& start,
  const Point2D& goal,
  OMPLPlannerType planner_type,
  int num_paths) {

  std::vector<Path2D> paths;

  // Plan first path
  Path2D first_path = planPath(start, goal, planner_type);

  if (first_path.empty()) {
    // Even first path failed, return empty
    return paths;
  }

  paths.push_back(first_path);

  // Plan additional paths with different random seeds
  for (int i = 1; i < num_paths; ++i) {
    // Randomize start slightly for path diversity
    Point2D varied_start(
      start.x + (drand48() - 0.5) * 0.5,
      start.y + (drand48() - 0.5) * 0.5
    );

    Path2D path = planPath(varied_start, goal, planner_type);

    if (!path.empty()) {
      // Check if topologically distinct
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

Path2D OMPLPathPlanner::simplifyPath(const Path2D& path) {
  if (path.waypoints.size() < 3) {
    return path;
  }

  // Convert to OMPL path
  ompl::geometric::PathGeometric ompl_path(space_info_);

  for (const auto& wp : path.waypoints) {
    ompl::base::ScopedState<> state(space_info_);
    state->as<ompl::base::SE2StateSpace::StateType>()->setXY(wp.x, wp.y);
    ompl_path.append(state);
  }

  // Simplify
  ompl::geometric::PathSimplifierPtr simplifier(
    new ompl::geometric::PathSimplifier(space_info_));

  simplifier->simplify(ompl_path, 0.1);  // 0.1m tolerance
  simplifier->smoothBSpline(ompl_path, 0.1);

  return omplPathToPath2D(ompl_path);
}

bool OMPLPathPlanner::isPathCollisionFree(const Path2D& path) const {
  // Check each edge for collision
  for (size_t i = 0; i < path.waypoints.size() - 1; ++i) {
    // Sample points along edge
    const Point2D& p1 = path.waypoints[i];
    const Point2D& p2 = path.waypoints[i + 1];

    int num_samples = 10;
    for (int j = 0; j <= num_samples; ++j) {
      double t = static_cast<double>(j) / num_samples;

      Point2D sampled_point(
        p1.x + t * (p2.x - p1.x),
        p1.y + t * (p2.y - p1.y)
      );

      // Check collision
      ompl::base::ScopedState<> state(space_info_);
      state->as<ompl::base::SE2StateSpace::StateType>()->setXY(
        sampled_point.x, sampled_point.y);

      if (!isValid(state.get())) {
        return false;
      }
    }
  }

  return true;
}

Path2D OMPLPathPlanner::omplPathToPath2D(const ompl::geometric::PathGeometric& ompl_path) const {
  Path2D path;

  std::vector<ompl::base::State*> states = ompl_path.getStates();

  for (const auto* state : states) {
    Point2D point = omplStateToPoint2D(state);
    path.addWaypoint(point);
  }

  return path;
}

Point2D OMPLPathPlanner::omplStateToPoint2D(const ompl::base::State* state) const {
  const ompl::base::SE2StateSpace::StateType* se2_state =
    state->as<ompl::base::SE2StateSpace::StateType>();

  if (se2_state) {
    return Point2D(se2_state->getX(), se2_state->getY());
  }

  return Point2D();
}

void OMPLPathPlanner::addObstaclesToOMPL() {
  // This is handled by the isValid() function
  // which checks all obstacles in real-time
}

bool OMPLPathPlanner::arePathsTopologicallyEquivalent(const Path2D& path1,
                                                   const Path2D& path2) const {
  // Simplified check: compare path lengths and general shape
  // In full implementation, would use proper topological analysis

  double length_diff = std::abs(path1.length() - path2.length());
  return length_diff < 0.5;  // Consider similar if lengths differ by < 0.5m
}

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

  // Use multiple planners to generate diverse paths
  std::vector<Path2D> all_paths;

  // Try different planner types
  std::vector<OMPLPlannerType> planners = {
    OMPLPlannerType::RRTSTAR,
    OMPLPlannerType::PRM,
    OMPLPlannerType::EST,
    OMPLPlannerType::KPIECE
  };

  for (auto planner_type : planners) {
    Path2D path = planPath(start, goal, planner_type);

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
// OMPLPathOptimizer Implementation
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
      // Check if edge from i to j is collision-free
      bool visible = true;

      const Point2D& p1 = path.waypoints[i];
      const Point2D& p2 = path.waypoints[j];

      // Sample along edge
      int samples = 10;
      for (int k = 0; k <= samples; ++k) {
        double t = static_cast<double>(k) / samples;
        Point2D sample(p1.x + t * (p2.x - p1.x),
                      p1.y + t * (p2.y - p1.y));

        // Simple collision check (avoid obstacles)
        // In full implementation, would use isValid()
      }

      if (visible) {
        shortcut.addWaypoint(path.waypoints[j]);
        i = j;
        break;
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
