#ifndef TUBE_MPC_H
#define TUBE_MPC_H

#include <vector>
#include <map>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>

using namespace std;
using namespace Eigen;

class TubeMPC
{
public:
    TubeMPC();

    void LoadParams(const std::map<string, double> &params);
    vector<double> Solve(VectorXd state, VectorXd coeffs);

    void setDisturbanceSet(const MatrixXd& E_set);
    void setDisturbanceBound(double bound);

    VectorXd getNominalState() const;
    VectorXd getErrorState() const;
    MatrixXd getK() const;
    double getTubeRadius() const;

    // Terminal set methods (P1-1 implementation)
    void setTerminalSet(const VectorXd& center, double radius);
    void enableTerminalSet(bool enable);
    bool checkTerminalFeasibility(const VectorXd& terminal_state);
    bool isTerminalSetEnabled() const { return terminal_set_enabled_; }
    VectorXd getTerminalSetCenter() const { return terminal_set_center_; }
    double getTerminalSetRadius() const { return terminal_set_radius_; }

    vector<double> mpc_x;
    vector<double> mpc_y;
    vector<double> mpc_theta;

private:
    void computeLQRGain();
    void computeTubeInvariantSet();
    void decomposeSystem(const VectorXd& state);
    bool checkTubeInvariance(const VectorXd& e);
    
    pair<VectorXd, VectorXd> tightenConstraints(
        const VectorXd& lower, const VectorXd& upper);
    
    MatrixXd _A, _B;
    MatrixXd _Q, _R;
    MatrixXd _P;
    MatrixXd _K;
    MatrixXd _A_cl;
    
    MatrixXd _E_set;
    double _disturbance_bound;
    double _tube_radius;
    MatrixXd _tube_matrix;
    
    int _mpc_steps;
    double _dt;
    double _max_angvel;
    double _max_throttle;
    double _bound_value;
    
    VectorXd _z_nominal;
    VectorXd _e_current;
    
    int _x_start, _y_start, _theta_start, _v_start, _cte_start, _etheta_start;
    int _angvel_start, _a_start;
    int _z_start, _e_start, _v_nom_start;

    // Terminal set member variables (P1-1)
    bool terminal_set_enabled_;
    VectorXd terminal_set_center_;
    double terminal_set_radius_;
    double terminal_constraint_weight_;

    std::map<string, double> _params;
};

#endif
