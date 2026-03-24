#include "safe_regret_mpc/PerformanceMonitor.hpp"
#include <fstream>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace safe_regret_mpc;

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

PerformanceMonitor::PerformanceMonitor()
    : failed_solves_(0),
      safety_margin_(0.0),
      constraint_violations_(0),
      probability_violation_(0.0),
      tube_radius_(0.0),
      feasible_solves_(0),
      total_solves_(0),
      terminal_feasible_solves_(0),
      recursive_feasibility_(true),
      max_history_size_(1000),
      initialized_(false)
{
    metrics_ = PerformanceMetrics{};
}

PerformanceMonitor::~PerformanceMonitor() {
}

// ========== Initialization ==========

bool PerformanceMonitor::initialize() {
    reset();
    initialized_ = true;
    return true;
}

// ========== Timing ==========

void PerformanceMonitor::startTiming() {
    current_start_ = std::chrono::high_resolution_clock::now();
}

PerformanceMonitor::TimingData PerformanceMonitor::stopTiming() {
    TimingData data;
    data.end_time = std::chrono::high_resolution_clock::now();
    data.start_time = current_start_;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        data.end_time - data.start_time);
    data.total_time = duration.count() / 1000.0;  // Convert to ms

    // TODO: Add MPC, STL, DR timing breakdown
    data.mpc_solve_time = data.total_time;
    data.stl_eval_time = 0.0;
    data.dr_compute_time = 0.0;

    timing_history_.push_back(data);
    if (timing_history_.size() > max_history_size_) {
        timing_history_.pop_front();
    }

    return data;
}

// ========== Solve Time Recording ==========

void PerformanceMonitor::recordSolveTime(double solve_time, bool success) {
    solve_times_.push_back(solve_time);
    solve_success_.push_back(success);

    if (solve_times_.size() > max_history_size_) {
        solve_times_.pop_front();
        solve_success_.pop_front();
    }

    total_solves_++;
    if (!success) {
        failed_solves_++;
    } else {
        feasible_solves_++;
    }

    updateStatistics();
}

// ========== Performance Updates ==========

void PerformanceMonitor::updateTrackingPerformance(
    double cte, double etheta, double evel, double lateral_dev) {
    cte_history_.push_back(cte);
    etheta_history_.push_back(etheta);
    evel_history_.push_back(evel);
    lateral_dev_history_.push_back(lateral_dev);

    if (cte_history_.size() > max_history_size_) {
        cte_history_.pop_front();
        etheta_history_.pop_front();
        evel_history_.pop_front();
        lateral_dev_history_.pop_front();
    }

    metrics_.cross_track_error = cte;
    metrics_.heading_error = etheta;
    metrics_.velocity_error = evel;
    metrics_.lateral_deviation = lateral_dev;
}

void PerformanceMonitor::updateControlPerformance(
    double control_effort, double control_smoothness) {
    control_effort_history_.push_back(control_effort);
    control_smoothness_history_.push_back(control_smoothness);

    if (control_effort_history_.size() > max_history_size_) {
        control_effort_history_.pop_front();
        control_smoothness_history_.pop_front();
    }

    metrics_.control_effort = control_effort;
    metrics_.control_smoothness = control_smoothness;
}

void PerformanceMonitor::updateSafetyMetrics(
    double safety_margin, double prob_violation, double tube_radius) {
    safety_margin_ = safety_margin;
    probability_violation_ = prob_violation;
    tube_radius_ = tube_radius;

    metrics_.safety_margin = safety_margin;
    metrics_.probability_violation = prob_violation;
    metrics_.tube_radius = tube_radius;
}

void PerformanceMonitor::recordViolation() {
    constraint_violations_++;
    metrics_.constraint_violations = constraint_violations_;
}

void PerformanceMonitor::updateFeasibility(bool feasible, bool terminal_feasible) {
    total_solves_++;
    if (feasible) {
        feasible_solves_++;
    }
    if (terminal_feasible) {
        terminal_feasible_solves_++;
    }

    recursive_feasibility_ = (feasible && terminal_feasible);

    metrics_.feasibility_rate = static_cast<double>(feasible_solves_) / total_solves_;
    metrics_.recursive_feasibility = recursive_feasibility_;
    metrics_.terminal_feasibility = static_cast<double>(terminal_feasible_solves_) / total_solves_;
}

// ========== Get Metrics ==========

PerformanceMonitor::PerformanceMetrics PerformanceMonitor::getMetrics() const {
    return metrics_;
}

double PerformanceMonitor::computePerformanceScore() {
    // Compute overall performance score (0-1)
    // Based on multiple factors

    double tracking_score = 1.0 / (1.0 + std::abs(metrics_.cross_track_error) +
                                   std::abs(metrics_.heading_error));

    double safety_score = 1.0 / (1.0 + metrics_.constraint_violations +
                                metrics_.probability_violation);

    double feasibility_score = metrics_.feasibility_rate;
    double efficiency_score = 1.0 / (1.0 + metrics_.solve_time_mean / 100.0);  // 100ms target

    // Weighted average
    metrics_.performance_score = 0.3 * tracking_score +
                                  0.3 * safety_score +
                                  0.2 * feasibility_score +
                                  0.2 * efficiency_score;

    return metrics_.performance_score;
}

std::string PerformanceMonitor::getPerformanceGrade() {
    double score = computePerformanceScore();

    if (score >= 0.9) return "A";
    if (score >= 0.8) return "B";
    if (score >= 0.7) return "C";
    if (score >= 0.6) return "D";
    return "F";
}

// ========== Reset ==========

void PerformanceMonitor::reset() {
    solve_times_.clear();
    solve_success_.clear();
    failed_solves_ = 0;

    cte_history_.clear();
    etheta_history_.clear();
    evel_history_.clear();
    lateral_dev_history_.clear();

    control_effort_history_.clear();
    control_smoothness_history_.clear();

    safety_margin_ = 0.0;
    constraint_violations_ = 0;
    probability_violation_ = 0.0;
    tube_radius_ = 0.0;

    feasible_solves_ = 0;
    total_solves_ = 0;
    terminal_feasible_solves_ = 0;
    recursive_feasibility_ = true;

    timing_history_.clear();

    metrics_ = PerformanceMetrics{};
    metrics_.performance_grade = "N/A";
}

// ========== Export ==========

bool PerformanceMonitor::exportData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    // Header
    file << "time_step,solve_time,success,cte,etheta,evel,lateral_dev,"
         << "control_effort,control_smoothness,safety_margin,prob_violation\n";

    // Data
    for (size_t i = 0; i < solve_times_.size(); ++i) {
        file << i << ","
             << solve_times_[i] << ","
             << solve_success_[i] << ","
             << (i < cte_history_.size() ? cte_history_[i] : 0.0) << ","
             << (i < etheta_history_.size() ? etheta_history_[i] : 0.0) << ","
             << (i < evel_history_.size() ? evel_history_[i] : 0.0) << ","
             << (i < lateral_dev_history_.size() ? lateral_dev_history_[i] : 0.0) << ","
             << (i < control_effort_history_.size() ? control_effort_history_[i] : 0.0) << ","
             << (i < control_smoothness_history_.size() ? control_smoothness_history_[i] : 0.0) << ","
             << safety_margin_ << ","
             << probability_violation_ << "\n";
    }

    file.close();
    return true;
}

// ========== Static Utilities ==========

std::tuple<double, double, double, double>
PerformanceMonitor::computeStatistics(const std::vector<double>& values) {
    if (values.empty()) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();

    double variance = 0.0;
    for (double val : values) {
        variance += (val - mean) * (val - mean);
    }
    variance /= values.size();
    double std_dev = std::sqrt(variance);

    auto minmax = std::minmax_element(values.begin(), values.end());

    return {mean, std_dev, *minmax.first, *minmax.second};
}

// ========== Private Methods ==========

void PerformanceMonitor::updateStatistics() {
    if (solve_times_.empty()) {
        metrics_.solve_time_mean = 0.0;
        metrics_.solve_time_max = 0.0;
        metrics_.solve_time_std = 0.0;
        return;
    }

    auto [mean, std_dev, min_val, max_val] = computeStatistics(
        std::vector<double>(solve_times_.begin(), solve_times_.end()));

    metrics_.solve_time_mean = mean;
    metrics_.solve_time_max = max_val;
    metrics_.solve_time_std = std_dev;
    metrics_.frequency = computeFrequency();
}

double PerformanceMonitor::computeFrequency() {
    if (timing_history_.size() < 2) {
        return 0.0;
    }

    // Compute average time difference
    double total_time = 0.0;
    for (size_t i = 1; i < timing_history_.size(); ++i) {
        double dt = std::chrono::duration<double>(
            timing_history_[i].start_time - timing_history_[i-1].start_time).count();
        total_time += dt;
    }

    double avg_dt = total_time / (timing_history_.size() - 1);
    return (avg_dt > 0) ? (1.0 / avg_dt) : 0.0;
}

} // namespace safe_regret_mpc
