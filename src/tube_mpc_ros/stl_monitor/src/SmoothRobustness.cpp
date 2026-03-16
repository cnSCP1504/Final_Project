#include "stl_monitor/SmoothRobustness.h"
#include <iostream>
#include <limits>
#include <cmath>

namespace stl_monitor {

SmoothRobustness::SmoothRobustness(double temperature)
    : temperature_(temperature), compute_gradients_(true) {
    if (temperature_ <= 0.0) {
        temperature_ = 0.1;  // Default value
    }
}

SmoothRobustness::~SmoothRobustness() {}

void SmoothRobustness::setTemperature(double tau) {
    if (tau > 0.0) {
        temperature_ = tau;
    }
}

double SmoothRobustness::getTemperature() const {
    return temperature_;
}

void SmoothRobustness::setComputeGradients(bool enable) {
    compute_gradients_ = enable;
}

// Smooth min using log-sum-exp: smin(x) = -τ * log(Σ exp(-x_i/τ))
double SmoothRobustness::smin(const Eigen::VectorXd& x) const {
    if (x.size() == 0) return 0.0;
    if (x.size() == 1) return x(0);

    // Numerically stable implementation
    double max_val = x.maxCoeff();
    double sum_exp = 0.0;
    for (int i = 0; i < x.size(); ++i) {
        sum_exp += std::exp(-(x(i) - max_val) / temperature_);
    }

    return max_val - temperature_ * std::log(sum_exp);
}

double SmoothRobustness::smin(double a, double b) const {
    Eigen::VectorXd x(2);
    x(0) = a;
    x(1) = b;
    return smin(x);
}

// Smooth max using log-sum-exp: smax(x) = τ * log(Σ exp(x_i/τ))
double SmoothRobustness::smax(const Eigen::VectorXd& x) const {
    if (x.size() == 0) return 0.0;
    if (x.size() == 1) return x(0);

    // Numerically stable implementation
    double max_val = x.maxCoeff();
    double sum_exp = 0.0;
    for (int i = 0; i < x.size(); ++i) {
        sum_exp += std::exp((x(i) - max_val) / temperature_);
    }

    return max_val + temperature_ * std::log(sum_exp);
}

double SmoothRobustness::smax(double a, double b) const {
    Eigen::VectorXd x(2);
    x(0) = a;
    x(1) = b;
    return smax(x);
}

// Gradient of smooth min
Eigen::VectorXd SmoothRobustness::smin_grad(const Eigen::VectorXd& x) const {
    if (x.size() == 0) return Eigen::VectorXd();

    double max_val = x.maxCoeff();
    Eigen::VectorXd exp_terms(x.size());
    double sum_exp = 0.0;

    for (int i = 0; i < x.size(); ++i) {
        exp_terms(i) = std::exp(-(x(i) - max_val) / temperature_);
        sum_exp += exp_terms(i);
    }

    // ∂smin/∂x_i = exp(-x_i/τ) / Σ exp(-x_j/τ)
    Eigen::VectorXd grad(x.size());
    for (int i = 0; i < x.size(); ++i) {
        grad(i) = exp_terms(i) / sum_exp;
    }

    return grad;
}

// Gradient of smooth max
Eigen::VectorXd SmoothRobustness::smax_grad(const Eigen::VectorXd& x) const {
    if (x.size() == 0) return Eigen::VectorXd();

    double max_val = x.maxCoeff();
    Eigen::VectorXd exp_terms(x.size());
    double sum_exp = 0.0;

    for (int i = 0; i < x.size(); ++i) {
        exp_terms(i) = std::exp((x(i) - max_val) / temperature_);
        sum_exp += exp_terms(i);
    }

    // ∂smax/∂x_i = exp(x_i/τ) / Σ exp(x_j/τ)
    Eigen::VectorXd grad(x.size());
    for (int i = 0; i < x.size(); ++i) {
        grad(i) = exp_terms(i) / sum_exp;
    }

    return grad;
}

RobustnessValue SmoothRobustness::compute(
    const STLFormula& formula,
    const Trajectory& traj)
{
    if (traj.size() == 0) {
        return RobustnessValue(0.0);
    }

    // Compute robustness at each time step and take min over trajectory
    std::vector<double> robustness_values;

    for (size_t i = 0; i < traj.size(); ++i) {
        auto rob = computeAtTime(formula, traj, traj.timestamps[i]);
        robustness_values.push_back(rob.value);
    }

    // Overall robustness is min over time
    Eigen::VectorXd rob_vec(robustness_values.size());
    for (size_t i = 0; i < robustness_values.size(); ++i) {
        rob_vec(i) = robustness_values[i];
    }

    double min_rob = smin(rob_vec);

    return RobustnessValue(min_rob);
}

RobustnessValue SmoothRobustness::computeAtTime(
    const STLFormula& formula,
    const Trajectory& traj,
    double query_time)
{
    // Find closest time index
    size_t idx = 0;
    double min_diff = std::abs(traj.timestamps[0] - query_time);

    for (size_t i = 1; i < traj.timestamps.size(); ++i) {
        double diff = std::abs(traj.timestamps[i] - query_time);
        if (diff < min_diff) {
            min_diff = diff;
            idx = i;
        }
    }

    return computeNode_(formula.root.get(), traj, idx);
}

RobustnessValue SmoothRobustness::computeAtom_(
    const STLFormulaNode* node,
    const Trajectory& traj,
    size_t time_idx) const
{
    if (time_idx >= traj.states.size()) {
        return RobustnessValue(-std::numeric_limits<double>::infinity());
    }

    const auto& state = traj.states[time_idx];
    const auto& pred = node->atom;

    // Evaluate predicate
    double value = evaluatePredicate_(pred, state);

    return RobustnessValue(value);
}

double SmoothRobustness::evaluatePredicate_(
    const AtomicPredicate& pred,
    const Eigen::VectorXd& state) const
{
    switch (pred.type) {
        case PredicateType::REACHABILITY: {
            // distance to goal < threshold
            // state format: [x, y, theta, goal_x, goal_y]
            if (state.size() < 5) return 0.0;
            double dx = state[0] - state[3];
            double dy = state[1] - state[4];
            double dist = std::sqrt(dx*dx + dy*dy);
            return pred.threshold - dist;
        }
        case PredicateType::SAFETY: {
            // obstacle distance > threshold
            // TODO: implement with obstacle map
            return 1.0;  // Placeholder
        }
        case PredicateType::VELOCITY_LIMIT: {
            // |v| < threshold
            if (state.size() < 3) return 0.0;
            double v = state[2];
            return pred.threshold - std::abs(v);
        }
        case PredicateType::BATTERY_LEVEL: {
            // battery > threshold
            // TODO: implement with battery state
            return 1.0;  // Placeholder
        }
        default:
            return 0.0;
    }
}

// Recursive computation for each operator type
RobustnessValue SmoothRobustness::computeNode_(
    const STLFormulaNode* node,
    const Trajectory& traj,
    size_t time_idx) const
{
    if (!node) return RobustnessValue(0.0);

    switch (node->op) {
        case STLOperator::ATOM:
            return computeAtom_(node, traj, time_idx);

        case STLOperator::NOT: {
            auto child_rob = computeNode_(node->children[0].get(), traj, time_idx);
            return RobustnessValue(-child_rob.value);
        }

        case STLOperator::AND: {
            auto rob1 = computeNode_(node->children[0].get(), traj, time_idx);
            auto rob2 = computeNode_(node->children[1].get(), traj, time_idx);
            Eigen::VectorXd vals(2);
            vals(0) = rob1.value;
            vals(1) = rob2.value;
            return RobustnessValue(smin(vals));
        }

        case STLOperator::OR: {
            auto rob1 = computeNode_(node->children[0].get(), traj, time_idx);
            auto rob2 = computeNode_(node->children[1].get(), traj, time_idx);
            Eigen::VectorXd vals(2);
            vals(0) = rob1.value;
            vals(1) = rob2.value;
            return RobustnessValue(smax(vals));
        }

        case STLOperator::EVENTUALLY: {
            // ◇_I φ = max_{t∈I} ρ(φ, t)
            std::vector<size_t> indices = findIndicesInInterval_(traj, time_idx, node->interval);

            if (indices.empty()) {
                return RobustnessValue(-std::numeric_limits<double>::infinity());
            }

            Eigen::VectorXd rob_values(indices.size());
            for (size_t i = 0; i < indices.size(); ++i) {
                auto rob = computeNode_(node->children[0].get(), traj, indices[i]);
                rob_values(i) = rob.value;
            }

            return RobustnessValue(smax(rob_values));
        }

        case STLOperator::ALWAYS: {
            // □_I φ = min_{t∈I} ρ(φ, t)
            std::vector<size_t> indices = findIndicesInInterval_(traj, time_idx, node->interval);

            if (indices.empty()) {
                return RobustnessValue(-std::numeric_limits<double>::infinity());
            }

            Eigen::VectorXd rob_values(indices.size());
            for (size_t i = 0; i < indices.size(); ++i) {
                auto rob = computeNode_(node->children[0].get(), traj, indices[i]);
                rob_values(i) = rob.value;
            }

            return RobustnessValue(smin(rob_values));
        }

        case STLOperator::UNTIL: {
            // φ₁ U_I φ₂ = max_{t∈I} (min(ρ(φ₂, t), min_{t'∈[0,t]} ρ(φ₁, t')))
            std::vector<size_t> indices = findIndicesInInterval_(traj, time_idx, node->interval);

            if (indices.empty()) {
                return RobustnessValue(-std::numeric_limits<double>::infinity());
            }

            Eigen::VectorXd until_values(indices.size());
            for (size_t i = 0; i < indices.size(); ++i) {
                auto rob2 = computeNode_(node->children[1].get(), traj, indices[i]);

                // min over φ1 from start to current
                Eigen::VectorXd rob1_values(indices[i] - time_idx + 1);
                for (size_t j = 0; j < indices[i] - time_idx + 1; ++j) {
                    auto rob1 = computeNode_(node->children[0].get(), traj, time_idx + j);
                    rob1_values(j) = rob1.value;
                }

                double min_rob1 = smin(rob1_values);
                until_values(i) = smin(rob2.value, min_rob1);
            }

            return RobustnessValue(smax(until_values));
        }

        default:
            return RobustnessValue(0.0);
    }
}

std::vector<size_t> SmoothRobustness::findIndicesInInterval_(
    const Trajectory& traj,
    size_t start_idx,
    const TimeInterval& interval) const
{
    std::vector<size_t> indices;
    double start_time = traj.timestamps[start_idx];

    for (size_t i = start_idx; i < traj.size(); ++i) {
        double dt = traj.timestamps[i] - start_time;
        if (interval.lower <= dt && dt <= interval.upper) {
            indices.push_back(i);
        } else if (dt > interval.upper) {
            break;  // Past the interval
        }
    }

    return indices;
}

} // namespace stl_monitor
