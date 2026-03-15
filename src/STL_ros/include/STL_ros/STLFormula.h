#ifndef STL_FORMULA_H
#define STL_FORMULA_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace stl_ros {

/**
 * @brief STL formula types following the grammar:
 * φ ::= ⊤ | μ | ¬φ | φ₁ ∧ φ₂ | φ₁ ∨ φ₂ | φ₁ U_[a,b] φ₂ | ◇_[a,b] φ | □_[a,b] φ
 */
enum class FormulaType {
    TRUE,           // ⊤ (always true)
    PREDICATE,      // μ(predicate)
    NOT,            // ¬φ
    AND,            // φ₁ ∧ φ₂
    OR,             // φ₁ ∨ φ₂
    UNTIL,          // φ₁ U_[a,b] φ₂
    EVENTUALLY,     // ◇_[a,b] φ
    GLOBALLY        // □_[a,b] φ
};

/**
 * @brief Predicate function type
 * @param state Robot state vector
 * @param time Current time
 * @return Predicate value (positive = satisfied)
 */
using PredicateFunc = std::function<double(const Eigen::VectorXd& state, double time)>;

/**
 * @brief STL Formula base class
 */
class STLFormula {
public:
    using Ptr = std::shared_ptr<STLFormula>;

    STLFormula(FormulaType type) : type_(type) {}
    virtual ~STLFormula() = default;

    FormulaType getType() const { return type_; }

    /**
     * @brief Evaluate robustness over a trajectory
     * @param trajectory State trajectory [x_0, x_1, ..., x_N]
     * @param times Time points [t_0, t_1, ..., t_N]
     * @return Robustness value (positive = satisfied)
     */
    virtual double robustness(const std::vector<Eigen::VectorXd>& trajectory,
                             const std::vector<double>& times) const = 0;

    /**
     * @brief Get string representation of the formula
     */
    virtual std::string toString() const = 0;

    /**
     * @brief Clone the formula (deep copy)
     */
    virtual Ptr clone() const = 0;

protected:
    FormulaType type_;
};

/**
 * @brief Predicate (atomic proposition)
 */
class Predicate : public STLFormula {
public:
    Predicate(const std::string& name, PredicateFunc func)
        : STLFormula(FormulaType::PREDICATE), name_(name), func_(func) {}

    double robustness(const std::vector<Eigen::VectorXd>& trajectory,
                     const std::vector<double>& times) const override;

    std::string toString() const override { return name_; }
    Ptr clone() const override { return std::make_shared<Predicate>(name_, func_); }

private:
    std::string name_;
    PredicateFunc func_;
};

/**
 * @brief Unary operation (NOT)
 */
class UnaryOp : public STLFormula {
public:
    UnaryOp(FormulaType type, STLFormula::Ptr child)
        : STLFormula(type), child_(child) {}

    double robustness(const std::vector<Eigen::VectorXd>& trajectory,
                     const std::vector<double>& times) const override;

    std::string toString() const override;
    Ptr clone() const override;

protected:
    STLFormula::Ptr child_;
};

/**
 * @brief Binary operation (AND, OR, UNTIL)
 */
class BinaryOp : public STLFormula {
public:
    BinaryOp(FormulaType type, STLFormula::Ptr left, STLFormula::Ptr right,
             double time_lower = 0.0, double time_upper = 0.0)
        : STLFormula(type), left_(left), right_(right),
          time_lower_(time_lower), time_upper_(time_upper) {}

    double robustness(const std::vector<Eigen::VectorXd>& trajectory,
                     const std::vector<double>& times) const override;

    std::string toString() const override;
    Ptr clone() const override;

private:
    STLFormula::Ptr left_, right_;
    double time_lower_, time_upper_;  // For temporal operators
};

/**
 * @brief STL Formula parser
 */
class STLParser {
public:
    /**
     * @brief Parse STL formula from string
     * Supported syntax:
     * - "always[0,10](speed > 0.5)"
     * - "eventually[0,5](x > 10 && y < 20)"
     * - "(speed > 0.5) until[0,10](distance < 2.0)"
     */
    static STLFormula::Ptr parse(const std::string& formula_str,
                                 const std::map<std::string, PredicateFunc>& predicates);

    /**
     * @brief Register common predicates for robot navigation
     */
    static void registerNavigationPredicates(std::map<std::string, PredicateFunc>& predicates);
};

} // namespace stl_ros

#endif // STL_FORMULA_H
