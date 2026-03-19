# DR Tightening Package

## Overview

The `dr_tightening` package implements **Distributionally Robust (DR) Chance Constraint Tightening** for Safe-Regret MPC, based on Lemma 4.1 from the manuscript.

This package collects residuals from Tube MPC, computes data-driven safety margins online, and publishes constraint tightening for the MPC optimizer to enforce probabilistic safety guarantees.

## Theory (Manuscript Eq. 698 - Lemma 4.1)

The DR chance constraint is tightened as:

```
h(z_t) ≥ L_h·ē + μ_t + κ_{δ_t}·σ_t + σ_{k,t}
```

Where:
- `L_h·ē`: Tube offset (Lipschitz constant × tube radius)
- `μ_t`: Mean along sensitivity direction (c^T μ)
- `σ_t`: Standard deviation along sensitivity direction (√(c^T Σ c))
- `κ_{δ_t}`: Cantelli factor = √((1-δ_t)/δ_t)
- `σ_{k,t}`: Additional DR margin from ambiguity set

### Key Components

1. **ResidualCollector**: Sliding window (M=200) of residuals
2. **AmbiguityCalibrator**: Computes ambiguity set statistics
3. **TighteningComputer**: Calculates deterministic margins using Cantelli bound
4. **SafetyLinearization**: Linearizes safety function h(x) at nominal trajectory

## Installation

```bash
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

## Usage

### Standalone (with Tube MPC running separately)

Terminal 1:
```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

Terminal 2:
```bash
roslaunch dr_tightening dr_tightening.launch
```

### Integrated Launch

```bash
roslaunch dr_tightening dr_tube_mpc_integrated.launch
```

## ROS Topics

### Subscribed Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/tube_mpc/tracking_error` | `std_msgs/Float64MultiArray` | Tracking errors [cte, etheta, error_norm, tube_radius] |
| `/mpc_trajectory` | `nav_msgs/Path` | MPC predicted trajectory |
| `/odom` | `nav_msgs/Odometry` | Robot odometry |
| `/cmd_vel` | `geometry_msgs/Twist` | Velocity commands |

### Published Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/dr_margins` | `std_msgs/Float64MultiArray` | Constraint tightening margins for MPC |
| `/dr_margins_debug` | `visualization_msgs/MarkerArray` | Debug visualization |
| `/dr_statistics` | `std_msgs/Float64MultiArray` | Statistics [mean, std, residuals_count] |

## Parameters

Located in `src/dr_tightening/params/dr_tightening_params.yaml`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `sliding_window_size` | 200 | Residual window size M |
| `risk_level` | 0.1 | Total violation probability δ |
| `risk_allocation` | "uniform" | Risk allocation strategy (uniform/deadline_weighted) |
| `confidence_level` | 0.95 | Confidence for ambiguity set |
| `tube_radius` | 0.5 | Tube radius ē |
| `lipschitz_constant` | 1.0 | Safety function Lipschitz constant L_h |
| `safety_buffer` | 1.0 | Safety buffer for obstacles |
| `obstacle_influence_radius` | 5.0 | Obstacle influence range |
| `update_rate` | 10.0 | DR margin update rate (Hz) |
| `enable_visualization` | true | Enable RViz visualization |
| `enable_debug_output` | false | Enable debug logging |
| `log_to_csv` | false | Log margins to CSV |
| `csv_output_path` | "/tmp/dr_margins.csv" | CSV output path |

## Testing

### Quick Integration Test
```bash
./test_dr_tightening_integration.sh
```

### Full Integration Test
```bash
./test_dr_tightening_full.sh
```

### Unit Tests
```bash
# Test DR formulas
./devel/lib/dr_tightening/dr_tightening_test

# Comprehensive tests
./devel/lib/dr_tightening/dr_tightening_comprehensive_test

# Stress tests
./devel/lib/dr_tightening/test_stress
```

## Integration with Tube MPC

The Tube MPC node publishes tracking errors to `/tube_mpc/tracking_error`:

```cpp
// In TubeMPCNode.cpp
std_msgs::Float64MultiArray tracking_error_msg;
tracking_error_msg.data.push_back(cte);                    // Cross-track error
tracking_error_msg.data.push_back(etheta);                 // Heading error
tracking_error_msg.data.push_back(_e_current.norm());      // Error norm
tracking_error_msg.data.push_back(_tube_mpc.getTubeRadius()); // Tube radius
_pub_tracking_error.publish(tracking_error_msg);
```

The DR tightening node subscribes to this topic and computes safety margins.

## Performance

- **Residual Collection**: ~0.003 ms per sample
- **Margin Computation**: ~0.004 ms per horizon step
- **Memory**: ~4.8 KB for M=200, Dim=3
- **Maximum Frequency**: >100 Hz (suitable for real-time control)

## Verification

All 6 core formulas from Lemma 4.1 verified numerically with error < 1e-10:

1. ✅ Cantelli factor computation
2. ✅ Sensitivity statistics (μ_t, σ_t)
3. ✅ Tube offset (L_h·ē)
4. ✅ Complete margin formula
5. ✅ Risk allocation (uniform/weighted)
6. ✅ Safety function linearization

## Troubleshooting

### No DR margins published
- Check if Tube MPC is running and publishing to `/tube_mpc/tracking_error`
- Verify parameter file is loaded: `rosparam get /dr_tightening_node/sliding_window_size`
- Check node logs: `rosnode info dr_tightening_node`

### CSV logging not working
- Set `log_to_csv` to `true` in params file or launch argument
- Check write permissions for `csv_output_path`
- Verify CSV file path exists

### Visualization not showing
- Enable `enable_visualization` in params
- Add DR markers to RViz display
- Check topic names match

## References

- **Main Paper**: `latex/manuscript.tex` - Lemma 4.1 (Eq. 698)
- **Roadmap**: `PROJECT_ROADMAP.md` - Phase 3 implementation
- **Tube MPC**: `test/docs/TUBE_MPC_README.md`

## Next Steps

1. Test integration with actual robot navigation
2. Tune parameters for specific environments
3. Verify probabilistic safety guarantees experimentally
4. Integrate with Phase 4 (Reference Planner)
