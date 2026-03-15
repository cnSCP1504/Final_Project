# STL Robustness message
# Reports the robustness evaluation result for a single STL formula

Header header

# Formula identifier
string formula_name

# Robustness values
float64 robustness              # Current robustness ρ(φ)
float64 expected_robustness     # Belief-space expected robustness E[ρ(φ)]
float64 min_robustness          # Minimum robustness over horizon
float64 max_robustness          # Maximum robustness over horizon

# Satisfaction status
bool satisfied                  # true if robustness > 0
float64 violation_probability   # Pr(robustness < 0)

# Budget information
float64 budget                  # Current budget R_k
float64 budget_change           # ΔR = R_{k+1} - R_k
bool budget_feasible            # true if budget >= 0

# Per-timestep robustness (for visualization)
float64[] timestep_robustness   # ρ_t for t = k, ..., k+N

# Timing
float64 computation_time_ms     # Computation time

# Debug info (optional)
float64[] sample_values         # Monte Carlo samples (if enabled)
int32 num_samples               # Number of samples used
