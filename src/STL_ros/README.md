# STL_ros: STL Monitoring and Evaluation for Safe-Regret MPC

## Overview

STL_ros is a ROS package that implements **Signal Temporal Logic (STL)** monitoring and evaluation for the Safe-Regret MPC framework. It provides belief-space robustness computation, smooth surrogates for optimization, and budget management for long-term satisfaction guarantees.

## Key Features

- **STL Formula Parser**: Parse and evaluate STL specifications with temporal operators
- **Smooth Robustness**: Differentiable log-sum-exp approximation for MPC integration
- **Belief-Space Evaluation**: Monte Carlo-based expected robustness under uncertainty
- **Robustness Budget**: Rolling budget mechanism to prevent satisfaction erosion
- **ROS Integration**: Seamless integration with Tube MPC for real-time control

## Architecture

```
STL_ros/
├── include/STL_ros/          # Header files
│   ├── STLFormula.h          # STL formula representation and parser
│   ├── SmoothRobustness.h    # Log-sum-exp smooth approximation
│   ├── BeliefSpaceEvaluator.h # Belief-space expected robustness
│   ├── RobustnessBudget.h    # Budget management
│   ├── STLMonitor.h          # Main monitoring class
│   └── STLROSInterface.h     # ROS integration
├── src/                      # Implementation files
├── msg/                      # ROS message definitions
├── cfg/                      # Dynamic reconfigure configs
├── launch/                   # Launch files
└── params/                   # Parameter configurations
```

## Theoretical Foundation

Based on the Safe-Regret MPC manuscript, the package implements:

### Smooth Robustness (Manuscript Eq. 11-12)
```
smax_τ(z₁,...,zq) = τ * log(∑ᵢ e^(zᵢ/τ))
smin_τ(z₁,...,zq) = -τ * log(∑ᵢ e^(-zᵢ/τ))
```

Properties:
- Preserves ordering
- Differentiable (gradients available)
- Bounded error: |ρ^soft - ρ| ≤ τ * log(C(φ))

### Belief-Space Objective (Manuscript Eq. 7)
```
J_k = E[x∼β_k][∑ ℓ(x_t, u_t)] - λ * ρ^soft(φ; x_{k:k+N})
```

### Robustness Budget (Manuscript Eq. 8)
```
R_{k+1} = R_k + ρ^soft(·) - r̲
```

## Installation

### Dependencies

```bash
sudo apt install ros-noetic-standard-msgs \
                    ros-noetic-geometry-msgs \
                    ros-noetic-nav-msgs \
                    ros-noetic-sensor-msgs \
                    ros-noetic-dynamic-reconfigure
```

### Build

```bash
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

## Usage

### Basic Usage

1. **Start STL Monitor**:
```bash
roslaunch stl_ros stl_monitor.launch
```

2. **Launch with Tube MPC**:
```bash
roslaunch stl_ros tube_mpc_with_stl.launch
```

3. **Custom Formulas**:
Edit `params/formulas.yaml` to define your STL specifications.

### Formula Syntax

Supported operators:
- **Predicates**: `x > 0`, `v >= 0.5`, etc.
- **Logic**: `&&`, `||`, `!`
- **Temporal**:
  - `always[a,b](φ)` - Globally
  - `eventually[a,b](φ)` - Eventually
  - `φ1 until[a,b] φ2` - Until

Example formulas:
```yaml
# Stay in bounds
stay_in_bounds: "always[0,10]((x > 0) && (x < 10) && (y > 0) && (y < 10))"

# Reach goal
reach_goal: "eventually[0,20](at_goal)"

# Avoid obstacles
avoid_obstacle: "always[0,10](distance_to_obstacle > 1.0)"
```

### Parameters

Key parameters in `params/stl_params.yaml`:

| Parameter | Description | Default |
|-----------|-------------|---------|
| `temperature_tau` | Smoothness vs accuracy trade-off | 0.05 |
| `num_monte_carlo_samples` | Belief-space samples | 100 |
| `baseline_robustness_r` | Budget requirement r̲ | 0.1 |
| `stl_weight_lambda` | Objective weight λ | 1.0 |
| `prediction_horizon` | MPC horizon N | 20 |

## ROS Topics

### Subscribed Topics

- `~belief` (geometry_msgs/PoseStamped): Robot belief state
- `~mpc_trajectory` (nav_msgs/Path): MPC prediction
- `~robot_state` (sensor_msgs/JointState): Robot state

### Published Topics

- `~robustness` (stl_ros/STLRobustness): Robustness values
- `~budget` (stl_ros/STLBudget): Budget status
- `~formula` (stl_ros/STLFormula): Formula updates

## Integration with Tube MPC

The STL monitor integrates with Tube MPC through:

1. **Subscribe** to MPC prediction trajectory
2. **Evaluate** STL formulas over belief and prediction
3. **Publish** robustness and budget for MPC cost function
4. **MPC uses** robustness to compute: `J = cost - λ * ρ^soft(φ)`

## Configuration Files

### `params/formulas.yaml`
Define STL formulas and predicates.

### `params/stl_params.yaml`
STL monitor parameters (tau, samples, budget, etc.).

### Dynamic Reconfigure
Real-time parameter tuning:
```bash
rosrun rqt_reconfigure rqt_reconfigure
```

## Examples

### Example 1: Navigation with Constraints

```yaml
# Stay within workspace
stay_in_workspace: "always[0,20]((x > -5) && (x < 5) && (y > -5) && (y < 5))"

# Maintain minimum speed
min_speed: "always[0,20](v >= 0.2)"

# Reach goal eventually
reach_goal: "eventually[0,30](at_goal)"
```

### Example 2: Collaborative Assembly

```yaml
# Safe-Regret MPC task specification
task: "(always[0,100](no_collision)) && (eventually[0,50](at_A)) && (eventually[0,50]((at_B) && (always[0,5](grasped))))"
```

### Example 3: Logistics with Battery

```yaml
# Logistics task with safety
logistics_task: "(always[0,100](battery >= 0.2)) && (eventually[0,40](at_station1)) && (eventually[0,40](at_station2))"
```

## Troubleshooting

### Issue: Slow computation
**Solution**: Reduce `num_monte_carlo_samples` or enable `use_importance_sampling`.

### Issue: Infeasible budget
**Solution**: Lower `baseline_robustness_r` or increase `initial_budget`.

### Issue: Poor tracking
**Solution**: Reduce `stl_weight_lambda` to prioritize tracking over satisfaction.

## Performance

Typical computation times (Intel i7, 100 samples):

| Operation | Time (ms) |
|-----------|-----------|
| Simple predicate | 0.5 |
| Until formula | 2.0 |
| Belief-space eval (100 samples) | 5.0 |
| Full monitor (3 formulas) | 15.0 |

## References

- Safe-Regret MPC Manuscript: `latex/manuscript.tex`
- Phase 2 Implementation: `PROJECT_ROADMAP.md`
- Tube MPC Documentation: `test/docs/TUBE_MPC_README.md`

## Future Work

- [ ] Implement importance sampling
- [ ] Add formula optimizer
- [ ] Multi-threaded evaluation
- [ ] GPU acceleration
- [ ] Integration with global planner

## Authors

Dixon Chen - Safe-Regret MPC Implementation

## License

MIT
