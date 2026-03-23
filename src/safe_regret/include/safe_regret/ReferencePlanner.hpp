/**
 * @file ReferencePlanner.hpp
 * @brief Reference trajectory planner with no-regret guarantees
 *
 * Implements abstract layer planner that:
 * - Generates reference trajectories {z_k^ref, v_k^ref}
 * - Achieves o(T) regret relative to safety-feasible policy class
 * - Ensures references are tube-admissible (feasible for Tube MPC)
 *
 * Based on manuscript Theorem 4.8 (Regret Transfer)
 */

#pragma once

#include "safe_regret/RegretMetrics.hpp"
#include "safe_regret/RegretAnalyzer.hpp"

#ifdef HAVE_OMPL
#include "safe_regret/OMPLPathPlanner.hpp"
#endif

#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <random>

namespace safe_regret {

/**
 * @brief Belief state for uncertainty representation
 */
struct BeliefState {
  Eigen::VectorXd mean;          ///< μ: mean state
  Eigen::MatrixXd covariance;    ///< Σ: covariance matrix
  std::vector<Eigen::VectorXd> particles;  ///< Particle set (for non-Gaussian)

  BeliefState(int dim = 0)
    : mean(dim), covariance(dim, dim) {
    covariance.setIdentity();
  }

  /**
   * @brief Get belief entropy (information content)
   */
  double getEntropy() const;

  /**
   * @brief Sample from belief
   */
  Eigen::VectorXd sample(std::mt19937& rng) const;
};

/**
 * @brief STL specification for reference planning
 */
struct STLSpecification {
  std::string formula;           ///< STL formula string
  double time_horizon;           ///< Task horizon
  std::vector<std::string> predicates;  ///< Atomic predicates

  // Simple reachability specification: F_[0,T] (Goal)
  static STLSpecification createReachability(
    const std::string& goal_region,
    double deadline);

  // Safety specification: G_[0,T] (Safe)
  static STLSpecification createSafety(
    const std::string& safe_set,
    double duration);
};

/**
 * @brief No-regret learning algorithms
 */
enum class NoRegretAlgorithm {
  ONLINE_MIRROR_DESCENT,  ///< OMD
  FOLLOW_THE_REGULARIZED_LEADER,  ///< FTRL
  MULTIPlicative_WEIGHTS   ///< MW
};

/**
 * @brief Reference trajectory planner with no-regret guarantees
 *
 * Generates reference sequences {z_k^ref, v_k^ref} such that:
 * R_T^ref = o(T) relative to safety-feasible abstract comparator
 */
class ReferencePlanner {
public:
  /**
   * @brief Constructor
   * @param state_dim State dimension
   * @param input_dim Input dimension
   * @param algorithm No-regret algorithm
   */
  ReferencePlanner(
    int state_dim,
    int input_dim,
    NoRegretAlgorithm algorithm = NoRegretAlgorithm::ONLINE_MIRROR_DESCENT);

  /**
   * @brief Plan reference trajectory from current belief
   *
   * Solves abstract optimization:
   * min J_ref(b_k) subject to STL and safety constraints
   *
   * @param belief Current belief state
   * @param stl_spec STL specification
   * @param horizon Planning horizon
   * @param oracle Optional oracle for regret comparison
   * @return Reference trajectory
   */
  ReferenceTrajectory planReference(
    const BeliefState& belief,
    const STLSpecification& stl_spec,
    int horizon,
    const OracleComparator* oracle = nullptr);

  /**
   * @brief Update learning algorithm with executed cost
   *
   * Updates no-regret algorithm (e.g., OMD gradient step)
   *
   * @param executed_trajectory Actually executed trajectory
   * @param reference_cost Cost of reference trajectory
   */
  void updateLearning(
    const ReferenceTrajectory& executed_trajectory,
    double reference_cost);

  /**
   * @brief Compute abstract regret R_T^ref
   *
   * R_T^ref = Σ[ℓ(z_k^ref, v_k^ref) - ℓ(z_k^⋄, v_k^⋄)]
   *
   * @param oracle Abstract comparator (oracle trajectory)
   * @return Abstract regret
   */
  double computeAbstractRegret(const OracleComparator& oracle) const;

  /**
   * @brief Check if reference is tube-feasible
   *
   * Verifies Theorem 4.8 conditions:
   * - Curvature bound: κ(z_k) ≤ κ_max
   * - Velocity bound: ‖v_k‖ ≤ v_max
   * - Tube admissibility: z_k ∈ 𝒳 ⊖ ℰ
   *
   * @param reference Reference trajectory to check
   * @param v_max Max velocity
   * @param kappa_max Max curvature
   * @param tube_radius Tube error radius ē
   * @return true if feasible
   */
  bool isTubeFeasible(
    const ReferenceTrajectory& reference,
    double v_max,
    double kappa_max,
    double tube_radius) const;

  /**
   * @brief Set learning rate for no-regret algorithm
   */
  void setLearningRate(double eta) { learning_rate_ = eta; }

  /**
   * @brief Get current regret statistics
   */
  RegretMetrics getRegretMetrics() const;

  /**
   * @brief Reset planner state
   */
  void reset();

#ifdef HAVE_OMPL
  /**
   * @brief Set workspace bounds for OMPL planning
   * @param bounds [xmin, xmax, ymin, ymax]
   */
  void setWorkspaceBounds(const Eigen::VectorXd& bounds);

  /**
   * @brief Add obstacle for OMPL planning
   * @param obstacle Circular obstacle
   */
  void addObstacle(const Obstacle2D& obstacle);

  /**
   * @brief Enable/disable OMPL planning
   * @param enable true to use OMPL, false for simple geometric planning
   */
  void setOMPLEnabled(bool enable) { use_ompl_ = enable; }

  /**
   * @brief Check if OMPL is available
   */
  bool isOMPLAvailable() const { return use_ompl_ && ompl_planner_ != nullptr; }
#endif

private:
  // Dimensions
  int state_dim_;
  int input_dim_;

  // Learning algorithm
  NoRegretAlgorithm algorithm_;
  double learning_rate_;
  std::vector<double> strategy_weights_;  ///< For multiplicative weights

  // History
  std::vector<ReferenceTrajectory> reference_history_;
  std::vector<double> cost_history_;

#ifdef HAVE_OMPL
  // OMPL planner (for topological path planning)
  std::unique_ptr<OMPLTopologicalPlanner> ompl_planner_;
  Eigen::VectorXd workspace_bounds_;  ///< [xmin, xmax, ymin, ymax]
  std::vector<Obstacle2D> obstacles_;  ///< Known obstacles
  bool use_ompl_;  ///< Enable/disable OMPL planning
#endif

  // Helper functions

  // Helper functions
  /**
   * @brief Solve abstract planning problem
   *
   * Implements belief-space optimization with STL robustness
   */
  ReferenceTrajectory solveAbstractPlanning(
    const BeliefState& belief,
    const STLSpecification& stl_spec,
    int horizon) const;

#ifdef HAVE_OMPL
  /**
   * @brief Solve abstract planning using OMPL (internal)
   */
  ReferenceTrajectory solveAbstractPlanningWithOMPL(
    const BeliefState& belief,
    const STLSpecification& stl_spec,
    int horizon) const;
#endif

  /**
   * @brief Online Mirror Descent (OMD) update
   *
   * θ_{k+1} = Π_Θ(θ_k - η·∇ℓ(θ_k))
   */
  void updateOMD(const ReferenceTrajectory& reference, double cost);

  /**
   * @brief Follow-The-Regularized-Leader (FTRL) update
   *
   * θ_{k+1} = argmin_θ (η·ℓ(θ) + R(θ))
   */
  void updateFTRL(const ReferenceTrajectory& reference, double cost);

  /**
   * @brief Compute cost of reference trajectory
   *
   * J_ref = E[Σ ℓ(z_t, v_t) - λ·ρ(φ; z_{k:k+N})]
   */
  double computeReferenceCost(
    const ReferenceTrajectory& reference,
    const BeliefState& belief,
    const STLSpecification& stl_spec) const;

  /**
   * @brief Compute curvature at a point
   *
   * For 2D navigation: κ = |x'y'' - y'x''| / (x'^2 + y'^2)^(3/2)
   */
  double computeCurvature(
    const Eigen::VectorXd& state_prev,
    const Eigen::VectorXd& state_curr,
    const Eigen::VectorXd& state_next) const;

  /**
   * @brief Project to feasible set (for OMD projection)
   */
  ReferenceTrajectory projectToFeasible(
    const ReferenceTrajectory& reference,
    double v_max,
    double kappa_max) const;

#ifdef HAVE_OMPL
  /**
   * @brief Convert Path2D to ReferenceTrajectory
   *
   * Generates state and input sequences from geometric path
   */
  ReferenceTrajectory path2DToReferenceTrajectory(
    const Path2D& path,
    const BeliefState& belief,
    int horizon) const;
#endif
};

/**
 * @brief Feasibility checker for reference trajectories
 *
 * Ensures references satisfy Theorem 4.8 conditions
 */
class FeasibilityChecker {
public:
  /**
   * @brief Check all feasibility constraints
   *
   * @param reference Reference trajectory
   * @param v_max Max velocity constraint
   * @param kappa_max Max curvature constraint
   * @param tube_radius Tube radius ē (for 𝒳 ⊖ ℰ)
   * @param state_bounds State bounds 𝒳
   * @param input_bounds Input bounds 𝒰
   * @return Pair (is_feasible, violation_reason)
   */
  std::pair<bool, std::string> checkFeasibility(
    const ReferenceTrajectory& reference,
    double v_max,
    double kappa_max,
    double tube_radius,
    const Eigen::VectorXd& state_lower,
    const Eigen::VectorXd& state_upper,
    const Eigen::VectorXd& input_lower,
    const Eigen::VectorXd& input_upper) const;

  /**
   * @brief Sanitize infeasible trajectory
   *
   * Projects infeasible trajectory onto feasible set
   *
   * @param reference Infeasible reference
   * @param v_max Max velocity
   * @param kappa_max Max curvature
   * @param tube_radius Tube radius
   * @return Feasible reference trajectory
   */
  ReferenceTrajectory sanitizeInfeasible(
    const ReferenceTrajectory& reference,
    double v_max,
    double kappa_max,
    double tube_radius) const;

private:
  /**
   * @brief Check velocity constraint: ‖v_k‖ ≤ v_max
   */
  bool checkVelocityBound(
    const ReferenceTrajectory& reference,
    double v_max) const;

  /**
   * @brief Check curvature constraint: κ(z_k) ≤ κ_max
   */
  bool checkCurvatureBound(
    const ReferenceTrajectory& reference,
    double kappa_max) const;

  /**
   * @brief Check tube admissibility: z_k ∈ 𝒳 ⊖ ℰ
   */
  bool checkTubeAdmissibility(
    const ReferenceTrajectory& reference,
    double tube_radius,
    const Eigen::VectorXd& state_lower,
    const Eigen::VectorXd& state_upper) const;

  /**
   * @brief Smooth trajectory to reduce curvature
   */
  ReferenceTrajectory smoothTrajectory(
    const ReferenceTrajectory& reference,
    double kappa_max) const;
};

} // namespace safe_regret
