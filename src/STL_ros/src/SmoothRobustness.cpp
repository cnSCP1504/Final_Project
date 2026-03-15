#include "STL_ros/SmoothRobustness.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stl_ros {

// ==================== SmoothRobustness Implementation ====================

SmoothRobustness::SmoothRobustness(double tau) : tau_(tau) {
    if (tau <= 0) {
        throw std::invalid_argument("Temperature tau must be positive");
    }
}

double SmoothRobustness::smoothMax(const std::vector<double>& values) const {
    if (values.empty()) {
        throw std::invalid_argument("Cannot compute smooth max of empty vector");
    }

    // smax_τ(z) = τ * log(∑ᵢ e^(zᵢ/τ))
    std::vector<double> scaled_values;
    scaled_values.reserve(values.size());
    for (double v : values) {
        scaled_values.push_back(v / tau_);
    }

    return tau_ * logSumExp(scaled_values);
}

double SmoothRobustness::smoothMin(const std::vector<double>& values) const {
    if (values.empty()) {
        throw std::invalid_argument("Cannot compute smooth min of empty vector");
    }

    // smin_τ(z) = -smax_τ(-z) = -τ * log(∑ᵢ e^(-zᵢ/τ))
    std::vector<double> negated_values;
    negated_values.reserve(values.size());
    for (double v : values) {
        negated_values.push_back(-v);
    }

    return -smoothMax(negated_values);
}

double SmoothRobustness::smoothMaxInterval(const std::vector<double>& trajectory,
                                           const std::vector<double>& times,
                                           double t_start, double t_end) const {
    if (trajectory.size() != times.size()) {
        throw std::invalid_argument("Trajectory and times must have same size");
    }

    // Extract values in time interval
    std::vector<double> interval_values;
    for (size_t i = 0; i < times.size(); ++i) {
        if (times[i] >= t_start && times[i] <= t_end) {
            interval_values.push_back(trajectory[i]);
        }
    }

    if (interval_values.empty()) {
        return -std::numeric_limits<double>::infinity();
    }

    return smoothMax(interval_values);
}

double SmoothRobustness::smoothMinInterval(const std::vector<double>& trajectory,
                                           const std::vector<double>& times,
                                           double t_start, double t_end) const {
    if (trajectory.size() != times.size()) {
        throw std::invalid_argument("Trajectory and times must have same size");
    }

    // Extract values in time interval
    std::vector<double> interval_values;
    for (size_t i = 0; i < times.size(); ++i) {
        if (times[i] >= t_start && times[i] <= t_end) {
            interval_values.push_back(trajectory[i]);
        }
    }

    if (interval_values.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    return smoothMin(interval_values);
}

std::vector<double> SmoothRobustness::smoothMaxGradient(const std::vector<double>& values) const {
    if (values.empty()) {
        throw std::invalid_argument("Cannot compute gradient of empty vector");
    }

    // Gradient of smax: ∂/∂zᵢ smax_τ(z) = e^(zᵢ/τ) / ∑ⱼ e^(zⱼ/τ)
    std::vector<double> scaled_values;
    double max_scaled = *std::max_element(values.begin(), values.end()) / tau_;

    for (double v : values) {
        scaled_values.push_back(std::exp((v / tau_) - max_scaled));
    }

    double sum_exp = 0.0;
    for (double v : scaled_values) {
        sum_exp += v;
    }

    std::vector<double> gradient(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        gradient[i] = scaled_values[i] / sum_exp;
    }

    return gradient;
}

std::vector<double> SmoothRobustness::smoothMinGradient(const std::vector<double>& values) const {
    if (values.empty()) {
        throw std::invalid_argument("Cannot compute gradient of empty vector");
    }

    // Gradient of smin: ∂/∂zᵢ smin_τ(z) = -∂/∂zᵢ smax_τ(-z)
    std::vector<double> negated_values;
    for (double v : values) {
        negated_values.push_back(-v);
    }

    std::vector<double> grad_neg = smoothMaxGradient(negated_values);
    std::vector<double> gradient(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        gradient[i] = -grad_neg[i];
    }

    return gradient;
}

void SmoothRobustness::setTau(double tau) {
    if (tau <= 0) {
        throw std::invalid_argument("Temperature tau must be positive");
    }
    tau_ = tau;
}

double SmoothRobustness::errorBound(int num_terms) const {
    // From manuscript: |ρ^soft - ρ| ≤ τ * log(C(φ))
    // where C(φ) is the maximum number of terms aggregated
    return tau_ * std::log(static_cast<double>(num_terms));
}

double SmoothRobustness::logSumExp(const std::vector<double>& values) const {
    if (values.empty()) {
        throw std::invalid_argument("Cannot compute log-sum-exp of empty vector");
    }

    // Numerically stable computation:
    // log(∑ᵢ e^(xᵢ)) = max(x) + log(∑ᵢ e^(xᵢ - max(x)))

    double max_val = *std::max_element(values.begin(), values.end());

    double sum_exp = 0.0;
    for (double v : values) {
        sum_exp += std::exp(v - max_val);
    }

    return max_val + std::log(sum_exp);
}

// ==================== SmoothSTLSemantics Implementation ====================

double SmoothSTLSemantics::evaluate(const STLFormula::Ptr& formula,
                                    const std::vector<Eigen::VectorXd>& trajectory,
                                    const std::vector<double>& times) const {
    if (!formula) {
        throw std::invalid_argument("Null formula");
    }

    switch (formula->getType()) {
        case FormulaType::TRUE:
            return std::numeric_limits<double>::infinity();

        case FormulaType::PREDICATE: {
            // Evaluate atomic predicate
            std::vector<double> robustness_values;
            for (size_t i = 0; i < trajectory.size(); ++i) {
                robustness_values.push_back(formula->robustness(
                    {trajectory[i]}, {times[i]}));
            }
            return robustness_values[0];  // Current time
        }

        case FormulaType::NOT: {
            // ¬φ: -ρ(φ)
            auto* not_op = dynamic_cast<const UnaryOp*>(formula.get());
            return -evaluate(not_op->getChild(), trajectory, times);
        }

        case FormulaType::AND: {
            // φ₁ ∧ φ₂: smin(ρ(φ₁), ρ(φ₂))
            auto* and_op = dynamic_cast<const BinaryOp*>(formula.get());
            double rho1 = evaluate(and_op->getLeft(), trajectory, times);
            double rho2 = evaluate(and_op->getRight(), trajectory, times);
            return calc_.smoothMin({rho1, rho2});
        }

        case FormulaType::OR: {
            // φ₁ ∨ φ₂: smax(ρ(φ₁), ρ(φ₂))
            auto* or_op = dynamic_cast<const BinaryOp*>(formula.get());
            double rho1 = evaluate(or_op->getLeft(), trajectory, times);
            double rho2 = evaluate(or_op->getRight(), trajectory, times);
            return calc_.smoothMax({rho1, rho2});
        }

        case FormulaType::EVENTUALLY: {
            // ◇_[a,b] φ: smax over interval
            auto* ev_op = dynamic_cast<const BinaryOp*>(formula.get());
            std::vector<double> robustness_over_time;
            for (size_t i = 0; i < trajectory.size(); ++i) {
                robustness_over_time.push_back(
                    evaluate(ev_op->getLeft(), {trajectory[i]}, {times[i]}));
            }
            return calc_.smoothMaxInterval(robustness_over_time, times,
                                          ev_op->getTimeLower(), ev_op->getTimeUpper());
        }

        case FormulaType::GLOBALLY: {
            // □_[a,b] φ: smin over interval
            auto* glb_op = dynamic_cast<const BinaryOp*>(formula.get());
            std::vector<double> robustness_over_time;
            for (size_t i = 0; i < trajectory.size(); ++i) {
                robustness_over_time.push_back(
                    evaluate(glb_op->getLeft(), {trajectory[i]}, {times[i]}));
            }
            return calc_.smoothMinInterval(robustness_over_time, times,
                                          glb_op->getTimeLower(), glb_op->getTimeUpper());
        }

        case FormulaType::UNTIL: {
            // φ₁ U_[a,b] φ₂: exists t in [a,b] s.t. φ₂(t) ∧ ∀s<t, φ₁(s)
            auto* until_op = dynamic_cast<const BinaryOp*>(formula.get());
            std::vector<double> robustness_values;

            for (size_t i = 0; i < trajectory.size(); ++i) {
                if (times[i] >= until_op->getTimeLower() &&
                    times[i] <= until_op->getTimeUpper()) {

                    // Check φ₂ at time i
                    double phi2_rob = evaluate(until_op->getRight(),
                                              {trajectory[i]}, {times[i]});

                    // Check φ₁ for all times before i
                    bool phi1_always_satisfied = true;
                    double min_phi1_rob = std::numeric_limits<double>::infinity();
                    for (size_t j = 0; j < i; ++j) {
                        double phi1_rob = evaluate(until_op->getLeft(),
                                                   {trajectory[j]}, {times[j]});
                        min_phi1_rob = std::min(min_phi1_rob, phi1_rob);
                        if (phi1_rob < 0) {
                            phi1_always_satisfied = false;
                            break;
                        }
                    }

                    if (phi1_always_satisfied) {
                        robustness_values.push_back(std::min(min_phi1_rob, phi2_rob));
                    }
                }
            }

            if (robustness_values.empty()) {
                return -std::numeric_limits<double>::infinity();
            }

            return calc_.smoothMax(robustness_values);
        }

        default:
            throw std::runtime_error("Unsupported formula type");
    }
}

std::vector<Eigen::VectorXd> SmoothSTLSemantics::gradient(
    const STLFormula::Ptr& formula,
    const std::vector<Eigen::VectorXd>& trajectory,
    const std::vector<double>& times) const {

    // Compute gradient using automatic differentiation principles
    // This is a simplified implementation - full version would backpropagate
    // through the formula tree

    std::vector<Eigen::VectorXd> gradients(trajectory.size());
    int state_dim = trajectory[0].size();

    for (size_t i = 0; i < trajectory.size(); ++i) {
        gradients[i] = Eigen::VectorXd::Zero(state_dim);
    }

    // For predicates, compute gradient using finite differences
    if (formula->getType() == FormulaType::PREDICATE) {
        const double eps = 1e-6;
        double base_rob = evaluate(formula, trajectory, times);

        for (size_t t = 0; t < trajectory.size(); ++t) {
            for (int d = 0; d < state_dim; ++d) {
                std::vector<Eigen::VectorXd> perturbed_traj = trajectory;
                perturbed_traj[t](d) += eps;

                double perturbed_rob = evaluate(formula, perturbed_traj, times);
                gradients[t](d) = (perturbed_rob - base_rob) / eps;
            }
        }
    }

    return gradients;
}

} // namespace stl_ros
