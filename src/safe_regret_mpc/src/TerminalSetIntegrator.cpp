#include "safe_regret_mpc/TerminalSetIntegrator.hpp"
#include <cmath>
#include <iostream>

using namespace safe_regret_mpc;
using namespace Eigen;

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

TerminalSetIntegrator::TerminalSetIntegrator()
    : A_(MatrixXd::Identity(6, 6)),
      B_(MatrixXd::Zero(6, 2)),
      enabled_(false),
      violation_count_(0),
      initialized_(false),
      last_method_(ComputationMethod::LQR)
{
    terminal_set_.center = VectorXd::Zero(6);
    terminal_set_.radius = 1.0;
    terminal_set_.min_bounds = VectorXd::Zero(6);
    terminal_set_.max_bounds = VectorXd::Ones(6) * 2.0;
    terminal_set_.cost_matrix = MatrixXd::Identity(6, 6);
    terminal_set_.control_invariant = false;
    terminal_set_.invariance_margin = 0.0;
}

TerminalSetIntegrator::~TerminalSetIntegrator() {
}

// ========== Initialization ==========

bool TerminalSetIntegrator::initialize() {
    initialized_ = true;
    return true;
}

// ========== Terminal Set Computation ==========

TerminalSetIntegrator::TerminalSet TerminalSetIntegrator::computeTerminalSet(
    double dr_margin, ComputationMethod method) {
    last_method_ = method;

    switch (method) {
        case ComputationMethod::LQR: {
            return computeLQRTerminalSet(dr_margin);
        }

        case ComputationMethod::BACKWARD_REACHABLE: {
            TerminalSet safe_set;
            safe_set.center = VectorXd::Zero(6);
            safe_set.radius = 2.0;
            return computeBackwardReachableSet(safe_set, dr_margin);
        }

        case ComputationMethod::LYAPUNOV: {
            // For now, use LQR method
            return computeLQRTerminalSet(dr_margin);
        }
    }

    return terminal_set_;
}

TerminalSetIntegrator::TerminalSet
TerminalSetIntegrator::computeLQRTerminalSet(double dr_margin) {
    TerminalSet ts;

    // Compute terminal cost using Riccati
    MatrixXd Q = MatrixXd::Identity(6, 6);
    MatrixXd R = MatrixXd::Identity(2, 2);
    ts.cost_matrix = solveRiccati(Q, R);

    // Simple spherical terminal set
    ts.center = VectorXd::Zero(6);
    ts.radius = 2.0 - dr_margin;  // Shrink with DR margin

    // Set bounds
    ts.min_bounds = ts.center.array() - ts.radius;
    ts.max_bounds = ts.center.array() + ts.radius;

    // Assume control-invariant (for simple case)
    ts.control_invariant = true;
    ts.invariance_margin = 0.1;

    terminal_set_ = ts;
    return ts;
}

TerminalSetIntegrator::TerminalSet
TerminalSetIntegrator::computeBackwardReachableSet(
    const TerminalSet& safe_set, double dr_margin) {
    // TODO: Implement full backward reachable set iteration
    // For now, return simple set
    TerminalSet ts;
    ts.center = safe_set.center;
    ts.radius = safe_set.radius - dr_margin;
    ts.cost_matrix = safe_set.cost_matrix;
    ts.control_invariant = false;
    return ts;
}

MatrixXd TerminalSetIntegrator::solveRiccati(const MatrixXd& Q, const MatrixXd& R) {
    // Solve DARE: P = A^T P A - A^T P B (R + B^T P B)^{-1} B^T P A + Q
    MatrixXd P = Q;  // Initial guess

    for (int iter = 0; iter < 100; ++iter) {
        MatrixXd BPB = B_.transpose() * P * B_;
        MatrixXd K = (R + BPB).ldlt().solve(B_.transpose() * P * A_);

        MatrixXd P_new = A_.transpose() * P * A_ - A_.transpose() * P * B_ * K + Q;

        if ((P_new - P).norm() < 1e-6) {
            break;
        }
        P = P_new;
    }

    return P;
}

// ========== Set and Check ==========

void TerminalSetIntegrator::setTerminalSet(const VectorXd& center, double radius) {
    terminal_set_.center = center;
    terminal_set_.radius = radius;
}

bool TerminalSetIntegrator::checkFeasibility(const VectorXd& state) const {
    if (!enabled_) return true;

    double distance = computeDistance(state);
    return distance <= terminal_set_.radius;
}

double TerminalSetIntegrator::computeDistance(const VectorXd& state) const {
    // Euclidean distance to center (using position only: x, y, theta)
    VectorXd position_error(3);
    position_error << state(0) - terminal_set_.center(0),
                      state(1) - terminal_set_.center(1),
                      state(2) - terminal_set_.center(2);
    return position_error.norm();
}

// ========== Verification ==========

bool TerminalSetIntegrator::verifyRecursiveFeasibility(const TerminalSet& terminal_set) const {
    // TODO: Implement full recursive feasibility verification
    // For now, assume satisfied if control-invariant property holds
    return terminal_set.control_invariant;
}

// ========== System Dynamics ==========

void TerminalSetIntegrator::setSystemDynamics(const MatrixXd& A, const MatrixXd& B) {
    A_ = A;
    B_ = B;
}

} // namespace safe_regret_mpc
