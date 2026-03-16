#ifndef STL_MONITOR_STLTYPES_H
#define STL_MONITOR_STLTYPES_H

#include <Eigen/Dense>
#include <vector>
#include <memory>
#include <string>

namespace stl_monitor {

/**
 * @brief STL operator types
 */
enum class STLOperator {
    ATOM,           // Atomic predicate
    NOT,            // Negation ¬
    AND,            // Conjunction ∧
    OR,             // Disjunction ∨
    IMPLIES,        // Implication →
    EVENTUALLY,     // Eventually ◇
    ALWAYS,         // Globally □
    UNTIL,          // Until U
    ONCE,           // Once ◇ (past)
    HISTORICALLY    // Historically □ (past)
};

/**
 * @brief Atomic predicate type
 */
enum class PredicateType {
    REACHABILITY,       // distance to target < threshold
    SAFETY,             // distance to obstacles > threshold
    VELOCITY_LIMIT,     // velocity within bounds
    BATTERY_LEVEL,      // battery above threshold
    CUSTOM              // custom predicate function
};

/**
 * @brief Time interval for temporal operators
 */
struct TimeInterval {
    double lower;
    double upper;

    TimeInterval(double l = 0, double u = std::numeric_limits<double>::infinity())
        : lower(l), upper(u) {}

    bool contains(double t) const {
        return t >= lower && t <= upper;
    }
};

/**
 * @brief Atomic predicate definition
 */
struct AtomicPredicate {
    std::string name;
    PredicateType type;
    double threshold;
    Eigen::VectorXd parameters;

    AtomicPredicate(const std::string& n = "", PredicateType t = PredicateType::CUSTOM)
        : name(n), type(t), threshold(0.0) {}
};

/**
 * @brief STL formula tree node
 */
struct STLFormulaNode {
    STLOperator op;
    AtomicPredicate atom;
    TimeInterval interval;

    std::vector<std::shared_ptr<STLFormulaNode>> children;

    STLFormulaNode(STLOperator o = STLOperator::ATOM)
        : op(o) {}

    bool isLeaf() const { return op == STLOperator::ATOM; }
    size_t numChildren() const { return children.size(); }

    void addChild(std::shared_ptr<STLFormulaNode> child) {
        children.push_back(child);
    }
};

/**
 * @brief STL formula wrapper
 */
struct STLFormula {
    std::shared_ptr<STLFormulaNode> root;
    std::string description;

    STLFormula() : root(std::make_shared<STLFormulaNode>(STLOperator::ATOM)) {}
    STLFormula(std::shared_ptr<STLFormulaNode> r, const std::string& desc = "")
        : root(r), description(desc) {}
};

/**
 * @brief State trajectory for robustness evaluation
 */
struct Trajectory {
    std::vector<double> timestamps;
    std::vector<Eigen::VectorXd> states;

    void clear() {
        timestamps.clear();
        states.clear();
    }

    size_t size() const {
        return states.size();
    }

    void reserve(size_t n) {
        timestamps.reserve(n);
        states.reserve(n);
    }

    void push_back(double t, const Eigen::VectorXd& x) {
        timestamps.push_back(t);
        states.push_back(x);
    }
};

/**
 * @brief Robustness value with gradient information
 */
struct RobustnessValue {
    double value;
    Eigen::VectorXd gradient;
    bool has_gradient;

    RobustnessValue(double v = 0.0)
        : value(v), has_gradient(false) {}

    RobustnessValue(double v, const Eigen::VectorXd& grad)
        : value(v), gradient(grad), has_gradient(true) {}
};

/**
 * @brief Belief state distribution
 */
struct BeliefState {
    Eigen::VectorXd mean;        // Mean state
    Eigen::MatrixXd covariance;  // State covariance
    double timestamp;

    BeliefState(int dim = 0)
        : mean(Eigen::VectorXd::Zero(dim)),
          covariance(Eigen::MatrixXd::Zero(dim, dim)),
          timestamp(0.0) {}
};

/**
 * @brief Robustness budget state
 */
struct BudgetState {
    double budget;
    double baseline_requirement;
    double last_robustness;
    double violation_count;

    BudgetState()
        : budget(1.0),
          baseline_requirement(0.1),
          last_robustness(0.0),
          violation_count(0.0) {}
};

/**
 * @brief STL evaluation statistics
 */
struct STLEvaluationStats {
    double total_time;
    double robustness_min;
    double robustness_mean;
    double budget_final;
    size_t evaluation_count;

    STLEvaluationStats()
        : total_time(0.0),
          robustness_min(0.0),
          robustness_mean(0.0),
          budget_final(0.0),
          evaluation_count(0) {}
};

} // namespace stl_monitor

#endif // STL_MONITOR_STLTYPES_H
