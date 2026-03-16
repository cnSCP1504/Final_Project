#ifndef STL_MONITOR_STLPARSER_H
#define STL_MONITOR_STLPARSER_H

#include "stl_monitor/STLTypes.h"
#include <map>
#include <functional>

namespace stl_monitor {

/**
 * @brief STL Parser - Parse and construct STL formulas
 *
 * Supports:
 * - Atomic predicates: μ(x) ≥ 0
 * - Logical operators: ¬φ, φ₁ ∧ φ₂, φ₁ ∨ φ₂
 * - Temporal operators: ◇_I φ, □_I φ, φ₁ U_I φ₂
 */
class STLParser {
public:
    STLParser();
    ~STLParser();

    /**
     * @brief Parse STL formula from string
     * @param formula_str String representation of formula
     * @return STLFormula structure
     */
    STLFormula parse(const std::string& formula_str);

    /**
     * @brief Create atomic predicate
     */
    std::shared_ptr<STLFormulaNode> createAtom(
        const std::string& name,
        PredicateType type,
        double threshold,
        const Eigen::VectorXd& params = Eigen::VectorXd()
    );

    /**
     * @brief Create negation: ¬φ
     */
    std::shared_ptr<STLFormulaNode> createNot(
        std::shared_ptr<STLFormulaNode> phi
    );

    /**
     * @brief Create conjunction: φ₁ ∧ φ₂
     */
    std::shared_ptr<STLFormulaNode> createAnd(
        std::shared_ptr<STLFormulaNode> phi1,
        std::shared_ptr<STLFormulaNode> phi2
    );

    /**
     * @brief Create disjunction: φ₁ ∨ φ₂
     */
    std::shared_ptr<STLFormulaNode> createOr(
        std::shared_ptr<STLFormulaNode> phi1,
        std::shared_ptr<STLFormulaNode> phi2
    );

    /**
     * @brief Create eventually: ◇_I φ
     */
    std::shared_ptr<STLFormulaNode> createEventually(
        const TimeInterval& interval,
        std::shared_ptr<STLFormulaNode> phi
    );

    /**
     * @brief Create globally: □_I φ
     */
    std::shared_ptr<STLFormulaNode> createAlways(
        const TimeInterval& interval,
        std::shared_ptr<STLFormulaNode> phi
    );

    /**
     * @brief Create until: φ₁ U_I φ₂
     */
    std::shared_ptr<STLFormulaNode> createUntil(
        const TimeInterval& interval,
        std::shared_ptr<STLFormulaNode> phi1,
        std::shared_ptr<STLFormulaNode> phi2
    );

    /**
     * @brief Validate formula structure
     */
    bool validate(const STLFormula& formula) const;

    /**
     * @brief Get formula as string for debugging
     */
    std::string toString(const STLFormula& formula) const;
    std::string toString(const STLFormulaNode* node) const;

    /**
     * @brief Register custom predicate evaluation function
     */
    using PredicateFunc = std::function<double(const Eigen::VectorXd&, const AtomicPredicate&)>;
    void registerPredicate(const std::string& name, PredicateFunc func);

private:
    std::map<std::string, PredicateFunc> predicate_functions_;

    // Helper functions
    std::vector<std::string> tokenize_(const std::string& str) const;
    std::shared_ptr<STLFormulaNode> parseExpression_(const std::vector<std::string>& tokens, size_t& pos) const;
};

} // namespace stl_monitor

#endif // STL_MONITOR_STLPARSER_H
