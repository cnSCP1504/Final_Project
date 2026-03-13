# Tube MPC Metrics Collection System

## Overview

This document describes the metrics collection system added to Tube MPC for comprehensive performance evaluation according to Safe-Regret MPC paper acceptance criteria.

## Collected Metrics

### 1. Safety and Satisfaction Metrics
- **Empirical Risk (δ̂)**: Fraction of steps where safety constraints are violated
- **Satisfaction Probability (p̂_sat)**: 1 - δ̂
- **Safety Violation Rate**: Rate of h(x_t) < 0
- **95% Confidence Interval**: Wilson score interval for satisfaction probability

### 2. Feasibility Metrics
- **Recursive Feasibility Rate**: Fraction of MPC solves that succeeded
- **Total MPC Calls**: Total number of optimization attempts
- **Successful Solves**: Number of feasible solutions found
- **Failed Solves**: Number of infeasible/failed solves

### 3. Tracking Performance Metrics
- **Mean Tracking Error**: Average ||e_t|| over run
- **Max Tracking Error**: Maximum tracking error norm
- **Std Tracking Error**: Standard deviation of tracking error
- **Tube Violation Rate**: Fraction of time outside tube ℰ
- **Tube Occupancy Rate**: Fraction of time within tube

### 4. Computation Metrics
- **Mean Solve Time**: Average MPC solve time (ms)
- **Median Solve Time**: Median solve time (ms)
- **P90 Solve Time**: 90th percentile solve time
- **Max Solve Time**: Maximum solve time (ms)

### 5. Regret Metrics
- **Cumulative Dynamic Regret**: Σ (ℓ(x_t,u_t) - ℓ(x*_t,u*_t))
- **Average Dynamic Regret**: Cumulative regret / T
- **Cumulative Safe Regret**: Regret relative to safe policy class
- **Average Safe Regret**: Safe regret / T

### 6. Calibration Metrics
- **Calibration Error**: |δ̂ - δ_target|
- Tracks how well empirical risk matches target risk level

## File Structure

### New Files
```
src/tube_mpc_ros/mpc_ros/
├── include/
│   └── MetricsCollector.h           # Metrics collector header
├── src/
│   └── MetricsCollector.cpp          # Metrics collector implementation
└── METRICS_INTEGRATION_GUIDE.md     # This file
```

### Modified Files
```
src/tube_mpc_ros/mpc_ros/
├── include/TubeMPCNode.h            # Added metrics collector
├── src/TubeMPCNode.cpp              # Integrated metrics collection
└── CMakeLists.txt                   # Add MetricsCollector.cpp
```

## Installation Steps

### 1. Update CMakeLists.txt
Add `MetricsCollector.cpp` to the executable sources:

```cmake
add_executable(tube_mpc_node
  src/TubeMPCNode.cpp
  src/TubeMPC.cpp
  src/MetricsCollector.cpp          # ADD THIS LINE
  src/navMPC.cpp
  ...
)
```

### 2. Recompile the Workspace
```bash
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

### 3. Verify Installation
```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

Check for log files:
```bash
ls -l /tmp/tube_mpc_metrics.csv
ls -l /tmp/tube_mpc_summary.txt
```

## Usage

### Configuration Parameters
Add to your launch file or parameter server:

```xml
<param name="metrics_output_csv" value="/tmp/tube_mpc_metrics.csv"/>
<param name="target_risk_delta" value="0.1"/>  <!-- Target 10% risk -->
```

### ROS Topics
The metrics collector publishes real-time metrics:
- `/mpc_metrics/empirical_risk` (std_msgs/Float64)
- `/mpc_metrics/feasibility_rate` (std_msgs/Float64)
- `/mpc_metrics/tracking_error` (std_msgs/Float64)
- `/mpc_metrics/solve_time_ms` (std_msgs/Float64)
- `/mpc_metrics/regret_dynamic` (std_msgs/Float64)
- `/mpc_metrics/regret_safe` (std_msgs/Float64)

### Monitor Metrics in Real-Time
```bash
# Terminal 1: Run navigation
roslaunch tube_mpc_ros tube_mpc_navigation.launch

# Terminal 2: Monitor metrics
rostopic echo /mpc_metrics/empirical_risk
rostopic echo /mpc_metrics/feasibility_rate
rostopic echo /mpc_metrics/tracking_error
```

## Output Files

### 1. CSV Log: `/tmp/tube_mpc_metrics.csv`
Detailed time-series data at each control step:

```csv
timestamp,cte,etheta,vel,angvel,tracking_error_norm,tube_radius,
tube_violation,safety_margin,safety_violation,mpc_solve_time,
mpc_feasible,cost,robustness,regret_dynamic,regret_safe
```

### 2. Summary File: `/tmp/tube_mpc_summary.txt`
Written at goal completion with aggregated statistics:

```
Tube MPC Performance Summary
==============================

Safety Metrics:
  Empirical Risk (δ̂):       0.0850
  Target Risk (δ):          0.1000
  Calibration Error:        0.0150

Performance Metrics:
  Feasibility Rate:         99.50%
  Mean Tracking Error:      0.023456
  Median Solve Time:        5.23 ms

Regret Analysis:
  Cumulative Dynamic:       123.45
  Average Dynamic:          0.123

95% CI for Satisfaction: [0.8950, 0.9250]
```

### 3. Console Output
At goal completion, a comprehensive summary is printed:

```
========================================
     Tube MPC Metrics Summary
========================================

--- Safety and Satisfaction ---
Empirical risk (δ̂):          0.0850
Satisfaction probability:    0.9150
Safety violation rate:       0.0850
Target risk (δ):             0.1000
Calibration error:           0.0150

--- Feasibility ---
Total MPC calls:             2000
Successful solves:           1990
Failed solves:               10
Feasibility rate:            99.50%

--- Tracking Performance ---
Mean tracking error:         0.023456
Max tracking error:          0.123456
Std tracking error:          0.012345
Tube violation rate:         2.50%
Tube occupancy rate:         97.50%

--- Computation Time ---
Mean solve time:             5.45 ms
Median solve time:           5.23 ms
P90 solve time:              6.78 ms
Max solve time:              12.34 ms

--- Regret Analysis ---
Cumulative dynamic regret:   123.45
Average dynamic regret:      0.062
Cumulative safe regret:      123.45
Average safe regret:         0.062
Regret/√T ratio:            0.001 (o(√T) growth - good!)
========================================
```

## Analysis and Evaluation

### Check Safety Guarantees
```bash
# View empirical risk
grep "Empirical risk" /tmp/tube_mpc_summary.txt

# Verify δ̂ ≈ δ_target (within 10% tolerance)
```

### Verify Real-Time Performance
```bash
# Check P90 solve time < 10ms target
grep "P90 solve time" /tmp/tube_mpc_summary.txt
```

### Analyze Regret Growth
```bash
# Regret should grow sublinearly (o(T))
# Check Regret/√T ratio - should be small and stable
```

### Plot Metrics
```python
import pandas as pd
import matplotlib.pyplot as plt

# Load metrics
df = pd.read_csv('/tmp/tube_mpc_metrics.csv')

# Plot tracking error
plt.figure(figsize=(12, 6))
plt.plot(df['timestamp'], df['tracking_error_norm'])
plt.xlabel('Time (s)')
plt.ylabel('Tracking Error Norm')
plt.title('Tracking Error Over Time')
plt.grid(True)
plt.savefig('/tmp/tracking_error.png')

# Plot solve time
plt.figure(figsize=(12, 6))
plt.plot(df['timestamp'], df['mpc_solve_time'])
plt.xlabel('Time (s)')
plt.ylabel('Solve Time (ms)')
plt.title('MPC Solve Time Over Time')
plt.grid(True)
plt.savefig('/tmp/solve_time.png')
```

## Integration with Paper Evaluation

### Task 1: Collaborative Assembly
The metrics collector provides data for:
- ✅ Satisfaction probability p̂_sat with Wilson CI
- ✅ Empirical risk δ̂ vs target δ
- ✅ Recursive feasibility rate
- ✅ Computation time statistics (median, p90)
- ✅ Tracking error and tube occupancy

### Task 2: Logistics Transport
Additional metrics available:
- ✅ Dynamic regret R_T^dyn/T
- ✅ Safe regret R_T^safe/T
- ✅ Regret growth rate (o(T) verification)

### Baseline Comparison
Run multiple controllers and compare metrics:

```bash
# Tube MPC
roslaunch tube_mpc_ros tube_mpc_navigation.launch controller:=tube_mpc
# Collect: tube_mpc_metrics.csv

# Standard MPC
roslaunch tube_mpc_ros nav_gazebo.launch controller:=mpc
# Collect: mpc_metrics.csv

# Compare using Python scripts
```

## Troubleshooting

### Issue: CSV file not created
**Solution**: Check write permissions to `/tmp/`
```bash
ls -ld /tmp
sudo chmod 1777 /tmp  # If needed
```

### Issue: Metrics topics not publishing
**Solution**: Verify ROS publishers initialized
```bash
rostopic list | grep mpc_metrics
```

### Issue: Summary not printed at goal
**Solution**: Check if goal reached callback is firing
```bash
rosservice call /goal_is reached  # Test goal reached
```

## Future Enhancements (Phase 2+)

- **STL Robustness**: Integrate actual STL robustness ρ(φ) instead of proxy
- **Reference Costs**: Get reference costs from abstract planner
- **Safety Function h(x)**: Implement actual safety constraints from DR tightening
- **Confidence Bounds**: Add online confidence bound updates
- **Visualization**: RViz plugins for real-time metrics display

## References

- Paper Metrics: Section VI.E (Evaluation Metrics)
- Regret Analysis: Theorem 4.8
- Safety Guarantees: Theorem 4.7
- Feasibility: Theorem 4.5

---

**Last Updated**: 2026-03-13
**Status**: ✅ Implemented and Ready for Testing
