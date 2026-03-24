# STL Robustness Message
# Signal Temporal Logic robustness information

# Header with timestamp
Header header

# STL formula information
string stl_formula            # STL formula specification
string formula_type           # Formula type: "eventually", "globally", "until"

# Robustness values
float64 robustness_value      # Current robustness ρ(φ; x)
float64 smooth_robustness     # Smooth robustness ρ^soft(φ; x)
float64 belief_robustness     # Belief-space robustness ρ̃_k
float64 robustness_gradient   # Gradient ∇ρ (for MPC optimization)

# Budget information
float64 budget                # Current budget R_k
float64 budget_baseline       # Baseline r̲
float64 budget_change         # Budget change ΔR = R_{k+1} - R_k
float64 budget_prediction     # Predicted budget for next step

# Smoothness parameters
float64 temperature           # Temperature parameter τ
float64 smoothing_error       # Smoothing approximation error

# Subformula information
uint32 num_subformulas        # Number of subformulas
float64[] subformula_robustness  # Robustness of each subformula
string[] subformula_ids       # Subformula identifiers

# Violation tracking
bool satisfied                # Formula satisfaction status
float64 time_to_violation     # Estimated time to violation (if any)
float64 violation_severity    # Severity of current violation

# Temporal information
float64 time_horizon          # Evaluation horizon
float64 current_time          # Current time in formula
float64[] time_stamps         # Time stamps for robustness values
