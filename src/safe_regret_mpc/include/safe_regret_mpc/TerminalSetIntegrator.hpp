#ifndef TERMINAL_SET_INTEGRATOR_HPP
#define TERMINAL_SET_INTEGRATOR_HPP

#include <Eigen/Dense>
#include <vector>

namespace safe_regret_mpc {

/**
 * @brief Terminal Set Integrator for Safe-Regret MPC
 *
 * Integrates terminal set constraints ensuring recursive feasibility:
 * - Computes DR control-invariant set 𝒳_f
 * - Applies terminal constraint z_{k+N} ∈ 𝒳_f
 * - Verifies recursive feasibility (Theorem 4.5)
 * - Manages terminal cost matrix
 *
 * Key formulas:
 * - Terminal constraint: z_{k+N} ∈ 𝒳_f
 * - Control-invariant: ∀z∈𝒳_f, ∃u: f(z,u,w)∈𝒳_f
 * - Recursive feasibility: feasible at k=0 ⇒ feasible ∀k≥0
 */
class TerminalSetIntegrator {
public:
    using VectorXd = Eigen::VectorXd;
    using MatrixXd = Eigen::MatrixXd;

    /**
     * @brief Terminal set representation
     */
    struct TerminalSet {
        VectorXd center;              ///< Center of terminal set
        double radius;                ///< Radius (spherical approximation)
        VectorXd min_bounds;          ///< Minimum bounds (polytope)
        VectorXd max_bounds;          ///< Maximum bounds (polytope)
        MatrixXd cost_matrix;         ///< Terminal cost P
        bool control_invariant;       ///< DR control-invariant property
        double invariance_margin;     ///< Margin from invariance violation
    };

    /**
     * @brief Computation method
     */
    enum class ComputationMethod {
        BACKWARD_REACHABLE,           ///< Backward reachable set iteration
        LYAPUNOV,                     ///< Lyapunov function level set
        LQR                           ///< LQR invariant set
    };

    /**
     * @brief Constructor
     */
    TerminalSetIntegrator();

    /**
     * @brief Destructor
     */
    ~TerminalSetIntegrator();

    /**
     * @brief Initialize terminal set integrator
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Compute terminal set
     * @param dr_margin DR constraint tightening margin
     * @param method Computation method
     * @return Terminal set 𝒳_f
     */
    TerminalSet computeTerminalSet(double dr_margin,
                                   ComputationMethod method = ComputationMethod::LQR);

    /**
     * @brief Set terminal set
     * @param center Terminal set center
     * @param radius Terminal set radius
     */
    void setTerminalSet(const VectorXd& center, double radius);

    /**
     * @brief Get current terminal set
     * @return Current terminal set
     */
    const TerminalSet& getTerminalSet() const { return terminal_set_; }

    /**
     * @brief Check terminal feasibility
     * @param state State to check
     * @return True if state ∈ 𝒳_f
     */
    bool checkFeasibility(const VectorXd& state) const;

    /**
     * @brief Verify recursive feasibility
     * @param terminal_set Terminal set to verify
     * @return True if Theorem 4.5 conditions satisfied
     */
    bool verifyRecursiveFeasibility(const TerminalSet& terminal_set) const;

    /**
     * @brief Compute terminal cost matrix
     * @param Q State cost weight
     * @param R Input cost weight
     * @return Terminal cost matrix P
     */
    MatrixXd computeTerminalCost(const MatrixXd& Q, const MatrixXd& R);

    /**
     * @brief Set system dynamics for computation
     * @param A State matrix
     * @param B Input matrix
     */
    void setSystemDynamics(const MatrixXd& A, const MatrixXd& B);

    /**
     * @brief Get terminal cost matrix
     * @return Terminal cost P
     */
    MatrixXd getTerminalCost() const { return terminal_set_.cost_matrix; }

    /**
     * @brief Enable/disable terminal set
     * @param enable Enable flag
     */
    void enable(bool enable) { enabled_ = enable; }

    /**
     * @brief Check if enabled
     * @return Enabled status
     */
    bool isEnabled() const { return enabled_; }

    /**
     * @brief Get violation count
     * @return Number of feasibility violations
     */
    int getViolationCount() const { return violation_count_; }

    /**
     * @brief Reset violation counter
     */
    void resetViolations() { violation_count_ = 0; }

private:
    /**
     * @brief Compute LQR terminal set
     * @param dr_margin DR margin
     * @return Terminal set
     */
    TerminalSet computeLQRTerminalSet(double dr_margin);

    /**
     * @brief Compute backward reachable set
     * @param safe_set Safe set to start from
     * @param dr_margin DR margin
     * @return Control-invariant set
     */
    TerminalSet computeBackwardReachableSet(const TerminalSet& safe_set,
                                            double dr_margin);

    /**
     * @brief Solve Riccati equation for terminal cost
     * @param Q State cost
     * @param R Input cost
     * @return Terminal cost P
     */
    MatrixXd solveRiccati(const MatrixXd& Q, const MatrixXd& R);

    /**
     * @brief Compute distance to terminal set
     * @param state State to check
     * @return Distance (0 if inside)
     */
    double computeDistance(const VectorXd& state) const;

    // Member variables
    TerminalSet terminal_set_;
    MatrixXd A_, B_;                ///< System dynamics
    bool enabled_;
    int violation_count_;
    bool initialized_;
    ComputationMethod last_method_;
};

} // namespace safe_regret_mpc

#endif // TERMINAL_SET_INTEGRATOR_HPP
