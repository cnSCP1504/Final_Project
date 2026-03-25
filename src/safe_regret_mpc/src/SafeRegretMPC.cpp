#include "safe_regret_mpc/SafeRegretMPC.hpp"
#include <iostream>
#include <chrono>
#include <IpIpoptApplication.hpp>
#include <IpTNLP.hpp>

using namespace safe_regret_mpc;
using namespace Eigen;

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

SafeRegretMPC::SafeRegretMPC()
    : state_dim_(6),
      input_dim_(2),
      mpc_steps_(20),
      ref_vel_(1.0),
      stl_enabled_(false),
      stl_weight_(10.0),
      stl_robustness_(0.0),
      stl_budget_(1.0),
      stl_baseline_(0.1),
      dr_enabled_(false),
      dr_delta_(0.1),
      terminal_set_enabled_(false),
      terminal_radius_(0.0),
      cost_value_(0.0),
      mpc_feasible_(false),
      solve_time_(0.0),
      cppad_initialized_(false),
      debug_mode_(false)
{
    // Initialize integrators
    stl_integrator_ = std::make_unique<STLConstraintIntegrator>();
    dr_integrator_ = std::make_unique<DRConstraintIntegrator>();
    terminal_integrator_ = std::make_unique<TerminalSetIntegrator>();
    regret_tracker_ = std::make_unique<RegretTracker>();
    perf_monitor_ = std::make_unique<PerformanceMonitor>();

    // Initialize matrices
    x_min_ = VectorXd::Zero(state_dim_);
    x_max_ = VectorXd::Zero(state_dim_);
    u_min_ = VectorXd::Zero(input_dim_);
    u_max_ = VectorXd::Zero(input_dim_);
    A_ = MatrixXd::Identity(state_dim_, state_dim_);
    B_ = MatrixXd::Zero(state_dim_, input_dim_);
    G_ = MatrixXd::Zero(state_dim_, state_dim_);
    Q_ = MatrixXd::Identity(state_dim_, state_dim_);
    R_ = MatrixXd::Identity(input_dim_, input_dim_);

    // Initialize terminal center
    terminal_center_ = VectorXd::Zero(state_dim_);
}

SafeRegretMPC::~SafeRegretMPC() {
    // Unique pointers automatically clean up
}

// ========== Initialization ==========

bool SafeRegretMPC::initialize() {
    std::cout << "Initializing Safe-Regret MPC..." << std::endl;

    // Initialize integrators
    if (!stl_integrator_->initialize()) {
        std::cerr << "Failed to initialize STL integrator!" << std::endl;
        return false;
    }

    if (!dr_integrator_->initialize()) {
        std::cerr << "Failed to initialize DR integrator!" << std::endl;
        return false;
    }

    if (!terminal_integrator_->initialize()) {
        std::cerr << "Failed to initialize terminal set integrator!" << std::endl;
        return false;
    }

    if (!regret_tracker_->initialize()) {
        std::cerr << "Failed to initialize regret tracker!" << std::endl;
        return false;
    }

    if (!perf_monitor_->initialize()) {
        std::cerr << "Failed to initialize performance monitor!" << std::endl;
        return false;
    }

    // Set system dynamics for terminal set
    terminal_integrator_->setSystemDynamics(A_, B_);

    // Initialize CppAD
    initializeCppAD();

    std::cout << "Safe-Regret MPC initialized successfully!" << std::endl;
    return true;
}

void SafeRegretMPC::initializeCppAD() {
    // Setup variable sizes
    // Each step: 6 states + 2 inputs
    nx_.resize(mpc_steps_ + 1);
    for (size_t i = 0; i <= mpc_steps_; ++i) {
        nx_[i] = state_dim_ + input_dim_;
    }

    // Constraints: state constraints + terminal constraint + dynamics
    ng_.resize(mpc_steps_ + 1);
    for (size_t i = 0; i < mpc_steps_; ++i) {
        ng_[i] = state_dim_;  // Dynamics constraints
    }
    ng_[mpc_steps_] = state_dim_;  // Terminal constraints

    cppad_initialized_ = true;
}

// ========== Solve MPC ==========

bool SafeRegretMPC::solve(const VectorXd& current_state,
                         const std::vector<VectorXd>& reference_trajectory) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "Solving Safe-Regret MPC..." << std::endl;

    // Start performance monitoring
    perf_monitor_->startTiming();

    // Update STL robustness if enabled
    if (stl_enabled_) {
        // For now, use simple belief space
        MatrixXd cov = MatrixXd::Identity(state_dim_, state_dim_) * 0.1;
        stl_robustness_ = stl_integrator_->computeBeliefRobustness(
            current_state, cov, reference_trajectory);

        // Update budget
        stl_budget_ = stl_integrator_->updateBudget(stl_robustness_);

        std::cout << "STL robustness: " << stl_robustness_
                  << ", budget: " << stl_budget_ << std::endl;
    }

    // Initialize optimization variables along reference trajectory for better warm start
    std::vector<double> vars((state_dim_ + input_dim_) * (mpc_steps_ + 1), 0.0);

    // Initialize states along reference trajectory
    for (size_t t = 0; t <= mpc_steps_; ++t) {
        size_t idx = t * (state_dim_ + input_dim_);

        if (t == 0) {
            // First state is current state
            for (size_t i = 0; i < state_dim_; ++i) {
                vars[idx + i] = current_state(i);
            }
        } else if (t < reference_trajectory.size()) {
            // Use reference trajectory if available
            for (size_t i = 0; i < state_dim_; ++i) {
                vars[idx + i] = reference_trajectory[t](i);
            }
        } else {
            // Repeat last reference state
            for (size_t i = 0; i < state_dim_; ++i) {
                vars[idx + i] = reference_trajectory.back()(i);
            }
        }

        // Initialize inputs to small forward velocity
        if (t < mpc_steps_) {
            vars[idx + state_dim_] = 0.1;  // Small linear velocity
            vars[idx + state_dim_ + 1] = 0.0;  // Zero angular velocity
        }
    }

    // Solve with Ipopt solver (implemented in SafeRegretMPCSolver.cpp)
    bool solve_success = solveWithIpopt(vars);

    auto end_time = std::chrono::high_resolution_clock::now();
    solve_time_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    perf_monitor_->recordSolveTime(solve_time_, solve_success);

    mpc_feasible_ = solve_success;

    if (solve_success) {
        // Extract optimal control
        optimal_control_ = VectorXd::Zero(input_dim_);
        for (size_t i = 0; i < input_dim_; ++i) {
            optimal_control_(i) = vars[state_dim_ + i];
        }

        std::cout << "MPC solved successfully in " << solve_time_ << " ms" << std::endl;
    } else {
        std::cerr << "MPC solve failed!" << std::endl;
    }

    return solve_success;
}

VectorXd SafeRegretMPC::getOptimalControl() const {
    return optimal_control_;
}

std::vector<VectorXd> SafeRegretMPC::getPredictedTrajectory() const {
    return predicted_trajectory_;
}

double SafeRegretMPC::getCostValue() const {
    return cost_value_;
}

// ========== STL Integration ==========

void SafeRegretMPC::setSTLSpecification(const std::string& formula,
                                       double baseline,
                                       double weight) {
    stl_integrator_->setFormula(formula);
    stl_integrator_->setBaseline(baseline);
    stl_weight_ = weight;
    stl_baseline_ = baseline;
    stl_enabled_ = true;

    std::cout << "STL specification set: " << formula << std::endl;
}

double SafeRegretMPC::updateSTLRobustness(const VectorXd& belief_mean,
                                          const MatrixXd& belief_cov) {
    // TODO: Update robustness with belief space
    return stl_robustness_;
}

double SafeRegretMPC::getSTLRobustness() const {
    return stl_robustness_;
}

double SafeRegretMPC::getSTLBudget() const {
    return stl_budget_;
}

// ========== DR Integration ==========

void SafeRegretMPC::updateDRMargins(const std::vector<VectorXd>& residuals) {
    if (!dr_enabled_) return;

    // Add residuals to DR integrator
    for (const auto& residual : residuals) {
        dr_integrator_->addResidual(residual);
    }

    // Compute margins for horizon
    double tube_radius = 0.5;  // TODO: Get from Tube MPC
    double lipschitz_const = 1.0;
    dr_margins_ = dr_integrator_->computeMargins(mpc_steps_, tube_radius, lipschitz_const);

    std::cout << "DR margins updated, count: " << dr_margins_.size() << std::endl;
}

std::vector<double> SafeRegretMPC::getDRMargins() const {
    return dr_margins_;
}

void SafeRegretMPC::setRiskLevel(double delta) {
    dr_delta_ = delta;
    dr_integrator_->setRiskLevel(delta);
}

// ========== Terminal Set Integration ==========

void SafeRegretMPC::setTerminalSet(const VectorXd& center, double radius) {
    terminal_center_ = center;
    terminal_radius_ = radius;
    terminal_set_enabled_ = true;

    terminal_integrator_->setTerminalSet(center, radius);

    std::cout << "Terminal set set: center=["
              << center.transpose() << "], radius=" << radius << std::endl;
}

bool SafeRegretMPC::checkTerminalFeasibility(const VectorXd& terminal_state) const {
    if (!terminal_set_enabled_) return true;

    return terminal_integrator_->checkFeasibility(terminal_state);
}

// ========== Regret Tracking ==========

void SafeRegretMPC::updateRegret(double reference_cost, double actual_cost) {
    regret_tracker_->update(actual_cost, reference_cost,
                           VectorXd::Zero(state_dim_),  // TODO: actual state
                           VectorXd::Zero(input_dim_));  // TODO: actual control
}

double SafeRegretMPC::getCumulativeRegret() const {
    return regret_tracker_->getCumulativeRegret();
}

double SafeRegretMPC::getSafeRegret() const {
    return regret_tracker_->getSafeRegret();
}

void SafeRegretMPC::resetRegret() {
    regret_tracker_->reset();
}

// ========== Performance Monitoring ==========

PerformanceMonitor::PerformanceMetrics SafeRegretMPC::getPerformanceMetrics() const {
    return perf_monitor_->getMetrics();
}

void SafeRegretMPC::updatePerformanceMonitoring() {
    perf_monitor_->updateTrackingPerformance(0.0, 0.0, 0.0, 0.0);  // TODO: actual values
}

// ========== Parameters ==========

void SafeRegretMPC::setHorizon(size_t N) {
    mpc_steps_ = N;
    initializeCppAD();  // Rebuild with new horizon
}

void SafeRegretMPC::setSystemDynamics(const MatrixXd& A, const MatrixXd& B, const MatrixXd& G) {
    A_ = A;
    B_ = B;
    G_ = G;
    terminal_integrator_->setSystemDynamics(A, B);
}

void SafeRegretMPC::setCostWeights(const MatrixXd& Q, const MatrixXd& R) {
    Q_ = Q;
    R_ = R;
}

void SafeRegretMPC::setConstraints(const VectorXd& x_min, const VectorXd& x_max,
                                   const VectorXd& u_min, const VectorXd& u_max) {
    x_min_ = x_min;
    x_max_ = x_max;
    u_min_ = u_min;
    u_max_ = u_max;
}

// ========== Debug ==========

std::string SafeRegretMPC::getDebugInfo() const {
    std::ostringstream oss;
    oss << "Safe-Regret MPC Debug Info:\n";
    oss << "  Horizon: " << mpc_steps_ << "\n";
    oss << "  STL enabled: " << (stl_enabled_ ? "yes" : "no") << "\n";
    oss << "  DR enabled: " << (dr_enabled_ ? "yes" : "no") << "\n";
    oss << "  Terminal set enabled: " << (terminal_set_enabled_ ? "yes" : "no") << "\n";
    oss << "  STL robustness: " << stl_robustness_ << "\n";
    oss << "  STL budget: " << stl_budget_ << "\n";
    oss << "  MPC feasible: " << (mpc_feasible_ ? "yes" : "no") << "\n";
    oss << "  Solve time: " << solve_time_ << " ms";
    return oss.str();
}

// ========== Private Methods ==========

void SafeRegretMPC::buildOptimizationProblem() {
    // TODO: Build CppAD optimization problem
}

void SafeRegretMPC::evaluateObjective(const ADvector& vars, ADvector& fg) {
    // TODO: Implement objective function
    // Cost = Σ ℓ(x,u) - λ·ρ̃_k
}

void SafeRegretMPC::evaluateConstraints(const ADvector& vars, ADvector& fg) {
    // TODO: Implement constraints
    // - Dynamics
    // - DR constraints
    // - Terminal set
}

} // namespace safe_regret_mpc
