# STL-MPC Navigation System

## Overview

This launch file integrates Signal Temporal Logic (STL) monitoring and constraints with the Tube MPC navigation system, as part of Phase 2 of the Safe-Regret MPC implementation.

## Files Created

1. **`stl_mpc_navigation.launch`** - Main launch file with STL integration
2. **`params/stl_params.yaml`** - STL parameter configuration
3. **`params/stl_rviz_navigation.rviz`** - RViz configuration with STL visualization

## Launch Commands

### Basic Usage
```bash
# Launch STL-enabled Tube MPC navigation
roslaunch tube_mpc_ros stl_mpc_navigation.launch

# Launch without STL (fallback to standard Tube MPC)
roslaunch tube_mpc_ros stl_mpc_navigation.launch enable_stl:=false
```

### Advanced Options
```bash
# Custom STL configuration
roslaunch tube_mpc_ros stl_mpc_navigation.launch stl_config:=/path/to/custom_stl.yaml

# Different robot start pose
roslaunch tube_mpc_ros stl_mpc_navigation.launch x_pos:=1.0 y_pos:=2.0 yaw:=0.0

# Without Gazebo GUI (faster)
roslaunch tube_mpc_ros stl_mpc_navigation.launch gui:=false
```

## STL Features

### Current Implementation (Framework)
- ✅ Launch file integration with STL nodes
- ✅ Parameter configuration system
- ✅ Topic remapping for MPC-STL communication
- ✅ RViz visualization setup
- ✅ Data logging configuration

### Required Components (To Be Implemented)
- ⚠️ **STL Monitor Node** (`stl_ros/stl_monitor.py`) - Source code needs to be created
- ⚠️ **STL Visualizer Node** (`stl_ros/stl_visualizer.py`) - Source code needs to be created
- ⚠️ **STL Parser** - Parse STL formulas from text/spec
- ⚠️ **Smooth Robustness Calculator** - Compute ρ^soft(φ)
- ⚠️ **Belief-Space Evaluator** - E[ρ^soft(φ; x)] over belief β

## STL-MPC Integration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Navigation System                         │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐      ┌──────────────┐                     │
│  │ Global       │      │ Local        │                     │
│  │ Planner      │─────▶│ Tube MPC     │─────▶ cmd_vel      │
│  │              │      │              │                     │
│  └──────────────┘      └──────┬───────┘                     │
│                               │                              │
│                               │                              │
│  ┌────────────────────────────▼──────────────────────────┐  │
│  │            STL Monitoring System                       │  │
│  │                                                         │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐ │  │
│  │  │ STL          │  │ STL          │  │ STL         │ │  │
│  │  │ Parser       │─▶│ Robustness   │─▶│ Budget      │ │  │
│  │  │              │  │ Calculator   │  │ Manager     │ │  │
│  │  └──────────────┘  └──────┬───────┘  └─────────────┘ │  │
│  │                            │                            │  │
│  │                            ▼                            │  │
│  │                  ┌───────────────┐                     │  │
│  │                  │ Constraint    │                     │  │
│  │                  │ Generator     │─────▶ MPC          │  │
│  │                  └───────────────┘                     │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

## Topic Interface

### STL Monitor Subscribes To:
- `/amcl_pose` - Robot pose from localization
- `/move_base/goal` - Navigation goal
- `/move_base/GlobalPlanner/plan` - Global path plan
- `/odom` - Odometry data

### STL Monitor Publishes:
- `/stl/robustness` (Float32) - Current STL robustness value
- `/stl/violation` (Bool) - STL constraint violation flag
- `/stl/budget` (Float32) - Rolling robustness budget

### Tube MPC Subscribes To (STL-related):
- `/tube_mpc/stl_robustness` - Remapped from STL monitor
- `/tube_mpc/stl_constraint_violation` - Constraint violation info

## Parameter Configuration

### Key STL Parameters (`stl_params.yaml`)

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | bool | true | Enable STL constraints |
| `time_horizon` | float | 20.0 | STL evaluation horizon (s) |
| `temperature` | float | 0.1 | Smooth robustness accuracy |
| `baseline_requirement` | float | 0.1 | Per-step robustness (r̲) |
| `constraint_mode` | string | "soft" | "hard", "soft", or "penalty" |
| `penalty_weight` | float | 100.0 | Soft constraint weight |

### STL Predicates

```yaml
predicates:
  reachability:
    type: "distance"
    threshold: 0.5  # meters

  safety:
    type: "obstacle_distance"
    threshold: 0.3  # meters

  velocity:
    type: "velocity_limit"
    max_linear: 1.0  # m/s
    max_angular: 1.5  # rad/s
```

## Current Status

### Phase 2: STL Integration - 10% Complete

#### ✅ Complete:
- Launch file infrastructure
- Parameter configuration framework
- Topic communication setup
- Visualization configuration

#### 🔄 In Progress:
- STL Monitor node implementation
- STL Visualizer node implementation
- STL parser for formula specification

#### 📋 To Do:
- Implement smooth robustness computation (log-sum-exp)
- Belief-space robustness evaluation
- Gradient computation for MPC integration
- STL constraint tightening
- Testing and validation

## Testing the Launch File

### Prerequisites
```bash
# Build the workspace
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

### Test Launch
```bash
# This will attempt to launch all nodes
# Note: STL nodes may fail if source code is not implemented
roslaunch tube_mpc_ros stl_mpc_navigation.launch
```

### Expected Behavior
1. **Gazebo** opens with the robot and world
2. **AMCL** localizes the robot
3. **Move Base** plans navigation
4. **Tube MPC** controls the robot
5. **STL Monitor** (if implemented) evaluates constraints
6. **RViz** visualizes the system

### Troubleshooting

If STL nodes fail to launch:
```bash
# Check if STL_ros package exists
ls src/STL_ros/

# Check if executables are built
ls devel/lib/stl_ros/

# Launch without STL to verify base system
roslaunch tube_mpc_ros stl_mpc_navigation.launch enable_stl:=false
```

## Next Steps

1. **Implement STL Monitor Node** (`src/STL_ros/scripts/stl_monitor.py`)
   - Robot state subscriber
   - STL formula parser
   - Smooth robustness calculator
   - Belief-space evaluator

2. **Implement STL Visualizer Node** (`src/STL_ros/scripts/stl_visualizer.py`)
   - Robustness marker publisher
   - Visualization of constraint boundaries
   - Budget tracking display

3. **Integrate with Tube MPC**
   - Modify TubeMPC.cpp to use STL constraints
   - Implement constraint tightening
   - Add STL terms to cost function

4. **Testing and Validation**
   - Test with simple reachability specifications
   - Validate robustness computation
   - Measure performance impact

## References

- **Main Paper**: `latex/manuscript.tex` - Section 3: Belief-Space STL Robustness
- **Project Roadmap**: `PROJECT_ROADMAP.md` - Phase 2 tasks
- **Tube MPC Docs**: `test/docs/TUBE_MPC_README.md`

## Contact & Support

For issues or questions about STL-MPC integration:
1. Check `PROJECT_ROADMAP.md` for implementation status
2. Review `test/docs/` for troubleshooting guides
3. Refer to paper (`latex/manuscript.tex`) for theoretical background

---
**Last Updated**: 2026-03-15
**Phase**: 2 (STL Integration)
**Status**: Framework Complete, Implementation Pending