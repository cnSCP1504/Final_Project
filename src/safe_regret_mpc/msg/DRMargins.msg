# Distributionally Robust Margins Message
# DR constraint tightening margins

# Header with timestamp
Header header

# Margins for each time step in horizon
float64[] margins             # Margins σ_{k,t} for t=k,...,k+N
float64[] means               # Means μ_t along sensitivity direction
float64[] stds                # Standard deviations σ_t
float64[] cantelli_factors    # Cantelli factors κ_{δ_t}

# Risk allocation
float64 total_risk            # Total risk δ
float64[] risk_allocation     # Risk allocation δ_t for each time step
string allocation_method      # "uniform", "deadline_weighted", "adaptive"

# Ambiguity set information
string ambiguity_type         # "wasserstein", "chebyshev", "phi_divergence"
float64 ambiguity_radius      # Ambiguity set radius ε
uint32 sample_count           # Number of samples M

# Residual statistics
float64[] residual_mean       # Mean of residuals
float64[] residual_std        # Std dev of residuals
float64[] residual_bounds     # Residual bounds

# Tube compensation
float64 tube_radius           # Current tube radius ē
float64 tube_offset           # Tube offset L_h·ē
float64 tube_compensation     # Total tube compensation η_ℰ

# Confidence information
float64 confidence_level      # Confidence level (1-α)
float64 safety_margin         # Additional safety margin

# Computational info
float64 computation_time      # Computation time (ms)
int32 iterations              # Number of iterations
bool converged                # Convergence status
