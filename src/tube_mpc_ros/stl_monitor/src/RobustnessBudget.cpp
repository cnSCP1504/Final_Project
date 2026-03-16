#include "stl_monitor/RobustnessBudget.h"
#include <numeric>
#include <algorithm>

namespace stl_monitor {

RobustnessBudget::RobustnessBudget()
    : history_size_(100),
      decay_rate_(0.0),
      stats_cached_(false)
{
    state_.budget = 1.0;
    state_.baseline_requirement = 0.1;
    state_.last_robustness = 0.0;
    state_.violation_count = 0.0;
}

RobustnessBudget::~RobustnessBudget() {}

void RobustnessBudget::initialize(double initial_value, double baseline_requirement) {
    state_.budget = initial_value;
    state_.baseline_requirement = baseline_requirement;
    state_.last_robustness = 0.0;
    state_.violation_count = 0.0;
    clearHistory();
}

double RobustnessBudget::update(double robustness) {
    // Update budget: R_{k+1} = max{0, R_k + ρ_k - r̲}
    state_.last_robustness = robustness;
    state_.budget = std::max(0.0, state_.budget + robustness - state_.baseline_requirement);

    // Apply decay if enabled
    if (decay_rate_ > 0.0) {
        state_.budget *= (1.0 - decay_rate_);
    }

    // Track violations
    if (robustness < 0.0) {
        state_.violation_count += 1.0;
    }

    // Update history
    history_.push_back(state_.budget);
    if (history_.size() > history_size_) {
        history_.pop_front();
    }

    // Invalidate statistics cache
    invalidateCache_();

    return state_.budget;
}

double RobustnessBudget::getBudget() const {
    return state_.budget;
}

const BudgetState& RobustnessBudget::getState() const {
    return state_;
}

void RobustnessBudget::reset() {
    state_.budget = 1.0;
    state_.last_robustness = 0.0;
    state_.violation_count = 0.0;
    clearHistory();
}

bool RobustnessBudget::isHealthy(double threshold) const {
    return state_.budget >= threshold;
}

void RobustnessBudget::setBaselineRequirement(double r_baseline) {
    state_.baseline_requirement = r_baseline;
}

void RobustnessBudget::setDecayRate(double decay_rate) {
    decay_rate_ = std::max(0.0, std::min(1.0, decay_rate));
}

const std::deque<double>& RobustnessBudget::getHistory() const {
    return history_;
}

void RobustnessBudget::clearHistory() {
    history_.clear();
    invalidateCache_();
}

void RobustnessBudget::setHistorySize(size_t size) {
    history_size_ = size;
    while (history_.size() > history_size_) {
        history_.pop_front();
    }
}

RobustnessBudget::BudgetStats RobustnessBudget::getStatistics() const {
    if (!stats_cached_) {
        updateStatistics_();
    }
    return cached_stats_;
}

std::vector<double> RobustnessBudget::predictFuture(
    size_t horizon,
    double expected_robustness) const
{
    std::vector<double> predictions;
    predictions.reserve(horizon);

    double budget = state_.budget;
    double r_baseline = state_.baseline_requirement;

    for (size_t i = 0; i < horizon; ++i) {
        budget = std::max(0.0, budget + expected_robustness - r_baseline);

        // Apply decay if enabled
        if (decay_rate_ > 0.0) {
            budget *= (1.0 - decay_rate_);
        }

        predictions.push_back(budget);
    }

    return predictions;
}

bool RobustnessBudget::checkSufficiency(
    size_t horizon,
    double expected_robustness,
    double threshold) const
{
    auto predictions = predictFuture(horizon, expected_robustness);

    // Check if all predictions are above threshold
    for (double pred : predictions) {
        if (pred < threshold) {
            return false;
        }
    }

    return true;
}

void RobustnessBudget::invalidateCache_() {
    stats_cached_ = false;
}

void RobustnessBudget::updateStatistics_() const {
    BudgetStats stats;
    stats.violation_count = static_cast<size_t>(state_.violation_count);

    if (history_.empty()) {
        stats.mean = 0.0;
        stats.min = 0.0;
        stats.max = 0.0;
        stats.std = 0.0;
    } else {
        // Mean
        double sum = std::accumulate(history_.begin(), history_.end(), 0.0);
        stats.mean = sum / history_.size();

        // Min/Max
        auto minmax = std::minmax_element(history_.begin(), history_.end());
        stats.min = *minmax.first;
        stats.max = *minmax.second;

        // Standard deviation
        double variance = 0.0;
        for (double val : history_) {
            variance += (val - stats.mean) * (val - stats.mean);
        }
        variance /= history_.size();
        stats.std = std::sqrt(variance);
    }

    cached_stats_ = stats;
    stats_cached_ = true;
}

} // namespace stl_monitor
