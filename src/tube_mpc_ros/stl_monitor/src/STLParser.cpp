#include "stl_monitor/STLParser.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <set>

namespace stl_monitor {

STLParser::STLParser() {
    // Register default predicates
    registerPredicate("reachability",
        [](const Eigen::VectorXd& state, const AtomicPredicate& pred) -> double {
            // distance to target < threshold
            // state: [x, y, theta, goal_x, goal_y]
            if (state.size() < 5) return 0.0;
            double dx = state[0] - state[3];
            double dy = state[1] - state[4];
            double dist = std::sqrt(dx*dx + dy*dy);
            return pred.threshold - dist;  // positive when satisfied
        });

    registerPredicate("safety",
        [](const Eigen::VectorXd& state, const AtomicPredicate& pred) -> double {
            // distance to obstacles > threshold
            // For now, return dummy value
            return 1.0;  // TODO: implement obstacle distance
        });

    registerPredicate("velocity_limit",
        [](const Eigen::VectorXd& state, const AtomicPredicate& pred) -> double {
            // velocity within bounds
            if (state.size() < 3) return 0.0;
            double v = state[2];  // velocity
            return pred.threshold - std::abs(v);
        });
}

STLParser::~STLParser() {}

STLFormula STLParser::parse(const std::string& formula_str) {
    // Simple parsing for common patterns
    // TODO: Implement full recursive descent parser

    STLFormula formula;

    if (formula_str.find("F") == 0 || formula_str.find("eventually") == 0) {
        // Eventually formula
        auto atom = createAtom("reachability", PredicateType::REACHABILITY, 0.5);
        formula.root = createEventually(TimeInterval(0, 10), atom);
        formula.description = "Eventually reach goal";
    }
    else if (formula_str.find("G") == 0 || formula_str.find("always") == 0) {
        // Always formula
        auto atom = createAtom("safety", PredicateType::SAFETY, 0.3);
        formula.root = createAlways(TimeInterval(0, std::numeric_limits<double>::infinity()), atom);
        formula.description = "Always stay safe";
    }
    else {
        // Default: simple reachability
        formula.root = createAtom("reachability", PredicateType::REACHABILITY, 0.5);
        formula.description = "Simple reachability";
    }

    return formula;
}

std::shared_ptr<STLFormulaNode> STLParser::createAtom(
    const std::string& name,
    PredicateType type,
    double threshold,
    const Eigen::VectorXd& params)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::ATOM);
    node->atom.name = name;
    node->atom.type = type;
    node->atom.threshold = threshold;
    node->atom.parameters = params;
    return node;
}

std::shared_ptr<STLFormulaNode> STLParser::createNot(
    std::shared_ptr<STLFormulaNode> phi)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::NOT);
    node->addChild(phi);
    return node;
}

std::shared_ptr<STLFormulaNode> STLParser::createAnd(
    std::shared_ptr<STLFormulaNode> phi1,
    std::shared_ptr<STLFormulaNode> phi2)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::AND);
    node->addChild(phi1);
    node->addChild(phi2);
    return node;
}

std::shared_ptr<STLFormulaNode> STLParser::createOr(
    std::shared_ptr<STLFormulaNode> phi1,
    std::shared_ptr<STLFormulaNode> phi2)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::OR);
    node->addChild(phi1);
    node->addChild(phi2);
    return node;
}

std::shared_ptr<STLFormulaNode> STLParser::createEventually(
    const TimeInterval& interval,
    std::shared_ptr<STLFormulaNode> phi)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::EVENTUALLY);
    node->interval = interval;
    node->addChild(phi);
    return node;
}

std::shared_ptr<STLFormulaNode> STLParser::createAlways(
    const TimeInterval& interval,
    std::shared_ptr<STLFormulaNode> phi)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::ALWAYS);
    node->interval = interval;
    node->addChild(phi);
    return node;
}

std::shared_ptr<STLFormulaNode> STLParser::createUntil(
    const TimeInterval& interval,
    std::shared_ptr<STLFormulaNode> phi1,
    std::shared_ptr<STLFormulaNode> phi2)
{
    auto node = std::make_shared<STLFormulaNode>(STLOperator::UNTIL);
    node->interval = interval;
    node->addChild(phi1);
    node->addChild(phi2);
    return node;
}

bool STLParser::validate(const STLFormula& formula) const {
    if (!formula.root) return false;

    // Basic validation: check for cycles and valid structure
    std::set<const STLFormulaNode*> visited;
    std::function<bool(const STLFormulaNode*)> validateNode =
        [&](const STLFormulaNode* node) -> bool {
            if (!node) return false;
            if (visited.find(node) != visited.end()) return false;  // cycle
            visited.insert(node);

            if (node->isLeaf()) return true;

            for (const auto& child : node->children) {
                if (!validateNode(child.get())) return false;
            }

            return true;
        };

    return validateNode(formula.root.get());
}

std::string STLParser::toString(const STLFormula& formula) const {
    return toString(formula.root.get());
}

std::string STLParser::toString(const STLFormulaNode* node) const {
    if (!node) return "null";

    switch (node->op) {
        case STLOperator::ATOM:
            return node->atom.name;
        case STLOperator::NOT:
            return "¬(" + toString(node->children[0].get()) + ")";
        case STLOperator::AND:
            return "(" + toString(node->children[0].get()) + " ∧ " +
                   toString(node->children[1].get()) + ")";
        case STLOperator::OR:
            return "(" + toString(node->children[0].get()) + " ∨ " +
                   toString(node->children[1].get()) + ")";
        case STLOperator::EVENTUALLY:
            return "◇_" + std::to_string(node->interval.lower) +
                   "^" + std::to_string(node->interval.upper) +
                   "(" + toString(node->children[0].get()) + ")";
        case STLOperator::ALWAYS:
            return "□_" + std::to_string(node->interval.lower) +
                   "^" + std::to_string(node->interval.upper) +
                   "(" + toString(node->children[0].get()) + ")";
        case STLOperator::UNTIL:
            return "(" + toString(node->children[0].get()) + " U_" +
                   std::to_string(node->interval.lower) +
                   "^" + std::to_string(node->interval.upper) +
                   toString(node->children[1].get()) + ")";
        default:
            return "unknown";
    }
}

void STLParser::registerPredicate(const std::string& name, PredicateFunc func) {
    predicate_functions_[name] = func;
}

std::vector<std::string> STLParser::tokenize_(const std::string& str) const {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace stl_monitor
