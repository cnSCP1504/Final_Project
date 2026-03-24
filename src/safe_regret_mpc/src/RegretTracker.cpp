#include "safe_regret_mpc/RegretTracker.hpp"
#include "safe_regret_mpc/PerformanceMonitor.hpp"
#include <fstream>
#include <numeric>
#include <algorithm>

using namespace safe_regret_mpc;

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

RegretTracker::RegretTracker()
    : max_history_(10000),
      metrics_{},
      cumulative_tracking_error_(0.0),
      has_oracle_costs_(false)
{
    metrics_ = RegretMetrics{};
}

RegretTracker::~RegretTracker() {
}

// ========== Initialization ==========

bool RegretTracker::initialize(size_t max_history) {
    max_history_ = max_history;
    reset();
    return true;
}

// ========== Update Regret ==========

void RegretTracker::update(double actual_cost, double reference_cost,
                           const VectorXd& state, const VectorXd& control) {
    CostEntry entry;
    entry.actual_cost = actual_cost;
    entry.reference_cost = reference_cost;
    entry.oracle_cost = 0.0;  // Will be set if oracle available
    entry.state = state;
    entry.control = control;
    entry.time_stamp = 0.0;  // TODO: Use actual timestamp

    cost_history_.push_back(entry);

    // Maintain history size
    if (cost_history_.size() > max_history_) {
        cost_history_.pop_front();
    }

    // Update metrics
    double instantaneous_regret = actual_cost - reference_cost;
    metrics_.cumulative_regret += instantaneous_regret;
    metrics_.instantaneous_regret = instantaneous_regret;
    metrics_.time_step++;

    if (metrics_.time_step > 0) {
        metrics_.average_regret = metrics_.cumulative_regret / metrics_.time_step;
    }

    metrics_.reference_regret = reference_cost;

    // Compute safe regret
    metrics_.safe_regret = computeSafeRegret();
}

void RegretTracker::updateTrackingError(double tracking_error) {
    tracking_errors_.push_back(tracking_error);
    cumulative_tracking_error_ += tracking_error;

    metrics_.tracking_error_bound = cumulative_tracking_error_;
}

// ========== Get Metrics ==========

RegretTracker::RegretMetrics RegretTracker::getMetrics() const {
    return metrics_;
}

double RegretTracker::getRegretBound() const {
    return computeTheoreticalBound();
}

bool RegretTracker::verifyRegretBound() const {
    double bound = computeTheoreticalBound();
    return metrics_.cumulative_regret <= bound;
}

// ========== Reset ==========

void RegretTracker::reset() {
    cost_history_.clear();
    tracking_errors_.clear();
    oracle_costs_.clear();

    metrics_ = RegretMetrics{};
    cumulative_tracking_error_ = 0.0;
    has_oracle_costs_ = false;
}

// ========== Statistics ==========

std::tuple<double, double, double, double>
RegretTracker::computeRegretStatistics() const {
    if (cost_history_.empty()) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    std::vector<double> regrets;
    for (const auto& entry : cost_history_) {
        regrets.push_back(entry.actual_cost - entry.reference_cost);
    }

    return PerformanceMonitor::computeStatistics(regrets);
}

// ========== Oracle Costs ==========

void RegretTracker::setOracleCosts(const std::vector<double>& oracle_costs) {
    oracle_costs_ = oracle_costs;
    has_oracle_costs_ = true;
}

// ========== Export ==========

bool RegretTracker::exportData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    // Header
    file << "time_step,actual_cost,reference_cost,oracle_cost,"
         << "instantaneous_regret,cumulative_regret,safe_regret\n";

    // Data
    double cumulative = 0.0;
    for (size_t i = 0; i < cost_history_.size(); ++i) {
        const auto& entry = cost_history_[i];
        double regret = entry.actual_cost - entry.reference_cost;
        cumulative += regret;

        file << i << ","
             << entry.actual_cost << ","
             << entry.reference_cost << ","
             << (has_oracle_costs_ && i < oracle_costs_.size() ? oracle_costs_[i] : 0.0) << ","
             << regret << ","
             << cumulative << ","
             << metrics_.safe_regret << "\n";
    }

    file.close();
    return true;
}

// ========== Private Methods ==========

double RegretTracker::computeSafeRegret() const {
    // Safe regret = min over safe policies
    // For now, use cumulative regret as approximation
    // TODO: Implement full safe regret computation
    return metrics_.cumulative_regret;
}

double RegretTracker::computeTheoreticalBound() const {
    // o(T) bound: typically O(√T) for safe-regret algorithms
    // Bound = C·√T where C depends on problem parameters
    double C = 2.0;  // Problem-dependent constant
    return C * std::sqrt(metrics_.time_step);
}

bool RegretTracker::isSafePolicy(const std::vector<double>& costs) const {
    // Check if policy satisfies safety constraints
    // For now, assume all policies are safe if no violations
    // TODO: Implement actual safety check
    return true;
}

} // namespace safe_regret_mpc
