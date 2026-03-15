#include "STL_ros/RobustnessBudget.h"
#include <algorithm>
#include <numeric>

namespace stl_ros {

// ==================== RobustnessBudget Implementation ====================

RobustnessBudget::RobustnessBudget(double initial_budget, double baseline_r, double budget_max)
    : budget_(initial_budget), baseline_r_(baseline_r), budget_max_(budget_max) {

    if (baseline_r < 0) {
        throw std::invalid_argument("Baseline robustness r̲ must be non-negative");
    }
    if (budget_max <= 0) {
        throw std::invalid_argument("Maximum budget must be positive");
    }

    history_.clear();
}

double RobustnessBudget::update(double robustness) {
    // Simple budget update: R_{k+1} = R_k + ρ^soft(·) - r̲
    budget_ = budget_ + robustness - baseline_r_;

    // Clamp to [0, budget_max]
    budget_ = std::max(0.0, std::min(budget_, budget_max_));

    // Store in history
    history_.push_back(budget_);
    if (history_.size() > MAX_HISTORY_SIZE) {
        history_.pop_front();
    }

    return budget_;
}

double RobustnessBudget::updateAdaptive(double robustness, double satisfaction_rate) {
    // Adaptive baseline based on satisfaction rate
    // If satisfaction rate is low, reduce baseline to avoid excessive penalty
    double adaptive_baseline = baseline_r_ * satisfaction_rate;

    budget_ = budget_ + robustness - adaptive_baseline;
    budget_ = std::max(0.0, std::min(budget_, budget_max_));

    history_.push_back(budget_);
    if (history_.size() > MAX_HISTORY_SIZE) {
        history_.pop_front();
    }

    return budget_;
}

RobustnessBudget::Stats RobustnessBudget::getStats() const {
    Stats stats;

    if (history_.empty()) {
        stats.min_budget = budget_;
        stats.max_budget = budget_;
        stats.mean_budget = budget_;
        stats.violation_count = 0;
        return stats;
    }

    stats.min_budget = *std::min_element(history_.begin(), history_.end());
    stats.max_budget = *std::max_element(history_.begin(), history_.end());

    double sum = std::accumulate(history_.begin(), history_.end(), 0.0);
    stats.mean_budget = sum / history_.size();

    stats.violation_count = 0;
    for (double b : history_) {
        if (b < 0) stats.violation_count++;
    }

    return stats;
}

void RobustnessBudget::reset() {
    budget_ = 0.0;
    history_.clear();
}

double RobustnessBudget::getViolationProbability() const {
    if (history_.size() < 10) {
        return 0.0;  // Not enough data
    }

    int violations = 0;
    for (double b : history_) {
        if (b < 0) violations++;
    }

    return static_cast<double>(violations) / history_.size();
}

// ==================== BudgetConstraint Implementation ====================

BudgetConstraint::BudgetConstraint(RobustnessBudget& budget)
    : budget_(budget) {
}

double BudgetConstraint::evaluate(double robustness) const {
    // Constraint: R_{k+1} = R_k + ρ - r̲ >= 0
    // Return: R_k + ρ - r̲
    return budget_.getBudget() + robustness - budget_.getBaseline();
}

bool BudgetConstraint::isSatisfied(double robustness) const {
    return evaluate(robustness) >= 0.0;
}

// ==================== MultiFormulaBudget Implementation ====================

void MultiFormulaBudget::addFormula(const std::string& formula_name,
                                   double baseline_r,
                                   double priority) {
    FormulaInfo info;
    info.budget = RobustnessBudget(0.0, baseline_r);
    info.priority = priority;

    formulas_[formula_name] = info;
}

bool MultiFormulaBudget::updateAll(const std::map<std::string, double>& robustness_values) {
    bool all_feasible = true;

    for (const auto& pair : robustness_values) {
        const std::string& formula_name = pair.first;
        double robustness = pair.second;

        auto it = formulas_.find(formula_name);
        if (it != formulas_.end()) {
            it->second.budget.update(robustness);
            if (!it->second.budget.isFeasible()) {
                all_feasible = false;
            }
        }
    }

    return all_feasible;
}

bool MultiFormulaBudget::isFeasible() const {
    for (const auto& pair : formulas_) {
        if (!pair.second.budget.isFeasible()) {
            return false;
        }
    }
    return true;
}

std::string MultiFormulaBudget::getCriticalFormula() const {
    std::string critical;
    double min_budget = std::numeric_limits<double>::infinity();

    for (const auto& pair : formulas_) {
        double budget = pair.second.budget.getBudget();
        if (budget < min_budget) {
            min_budget = budget;
            critical = pair.first;
        }
    }

    return critical;
}

RobustnessBudget& MultiFormulaBudget::getBudget(const std::string& formula_name) {
    auto it = formulas_.find(formula_name);
    if (it == formulas_.end()) {
        throw std::runtime_error("Formula not found: " + formula_name);
    }
    return it->second.budget;
}

} // namespace stl_ros
