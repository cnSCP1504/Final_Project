# Tube MPC Metrics Collection System - Quick Start

## ✅ What Has Been Implemented

A comprehensive metrics collection system has been added to Tube MPC to support Safe-Regret MPC paper evaluation criteria. The system automatically collects all required metrics during navigation runs.

## 📊 Collected Metrics (Paper Compliance)

### ✅ 1. Satisfaction Probability (p̂_sat)
- Safety constraint satisfaction rate
- Wilson 95% confidence intervals
- **Target**: > 90% satisfaction

### ✅ 2. Empirical Risk (δ̂)
- Fraction of safety violations h(x_t) < 0
- Comparison with target risk δ
- **Target**: δ̂ ≈ δ ± 10%

### ✅ 3. Regret Analysis
- **Dynamic Regret**: R_T^dyn/T
- **Safe Regret**: R_T^safe/T
- Cumulative and average regret
- **Target**: o(T) growth rate

### ✅ 4. Feasibility Metrics
- Recursive feasibility rate
- MPC success/failure counts
- **Target**: > 99% feasibility

### ✅ 5. Computation Metrics
- Mean, median, P90, max solve times
- **Target**: P90 < 10ms @ 50Hz

### ✅ 6. Tracking Performance
- Mean/max/std tracking error ||e_t||
- Tube violation/occupancy rates
- **Target**: Tube occupancy > 95%

### ✅ 7. Calibration Accuracy
- |δ̂ - δ_target| deviation
- Tracks calibration quality

## 🚀 Quick Start

### Step 1: Install and Build
```bash
cd /home/dixon/Final_Project/catkin

# Run integration script
./src/tube_mpc_ros/mpc_ros/scripts/add_metrics_collector.sh

# Build
catkin_make
source devel/setup.bash
```

### Step 2: Run Navigation Test
```bash
# Terminal 1: Launch navigation
roslaunch tube_mpc_ros tube_mpc_navigation.launch

# Terminal 2: Set a goal in RViz
# The metrics will be collected automatically

# Terminal 3: Monitor real-time metrics
rostopic echo /mpc_metrics/empirical_risk
rostopic echo /mpc_metrics/feasibility_rate
rostopic echo /mpc_metrics/tracking_error
```

### Step 3: View Results
After goal completion, metrics summary is automatically printed:

```bash
# View console summary (printed at goal)
# View detailed summary file
cat /tmp/tube_mpc_summary.txt

# View raw CSV data
head /tmp/tube_mpc_metrics.csv
```

### Step 4: Analyze and Plot
```bash
# Run analysis script
cd /home/dixon/Final_Project/catkin
python3 src/tube_mpc_ros/mpc_ros/scripts/analyze_metrics.py

# Results saved to /tmp/metrics_analysis/
# - tracking_error.png
# - solve_time.png
# - cumulative_regret.png
# - safety_margin.png
# - control_inputs.png
# - metrics_report.txt
```

## 📁 File Structure

```
catkin/
├── src/tube_mpc_ros/mpc_ros/
│   ├── include/
│   │   └── MetricsCollector.h          # Metrics collector header
│   ├── src/
│   │   └── MetricsCollector.cpp         # Metrics collector implementation
│   ├── scripts/
│   │   ├── add_metrics_collector.sh    # Auto-integration script
│   │   ├── test_metrics.sh             # Verification script
│   │   └── analyze_metrics.py          # Analysis and plotting tool
│   └── METRICS_INTEGRATION_GUIDE.md    # Detailed guide
└── METRICS_IMPLEMENTATION_README.md    # This file
```

## 🔧 Configuration

### Parameters (add to launch file)
```xml
<!-- Metrics output file -->
<param name="metrics_output_csv" value="/tmp/tube_mpc_metrics.csv"/>

<!-- Target risk level δ (default: 0.1 = 10%) -->
<param name="target_risk_delta" value="0.1"/>
```

### ROS Topics
Real-time metrics are published to:
- `/mpc_metrics/empirical_risk` (std_msgs/Float64)
- `/mpc_metrics/feasibility_rate` (std_msgs/Float64)
- `/mpc_metrics/tracking_error` (std_msgs/Float64)
- `/mpc_metrics/solve_time_ms` (std_msgs/Float64)
- `/mpc_metrics/regret_dynamic` (std_msgs/Float64)
- `/mpc_metrics/regret_safe` (std_msgs/Float64)

## 📈 Example Output

### Console Summary (at goal)
```
========================================
     Tube MPC Metrics Summary
========================================

--- Safety and Satisfaction ---
Empirical risk (δ̂):          0.0850
Satisfaction probability:    0.9150
95% CI:                      [0.8950, 0.9350]
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
Regret/√T:                   0.001 (o(√T) growth - good!)
========================================
```

## 🎯 Paper Acceptance Criteria

All required metrics are collected:

| Metric | Collected | Target Status |
|--------|-----------|---------------|
| Satisfaction probability p̂_sat | ✅ | Automated |
| 95% Confidence Intervals | ✅ | Wilson score interval |
| Empirical risk δ̂ | ✅ | Automated |
| Dynamic regret R_T^dyn/T | ✅ | Automated |
| Safe regret R_T^safe/T | ✅ | Automated |
| Recursive feasibility rate | ✅ | Automated |
| Computation time (median, p90) | ✅ | Automated |
| Tracking error statistics | ✅ | Automated |
| Tube occupancy/violation | ✅ | Automated |
| Calibration accuracy | ✅ | Automated |

## 🔍 Verification

### Quick Test
```bash
# Run verification script
./src/tube_mpc_ros/mpc_ros/scripts/test_metrics.sh
```

### Manual Verification
1. ✅ Check CSV file created: `ls -l /tmp/tube_mpc_metrics.csv`
2. ✅ Check ROS topics: `rostopic list | grep mpc_metrics`
3. ✅ Monitor topics: `rostopic hz /mpc_metrics/empirical_risk`
4. ✅ Run navigation and check summary at goal

## 📚 Documentation

- **Detailed Guide**: `src/tube_mpc_ros/mpc_ros/METRICS_INTEGRATION_GUIDE.md`
- **Implementation**: `include/MetricsCollector.h`, `src/MetricsCollector.cpp`
- **Analysis Tool**: `scripts/analyze_metrics.py`

## 🐛 Troubleshooting

### CSV not created
```bash
# Check permissions
ls -ld /tmp
sudo chmod 1777 /tmp
```

### Topics not publishing
```bash
# Check if publishers initialized
rostopic list | grep mpc_metrics

# Rebuild workspace
catkin_make
source devel/setup.bash
```

### Summary not printed
- Ensure goal is reached (check AMCL pose)
- Check console for error messages
- Verify metrics are being recorded in CSV

## 🔄 Integration Status

- ✅ MetricsCollector class implemented
- ✅ Integrated into TubeMPCNode
- ✅ CMakeLists.txt update script
- ✅ Test and verification scripts
- ✅ Analysis and visualization tools
- ✅ Documentation

**Status**: Ready for testing and deployment

## 📞 Next Steps

1. **Test**: Run navigation and verify metrics collection
2. **Analyze**: Use analyze_metrics.py to generate plots
3. **Compare**: Run multiple controllers and compare metrics
4. **Report**: Use metrics for paper evaluation

---

**Last Updated**: 2026-03-13
**Implementation**: Complete and Ready for Testing
**Documentation**: METRICS_INTEGRATION_GUIDE.md
