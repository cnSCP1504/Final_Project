/**
 * @file OMPLPathPlanner.hpp
 * @brief OMPL-based path planner for Safe-Regret MPC
 *
 * Integrates OMPL (Open Motion Planning Library) for topological path planning.
 * Supports multiple planning algorithms and topological reasoning.
 *
 * Note: If OMPL is not available, fallback implementations using simple
 * geometric algorithms are provided.
 */

#pragma once

#include "safe_regret/TopologicalAbstraction.hpp"
#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <map>
#include <string>

#ifdef HAVE_OMPL
#include <ompl/base/SpaceInformation.h>
#include <ompl/base/spaces/SE2StateSpace.h>
#include <ompl/geometric/SimpleSetup.h>
#include <ompl/geometric/planners/rrt/RRT.h>
#include <ompl/geometric/planners/rrt/RRTstar.h>
#include <ompl/geometric/planners/prm/PRM.h>
#include <ompl/geometric/planners/est/EST.h>
#include <ompl/geometric/planners/pdst/PDST.h>
#include <ompl/base/objectives/PathLengthOptimizationObjective.h>
#endif

namespace safe_regret {

/**
 * @brief Planning algorithm types supported by OMPL
 */
enum class OMPLPlannerType {
  RRT,           // Rapidly-exploring Random Tree
  RRTSTAR,       // RRT* (optimal)
  PRM,           // Probabilistic Roadmap
  EST,           // Expansive Space Trees
  PDST,          // PDST (PDST-like)
  KPIECE,       // KPIECE (LazyPRM*)
  LAZYPRM,       // LazyPRM*
  SPARS,         // SPARS (Sparse Roadmap)
  STRLIE        // Straight-line
};

/**
 * @brief OMPL-based path planner
 *
 * Wraps OMPL functionality for use in Safe-Regret MPC framework
 */
class OMPLPathPlanner {
public:
  /**
   * @brief Constructor
   * @param workspace_bounds Workspace bounds [xmin, xmax, ymin, ymax]
   * @param planning_time Maximum planning time (seconds)
   */
  OMPLPathPlanner(const Eigen::VectorXd& workspace_bounds,
                   double planning_time = 1.0);

  /**
   * @brief Destructor
   */
  ~OMPLPathPlanner();

  /**
   * @brief Add obstacle to the environment
   * @param obstacle Circular obstacle
   */
  void addObstacle(const Obstacle2D& obstacle);

  /**
   * @brief Add polygon obstacle
   * @param vertices Polygon vertices
   */
  void addPolygonObstacle(const std::vector<Point2D>& vertices);

  /**
   * @brief Clear all obstacles
   */
  void clearObstacles();

  /**
   * @brief Plan single path from start to goal
   * @param start Start position
   * @param goal Goal position
   * @param planner_type Planning algorithm to use
   * @return Planned path (empty if failed)
   */
  Path2D planPath(const Point2D& start,
                  const Point2D& goal,
                  OMPLPlannerType planner_type = OMPLPlannerType::RRTSTAR);

  /**
   * @brief Plan multiple topologically distinct paths
   * @param start Start position
   * @param goal Goal position
   * @param planner_type Planning algorithm
   * @param num_paths Number of distinct paths to generate
   * @return Vector of topologically distinct paths
   */
  std::vector<Path2D> planMultiplePaths(
    const Point2D& start,
    const Point2D& goal,
    OMPLPlannerType planner_type = OMPLPlannerType::RRTSTAR,
    int num_paths = 5);

  /**
   * @brief Simplify a path
   * @param path Path to simplify
   * @return Simplified path
   */
  Path2D simplifyPath(const Path2D& path);

  /**
   * @brief Check if path is collision-free
   * @param path Path to check
   * @return true if collision-free
   */
  bool isPathCollisionFree(const Path2D& path) const;

  /**
   * @brief Set planning time limit
   * @param time Time limit in seconds
   */
  void setPlanningTime(double time) { planning_time_ = time; }

#ifdef HAVE_OMPL
  /**
   * @brief Get OMPL simple setup for custom planning (OMPL only)
   */
  ompl::geometric::SimpleSetupPtr getSimpleSetup() const { return simple_setup_; }

  /**
   * @brief Get state space (OMPL only)
   */
  ompl::base::StateSpacePtr getStateSpace() const { return state_space_; }
#endif

private:
  // Parameters
  Eigen::VectorXd workspace_bounds_;
  double planning_time_;

  // Obstacles
  std::vector<Obstacle2D> circular_obstacles_;
  std::vector<std::vector<Point2D>> polygon_obstacles_;

#ifdef HAVE_OMPL
  // OMPL components
  ompl::base::StateSpacePtr state_space_;
  ompl::geometric::SimpleSetupPtr simple_setup_;
  ompl::base::SpaceInformationPtr space_info_;

  // OMPL-specific internal methods
  void createSimpleSetup();
  bool isValid(const ompl::base::State* state) const;
  ompl::base::PlannerPtr createPlanner(OMPLPlannerType type);
  Path2D omplPathToPath2D(const ompl::geometric::PathGeometric& ompl_path) const;
  Point2D omplStateToPoint2D(const ompl::base::State* state) const;
  void addObstaclesToOMPL();
#else
  // Fallback geometric planning methods
  Path2D planGeometricPath(const Point2D& start, const Point2D& goal);
  std::vector<Path2D> planMultipleGeometricPaths(const Point2D& start,
                                                 const Point2D& goal,
                                                 int num_paths);
  bool arePathsTopologicallyEquivalent(const Path2D& path1,
                                      const Path2D& path2) const;
#endif
};

/**
 * @brief OMPL-based topological planner
 *
 * Specializes in generating topologically distinct paths
 */
class OMPLTopologicalPlanner : public OMPLPathPlanner {
public:
  OMPLTopologicalPlanner(const Eigen::VectorXd& workspace_bounds,
                        double planning_time = 2.0)
    : OMPLPathPlanner(workspace_bounds, planning_time) {}

  /**
   * @brief Generate homology-informed paths
   *
   * Uses OMPL's topological properties to generate paths that
   * are topologically distinct
   *
   * @param start Start position
   * @param goal Goal position
   * @param num_homology_classes Number of homology classes to generate
   * @return Paths from different homology classes
   */
  std::vector<Path2D> generateHomologyPaths(
    const Point2D& start,
    const Point2D& goal,
    int num_homology_classes = 3);

private:
  /**
   * @brief Compute path signature for homology classification
   */
  std::string computePathSignature(const Path2D& path) const;

  /**
   * @brief Check if two paths are topologically equivalent
   */
  bool areTopologicallyEquivalent(const Path2D& path1,
                                   const Path2D& path2) const;
};

/**
 * @brief OMPL path optimizer
 *
 * Optimizes paths for quality and smoothness
 */
class OMPLPathOptimizer {
public:
  /**
   * @brief Optimize path for smoothness
   * @param path Input path
   * @return Optimized path
   */
  static Path2D optimizePath(const Path2D& path);

  /**
   * @brief Shortcut path
   * @param path Input path
   * @return Shortcutted path
   */
  static Path2D shortcutPath(const Path2D& path);

  /**
   * @brief Smooth path using spline interpolation
   * @param path Input path
   * @param resolution Output resolution
   * @return Smoothed path
   */
  static Path2D smoothPath(const Path2D& path, double resolution = 0.1);

private:
  static double computeCurvature(const Point2D& p1,
                                 const Point2D& p2,
                                 const Point2D& p3);
};

} // namespace safe_regret
