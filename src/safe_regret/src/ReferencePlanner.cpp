/**
 * @file ReferencePlanner.cpp
 * @brief Implementation of reference trajectory planner
 */

#include "safe_regret/ReferencePlanner.hpp"
#include "safe_regret/TopologicalAbstraction.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace safe_regret {

// ============================================================================
// BeliefState Implementation
// ============================================================================

double BeliefState::getEntropy() const {
  // Gaussian entropy: H = 0.5·log((2πe)^n·det(Σ))
  if (covariance.rows() == 0) {
    return 0.0;
  }

  Eigen::LLT<Eigen::MatrixXd> chol(covariance);
  if (chol.info() != Eigen::Success) {
    return std::numeric_limits<double>::infinity();
  }

  double log_det = 2.0 * chol.matrixL().toDenseMatrix().diagonal().array().log().sum();
  int n = covariance.rows();

  return 0.5 * (n * std::log(2.0 * M_PI * M_E) + log_det);
}

Eigen::VectorXd BeliefState::sample(std::mt19937& rng) const {
  std::normal_distribution<double> dist(0.0, 1.0);

  if (!particles.empty()) {
    // Sample from particle set
    std::uniform_int_distribution<int> idx_dist(0, particles.size() - 1);
    return particles[idx_dist(rng)];
  }

  // Sample from Gaussian: N(μ, Σ)
  // Use Cholesky decomposition: x = μ + L·z, where z ~ N(0, I)
  Eigen::LLT<Eigen::MatrixXd> chol(covariance);

  if (chol.info() != Eigen::Success) {
    // Fallback: return mean
    return mean;
  }

  Eigen::VectorXd z(mean.size());
  for (int i = 0; i < mean.size(); ++i) {
    z[i] = dist(rng);
  }

  return mean + chol.matrixL() * z;
}

// ============================================================================
// STLSpecification Implementation
// ============================================================================

STLSpecification STLSpecification::createReachability(
  const std::string& goal_region,
  double deadline) {

  STLSpecification spec;
  spec.formula = "F_[0," + std::to_string(deadline) + "] (" + goal_region + ")";
  spec.time_horizon = deadline;
  spec.predicates = {goal_region};

  return spec;
}

STLSpecification STLSpecification::createSafety(
  const std::string& safe_set,
  double duration) {

  STLSpecification spec;
  spec.formula = "G_[0," + std::to_string(duration) + "] (" + safe_set + ")";
  spec.time_horizon = duration;
  spec.predicates = {safe_set};

  return spec;
}

// ============================================================================
// ReferencePlanner Implementation
// ============================================================================

ReferencePlanner::ReferencePlanner(
  int state_dim,
  int input_dim,
  NoRegretAlgorithm algorithm)
  : state_dim_(state_dim),
    input_dim_(input_dim),
    algorithm_(algorithm),
    learning_rate_(0.1),
    strategy_weights_(state_dim, 1.0)
#ifdef HAVE_OMPL
    , use_ompl_(true),
      goal_x_(0.0),
      goal_y_(0.0),
      has_goal_(false)
#endif
    {

  // Initialize strategy weights for multiplicative weights

#ifdef HAVE_OMPL
  // Initialize default workspace bounds [-10, 10, -10, 10]
  workspace_bounds_.resize(4);
  workspace_bounds_ << -10.0, 10.0, -10.0, 10.0;

  // Create OMPL planner
  ompl_planner_ = std::unique_ptr<OMPLTopologicalPlanner>(
    new OMPLTopologicalPlanner(workspace_bounds_, 2.0)
  );
#endif
}

ReferenceTrajectory ReferencePlanner::planReference(
  const BeliefState& belief,
  const STLSpecification& stl_spec,
  int horizon,
  const OracleComparator* oracle) {

  // Solve abstract planning problem
  ReferenceTrajectory reference = solveAbstractPlanning(belief, stl_spec, horizon);

  // Store in history
  reference_history_.push_back(reference);

  return reference;
}

void ReferencePlanner::updateLearning(
  const ReferenceTrajectory& executed_trajectory,
  double reference_cost) {

  // Update based on algorithm
  switch (algorithm_) {
    case NoRegretAlgorithm::ONLINE_MIRROR_DESCENT:
      updateOMD(executed_trajectory, reference_cost);
      break;

    case NoRegretAlgorithm::FOLLOW_THE_REGULARIZED_LEADER:
      updateFTRL(executed_trajectory, reference_cost);
      break;

    case NoRegretAlgorithm::MULTIPlicative_WEIGHTS:
      // Similar to OMD but with multiplicative update
      updateOMD(executed_trajectory, reference_cost);
      break;
  }

  // Store cost history
  cost_history_.push_back(reference_cost);
}

double ReferencePlanner::computeAbstractRegret(const OracleComparator& oracle) const {
  double regret = 0.0;

  for (size_t k = 0; k < reference_history_.size(); ++k) {
    if (oracle.hasOracleAt(k)) {
      double ref_cost = 0.0;

      // Compute cost of reference trajectory at step k
      if (k < reference_history_.size()) {
        const auto& ref_traj = reference_history_[k];
        for (const auto& pt : ref_traj.points) {
          ref_cost += pt.state.squaredNorm() + pt.input.squaredNorm();
        }
      }

      double oracle_cost = oracle.getOptimalCost(k);
      regret += (ref_cost - oracle_cost);
    }
  }

  return regret;
}

bool ReferencePlanner::isTubeFeasible(
  const ReferenceTrajectory& reference,
  double v_max,
  double kappa_max,
  double tube_radius) const {

  // Check velocity bounds
  for (const auto& pt : reference.points) {
    if (pt.input.norm() > v_max) {
      return false;
    }
  }

  // Check curvature bounds
  for (size_t i = 1; i < reference.points.size() - 1; ++i) {
    double kappa = computeCurvature(
      reference.points[i-1].state,
      reference.points[i].state,
      reference.points[i+1].state);

    if (kappa > kappa_max) {
      return false;
    }
  }

  // Check tube admissibility (simplified: check state bounds)
  for (const auto& pt : reference.points) {
    if (pt.state.norm() > tube_radius) {
      return false;
    }
  }

  return true;
}

RegretMetrics ReferencePlanner::getRegretMetrics() const {
  RegretMetrics metrics;
  // TODO: Implement regret metrics computation
  return metrics;
}

void ReferencePlanner::reset() {
  reference_history_.clear();
  cost_history_.clear();
  strategy_weights_.assign(state_dim_, 1.0);

#ifdef HAVE_OMPL
  // Recreate OMPL planner
  if (use_ompl_) {
    ompl_planner_ = std::unique_ptr<OMPLTopologicalPlanner>(
      new OMPLTopologicalPlanner(workspace_bounds_, 2.0)
    );

    // Re-add obstacles
    for (const auto& obs : obstacles_) {
      ompl_planner_->addObstacle(obs);
    }
  }
#endif
}

// ============================================================================
// Goal Management (OMPL-enabled)
// ============================================================================

#ifdef HAVE_OMPL
void ReferencePlanner::setGoal(double goal_x, double goal_y) {
  goal_x_ = goal_x;
  goal_y_ = goal_y;
  has_goal_ = true;
}

void ReferencePlanner::getGoal(double& goal_x, double& goal_y) const {
  goal_x = goal_x_;
  goal_y = goal_y_;
}
#endif

ReferenceTrajectory ReferencePlanner::solveAbstractPlanning(
  const BeliefState& belief,
  const STLSpecification& stl_spec,
  int horizon) const {

#ifdef HAVE_OMPL
  if (use_ompl_ && ompl_planner_ != nullptr) {
    // Use OMPL for topological path planning
    return solveAbstractPlanningWithOMPL(belief, stl_spec, horizon);
  }
#endif

  // Fallback: simple geometric planning
  ReferenceTrajectory reference;

  // Generate straight-line trajectory towards goal
  // In practice, this would solve a belief-space optimization problem

  for (int t = 0; t < horizon; ++t) {
    ReferencePoint pt(state_dim_, input_dim_);

    // Linearly interpolate towards goal
    double alpha = static_cast<double>(t) / static_cast<double>(horizon);

    // State: move towards origin (goal)
    pt.state = (1.0 - alpha) * belief.mean;

    // Input: proportional control towards goal
    pt.input = -alpha * belief.mean;

    // Timestamp
    pt.timestamp = static_cast<double>(t);

    reference.addPoint(pt);
  }

  return reference;
}

void ReferencePlanner::updateOMD(
  const ReferenceTrajectory& reference,
  double cost) {

  // Online Mirror Descent: θ_{k+1} = Π_Θ(θ_k - η·∇ℓ(θ_k))

  // Simplified: update strategy weights
  for (size_t i = 0; i < strategy_weights_.size(); ++i) {
    // Gradient step (simplified)
    double gradient = cost / static_cast<double>(strategy_weights_.size());
    strategy_weights_[i] = std::max(0.0, strategy_weights_[i] - learning_rate_ * gradient);
  }

  // Normalize weights
  double sum = std::accumulate(strategy_weights_.begin(), strategy_weights_.end(), 0.0);
  if (sum > 1e-10) {
    for (auto& w : strategy_weights_) {
      w /= sum;
    }
  }
}

void ReferencePlanner::updateFTRL(
  const ReferenceTrajectory& reference,
  double cost) {

  // Follow-The-Regularized-Leader
  // θ_{k+1} = argmin_θ (η·ℓ(θ) + R(θ))

  // Similar to OMD but with regularization
  updateOMD(reference, cost);
}

double ReferencePlanner::computeReferenceCost(
  const ReferenceTrajectory& reference,
  const BeliefState& belief,
  const STLSpecification& stl_spec) const {

  double cost = 0.0;

  // Stage cost: E[Σ ℓ(z_t, v_t)]
  for (const auto& pt : reference.points) {
    cost += pt.state.squaredNorm() + pt.input.squaredNorm();
  }

  // STL robustness bonus (simplified)
  // In practice: -λ·ρ^soft(φ; z_{k:k+N})

  return cost;
}

double ReferencePlanner::computeCurvature(
  const Eigen::VectorXd& state_prev,
  const Eigen::VectorXd& state_curr,
  const Eigen::VectorXd& state_next) const {

  if (state_curr.size() < 2) {
    return 0.0;
  }

  // For 2D: κ = |x'y'' - y'x''| / (x'^2 + y'^2)^(3/2)

  double dx1 = state_curr[0] - state_prev[0];
  double dy1 = state_curr[1] - state_prev[1];
  double dx2 = state_next[0] - state_curr[0];
  double dy2 = state_next[1] - state_curr[1];

  double dx = 0.5 * (dx1 + dx2);
  double dy = 0.5 * (dy1 + dy2);
  double ddx = dx2 - dx1;
  double ddy = dy2 - dy1;

  double numerator = std::abs(dx * ddy - dy * ddx);
  double denominator = std::pow(dx*dx + dy*dy, 1.5);

  if (denominator < 1e-6) {
    return 0.0;
  }

  return numerator / denominator;
}

ReferenceTrajectory ReferencePlanner::projectToFeasible(
  const ReferenceTrajectory& reference,
  double v_max,
  double kappa_max) const {

  // Project infeasible trajectory onto feasible set
  // Simplified implementation: clip inputs

  ReferenceTrajectory projected = reference;

  for (auto& pt : projected.points) {
    // Clip input velocity
    if (pt.input.norm() > v_max) {
      pt.input = v_max * pt.input.normalized();
    }
  }

  // Note: curvature projection is more complex and omitted here

  return projected;
}

// ============================================================================
// FeasibilityChecker Implementation
// ============================================================================

std::pair<bool, std::string> FeasibilityChecker::checkFeasibility(
  const ReferenceTrajectory& reference,
  double v_max,
  double kappa_max,
  double tube_radius,
  const Eigen::VectorXd& state_lower,
  const Eigen::VectorXd& state_upper,
  const Eigen::VectorXd& input_lower,
  const Eigen::VectorXd& input_upper) const {

  // Check velocity
  if (!checkVelocityBound(reference, v_max)) {
    return {false, "Velocity bound violated"};
  }

  // Check curvature
  if (!checkCurvatureBound(reference, kappa_max)) {
    return {false, "Curvature bound violated"};
  }

  // Check tube admissibility
  if (!checkTubeAdmissibility(reference, tube_radius, state_lower, state_upper)) {
    return {false, "Tube admissibility violated"};
  }

  return {true, ""};
}

ReferenceTrajectory FeasibilityChecker::sanitizeInfeasible(
  const ReferenceTrajectory& reference,
  double v_max,
  double kappa_max,
  double tube_radius) const {

  // First smooth trajectory if curvature too high
  ReferenceTrajectory smoothed = smoothTrajectory(reference, kappa_max);

  // Then project onto feasible set
  ReferenceTrajectory projected = smoothed;

  for (auto& pt : projected.points) {
    // Clip input
    if (pt.input.norm() > v_max) {
      pt.input = v_max * pt.input.normalized();
    }

    // Clip state (tube admissibility)
    if (pt.state.norm() > tube_radius) {
      pt.state = tube_radius * pt.state.normalized();
    }
  }

  return projected;
}

bool FeasibilityChecker::checkVelocityBound(
  const ReferenceTrajectory& reference,
  double v_max) const {

  for (const auto& pt : reference.points) {
    if (pt.input.norm() > v_max + 1e-6) {
      return false;
    }
  }
  return true;
}

bool FeasibilityChecker::checkCurvatureBound(
  const ReferenceTrajectory& reference,
  double kappa_max) const {

  if (reference.points.size() < 3) {
    return true;
  }

  for (size_t i = 1; i < reference.points.size() - 1; ++i) {
    // Compute curvature
    Eigen::VectorXd prev = reference.points[i-1].state;
    Eigen::VectorXd curr = reference.points[i].state;
    Eigen::VectorXd next = reference.points[i+1].state;

    if (curr.size() >= 2) {
      double dx1 = curr[0] - prev[0];
      double dy1 = curr[1] - prev[1];
      double dx2 = next[0] - curr[0];
      double dy2 = next[1] - curr[1];

      double dx = 0.5 * (dx1 + dx2);
      double dy = 0.5 * (dy1 + dy2);
      double ddx = dx2 - dx1;
      double ddy = dy2 - dy1;

      double numerator = std::abs(dx * ddy - dy * ddx);
      double denominator = std::pow(dx*dx + dy*dy, 1.5);

      if (denominator > 1e-6) {
        double kappa = numerator / denominator;
        if (kappa > kappa_max + 1e-6) {
          return false;
        }
      }
    }
  }

  return true;
}

bool FeasibilityChecker::checkTubeAdmissibility(
  const ReferenceTrajectory& reference,
  double tube_radius,
  const Eigen::VectorXd& state_lower,
  const Eigen::VectorXd& state_upper) const {

  for (const auto& pt : reference.points) {
    // Check state bounds with tube margin
    for (int i = 0; i < pt.state.size(); ++i) {
      double lower = state_lower.size() > i ? state_lower[i] : -tube_radius;
      double upper = state_upper.size() > i ? state_upper[i] : tube_radius;

      if (pt.state[i] < lower + tube_radius || pt.state[i] > upper - tube_radius) {
        return false;
      }
    }
  }

  return true;
}

ReferenceTrajectory FeasibilityChecker::smoothTrajectory(
  const ReferenceTrajectory& reference,
  double kappa_max) const {

  // Simple smoothing: moving average
  ReferenceTrajectory smoothed;

  if (reference.points.size() < 3) {
    return reference;
  }

  // Keep first point
  smoothed.addPoint(reference.points[0]);

  // Smooth middle points
  for (size_t i = 1; i < reference.points.size() - 1; ++i) {
    ReferencePoint pt(reference.points[i].state.size(),
                     reference.points[i].input.size());

    // Moving average
    pt.state = 0.25 * reference.points[i-1].state +
              0.5 * reference.points[i].state +
              0.25 * reference.points[i+1].state;

    pt.input = 0.25 * reference.points[i-1].input +
              0.5 * reference.points[i].input +
              0.25 * reference.points[i+1].input;

    pt.timestamp = reference.points[i].timestamp;

    smoothed.addPoint(pt);
  }

  // Keep last point
  smoothed.addPoint(reference.points.back());

  return smoothed;
}

// ============================================================================
// OMPL Integration (if available)
// ============================================================================

#ifdef HAVE_OMPL

void ReferencePlanner::setWorkspaceBounds(const Eigen::VectorXd& bounds) {
  workspace_bounds_ = bounds;

  // Recreate OMPL planner with new bounds
  ompl_planner_ = std::unique_ptr<OMPLTopologicalPlanner>(
    new OMPLTopologicalPlanner(workspace_bounds_, 2.0)
  );

  // Re-add obstacles
  for (const auto& obs : obstacles_) {
    ompl_planner_->addObstacle(obs);
  }
}

void ReferencePlanner::addObstacle(const Obstacle2D& obstacle) {
  obstacles_.push_back(obstacle);

  if (ompl_planner_) {
    ompl_planner_->addObstacle(obstacle);
  }
}

ReferenceTrajectory ReferencePlanner::solveAbstractPlanningWithOMPL(
  const BeliefState& belief,
  const STLSpecification& stl_spec,
  int horizon) const {

  ReferenceTrajectory reference;

  if (!ompl_planner_) {
    // Fallback to simple planning
    return solveAbstractPlanning(belief, stl_spec, horizon);
  }

  // Extract 2D position from belief state (assuming first 2 elements are x, y)
  Point2D start(belief.mean[0], belief.mean[1]);

  // Use set goal if available, otherwise fallback to origin
  Point2D goal(has_goal_ ? goal_x_ : 0.0, has_goal_ ? goal_y_ : 0.0);

  // Generate multiple topological paths
  int num_alternatives = 3;
  auto topological_paths = ompl_planner_->generateHomologyPaths(
    start, goal, num_alternatives
  );

  if (topological_paths.empty()) {
    // No path found, fallback to simple planning
    std::cerr << "Warning: OMPL failed to find paths, using fallback" << std::endl;
    return solveAbstractPlanning(belief, stl_spec, horizon);
  }

  // Select best path based on cost (in practice, would consider STL robustness)
  Path2D best_path = topological_paths[0];  // Already sorted by length

  // Convert Path2D to ReferenceTrajectory
  return path2DToReferenceTrajectory(best_path, belief, horizon);
}

ReferenceTrajectory ReferencePlanner::path2DToReferenceTrajectory(
  const Path2D& path,
  const BeliefState& belief,
  int horizon) const {

  ReferenceTrajectory reference;

  if (path.waypoints.empty()) {
    return reference;
  }

  // Resample path to have exactly 'horizon' points
  for (int t = 0; t < horizon; ++t) {
    ReferencePoint pt(state_dim_, input_dim_);

    // Interpolate along path
    double alpha = static_cast<double>(t) / static_cast<double>(horizon - 1);
    size_t path_idx = static_cast<size_t>(alpha * (path.waypoints.size() - 1));
    path_idx = std::min(path_idx, path.waypoints.size() - 1);

    const Point2D& wp = path.waypoints[path_idx];

    // State: position from path, other dimensions from belief
    pt.state.resize(state_dim_);
    pt.state[0] = wp.x;
    pt.state[1] = wp.y;

    // Fill remaining state dimensions with belief mean
    for (int i = 2; i < state_dim_; ++i) {
      if (i < belief.mean.size()) {
        pt.state[i] = belief.mean[i];
      } else {
        pt.state[i] = 0.0;
      }
    }

    // Input: velocity towards next waypoint
    if (path_idx < path.waypoints.size() - 1) {
      const Point2D& next_wp = path.waypoints[path_idx + 1];
      double dx = next_wp.x - wp.x;
      double dy = next_wp.y - wp.y;
      double dist = std::sqrt(dx*dx + dy*dy);

      // Normalize velocity
      double v_norm = std::min(dist, 1.0);  // Max velocity 1.0
      pt.input.resize(input_dim_);

      if (input_dim_ >= 2) {
        pt.input[0] = (dist > 1e-6) ? v_norm * dx / dist : 0.0;
        pt.input[1] = (dist > 1e-6) ? v_norm * dy / dist : 0.0;
      }

      // Fill remaining input dimensions
      for (int i = 2; i < input_dim_; ++i) {
        pt.input[i] = 0.0;
      }
    } else {
      // Last waypoint: zero velocity
      pt.input = Eigen::VectorXd::Zero(input_dim_);
    }

    // Timestamp
    pt.timestamp = static_cast<double>(t);

    reference.addPoint(pt);
  }

  return reference;
}

#endif // HAVE_OMPL

} // namespace safe_regret
