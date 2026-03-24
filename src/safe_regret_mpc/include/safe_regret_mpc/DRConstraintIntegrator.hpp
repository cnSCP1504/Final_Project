#ifndef DR_CONSTRAINT_INTEGRATOR_HPP
#define DR_CONSTRAINT_INTEGRATOR_HPP

#include <Eigen/Dense>
#include <vector>
#include <deque>

namespace safe_regret_mpc {

/**
 * @brief Distributionally Robust Constraint Integrator
 *
 * Integrates DR chance constraints into MPC optimization:
 * - Collects tracking residuals
 * - Calibrates ambiguity set
 * - Computes deterministic margins
 * - Applies constraint tightening
 *
 * Key formulas (Lemma 4.3):
 * - DR margin: σ_{k,t} = μ_t + κ_{δ_t}·σ_t
 * - Tightened constraint: h(z_t) ≥ σ_{k,t} + η_ℰ
 * - Cantelli factor: κ_{δ_t} = sqrt((1-δ_t)/δ_t)
 */
class DRConstraintIntegrator {
public:
    using VectorXd = Eigen::VectorXd;
    using MatrixXd = Eigen::MatrixXd;

    /**
     * @brief Risk allocation strategy
     */
    enum class RiskAllocation {
        UNIFORM,              ///< δ_t = δ/N
        DEADLINE_WEIGHTED,    ///< Weighted by deadline proximity
        ADAPTIVE              ///< Adaptively allocated
    };

    /**
     * @brief Ambiguity set type
     */
    enum class AmbiguityType {
        CHEBYSHEV,            ///< Chebyshev inequality
        WASSERSTEIN,          ///< Wasserstein ball
        PHI_DIVERGENCE        ///< φ-divergence
    };

    /**
     * @brief Constructor
     */
    DRConstraintIntegrator();

    /**
     * @brief Destructor
     */
    ~DRConstraintIntegrator();

    /**
     * @brief Initialize DR integrator
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Add residual to sliding window
     * @param residual Tracking residual w_k = x_k - z_k
     */
    void addResidual(const VectorXd& residual);

    /**
     * @brief Compute DR margins for horizon
     * @param horizon Prediction horizon
     * @param tube_radius Current tube radius ē
     * @param lipschitz_const Lipschitz constant L_h
     * @return Margins σ_{k,t} for t=k,...,k+N
     */
    std::vector<double> computeMargins(size_t horizon,
                                       double tube_radius,
                                       double lipschitz_const);

    /**
     * @brief Get current margins
     * @return Current margins
     */
    std::vector<double> getMargins() const { return margins_; }

    /**
     * @brief Set risk level
     * @param delta Risk level δ
     */
    void setRiskLevel(double delta) { delta_ = delta; }

    /**
     * @brief Get risk level
     * @return Risk level δ
     */
    double getRiskLevel() const { return delta_; }

    /**
     * @brief Set risk allocation strategy
     * @param allocation Allocation strategy
     */
    void setRiskAllocation(RiskAllocation allocation) {
        allocation_strategy_ = allocation;
    }

    /**
     * @brief Set ambiguity set type
     * @param type Ambiguity set type
     */
    void setAmbiguityType(AmbiguityType type) {
        ambiguity_type_ = type;
    }

    /**
     * @brief Get residual statistics
     * @return Mean and covariance of residuals
     */
    std::pair<VectorXd, MatrixXd> getResidualStatistics() const;

    /**
     * @brief Get number of residuals collected
     * @return Number of residuals
     */
    size_t getNumResiduals() const { return residuals_window_.size(); }

    /**
     * @brief Reset residual buffer
     */
    void resetResiduals();

    /**
     * @brief Set window size
     * @param size Window size M
     */
    void setWindowSize(size_t size) { window_size_ = size; }

    /**
     * @brief Get ambiguity set radius
     * @return Radius ε
     */
    double getAmbiguityRadius() const { return ambiguity_radius_; }

private:
    /**
     * @brief Compute residual statistics
     */
    void updateStatistics();

    /**
     * @brief Allocate risk across horizon
     * @param horizon Prediction horizon
     * @return Risk allocation δ_t
     */
    std::vector<double> allocateRisk(size_t horizon);

    /**
     * @brief Compute Cantelli factor
     * @param delta Risk level
     * @return κ_δ = sqrt((1-δ)/δ)
     */
    static double computeCantelliFactor(double delta);

    /**
     * @brief Compute margin for single time step
     * @param mean Mean μ_t
     * @param std Standard deviation σ_t
     * @param delta Risk level δ_t
     * @param tube_offset Tube offset L_h·ē
     * @return Margin σ_{k,t}
     */
    double computeSingleMargin(double mean, double std,
                               double delta, double tube_offset);

    // Member variables
    std::deque<VectorXd> residuals_window_;
    size_t window_size_;             ///< M = 200
    VectorXd residual_mean_;
    MatrixXd residual_cov_;

    double delta_;                   ///< Total risk δ
    RiskAllocation allocation_strategy_;
    AmbiguityType ambiguity_type_;
    double ambiguity_radius_;        ///< ε

    std::vector<double> margins_;    ///< σ_{k,t}
    bool statistics_updated_;
    bool initialized_;
};

} // namespace safe_regret_mpc

#endif // DR_CONSTRAINT_INTEGRATOR_HPP
