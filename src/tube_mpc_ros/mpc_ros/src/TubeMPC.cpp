#define HAVE_CSTDDEF
#include "TubeMPC.h"
#include <cppad/ipopt/solve.hpp>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <iostream>
#undef HAVE_CSTDDEF

using CppAD::AD;

class FG_eval_Tube {
public:
    Eigen::VectorXd coeffs;

    double _dt, _ref_cte, _ref_etheta, _ref_vel;
    double _w_cte, _w_etheta, _w_vel, _w_angvel, _w_accel;
    int _mpc_steps;

    MatrixXd _K;
    MatrixXd _A_cl;
    double _tube_radius;

    // Terminal set support (P1-1)
    bool _use_terminal_set;
    Eigen::VectorXd _terminal_set_center;
    double _terminal_set_radius;
    double _terminal_constraint_weight;

    int _z_start, _e_start, _v_nom_start;
    
    FG_eval_Tube(Eigen::VectorXd coeffs) {
        this->coeffs = coeffs;

        _dt = 0.1;
        _ref_cte = 0;
        _ref_etheta = 0;
        _ref_vel = 0.5;
        _w_cte = 50;
        _w_etheta = 50;
        _w_vel = 200;
        _w_angvel = 100;
        _w_accel = 50;
        _mpc_steps = 20;

        // Terminal set initialization (P1-1)
        _use_terminal_set = false;
        _terminal_set_center = Eigen::VectorXd::Zero(6);
        _terminal_set_radius = 0.0;
        _terminal_constraint_weight = 1000.0;

        _z_start = 0;
        _e_start = _z_start + 6 * (_mpc_steps + 1);
        _v_nom_start = _e_start + 6 * (_mpc_steps + 1);
    }
    
    void setTubeParams(const MatrixXd& K, const MatrixXd& A_cl, double tube_radius) {
        _K = K;
        _A_cl = A_cl;
        _tube_radius = tube_radius;
    }

    // Terminal set methods (P1-1)
    void setTerminalSet(const Eigen::VectorXd& center, double radius, double weight = 1000.0) {
        _terminal_set_center = center;
        _terminal_set_radius = radius;
        _terminal_constraint_weight = weight;
        _use_terminal_set = true;
    }

    void disableTerminalSet() {
        _use_terminal_set = false;
    }
    
    void LoadParams(const std::map<string, double> &params) {
        _dt = params.find("DT") != params.end() ? params.at("DT") : _dt;
        _mpc_steps = params.find("STEPS") != params.end() ? params.at("STEPS") : _mpc_steps;
        _ref_cte = params.find("REF_CTE") != params.end() ? params.at("REF_CTE") : _ref_cte;
        _ref_etheta = params.find("REF_ETHETA") != params.end() ? params.at("REF_ETHETA") : _ref_etheta;
        _ref_vel = params.find("REF_V") != params.end() ? params.at("REF_V") : _ref_vel;
        
        _w_cte = params.find("W_CTE") != params.end() ? params.at("W_CTE") : _w_cte;
        _w_etheta = params.find("W_EPSI") != params.end() ? params.at("W_EPSI") : _w_etheta;
        _w_vel = params.find("W_V") != params.end() ? params.at("W_V") : _w_vel;
        _w_angvel = params.find("W_ANGVEL") != params.end() ? params.at("W_ANGVEL") : _w_angvel;
        _w_accel = params.find("W_A") != params.end() ? params.at("W_A") : _w_accel;
        
        _z_start = 0;
        _e_start = _z_start + 6 * (_mpc_steps + 1);
        _v_nom_start = _e_start + 6 * (_mpc_steps + 1);
    }
    
    typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
    
    void operator()(ADvector& fg, const ADvector& vars) {
        fg[0] = 0;
        
        for (int i = 0; i < _mpc_steps; i++) {
            fg[0] += _w_cte * CppAD::pow(vars[_z_start + 6 * i + 4] - _ref_cte, 2);
            fg[0] += _w_etheta * CppAD::pow(vars[_z_start + 6 * i + 5] - _ref_etheta, 2);
            fg[0] += _w_vel * CppAD::pow(vars[_z_start + 6 * i + 3] - _ref_vel, 2);
        }
        
        for (int i = 0; i < _mpc_steps - 1; i++) {
            fg[0] += _w_angvel * CppAD::pow(vars[_v_nom_start + 2 * i], 2);
            fg[0] += _w_accel * CppAD::pow(vars[_v_nom_start + 2 * i + 1], 2);
        }
        
        for (int i = 0; i < _mpc_steps; i++) {
            AD<double> e_norm_sq = 0;
            for (int j = 0; j < 6; j++) {
                e_norm_sq += CppAD::pow(vars[_e_start + 6 * i + j], 2);
            }
            fg[0] += e_norm_sq;
        }

        // Terminal set constraint (P1-1) - Soft constraint on terminal state
        if (_use_terminal_set) {
            int terminal_idx = _z_start + 6 * _mpc_steps;  // Last nominal state

            // Compute distance from terminal state to terminal set center
            // Only consider position (x, y, theta) for terminal constraint
            AD<double> terminal_error_x = vars[terminal_idx + 0] - _terminal_set_center(0);
            AD<double> terminal_error_y = vars[terminal_idx + 1] - _terminal_set_center(1);
            AD<double> terminal_error_theta = vars[terminal_idx + 2] - _terminal_set_center(2);

            AD<double> terminal_distance_sq = terminal_error_x * terminal_error_x +
                                               terminal_error_y * terminal_error_y +
                                               terminal_error_theta * terminal_error_theta;

            // Add penalty if terminal state is outside terminal set
            // Soft constraint: penalty = weight * max(0, distance - radius)^2
            AD<double> radius_violation = CppAD::sqrt(terminal_distance_sq) - _terminal_set_radius;

            // Use smooth approximation of max(0, x): x if x > 0, else 0
            // CppAD::CondExpGt(comparison, left, right, if_true, if_false)
            AD<double> violation_penalty = CppAD::pow(
                CppAD::CondExpGt(radius_violation, AD<double>(0.0),
                                 radius_violation, AD<double>(0.0)), 2);

            fg[0] += _terminal_constraint_weight * violation_penalty;
        }

        for (int i = 0; i < 6; i++) {
            fg[1 + _z_start + i] = vars[_z_start + i];
            fg[1 + _e_start + i] = vars[_e_start + i];
        }
        
        for (int i = 0; i < _mpc_steps - 1; i++) {
            AD<double> z0_x = vars[_z_start + 6 * i];
            AD<double> z0_y = vars[_z_start + 6 * i + 1];
            AD<double> z0_theta = vars[_z_start + 6 * i + 2];
            AD<double> z0_v = vars[_z_start + 6 * i + 3];
            AD<double> z0_cte = vars[_z_start + 6 * i + 4];
            AD<double> z0_etheta = vars[_z_start + 6 * i + 5];
            
            AD<double> v0_w = vars[_v_nom_start + 2 * i];
            AD<double> v0_a = vars[_v_nom_start + 2 * i + 1];
            
            AD<double> f0 = 0.0;
            for (int j = 0; j < coeffs.size(); j++) {
                f0 += coeffs[j] * CppAD::pow(z0_x, j);
            }
            
            AD<double> trj_grad0 = 0.0;
            for (int j = 1; j < coeffs.size(); j++) {
                trj_grad0 += j * coeffs[j] * CppAD::pow(z0_x, j - 1);
            }
            trj_grad0 = CppAD::atan(trj_grad0);
            
            fg[12 + 6 * i] = vars[_z_start + 6 * (i + 1)] - 
                (z0_x + z0_v * CppAD::cos(z0_theta) * _dt);
            fg[12 + 6 * i + 1] = vars[_z_start + 6 * (i + 1) + 1] - 
                (z0_y + z0_v * CppAD::sin(z0_theta) * _dt);
            fg[12 + 6 * i + 2] = vars[_z_start + 6 * (i + 1) + 2] - 
                (z0_theta + v0_w * _dt);
            fg[12 + 6 * i + 3] = vars[_z_start + 6 * (i + 1) + 3] - 
                (z0_v + v0_a * _dt);
            fg[12 + 6 * i + 4] = vars[_z_start + 6 * (i + 1) + 4] - 
                ((f0 - z0_y) + (z0_v * CppAD::sin(z0_etheta) * _dt));
            fg[12 + 6 * i + 5] = vars[_z_start + 6 * (i + 1) + 5] - 
                ((z0_theta - trj_grad0) + v0_w * _dt);
            
            for (int j = 0; j < 6; j++) {
                AD<double> e0 = vars[_e_start + 6 * i + j];
                fg[12 + 6 * (_mpc_steps - 1) + 6 * i + j] = vars[_e_start + 6 * (i + 1) + j] - e0;
            }
        }
    }
};

TubeMPC::TubeMPC() {
    _A = MatrixXd::Identity(6, 6);
    _B = MatrixXd::Zero(6, 2);
    _B(2, 0) = 1;
    _B(3, 1) = 1;

    _Q = MatrixXd::Identity(6, 6);
    _R = MatrixXd::Identity(2, 2);

    _disturbance_bound = 0.1;
    _tube_radius = 0.0;

    _mpc_steps = 20;
    _dt = 0.1;
    _max_angvel = 3.0;
    _max_throttle = 1.0;
    _bound_value = 1.0e3;

    _z_nominal = VectorXd::Zero(6);
    _e_current = VectorXd::Zero(6);

    _z_start = 0;
    _e_start = 0;
    _v_nom_start = 0;

    // Initialize terminal set variables (P1-1)
    terminal_set_enabled_ = false;
    terminal_set_center_ = VectorXd::Zero(6);
    terminal_set_radius_ = 0.0;
    terminal_constraint_weight_ = 1000.0;  // High weight for terminal constraint
}

void TubeMPC::LoadParams(const std::map<string, double> &params) {
    _params = params;
    
    _mpc_steps = params.find("STEPS") != params.end() ? params.at("STEPS") : _mpc_steps;
    _max_angvel = params.find("ANGVEL") != params.end() ? params.at("ANGVEL") : _max_angvel;
    _max_throttle = params.find("MAXTHR") != params.end() ? params.at("MAXTHR") : _max_throttle;
    _bound_value = params.find("BOUND") != params.end() ? params.at("BOUND") : _bound_value;
    
    _dt = params.find("DT") != params.end() ? params.at("DT") : _dt;
    
    computeLQRGain();
    computeTubeInvariantSet();
    
    cout << "\n!! Tube MPC parameters updated !!" << endl;
    cout << "MPC steps: " << _mpc_steps << endl;
    cout << "Tube radius: " << _tube_radius << endl;
}

void TubeMPC::setDisturbanceSet(const MatrixXd& E_set) {
    _E_set = E_set;
    
    double max_norm = 0;
    for (int i = 0; i < E_set.cols(); i++) {
        double norm = E_set.col(i).norm();
        if (norm > max_norm) {
            max_norm = norm;
        }
    }
    
    _disturbance_bound = max_norm;
    _tube_radius = _disturbance_bound;
    
    cout << "Disturbance set set. Bound: " << _disturbance_bound << endl;
}

void TubeMPC::setDisturbanceBound(double bound) {
    _disturbance_bound = bound;
    _tube_radius = bound;
    
    cout << "Disturbance bound set to: " << bound << endl;
}

void TubeMPC::computeLQRGain() {
    MatrixXd P = _Q;
    MatrixXd P_prev;
    const int max_iterations = 1000;
    const double tolerance = 1e-6;
    
    for (int i = 0; i < max_iterations; i++) {
        P_prev = P;
        
        MatrixXd temp = _R + _B.transpose() * P * _B;
        MatrixXd K_temp = -temp.ldlt().solve(_B.transpose() * P * _A);
        
        P = _Q + K_temp.transpose() * _R * K_temp + 
              (_A + _B * K_temp).transpose() * P * (_A + _B * K_temp);
        
        if ((P - P_prev).cwiseAbs().maxCoeff() < tolerance) {
            break;
        }
    }
    
    _P = P;
    _K = -(_R + _B.transpose() * P * _B).ldlt().solve(_B.transpose() * P * _A);
    
    _A_cl = _A - _B * _K;
    
    cout << "LQR gain computed:" << endl;
    cout << "  K = " << _K << endl;
    cout << "  A_cl = " << _A_cl << endl;
}

void TubeMPC::computeTubeInvariantSet() {
    double w_max = _disturbance_bound;
    
    MatrixXd I_minus_A_cl = MatrixXd::Identity(_A_cl.rows(), _A_cl.cols()) - _A_cl;
    
    JacobiSVD<MatrixXd> svd(I_minus_A_cl, ComputeThinU | ComputeThinV);
    VectorXd singular_values = svd.singularValues();
    double max_singular = singular_values.maxCoeff();
    
    double inv_norm = (max_singular > 1e-10 && max_singular < 1.0) ? 
                     1.0 / (1.0 - max_singular) : 10.0;
    
    _tube_radius = 2.0 * inv_norm * w_max;
    
    if (_tube_radius > 10.0) {
        _tube_radius = 10.0;
        cout << "Warning: Tube radius clamped to 10.0" << endl;
    }
    
    _tube_matrix = _P.llt().matrixL().transpose();
    
    cout << "Tube invariant set computed:" << endl;
    cout << "  Tube radius: " << _tube_radius << endl;
    cout << "  Max singular value: " << max_singular << endl;
}

bool TubeMPC::checkTubeInvariance(const VectorXd& e) {
    double e_norm = sqrt(e.transpose() * _P * e);
    
    bool in_tube = e_norm <= _tube_radius * 1.01;
    
    if (!in_tube) {
        cout << "Warning: Error outside tube! Norm: " << e_norm 
             << ", Radius: " << _tube_radius << endl;
    }
    
    return in_tube;
}

pair<VectorXd, VectorXd> TubeMPC::tightenConstraints(
    const VectorXd& lower, const VectorXd& upper) {
    
    int n_constraints = lower.size();
    VectorXd tightened_lower(n_constraints);
    VectorXd tightened_upper(n_constraints);
    
    for (int i = 0; i < n_constraints; i++) {
        tightened_lower(i) = lower(i) - _tube_radius;
        tightened_upper(i) = upper(i) + _tube_radius;
    }
    
    return make_pair(tightened_lower, tightened_upper);
}

void TubeMPC::decomposeSystem(const VectorXd& state) {
    _z_nominal = state;
    _e_current = VectorXd::Zero(6);
    
    cout << "System decomposed:" << endl;
    cout << "  Nominal state z: " << _z_nominal.transpose() << endl;
    cout << "  Error e: " << _e_current.transpose() << endl;
}

VectorXd TubeMPC::getNominalState() const {
    return _z_nominal;
}

VectorXd TubeMPC::getErrorState() const {
    return _e_current;
}

MatrixXd TubeMPC::getK() const {
    return _K;
}

double TubeMPC::getTubeRadius() const {
    return _tube_radius;
}

// Terminal set methods (P1-1 implementation)

void TubeMPC::setTerminalSet(const VectorXd& center, double radius) {
    terminal_set_center_ = center;
    terminal_set_radius_ = radius;
    terminal_set_enabled_ = true;

    cout << "Terminal set configured:" << endl;
    cout << "  Center: " << terminal_set_center_.transpose() << endl;
    cout << "  Radius: " << terminal_set_radius_ << endl;
    cout << "  Enabled: " << (terminal_set_enabled_ ? "YES" : "NO") << endl;
}

void TubeMPC::enableTerminalSet(bool enable) {
    terminal_set_enabled_ = enable;

    if (enable && terminal_set_radius_ > 0) {
        cout << "Terminal set constraint enabled" << endl;
    } else {
        cout << "Terminal set constraint disabled" << endl;
    }
}

bool TubeMPC::checkTerminalFeasibility(const VectorXd& terminal_state) {
    if (!terminal_set_enabled_) {
        return true;  // No terminal constraint, always feasible
    }

    // Check if terminal state is within the terminal set
    // For simplicity, we check Euclidean distance to center
    VectorXd diff = terminal_state - terminal_set_center_;

    // Only consider position (x, y) and orientation (theta) for feasibility
    VectorXd position_error(3);
    position_error << diff(0), diff(1), diff(2);  // x, y, theta

    double distance = position_error.norm();

    bool feasible = distance <= terminal_set_radius_;

    if (!feasible) {
        cout << "Terminal feasibility violation:" << endl;
        cout << "  Distance to center: " << distance << endl;
        cout << "  Terminal radius: " << terminal_set_radius_ << endl;
        cout << "  Terminal state: " << terminal_state.transpose() << endl;
        cout << "  Terminal center: " << terminal_set_center_.transpose() << endl;
    }

    return feasible;
}

vector<double> TubeMPC::Solve(VectorXd state, VectorXd coeffs) {
    bool ok = true;
    typedef CPPAD_TESTVECTOR(double) Dvector;
    
    decomposeSystem(state);
    
    _z_start = 0;
    _e_start = 6 * (_mpc_steps + 1);
    _v_nom_start = 12 * (_mpc_steps + 1);
    
    FG_eval_Tube fg_eval(coeffs);
    fg_eval.setTubeParams(_K, _A_cl, _tube_radius);
    fg_eval.LoadParams(_params);

    // Configure terminal set (P1-1)
    if (terminal_set_enabled_) {
        fg_eval.setTerminalSet(terminal_set_center_, terminal_set_radius_,
                               terminal_constraint_weight_);
        cout << "Terminal set enabled in MPC solver" << endl;
    } else {
        fg_eval.disableTerminalSet();
    }

    _z_start = fg_eval._z_start;
    _e_start = fg_eval._e_start;
    _v_nom_start = fg_eval._v_nom_start;
    
    size_t n_vars = 6 * (_mpc_steps + 1) + 6 * (_mpc_steps + 1) + 2 * (_mpc_steps - 1);
    size_t n_constraints = 12 + 12 * (_mpc_steps - 1);
    
    cout << "MPC Solve: steps=" << _mpc_steps << ", n_vars=" << n_vars 
         << ", n_constraints=" << n_constraints << endl;
    cout << "Indices: z_start=" << _z_start << ", e_start=" << _e_start 
         << ", v_nom_start=" << _v_nom_start << endl;
    
    if (_v_nom_start + 2 * (_mpc_steps - 1) > n_vars) {
        cerr << "Error: Index calculation error! v_nom_start=" << _v_nom_start 
             << ", max=" << n_vars << endl;
        return {0.0, 0.0};
    }
    
    Dvector vars(n_vars);
    for (int i = 0; i < n_vars; i++) {
        vars[i] = 0;
    }
    
    for (int i = 0; i < 6; i++) {
        vars[_z_start + i] = _z_nominal(i);
        vars[_e_start + i] = _e_current(i);
    }
    
    Dvector vars_lowerbound(n_vars);
    Dvector vars_upperbound(n_vars);
    
    for (int i = 0; i < _z_start + 6 * (_mpc_steps + 1); i++) {
        vars_lowerbound[i] = -_bound_value;
        vars_upperbound[i] = _bound_value;
    }
    
    for (int i = 0; i < _mpc_steps + 1; i++) {
        int v_idx = _z_start + 6 * i + 3;
        vars_lowerbound[v_idx] = 0.03;
        vars_upperbound[v_idx] = _bound_value;
    }
    
    for (int i = _e_start; i < _v_nom_start; i++) {
        vars_lowerbound[i] = -_tube_radius;
        vars_upperbound[i] = _tube_radius;
    }
    
    for (int i = _v_nom_start; i < n_vars; i++) {
        int idx = i - _v_nom_start;
        if (idx % 2 == 0) {
            vars_lowerbound[i] = -_max_angvel;
            vars_upperbound[i] = _max_angvel;
        } else {
            vars_lowerbound[i] = -_max_throttle;
            vars_upperbound[i] = _max_throttle;
        }
    }
    
    Dvector constraints_lowerbound(n_constraints);
    Dvector constraints_upperbound(n_constraints);
    for (int i = 0; i < n_constraints; i++) {
        constraints_lowerbound[i] = 0;
        constraints_upperbound[i] = 0;
    }
    
    for (int i = 0; i < 6; i++) {
        constraints_lowerbound[1 + i] = _z_nominal(i);
        constraints_upperbound[1 + i] = _z_nominal(i);
        constraints_lowerbound[7 + i] = _e_current(i);
        constraints_upperbound[7 + i] = _e_current(i);
    }
    
    std::string options;
    options += "Integer print_level  0\n";
    options += "Sparse  true        forward\n";
    options += "Sparse  true        reverse\n";
    options += "Numeric max_cpu_time          0.5\n";
    
    CppAD::ipopt::solve_result<Dvector> solution;
    CppAD::ipopt::solve<Dvector, FG_eval_Tube>(
        options, vars, vars_lowerbound, vars_upperbound,
        constraints_lowerbound, constraints_upperbound, 
        fg_eval, solution);
    
    ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;
    
    auto cost = solution.obj_value;
    std::cout << "------------ Tube MPC Cost: " << cost << "------------" << std::endl;
    
    this->mpc_x = {};
    this->mpc_y = {};
    this->mpc_theta = {};
    for (int i = 0; i < _mpc_steps; i++) {
        this->mpc_x.push_back(solution.x[_z_start + 6 * i]);
        this->mpc_y.push_back(solution.x[_z_start + 6 * i + 1]);
        this->mpc_theta.push_back(solution.x[_z_start + 6 * i + 2]);
    }
    
    for (int i = 0; i < 6; i++) {
        _z_nominal(i) = solution.x[_z_start + i];
    }
    
    vector<double> result;
    result.push_back(solution.x[_v_nom_start]);
    result.push_back(solution.x[_v_nom_start + 1]);
    
    return result;
}
