# Safe-Regret MPC Comprehensive Metrics Message
# Based on manuscript Section VI.B.1 (Metrics)
# All metrics are collected per time step and aggregated over experiments

# Header with timestamp
Header header

# ========== Task Performance Metrics ==========

# STL Satisfaction probability
# Fraction of runs with ρ(φ;ξ) ≥ 0
# Wilson 95% confidence interval: [p_lower, p_upper]
float64 stl_satisfaction_prob
float64 stl_satisfaction_ci_lower  # 95% CI lower bound
float64 stl_satisfaction_ci_upper  # 95% CI upper bound

# STL Robustness (instantaneous and budget)
float64 stl_robustness             # ρ̃_k (current belief-space robustness)
float64 stl_robustness_budget      # R_k (current budget)
float64 stl_robustness_baseline    # r̲ (baseline requirement)

# ========== Safety and Risk Metrics ==========

# Empirical risk (constraint violation rate)
# Fraction of steps with h(x_t) < 0
float64 empirical_risk             # ̂δ (observed violation rate)
float64 target_risk                # δ (target risk level)

# DR Margins (distributionally robust constraint tightening)
float64[] dr_margins               # σ_{k,t} for t=k,...,k+N
float64 dr_margin_mean             # Average margin over horizon
float64 dr_margin_std              # Std of margins

# Safety constraint violations
uint32 safety_violation_count      # Number of h(x) < 0 violations
float64 safety_margin_min          # Minimum safety margin observed
float64 safety_margin_mean         # Mean safety margin

# ========== Regret Metrics ==========

# Dynamic regret (normalized by time)
# R_T^dyn / T
float64 dynamic_regret_cumulative  # Cumulative dynamic regret
float64 dynamic_regret_normalized  # Normalized by time steps
float64 dynamic_regret_instant     # Instantaneous regret at this step

# Safe regret (normalized by time)
# R_T^safe / T
float64 safe_regret_cumulative     # Cumulative safe regret
float64 safe_regret_normalized     # Normalized by time steps
float64 safe_regret_instant        # Instantaneous regret at this step

# Reference cost (for regret calculation)
float64 reference_cost_instant     # Cost from reference planner
float64 actual_cost_instant        # Actual cost incurred

# ========== MPC Feasibility and Computation ==========

# MPC solve status
bool mpc_feasible                  # True if MPC solve succeeded
bool mpc_optimal                   # True if optimal solution found
float64 mpc_solve_time             # Solve time in milliseconds
float64 mpc_iterations             # Number of iterations
float64 mpc_objective_value        # Final objective value

# Feasibility rate (recursive)
float64 feasibility_rate            # Fraction of feasible MPC solves so far
uint32 feasibility_count           # Number of feasible solves
uint32 total_solve_count           # Total number of solve attempts

# Constraint violations in MPC
uint32 mpc_constraint_violations   # Number of constraint violations
float64 mpc_constraint_slack       # Total constraint slack

# ========== Tracking and Tube Metrics ==========

# Tracking error (tube MPC decomposition)
# e_t = x_t - z_t (error from nominal trajectory)
float64 tracking_error_norm         # ‖e_t‖_2
float64 tracking_error_x           # x-component
float64 tracking_error_y           # y-component
float64 tracking_error_theta       # θ-component

# Tube occupancy and violations
bool tube_occupied                 # True if e_t ∈ ℰ
float64 tube_radius                # Current tube radius
uint32 tube_violation_count        # Number of tube violations
float64 tube_violation_max         # Maximum violation observed

# Cross-track error and heading error
float64 cte                        # CTE
float64 heading_error              # eθ (radians)

# ========== Control Performance ==========

# Control inputs
float64 cmd_linear_vel             # v (linear velocity)
float64 cmd_angular_vel            # ω (angular velocity)

# Stage cost
# ℓ(x,u) = ‖x-x_ref‖_Q^2 + ‖u‖_R^2
float64 stage_cost                 # ℓ(x_t, u_t)
float64 tracking_cost              # Tracking cost component
float64 control_cost               # Control effort component

# In-place rotation status
bool in_place_rotation             # True if in pure rotation mode
float64 heading_error_degrees      # |eθ| in degrees

# ========== Calibration Metrics ==========

# Calibration accuracy (observed vs nominal risk)
float64 calibration_error          # |̂δ - δ| (absolute error)
float64 calibration_ratio          # ̂δ / δ (ratio)
bool well_calibrated               # True if |̂δ - δ| < 0.05

# Ambiguity set statistics
float64 ambiguity_radius           # ε_k (Wasserstein radius)
uint32 residual_window_size        # M (window size)
float64 residual_mean              # Mean of residuals
float64 residual_std               # Std of residuals

# ========== Experiment Metadata ==========

# Time information
float64 experiment_time            # Time since experiment start (seconds)
uint32 time_step                   # Time step index
uint32 total_steps                 # Total steps so far

# Task information
string task_name                   # Task identifier (e.g., "logistics")
string scenario_id                 # Scenario/seed identifier
string method_name                 # Method name (e.g., "safe_regret_mpc")

# Configuration
float64 horizon_n                  # MPC horizon
float64 risk_delta                 # Target risk δ
float64 stl_weight_lambda          # STL weight λ
float64 temperature_tau            # Temperature τ
