# Safe-Regret MPC Performance Metrics
# Detailed performance metrics for analysis

# Header with timestamp
Header header

# Tracking performance
float64 cross_track_error      # CTE (m)
float64 heading_error          # Heading error (rad)
float64 velocity_error         # Velocity error (m/s)
float64 lateral_deviation      # Lateral deviation (m)

# Control performance
float64 control_effort         # Total control effort
float64 control_smoothness     # Control smoothness metric
float64 input_saturation       # Input saturation ratio

# Safety metrics
float64 safety_margin          # Minimum safety margin (m)
float64 constraint_violations  # Number of constraint violations
float64 probability_violation  # Estimated probability of violation
float64 tube_radius            # Current tube radius (m)

# STL satisfaction
float64 stl_satisfaction_rate  # STL satisfaction rate
float64 stl_robustness_mean    # Mean robustness
float64 stl_robustness_std     # Robustness std dev
float64 stl_budget_utilization # Budget utilization ratio

# Regret metrics
float64 dynamic_regret         # Dynamic regret R_T^dyn
float64 safe_regret            # Safe regret R_T^safe
float64 regret_bound           # Theoretical regret bound
float64 regret_ratio           # Actual / theoretical ratio

# Computational metrics
float64 solve_time_mean        # Mean solve time (ms)
float64 solve_time_max         # Max solve time (ms)
float64 solve_time_std         # Solve time std dev
float64 frequency              # Control frequency (Hz)

# Feasibility metrics
float64 feasibility_rate       # MPC feasibility rate
float64 recursive_feasibility  # Recursive feasibility indicator
float64 terminal_feasibility   # Terminal feasibility rate

# Overall performance score
float64 performance_score      # Overall performance (0-1)
string performance_grade       # Performance grade (A-F)
