#include "STL_ros/STLFormula.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace stl_ros {

// ==================== Predicate Implementation ====================

double Predicate::robustness(const std::vector<Eigen::VectorXd>& trajectory,
                             const std::vector<double>& times) const {
    if (trajectory.empty() || times.empty()) {
        throw std::runtime_error("Empty trajectory or times");
    }

    // Evaluate predicate at current time (first state)
    return func_(trajectory[0], times[0]);
}

// ==================== UnaryOp Implementation ====================

double UnaryOp::robustness(const std::vector<Eigen::VectorXd>& trajectory,
                          const std::vector<double>& times) const {
    double child_rob = child_->robustness(trajectory, times);

    switch (type_) {
        case FormulaType::NOT:
            return -child_rob;  // ¬φ: negate robustness
        default:
            throw std::runtime_error("Unsupported unary operation");
    }
}

std::string UnaryOp::toString() const {
    std::string op_symbol = (type_ == FormulaType::NOT) ? "!" : "?";
    return "(" + op_symbol + child_->toString() + ")";
}

STLFormula::Ptr UnaryOp::clone() const {
    return std::make_shared<UnaryOp>(type_, child_->clone());
}

// ==================== BinaryOp Implementation ====================

double BinaryOp::robustness(const std::vector<Eigen::VectorXd>& trajectory,
                           const std::vector<double>& times) const {
    double left_rob = left_->robustness(trajectory, times);
    double right_rob = right_->robustness(trajectory, times);

    switch (type_) {
        case FormulaType::AND:
            return std::min(left_rob, right_rob);  // φ₁ ∧ φ₂
        case FormulaType::OR:
            return std::max(left_rob, right_rob);  // φ₁ ∨ φ₂
        case FormulaType::UNTIL: {
            // φ₁ U_[a,b] φ₂: exists t in [a,b] s.t. φ₂ holds and φ₁ holds for all times before t
            // Find indices within time interval
            std::vector<size_t> interval_indices;
            for (size_t i = 0; i < times.size(); ++i) {
                if (times[i] >= time_lower_ && times[i] <= time_upper_) {
                    interval_indices.push_back(i);
                }
            }

            if (interval_indices.empty()) {
                return -1e9;  // No time in interval, very negative
            }

            double max_robustness = -1e9;
            for (size_t idx : interval_indices) {
                // Check φ₂ at this time
                std::vector<Eigen::VectorXd> sub_traj trajectory.begin(), trajectory.begin() + idx + 1);
                std::vector<double> sub_times(times.begin(), times.begin() + idx + 1);
                double phi2_rob = right_->robustness(sub_traj, sub_times);

                // Check φ₁ for all times before idx
                bool phi1_always_true = true;
                for (size_t i = 0; i < idx; ++i) {
                    std::vector<Eigen::VectorXd> phi1_traj(trajectory.begin(), trajectory.begin() + i + 1);
                    std::vector<double> phi1_times(times.begin(), times.begin() + i + 1);
                    if (left_->robustness(phi1_traj, phi1_times) < 0) {
                        phi1_always_true = false;
                        break;
                    }
                }

                if (phi1_always_true) {
                    max_robustness = std::max(max_robustness, phi2_rob);
                }
            }
            return max_robustness;
        }
        default:
            throw std::runtime_error("Unsupported binary operation");
    }
}

std::string BinaryOp::toString() const {
    std::string op_symbol;
    switch (type_) {
        case FormulaType::AND: op_symbol = "&&"; break;
        case FormulaType::OR: op_symbol = "||"; break;
        case FormulaType::UNTIL: op_symbol = " U "; break;
        default: op_symbol = "?";
    }

    std::ostringstream oss;
    if (type_ == FormulaType::UNTIL) {
        oss << "(" << left_->toString() << op_symbol
            << "[" << time_lower_ << "," << time_upper_ << "] "
            << right_->toString() << ")";
    } else {
        oss << "(" << left_->toString() << op_symbol << right_->toString() << ")";
    }
    return oss.str();
}

STLFormula::Ptr BinaryOp::clone() const {
    return std::make_shared<BinaryOp>(type_, left_->clone(), right_->clone(),
                                      time_lower_, time_upper_);
}

// ==================== STL Parser Implementation ====================

STLFormula::Ptr STLParser::parse(
    const std::string& formula_str,
    const std::map<std::string, PredicateFunc>& predicates) {

    // Simple recursive descent parser for STL
    // Grammar:
    // formula ::= implies | or
    // implies ::= or ('->' or)?
    // or ::= and ('||' and)*
    // and ::= not ('&&' not)*
    // not ::= '!' not | primary
    // primary ::= predicate | '(' formula ')' | temporal_op primary
    // temporal_op ::= 'always' | 'eventually' | 'until'

    // For simplicity, this is a basic implementation
    // In production, use a proper parser generator or boost::spirit

    std::string str = formula_str;
    // Remove whitespace
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());

    // Parse temporal operators
    // "always[a,b](formula)" -> G_[a,b] formula
    size_t always_pos = str.find("always[");
    if (always_pos != std::string::npos) {
        size_t end_bracket = str.find("]", always_pos);
        if (end_bracket == std::string::npos) {
            throw std::runtime_error("Missing closing bracket in always operator");
        }

        // Parse interval
        std::string interval = str.substr(always_pos + 7, end_bracket - always_pos - 7);
        size_t comma = interval.find(",");
        double a = std::stod(interval.substr(0, comma));
        double b = std::stod(interval.substr(comma + 1));

        // Parse inner formula
        size_t open_paren = str.find("(", end_bracket);
        size_t close_paren = str.find(")", open_paren);
        std::string inner_str = str.substr(open_paren + 1, close_paren - open_paren - 1);
        STLFormula::Ptr inner = parse(inner_str, predicates);

        // Always = G_[a,b] φ = ¬(true U_[a,b] ¬φ)
        // Simplified: min over interval
        return std::make_shared<BinaryOp>(FormulaType::GLOBALLY, inner, nullptr, a, b);
    }

    // "eventually[a,b](formula)" -> F_[a,b] formula
    size_t eventually_pos = str.find("eventually[");
    if (eventually_pos != std::string::npos) {
        size_t end_bracket = str.find("]", eventually_pos);
        if (end_bracket == std::string::npos) {
            throw std::runtime_error("Missing closing bracket in eventually operator");
        }

        // Parse interval
        std::string interval = str.substr(eventually_pos + 11, end_bracket - eventually_pos - 11);
        size_t comma = interval.find(",");
        double a = std::stod(interval.substr(0, comma));
        double b = std::stod(interval.substr(comma + 1));

        // Parse inner formula
        size_t open_paren = str.find("(", end_bracket);
        size_t close_paren = str.find(")", open_paren);
        std::string inner_str = str.substr(open_paren + 1, close_paren - open_paren - 1);
        STLFormula::Ptr inner = parse(inner_str, predicates);

        return std::make_shared<BinaryOp>(FormulaType::EVENTUALLY, inner, nullptr, a, b);
    }

    // Handle predicates and binary operators
    // This is simplified - full implementation would handle parentheses and precedence
    size_t and_pos = str.find("&&");
    if (and_pos != std::string::npos) {
        std::string left_str = str.substr(0, and_pos);
        std::string right_str = str.substr(and_pos + 2);
        return std::make_shared<BinaryOp>(FormulaType::AND,
                                          parse(left_str, predicates),
                                          parse(right_str, predicates));
    }

    size_t or_pos = str.find("||");
    if (or_pos != std::string::npos) {
        std::string left_str = str.substr(0, or_pos);
        std::string right_str = str.substr(or_pos + 2);
        return std::make_shared<BinaryOp>(FormulaType::OR,
                                          parse(left_str, predicates),
                                          parse(right_str, predicates));
    }

    // Handle negation
    size_t not_pos = str.find("!");
    if (not_pos == 0) {
        std::string inner_str = str.substr(1);
        return std::make_shared<UnaryOp>(FormulaType::NOT, parse(inner_str, predicates));
    }

    // Handle parentheses
    if (str[0] == '(') {
        size_t close_pos = str.find(")");
        if (close_pos == std::string::npos) {
            throw std::runtime_error("Missing closing parenthesis");
        }
        std::string inner_str = str.substr(1, close_pos - 1);
        return parse(inner_str, predicates);
    }

    // Must be a predicate
    // Parse "var > value" or "var < value"
    size_t gt_pos = str.find(">");
    size_t lt_pos = str.find("<");

    if (gt_pos != std::string::npos) {
        std::string var = str.substr(0, gt_pos);
        double value = std::stod(str.substr(gt_pos + 1));

        auto it = predicates.find(var);
        if (it != predicates.end()) {
            // Use registered predicate
            return std::make_shared<Predicate>(str, it->second);
        } else {
            // Create generic predicate
            auto func = [var, value](const Eigen::VectorXd& state, double time) -> double {
                // Assuming state has named indices - this is simplified
                // In practice, use a proper state index map
                if (var == "x") return state(0) - value;
                if (var == "y") return state(1) - value;
                if (var == "theta") return state(2) - value;
                if (var == "v") return state(3) - value;
                if (var == "w") return state(4) - value;
                throw std::runtime_error("Unknown state variable: " + var);
            };
            return std::make_shared<Predicate>(str, func);
        }
    }

    if (lt_pos != std::string::npos) {
        std::string var = str.substr(0, lt_pos);
        double value = std::stod(str.substr(lt_pos + 1));

        auto func = [var, value](const Eigen::VectorXd& state, double time) -> double {
            if (var == "x") return value - state(0);
            if (var == "y") return value - state(1);
            if (var == "theta") return value - state(2);
            if (var == "v") return value - state(3);
            if (var == "w") return value - state(4);
            throw std::runtime_error("Unknown state variable: " + var);
        };
        return std::make_shared<Predicate>(str, func);
    }

    throw std::runtime_error("Could not parse formula: " + formula_str);
}

void STLParser::registerNavigationPredicates(std::map<std::string, PredicateFunc>& predicates) {
    // Speed predicate
    predicates["speed_above"] = [](const Eigen::VectorXd& state, double time) -> double {
        return state(3);  // v > 0
    };

    // Position predicates
    predicates["in_region"] = [](const Eigen::VectorXd& state, double time) -> double {
        // Example: check if robot is in a specific region
        double x = state(0);
        double y = state(1);
        // Region bounds would be parameters
        return std::min(x - 0.0, 10.0 - x) + std::min(y - 0.0, 10.0 - y);
    };

    // Avoidance predicate
    predicates["avoid_obstacle"] = [](const Eigen::VectorXd& state, double time) -> double {
        // Distance from obstacle
        // This would be parameterized with obstacle position
        return 5.0;  // Placeholder
    };

    // Goal reaching
    predicates["at_goal"] = [](const Eigen::VectorXd& state, double time) -> double {
        double goal_x = 10.0;
        double goal_y = 10.0;
        double dx = state(0) - goal_x;
        double dy = state(1) - goal_y;
        return 2.0 - std::sqrt(dx*dx + dy*dy);  // 2.0 tolerance
    };
}

} // namespace stl_ros
