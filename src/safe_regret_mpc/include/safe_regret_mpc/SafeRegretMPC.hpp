#ifndef SAFE_REGRET_MPC_HPP
#define SAFE_REGRET_MPC_HPP

#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <cppad/cppad.hpp>

#include "safe_regret_mpc/STLConstraintIntegrator.hpp"
#include "safe_regret_mpc/DRConstraintIntegrator.hpp"
#include "safe_regret_mpc/TerminalSetIntegrator.hpp"
#include "safe_regret_mpc/RegretTracker.hpp"
#include "safe_regret_mpc/PerformanceMonitor.hpp"

namespace safe_regret_mpc {

/**
 * @brief Unified Safe-Regret MPC solver
 *
 * Integrates:
 * - STL monitoring and robustness optimization
 * - Distributionally robust chance constraints
 * - Tube MPC with error feedback
 * - Terminal set constraints
 * - Regret tracking and analysis
 *
 * Based on Problem 4.5 from the Safe-Regret MPC paper:
 *
 * min  E[Σ ℓ(x_t,u_t)] - λ·ρ̃^soft(φ; x_{k:k+N})
 * s.t. x_{t+1} = f̄(x_t, u_t) + Gω_t           (dynamics)
 *      x_t = z_t + e_t                          (tube decomposition)
 *      h(z_t) ≥ σ_{k,t} + η_ℰ                   (DR constraint)
 *      z_{k+N} ∈ 𝒳_f                            (terminal set)
 *      R_{k+1} = max{0, R_k + ρ̃_k - r̲}        (robustness budget)
 */
class SafeRegretMPC {
public:
    // Type definitions
    using VectorXd = Eigen::VectorXd;
    using MatrixXd = Eigen::MatrixXd;
    using ADvector = CppAD::vector<CppAD::AD<double>>;
    using SizeVector = std::vector<size_t>;

    /**
     * @brief Constructor
     */
    SafeRegretMPC();

    /**
     * @brief Destructor
     */
    ~SafeRegretMPC();

    /**
     * @brief Initialize the MPC solver
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Solve the MPC optimization problem
     * @param current_state Current system state [x, y, θ, v, cte, e_θ]
     * @param reference_trajectory Reference trajectory to follow
     * @return True if optimization succeeded
     */
    bool solve(const VectorXd& current_state,
               const std::vector<VectorXd>& reference_trajectory);

    /**
     * @brief Get optimal control input
     * @return First control input [v, ω]
     */
    VectorXd getOptimalControl() const;

    /**
     * @brief Get predicted trajectory
     * @return Predicted states for horizon
     */
    std::vector<VectorXd> getPredictedTrajectory() const;

    /**
     * @brief Get current cost value
     * @return Objective function value
     */
    double getCostValue() const;

    /**
     * @brief Check if MPC solution is feasible
     * @return Feasibility status
     */
    bool isFeasible() const { return mpc_feasible_; }

    /**
     * @brief Get solve time
     * @return Solve time in milliseconds
     */
    double getSolveTime() const { return solve_time_; }

    // ========== STL Integration ==========

    /**
     * @brief Set STL specification
     * @param formula STL formula string
     * @param baseline Robustness baseline r̲
     * @param weight Weight λ in cost function
     */
    void setSTLSpecification(const std::string& formula,
                             double baseline,
                             double weight);

    /**
     * @brief Update STL robustness
     * @param belief Current belief distribution
     * @return Current robustness value
     */
    double updateSTLRobustness(const VectorXd& belief_mean,
                               const MatrixXd& belief_cov);

    /**
     * @brief Get STL robustness
     * @return Current robustness value
     */
    double getSTLRobustness() const;

    /**
     * @brief Get STL budget
     * @return Current budget R_k
     */
    double getSTLBudget() const;

    // ========== DR Integration ==========

    /**
     * @brief Update DR margins
     * @param residuals Recent tracking residuals
     */
    void updateDRMargins(const std::vector<VectorXd>& residuals);

    /**
     * @brief Get DR margins
     * @return Margins for each time step
     */
    std::vector<double> getDRMargins() const;

    /**
     * @brief Set risk level
     * @param delta Risk level δ
     */
    void setRiskLevel(double delta);

    // ========== Terminal Set Integration ==========

    /**
     * @brief Set terminal set
     * @param center Terminal set center
     * @param radius Terminal set radius
     */
    void setTerminalSet(const VectorXd& center, double radius);

    /**
     * @brief Check terminal feasibility
     * @param terminal_state Terminal state to check
     * @return True if terminal state is feasible
     */
    bool checkTerminalFeasibility(const VectorXd& terminal_state) const;

    // ========== Regret Tracking ==========

    /**
     * @brief Update regret metrics
     * @param reference_cost Reference planner cost
     * @param actual_cost Actual cost incurred
     */
    void updateRegret(double reference_cost, double actual_cost);

    /**
     * @brief Get cumulative regret
     * @return R_T^dyn
     */
    double getCumulativeRegret() const;

    /**
     * @brief Get safe regret
     * @return R_T^safe
     */
    double getSafeRegret() const;

    /**
     * @brief Reset regret tracker
     */
    void resetRegret();

    // ========== Performance Monitoring ==========

    /**
     * @brief Get performance metrics
     * @return Performance metrics structure
     */
    PerformanceMonitor::PerformanceMetrics getPerformanceMetrics() const;

    /**
     * @brief Update performance monitoring
     */
    void updatePerformanceMonitoring();

    // ========== Parameters ==========

    /**
     * @brief Set MPC horizon
     * @param N Prediction horizon
     */
    void setHorizon(size_t N);

    /**
     * @brief Set system dynamics
     * @param A State matrix
     * @param B Input matrix
     * @param G Disturbance matrix
     */
    void setSystemDynamics(const MatrixXd& A, const MatrixXd& B, const MatrixXd& G);

    /**
     * @brief Set cost weights
     * @param Q State cost weight
     * @param R Input cost weight
     */
    void setCostWeights(const MatrixXd& Q, const MatrixXd& R);

    /**
     * @brief Set state/input constraints
     * @param x_min Minimum state bounds
     * @param x_max Maximum state bounds
     * @param u_min Minimum input bounds
     * @param u_max Maximum input bounds
     */
    void setConstraints(const VectorXd& x_min, const VectorXd& x_max,
                        const VectorXd& u_min, const VectorXd& u_max);

    // ========== Configuration ==========

    /**
     * @brief Enable/disable STL constraints
     * @param enable Enable flag
     */
    void enableSTLConstraints(bool enable) { stl_enabled_ = enable; }

    /**
     * @brief Enable/disable DR constraints
     * @param enable Enable flag
     */
    void enableDRConstraints(bool enable) { dr_enabled_ = enable; }

    /**
     * @brief Enable/disable terminal set
     * @param enable Enable flag
     */
    void enableTerminalSet(bool enable) { terminal_set_enabled_ = enable; }

    /**
     * @brief Get debug information
     * @return Debug string
     */
    std::string getDebugInfo() const;

private:
    // ========== Internal Methods ==========

    // Friend class for Ipopt TNLP solver
    friend class SafeRegretMPCTNLP;

    /**
     * @brief Initialize CppAD automatic differentiation
     */
    void initializeCppAD();

    /**
     * @brief Build MPC optimization problem
     */
    void buildOptimizationProblem();

    /**
     * @brief Evaluate objective function
     * @param vars Optimization variables
     * @param fg Objective and constraint values
     */
    void evaluateObjective(const ADvector& vars, ADvector& fg);

    /**
     * @brief Evaluate constraints
     * @param vars Optimization variables
     * @param fg Objective and constraint values
     */
    void evaluateConstraints(const ADvector& vars, ADvector& fg);

    /**
     * @brief Solve using Ipopt
     * @param vars Initial guess and solution output
     * @return Success status
     */
    bool solveWithIpopt(std::vector<double>& vars);

    // ========== Member Variables ==========

    // System parameters
    size_t state_dim_;              ///< State dimension (6)
    size_t input_dim_;              ///< Input dimension (2)
    size_t mpc_steps_;              ///< MPC horizon N
    double ref_vel_;                ///< Reference velocity
    VectorXd x_min_, x_max_;        ///< State bounds
    VectorXd u_min_, u_max_;        ///< Input bounds
    MatrixXd A_, B_, G_;            ///< System matrices
    MatrixXd Q_, R_;                ///< Cost weights

    // STL integration
    std::unique_ptr<STLConstraintIntegrator> stl_integrator_;
    bool stl_enabled_;
    double stl_weight_;
    double stl_robustness_;
    double stl_budget_;
    double stl_baseline_;

    // DR integration
    std::unique_ptr<DRConstraintIntegrator> dr_integrator_;
    bool dr_enabled_;
    double dr_delta_;
    std::vector<double> dr_margins_;

    // Terminal set
    std::unique_ptr<TerminalSetIntegrator> terminal_integrator_;
    bool terminal_set_enabled_;
    VectorXd terminal_center_;
    double terminal_radius_;

    // Regret tracking
    std::unique_ptr<RegretTracker> regret_tracker_;

    // Performance monitoring
    std::unique_ptr<PerformanceMonitor> perf_monitor_;

    // Solution
    VectorXd optimal_control_;
    std::vector<VectorXd> predicted_trajectory_;
    double cost_value_;
    bool mpc_feasible_;
    double solve_time_;

    // CppAD variables
    bool cppad_initialized_;
    SizeVector nx_;               ///< Number of variables
    SizeVector ng_;               ///< Number of constraints
    SizeVector n_sample_;         ///< Number of sample points

    // Debug
    bool debug_mode_;
    std::string debug_info_;
};

} // namespace safe_regret_mpc

#endif // SAFE_REGRET_MPC_HPP
