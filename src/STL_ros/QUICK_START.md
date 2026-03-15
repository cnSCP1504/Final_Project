# STL_ros Quick Start Guide

## Installation & Build

```bash
# Already built - just source
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
```

## Verification

```bash
# Check package is available
rospack find stl_ros
# Should output: /home/dixon/Final_Project/catkin/src/STL_ros

# Check messages
rosmsg show stl_ros/STLRobustness
rosmsg show stl_ros/STLBudget
rosmsg show stl_ros/STLFormula
```

## Basic Usage

### 1. Standalone STL Monitor

```bash
# Terminal 1: ROS master
roscore

# Terminal 2: STL monitor
roslaunch stl_ros stl_monitor.launch
```

### 2. With Tube MPC (Future)

```bash
# Launch integrated system
roslaunch stl_ros tube_mpc_with_stl.launch
```

## Configuration

### Edit STL Formulas

```bash
# Edit formula definitions
nano ~/Final_Project/catkin/src/STL_ros/params/formulas.yaml
```

Example formula:
```yaml
stay_in_bounds: "always[0,10]((x > 0) && (x < 10))"
reach_goal: "eventually[0,20](at_goal)"
```

### Adjust Parameters

```bash
# Edit parameters
nano ~/Final_Project/catkin/src/STL_ros/params/stl_params.yaml
```

Key parameters:
- `temperature_tau`: 0.05 (lower = more accurate)
- `num_monte_carlo_samples`: 100 (more = better accuracy)
- `baseline_robustness_r`: 0.1 (budget requirement)
- `stl_weight_lambda`: 1.0 (MPC weight)

### Runtime Reconfiguration

```bash
# Launch reconfigure GUI
rosrun rqt_reconfigure rqt_reconfigure

# Navigate to stl_ros and adjust parameters in real-time
```

## Formula Syntax

### Supported Operators

**Predicates**: `x > 0`, `v >= 0.5`, `y < 10`
**Logic**: `&&` (AND), `||` (OR), `!` (NOT)
**Temporal**:
- `always[a,b](φ)` - φ holds for all t in [a,b]
- `eventually[a,b](φ)` - φ holds at some t in [a,b]
- `φ1 until[a,b] φ2` - φ1 holds until φ2 becomes true

### Examples

```yaml
# 1. Safety: Stay within bounds
stay_safe: "always[0,10]((x > 0) && (x < 10) && (y > 0) && (y < 10))"

# 2. Reachability: Eventually reach goal
reach_goal: "eventually[0,30](distance_to_goal < 1.0)"

# 3. Speed constraint: Maintain minimum speed
min_speed: "always[0,20](v >= 0.3)"

# 4. Avoidance: Always stay away from obstacles
avoid_obs: "always[0,10](distance_to_obstacle > 0.5)"

# 5. Complex task (collaborative assembly)
task_spec: "(always[0,100](no_collision)) && (eventually[0,50](at_A)) && (eventually[0,50]((at_B) && (always[0,5](grasped))))"
```

## ROS Topics

### Published by STL Monitor

- `/stl_monitor/robustness` (stl_ros/STLRobustness)
  - Current robustness values
  - Satisfaction status
  - Per-timestep robustness

- `/stl_monitor/budget` (stl_ros/STLBudget)
  - Current budget R_k
  - Budget statistics
  - Feasibility status

### Subscribed by STL Monitor

- `/stl_monitor/belief` (geometry_msgs/PoseStamped)
  - Robot belief state from estimator

- `/stl_monitor/mpc_trajectory` (nav_msgs/Path)
  - Predicted trajectory from MPC

## Testing

### 1. Test Message Publishing

```bash
# Publish mock belief state
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped "
header:
  stamp: now
  frame_id: 'map'
pose:
  position: {x: 5.0, y: 5.0, z: 0.0}
  orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}"
```

### 2. Monitor Robustness Output

```bash
# Subscribe to robustness messages
rostopic echo /stl_monitor/robustness

# Subscribe to budget status
rostopic echo /stl_monitor/budget
```

### 3. Visualize Results

```bash
# Launch visualizer (matplotlib window)
rosrun stl_ros stl_visualizer.py
```

## Troubleshooting

### Issue: Package not found

```bash
# Source the workspace
source devel/setup.bash

# Rebuild if needed
catkin_make
```

### Issue: Slow computation

**Solution**: Reduce Monte Carlo samples
```yaml
# In stl_params.yaml
num_monte_carlo_samples: 50  # Reduce from 100
```

### Issue: Infeasible budget

**Solution**: Lower baseline requirement
```yaml
# In stl_params.yaml
baseline_robustness_r: 0.05  # Reduce from 0.1
```

### Issue: Poor tracking performance

**Solution**: Reduce STL weight
```yaml
# In stl_params.yaml
stl_weight_lambda: 0.5  # Reduce from 1.0
```

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Safe-Regret MPC                         │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐      ┌──────────────┐                    │
│  │   Tube MPC   │◄─────┤  STL Monitor │                    │
│  │              │      │              │                    │
│  │  • Tracking  │      │  • STL Eval  │                    │
│  │  • Safety    │      │  • Budget    │                    │
│  │  • Control   │      │  • Robustness│                    │
│  └──────────────┘      └──────────────┘                    │
│         ▲                       ▲                           │
│         │                       │                           │
│    ┌────┴─────┐         ┌──────┴──────┐                    │
│    │ Robot    │         │  Belief     │                    │
│    │ State    │         │  Estimator  │                    │
│    └──────────┘         └─────────────┘                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Advanced Features

### 1. Custom Predicates

Define custom predicates in formulas.yaml:
```yaml
predicates:
  at_goal:
    type: "distance"
    target: [10.0, 10.0]
    threshold: 2.0

  near_station:
    type: "distance"
    target: [5.0, 3.0]
    threshold: 1.5
```

### 2. Multi-Formula Monitoring

Monitor multiple formulas simultaneously:
```yaml
formula_1:
  formula_string: "always[0,10](safe)"
  priority: 2.0
  lambda: 1.0

formula_2:
  formula_string: "eventually[0,20](goal)"
  priority: 1.0
  lambda: 0.5
```

### 3. Budget Management

Each formula can have independent budget:
```yaml
# Automatic budget tracking per formula
# Budget constraints prevent satisfaction erosion
# Adaptive baseline adjustment available
```

## Performance Tips

1. **Reduce samples** for faster evaluation (50-100 samples)
2. **Increase temperature τ** for smoother gradients (0.05-0.1)
3. **Use caching** for repeated evaluations
4. **Profile computation** with `enable_profiling: true`

## Next Steps

1. ✅ **Verify build** - `rospack find stl_ros`
2. ✅ **Test messages** - `rosmsg show stl_ros/STLRobustness`
3. ⏳ **Integrate with Tube MPC** - Modify Tube MPC to use STL costs
4. ⏳ **Run experiments** - Test on navigation tasks
5. ⏳ **Collect data** - Analyze satisfaction rates and regret

## Support & Documentation

- **Full Documentation**: `README.md`
- **Implementation Details**: `IMPLEMENTATION_SUMMARY.md`
- **Theory**: See `latex/manuscript.tex` for mathematical foundations
- **Project Roadmap**: `PROJECT_ROADMAP.md` (Phase 2)

## Example: Complete Navigation Task

```yaml
# Navigation with safety and task constraints

# 1. Stay within workspace
stay_in_workspace:
  formula_string: "always[0,30]((x > -5) && (x < 5) && (y > -5) && (y < 5))"
  priority: 3.0
  lambda: 1.5

# 2. Avoid obstacles
avoid_obstacles:
  formula_string: "always[0,30](min_obstacle_distance > 0.5)"
  priority: 5.0
  lambda: 2.0

# 3. Maintain progress
maintain_speed:
  formula_string: "always[0,30](v >= 0.2)"
  priority: 1.0
  lambda: 0.5

# 4. Reach goal
reach_goal:
  formula_string: "eventually[0,30](distance_to_goal < 1.0)"
  priority: 4.0
  lambda: 2.0
```

This ensures: **Safety** + **Progress** + **Task Completion** ✅
