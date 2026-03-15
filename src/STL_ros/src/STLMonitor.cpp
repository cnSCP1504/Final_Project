#include "STL_ros/STLMonitor.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

namespace stl_ros {

// ==================== STLMonitor Implementation ====================

STLMonitor::STLMonitor(const Config& config) : config_(config) {
    // Create evaluators for formulas will be done when formulas are added
}

void STLMonitor::addFormula(const std::string& name, STLFormula::Ptr formula, double priority) {
    formulas_[name] = formula;
    priorities_[name] = priority;

    // Create budget for this formula
    RobustnessBudget budget(config_.initial_budget, config_.baseline_r);
    budgets_[name] = budget;

    // Create evaluator for this formula
    BeliefSpaceEvaluator evaluator(config_.num_samples, config_.tau);
    evaluators_[name] = evaluator;
}

void STLMonitor::removeFormula(const std::string& name) {
    formulas_.erase(name);
    priorities_.erase(name);
    budgets_.erase(name);
    evaluators_.erase(name);
}

std::map<std::string, MonitorResult> STLMonitor::evaluate(
    const BeliefState& current_belief,
    const std::vector<Eigen::VectorXd>& prediction_state) {

    std::map<std::string, MonitorResult> results;

    for (const auto& pair : formulas_) {
        const std::string& name = pair.first;
        const STLFormula::Ptr& formula = pair.second;

        MonitorResult result = evaluateFormula(name, current_belief, prediction_state);
        results[name] = result;
    }

    return results;
}

MonitorResult STLMonitor::evaluateFormula(
    const std::string& formula_name,
    const BeliefState& current_belief,
    const std::vector<Eigen::VectorXd>& prediction_state) {

    MonitorResult result;

    auto formula_it = formulas_.find(formula_name);
    if (formula_it == formulas_.end()) {
        throw std::runtime_error("Formula not found: " + formula_name);
    }

    auto evaluator_it = evaluators_.find(formula_name);
    if (evaluator_it == evaluators_.end()) {
        throw std::runtime_error("Evaluator not found for: " + formula_name);
    }

    STLFormula::Ptr formula = formula_it->second;
    BeliefSpaceEvaluator& evaluator = evaluator_it->second;

    // Create time vector
    std::vector<double> times;
    for (size_t i = 0; i < prediction_state.size(); ++i) {
        times.push_back(i * config_.dt);
    }

    // Compute expected robustness over belief
    auto start_time = std::chrono::high_resolution_clock::now();
    result.expected_robustness = evaluator.expectedRobustness(
        formula, current_belief, config_.prediction_horizon, config_.dt);

    // Also compute robustness for nominal trajectory
    result.robustness = evaluator.getSemantics().evaluate(
        formula, prediction_state, times);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.computation_time_ms = duration.count() / 1000.0;

    // Get budget status
    auto budget_it = budgets_.find(formula_name);
    if (budget_it != budgets_.end()) {
        result.budget = budget_it->second.getBudget();
        result.violation_probability = budget_it->second.getViolationProbability();
    }

    // Determine satisfaction
    result.satisfied = result.robustness > 0;

    // Store per-timestep robustness
    result.timestep_robustness.clear();
    for (size_t i = 0; i < prediction_state.size(); ++i) {
        std::vector<Eigen::VectorXd> single_state(1, prediction_state[i]);
        std::vector<double> single_time(1, times[i]);

        double step_robustness = evaluator.getSemantics().evaluate(
            formula, single_state, single_time);
        result.timestep_robustness.push_back(step_robustness);
    }

    // Store samples for debugging
    result.samples = evaluator.getSampleValues();

    return result;
}

double STLMonitor::getMPCObjective(
    double stage_cost,
    const std::map<std::string, MonitorResult>& results) const {

    double total_stl_cost = 0.0;

    for (const auto& pair : results) {
        const std::string& formula_name = pair.first;
        const MonitorResult& result = pair.second;

        auto priority_it = priorities_.find(formula_name);
        double priority = (priority_it != priorities_.end()) ? priority_it->second : 1.0;

        // Weighted robustness contribution
        total_stl_cost += priority * result.expected_robustness;
    }

    // J_k = E[ℓ(x,u)] - λ * E[ρ(φ)]
    return stage_cost - config_.lambda * total_stl_cost;
}

bool STLMonitor::isFeasible(const std::map<std::string, MonitorResult>& results) const {
    // Check all budgets are feasible
    for (const auto& pair : results) {
        if (pair.second.budget < 0) {
            return false;
        }
    }
    return true;
}

void STLMonitor::updateBudgets(const std::map<std::string, MonitorResult>& results) {
    for (const auto& pair : results) {
        const std::string& formula_name = pair.first;
        const MonitorResult& result = pair.second;

        auto budget_it = budgets_.find(formula_name);
        if (budget_it != budgets_.end()) {
            budget_it->second.update(result.robustness);
        }
    }
}

RobustnessBudget& STLMonitor::getBudget(const std::string& name) {
    auto it = budgets_.find(name);
    if (it == budgets_.end()) {
        throw std::runtime_error("Budget not found: " + name);
    }
    return it->second;
}

BeliefSpaceEvaluator STLMonitor::createEvaluator() const {
    return BeliefSpaceEvaluator(config_.num_samples, config_.tau);
}

bool STLMonitor::loadFormulasFromFile(const std::string& filename) {
    try {
        YAML::Node config = YAML::LoadFile(filename);

        for (const auto& node : config) {
            std::string name = node.first.as<std::string>();
            YAML::Node formula_config = node.second;

            if (formula_config["formula_string"]) {
                std::string formula_str = formula_config["formula_string"].as<std::string>();
                double priority = formula_config["priority"] ? formula_config["priority"].as<double>() : 1.0;

                // Parse formula (simplified - would use full parser in production)
                // For now, just store the string
                // STLFormula::Ptr formula = STLParser::parse(formula_str, predicates_);

                // Create placeholder formula
                auto formula = std::make_shared<Predicate>(name, [](const Eigen::VectorXd& state, double time) -> double {
                    return 1.0;  // Placeholder
                });

                addFormula(name, formula, priority);
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading formulas: " << e.what() << std::endl;
        return false;
    }
}

bool STLMonitor::saveFormulasToFile(const std::string& filename) const {
    try {
        YAML::Node config;

        for (const auto& pair : formulas_) {
            const std::string& name = pair.first;
            YAML::Node formula_config;

            formula_config["formula_string"] = pair.second->toString();

            auto priority_it = priorities_.find(name);
            if (priority_it != priorities_.end()) {
                formula_config["priority"] = priority_it->second;
            }

            config[name] = formula_config;
        }

        std::ofstream fout(filename);
        fout << config;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving formulas: " << e.what() << std::endl;
        return false;
    }
}

void STLMonitor::reset() {
    for (auto& pair : budgets_) {
        pair.second.reset();
    }
}

// ==================== RealtimeSTLMonitor Implementation ====================

RealtimeSTLMonitor::RealtimeSTLMonitor(const Config& config)
    : STLMonitor(config), cache_enabled_(true) {
    cache_.clear();
}

std::map<std::string, MonitorResult> RealtimeSTLMonitor::evaluateFast(
    const BeliefState& current_belief,
    const std::vector<Eigen::VectorXd>& prediction_state) {

    if (!cache_enabled_) {
        return evaluate(current_belief, prediction_state);
    }

    // Check cache for similar inputs
    for (const auto& entry : cache_) {
        // Simple cache hit detection (would use more sophisticated metrics)
        bool belief_match = (entry.belief.mean - current_belief.mean).norm() < 0.01;
        bool traj_match = (entry.trajectory.size() == prediction_state.size());

        if (belief_match && traj_match) {
            return entry.results;  // Cache hit
        }
    }

    // Cache miss - compute and store
    auto results = evaluate(current_belief, prediction_state);

    if (cache_.size() >= MAX_CACHE_SIZE) {
        cache_.erase(cache_.begin());
    }

    CacheEntry new_entry;
    new_entry.belief = current_belief;
    new_entry.trajectory = prediction_state;
    new_entry.results = results;
    cache_.push_back(new_entry);

    return results;
}

void RealtimeSTLMonitor::clearCache() {
    cache_.clear();
}

} // namespace stl_ros
