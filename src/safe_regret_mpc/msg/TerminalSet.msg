# Terminal Set Message
# DR control-invariant terminal set information

# Header with timestamp
Header header

# Terminal set definition
float64[6] center             # Terminal set center [x, y, θ, v, cte, e_θ]
float64 radius                # Terminal set radius (spherical approximation)
float64[6] min_bounds         # Minimum bounds (for polytope representation)
float64[6] max_bounds         # Maximum bounds (for polytope representation)

# Terminal set properties
bool control_invariant        # DR control-invariant property
bool recursively_feasible     # Recursive feasibility status
float64 invariance_margin     # Margin from control-invariance violation

# Computation info
string computation_method     # "backward_reachable", "lyapunov", "lqr"
int32 iterations              # Number of iterations to compute
float64 computation_time      # Computation time (ms)

# Feasibility status
bool feasible                 # Terminal set feasibility
float64 feasibility_margin    # Distance to feasibility boundary
int32 violation_count         # Number of violations detected

# Terminal cost matrix (LQR)
float64[36] terminal_cost     # Terminal cost matrix P (6x6 flattened)

# Terminal constraints
float64[6] terminal_state     # Current terminal state z_{k+N}
float64 terminal_cost_value   # Terminal cost value
float64 terminal_constraint   # Terminal constraint violation

# Visualization
bool visualize                # Visualization enable flag
string viz_type               # "sphere", "cylinder", "polytope"
float64[3] viz_color          # RGB color for visualization
float64 viz_alpha             # Transparency (0-1)
