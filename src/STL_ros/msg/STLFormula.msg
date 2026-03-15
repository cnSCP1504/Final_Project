# STL Formula message
# Defines an STL formula specification

Header header

# Formula definition
string formula_name             # Unique identifier
string formula_string           # Formula text representation
string formula_type             # Type: predicate, until, eventually, globally, etc.

# Temporal bounds (if applicable)
float64 time_lower              # Lower time bound a
float64 time_upper              # Upper time bound b
bool has_temporal_bounds        # Whether temporal bounds are set

# Priority and parameters
float64 priority                # Priority weight (for multi-formula)
float64 lambda                  # Objective weight in MPC

# Status
bool active                     # Whether formula is currently active
float64 current_robustness      # Current robustness value
bool satisfied                  # Current satisfaction status

# Formula tree structure (for complex formulas)
string[] subformulas            # Child formulas (if any)
string[] operators              # Operators combining subformulas

# Predicate definition (if atomic)
string predicate_name           # Predicate identifier
string predicate_expression     # Mathematical expression
float64 threshold               # Threshold value
