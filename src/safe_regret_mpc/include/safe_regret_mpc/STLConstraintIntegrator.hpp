#ifndef STL_CONSTRAINT_INTEGRATOR_HPP
#define STL_CONSTRAINT_INTEGRATOR_HPP

#include <Eigen/Dense>
#include <string>
#include <vector>
#include <memory>

namespace safe_regret_mpc {

/**
 * @brief STL Constraint Integrator for Safe-Regret MPC
 *
 * Integrates Signal Temporal Logic constraints into MPC optimization:
 * - Parses STL formulas
 * - Computes smooth robustness ρ^soft(φ; x_{k:k+N})
 * - Evaluates belief-space robustness ρ̃_k
 * - Manages robustness budget R_k
 * - Computes gradients for MPC optimization
 *
 * Key formulas:
 * - Smooth min: smin_τ(z) = -τ·log(Σ exp(-z_i/τ))
 * - Smooth max: smax_τ(z) = τ·log(Σ exp(z_i/τ))
 * - Budget update: R_{k+1} = max{0, R_k + ρ̃_k - r̲}
 */
class STLConstraintIntegrator {
public:
    using VectorXd = Eigen::VectorXd;
    using MatrixXd = Eigen::MatrixXd;

    /**
     * @brief Constructor
     */
    STLConstraintIntegrator();

    /**
     * @brief Destructor
     */
    ~STLConstraintIntegrator();

    /**
     * @brief Initialize STL integrator
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Set STL formula
     * @param formula STL formula string
     * @return True if parsing successful
     */
    bool setFormula(const std::string& formula);

    /**
     * @brief Compute smooth robustness
     * @param trajectory Trajectory x_{k:k+N}
     * @param time_stamps Time stamps for each state
     * @return Smooth robustness value ρ^soft(φ; x_{k:k+N})
     */
    double computeSmoothRobustness(const std::vector<VectorXd>& trajectory,
                                    const std::vector<double>& time_stamps);

    /**
     * @brief Compute belief-space robustness
     * @param belief_mean Mean of belief distribution
     * @param belief_cov Covariance of belief distribution
     * @param trajectory Nominal trajectory
     * @return Belief-space robustness ρ̃_k
     */
    double computeBeliefRobustness(const VectorXd& belief_mean,
                                   const MatrixXd& belief_cov,
                                   const std::vector<VectorXd>& trajectory);

    /**
     * @brief Compute robustness gradient
     * @param trajectory Trajectory x_{k:k+N}
     * @return Gradient ∇ρ with respect to states
     */
    std::vector<VectorXd> computeRobustnessGradient(
        const std::vector<VectorXd>& trajectory);

    /**
     * @brief Update robustness budget
     * @param current_robustness Current robustness ρ̃_k
     * @return Updated budget R_{k+1}
     */
    double updateBudget(double current_robustness);

    /**
     * @brief Set baseline robustness
     * @param baseline Baseline r̲
     */
    void setBaseline(double baseline) { baseline_ = baseline; }

    /**
     * @brief Get current budget
     * @return Current budget R_k
     */
    double getBudget() const { return budget_; }

    /**
     * @brief Set temperature parameter
     * @param tau Temperature τ
     */
    void setTemperature(double tau) { temperature_ = tau; }

    /**
     * @brief Get temperature parameter
     * @return Temperature τ
     */
    double getTemperature() const { return temperature_; }

    /**
     * @brief Check if budget is satisfied
     * @return True if R_k >= 0
     */
    bool isBudgetSatisfied() const { return budget_ >= 0.0; }

    /**
     * @brief Get violation count
     * @return Number of budget violations
     */
    int getViolationCount() const { return violation_count_; }

    /**
     * @brief Reset budget and counters
     */
    void reset();

    // ========== Smooth Approximation Functions ==========

    /**
     * @brief Smooth minimum (log-sum-exp)
     * @param values Input values
     * @param tau Temperature parameter
     * @return smin_τ(values)
     */
    static double smoothMin(const std::vector<double>& values, double tau);

    /**
     * @brief Smooth maximum (log-sum-exp)
     * @param values Input values
     * @param tau Temperature parameter
     * @return smax_τ(values)
     */
    static double smoothMax(const std::vector<double>& values, double tau);

    /**
     * @brief Smooth gradient of minimum
     * @param values Input values
     * @param tau Temperature parameter
     * @return Gradient of smooth min
     */
    static std::vector<double> smoothMinGradient(
        const std::vector<double>& values, double tau);

    /**
     * @brief Smooth gradient of maximum
     * @param values Input values
     * @param tau Temperature parameter
     * @return Gradient of smooth max
     */
    static std::vector<double> smoothMaxGradient(
        const std::vector<double>& values, double tau);

private:
    // Formula parsing
    struct STLNode {
        enum Type { PREDICATE, NOT, AND, OR, EVENTUALLY, GLOBALLY, UNTIL };
        Type type;
        std::vector<STLNode*> children;
        std::string predicate_id;
        double interval_start, interval_end;

        ~STLNode() {
            for (auto* child : children) {
                delete child;
            }
        }
    };

    /**
     * @brief Parse STL formula string
     * @param formula Formula string
     * @return Parse tree
     */
    STLNode* parseFormula(const std::string& formula);

    /**
     * @brief Evaluate STL formula on trajectory
     * @param node Current node in parse tree
     * @param trajectory Trajectory
     * @param time_stamps Time stamps
     * @return Robustness value
     */
    double evaluateNode(STLNode* node,
                       const std::vector<VectorXd>& trajectory,
                       const std::vector<double>& time_stamps);

    /**
     * @brief Evaluate atomic predicate
     * @param predicate_id Predicate identifier
     * @param state State to evaluate
     * @return Predicate value
     */
    double evaluatePredicate(const std::string& predicate_id,
                            const VectorXd& state);

    // Member variables
    STLNode* formula_tree_;
    double baseline_;              ///< Baseline robustness r̲
    double budget_;                ///< Current budget R_k
    double initial_budget_;        ///< Initial budget R_0
    double temperature_;           ///< Temperature τ
    int violation_count_;          ///< Number of violations
    bool initialized_;

    // Particle filtering for belief space
    size_t num_particles_;
    std::vector<VectorXd> particles_;
    std::vector<double> particle_weights_;
};

} // namespace safe_regret_mpc

#endif // STL_CONSTRAINT_INTEGRATOR_HPP
