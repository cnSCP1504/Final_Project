# Safe-Regret MPC System State
# Contains complete state of the integrated system

# Header with timestamp
Header header

# System mode
string mode  # "STL_MPC", "DR_MPC", "SAFE_REGRET_MPC", "TUBE_MPC"

# State estimates
float64[6] nominal_state      # [x, y, theta, v, cte, etheta]
float64[6] error_state        # [x, y, theta, v, cte, etheta]
float64[6] belief_mean        # Belief space mean
float64[36] belief_covariance  # Belief space covariance (6x6 flattened)

# Control inputs
float64[2] nominal_control     # [v_linear, v_angular]
float64[2] feedback_control    # [K_e_x, K_e_y]
float64[2] total_control       # [v_total, omega_total]

# STL robustness
float64 stl_robustness         # Current STL robustness value
float64 stl_budget             # Current robustness budget
float64 stl_budget_baseline    # Budget baseline r̲
int32 stl_violation_count      # Number of violations

# DR margins
float64[] dr_margins           # DR constraint margins for horizon
float64 dr_risk_level          # Current risk level δ
float64 dr_confidence          # Confidence level

# Terminal set
float64[6] terminal_center     # Terminal set center
float64 terminal_radius        # Terminal set radius
bool terminal_feasible         # Terminal feasibility status

# Regret metrics
float64 cumulative_regret      # Cumulative regret
float64 average_regret         # Average regret per step
float64 instantaneous_regret   # Instantaneous regret
float64 reference_regret       # Reference planner regret

# Performance metrics
float64 mpc_solve_time         # MPC solve time (ms)
float64 stl_eval_time          # STL evaluation time (ms)
float64 dr_compute_time        # DR computation time (ms)
float64 total_compute_time     # Total compute time (ms)

bool mpc_feasible              # MPC feasibility status
int32 mpc_iterations            # Number of MPC iterations

# System status
bool system_ready              # System ready flag
string system_status           # Status message
