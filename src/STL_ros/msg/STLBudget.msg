# STL Budget message
# Reports the budget status for robustness management

Header header

# Overall budget status
float64 current_budget          # Current budget R_k
float64 baseline_r              # Per-step requirement r̲
bool feasible                   # true if budget >= 0

# Budget statistics
float64 min_budget              # Minimum budget in history
float64 max_budget              # Maximum budget in history
float64 mean_budget             # Mean budget
int32 violation_count           # Number of budget violations

# Per-formula budget information
string[] formula_names          # Formula identifiers
float64[] budgets               # Budget for each formula
bool[] formula_feasible         # Feasibility for each formula

# Budget prediction
float64[] predicted_budgets     # Predicted budgets over horizon
float64[] confidence_bounds     # Confidence intervals (±)

# Timing
float64 update_frequency_hz     # Update rate
