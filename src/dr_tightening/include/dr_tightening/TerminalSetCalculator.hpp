#ifndef TERMINAL_SET_CALCULATOR_HPP
#define TERMINAL_SET_CALCULATOR_HPP

#include <Eigen/Dense>
#include <vector>
#include <memory>

namespace dr_tightening {

/**
 * @brief Terminal Set Calculator for DR Control-Invariant Sets
 *
 * Implements computation of maximum control-invariant terminal sets
 * for Distributionally Robust MPC, ensuring recursive feasibility
 * (Theorem 4.5 in the Safe-Regret MPC paper).
 *
 * Theory:
 * The terminal set 𝒳_f must satisfy:
 * 1. 𝒳_f ⊆ safety region
 * 2. For all x ∈ 𝒳_f, ∃ u such that f(x,u,w) ∈ 𝒳_f for all w ∈ 𝒲
 * 3. 𝒳_f is DR control-invariant under the tightened constraints
 *
 * This class computes:
 * - Maximum control-invariant set for linear systems
 * - Terminal cost and constraint bounds
 * - DR feasibility verification
 */
class TerminalSetCalculator {
public:
    /**
     * @brief System dynamics parameters
     */
    struct SystemDynamics {
        Eigen::MatrixXd A;  // State transition matrix
        Eigen::MatrixXd B;  // Input matrix
        Eigen::MatrixXd G;  // Disturbance matrix

        int state_dim;
        int input_dim;
        int dist_dim;
    };

    /**
     * @brief Constraint parameters
     */
    struct ConstraintParams {
        Eigen::VectorXd x_min;  // State lower bounds
        Eigen::VectorXd x_max;  // State upper bounds
        Eigen::VectorXd u_min;  // Input lower bounds
        Eigen::VectorXd u_max;  // Input upper bounds
        Eigen::VectorXd w_bound;  // Disturbance bounds

        double safety_margin;  // Additional safety margin for DR constraints
    };

    /**
     * @brief Terminal set parameters
     */
    struct TerminalSetParams {
        int max_iterations = 100;  // Maximum set computation iterations
        double convergence_tolerance = 1e-6;  // Convergence threshold
        bool use_dr_tightening = true;  // Apply DR constraint tightening
        double risk_delta = 0.1;  // Risk level δ for DR constraints
    };

    /**
     * @brief Computed terminal set
     */
    struct TerminalSet {
        Eigen::MatrixXd vertices;  // Polytope vertices (for 2D visualization)
        Eigen::VectorXd center;   // Set center (for tracking)
        std::vector<Eigen::VectorXd> facets;  // Half-space representation: a_i^T x ≤ b_i

        // Terminal cost (Lyapunov function)
        Eigen::MatrixXd P;  // Positive definite matrix: V(x) = x^T P x
        double alpha;       // Level set value

        // DR feasibility bounds
        double dr_constraint_tightening;  // Additional constraint margin

        bool is_empty;      // Whether set is empty (infeasible)
        bool is_bounded;    // Whether set is bounded
    };

    /**
     * @brief Constructor
     */
    TerminalSetCalculator();

    /**
     * @brief Set system dynamics
     */
    void setSystemDynamics(const SystemDynamics& dynamics);

    /**
     * @brief Set constraint parameters
     */
    void setConstraints(const ConstraintParams& constraints);

    /**
     * @brief Set terminal set computation parameters
     */
    void setTerminalParams(const TerminalSetParams& params);

    /**
     * @brief Compute maximum control-invariant terminal set
     *
     * Algorithm:
     * 1. Initialize with state constraint set
     * 2. Iteratively remove states that cannot be kept in set
     * 3. Apply DR constraint tightening
     * 4. Compute terminal cost P matrix
     *
     * @param dr_tightening_margin Additional DR constraint margin (from Lemma 4.3)
     * @return Computed terminal set
     */
    TerminalSet computeTerminalSet(double dr_tightening_margin = 0.0);

    /**
     * @brief Compute LQR terminal cost matrix P
     *
     * Solves the Riccati equation:
     * P = A^T P A - A^T P B (R + B^T P B)^{-1} B^T P A + Q
     *
     * @param Q State cost matrix
     * @param R Input cost matrix
     * @return Terminal cost matrix P
     */
    Eigen::MatrixXd computeTerminalCost(
        const Eigen::MatrixXd& Q,
        const Eigen::MatrixXd& R
    );

    /**
     * @brief Verify if a state is in the terminal set
     *
     * @param state State to check
     * @param terminal_set Terminal set definition
     * @return True if state ∈ terminal_set
     */
    bool isInTerminalSet(
        const Eigen::VectorXd& state,
        const TerminalSet& terminal_set
    ) const;

    /**
     * @brief Apply DR constraint tightening to terminal set
     *
     * Tightens the terminal set constraints by the DR margin:
     * x_min := x_min + σ_dr
     * x_max := x_max - σ_dr
     *
     * @param terminal_set Original terminal set
     * @param dr_margin DR tightening margin (from TighteningComputer)
     * @return Tightened terminal set
     */
    TerminalSet applyDRTightening(
        const TerminalSet& terminal_set,
        double dr_margin
    );

    /**
     * @brief Check recursive feasibility condition
     *
     * Verifies Theorem 4.5 condition:
     * If x_k ∈ feasible_set and u_k ∈ U(x_k), then x_{k+1} ∈ feasible_set
     *
     * @param terminal_set Terminal set to verify
     * @return True if recursive feasibility holds
     */
    bool verifyRecursiveFeasibility(const TerminalSet& terminal_set) const;

    /**
     * @brief Get terminal set bounds for visualization
     *
     * Returns min/max bounds for each state dimension
     *
     * @param terminal_set Terminal set
     * @return Vector of (min, max) pairs for each dimension
     */
    std::vector<std::pair<double, double>> getBounds(
        const TerminalSet& terminal_set
    ) const;

    /**
     * @brief Compute control invariant set using backward reachable set
     *
     * Algorithm:
     * Ω_0 = safe set
     * Ω_{k+1} = {x | ∃ u: f(x,u,w) ∈ Ω_k, ∀ w ∈ 𝒲}
     *
     * Stops when Ω_{k+1} = Ω_k (converged to control-invariant set)
     *
     * @param safe_set Initial safe set (state constraints)
     * @param dr_margin DR constraint tightening margin
     * @return Control-invariant set
     */
    TerminalSet computeControlInvariantSet(
        const TerminalSet& safe_set,
        double dr_margin = 0.0
    );

private:
    SystemDynamics dynamics_;
    ConstraintParams constraints_;
    TerminalSetParams params_;

    bool dynamics_set_ = false;
    bool constraints_set_ = false;

    /**
     * @brief Project state onto constraint set
     */
    Eigen::VectorXd projectOntoConstraints(
        const Eigen::VectorXd& state,
        const ConstraintParams& constraints
    ) const;

    /**
     * @brief Check if state satisfies tightened constraints
     */
    bool satisfiesTightenedConstraints(
        const Eigen::VectorXd& state,
        double dr_margin
    ) const;

    /**
     * @brief Compute next state with disturbance
     */
    Eigen::VectorXd computeNextState(
        const Eigen::VectorXd& state,
        const Eigen::VectorXd& input,
        const Eigen::VectorXd& disturbance
    ) const;
};

} // namespace dr_tightening

#endif // TERMINAL_SET_CALCULATOR_HPP
