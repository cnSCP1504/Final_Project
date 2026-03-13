# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a ROS Noetic catkin workspace implementing **Safe-Regret MPC**, a unified framework integrating temporal-logic task specification, probabilistic safety, and learning guarantees for safety-critical robotic control under uncertainty and partial observability.

### Research Goal

Based on the paper **"Safe-Regret MPC for Temporal-Logic Tasks in Stochastic, Partially Observable Robots"** (located in `latex/manuscript.tex`), this project aims to implement a complete system that:

1. **Optimizes temporal-logic task performance** using Signal Temporal Logic (STL) specifications in belief space
2. **Enforces probabilistic safety** through distributionally robust chance constraints with data-driven tightening
3. **Provides learning guarantees** by bounding regret relative to a safety-feasible policy class
4. **Bridges abstract planning and continuous control** via tube/track MPC with provable regret transfer

### Implementation Status: ✅ Phase 1 Complete | 🔄 Phase 2 In Progress

- ✅ **Phase 1 (COMPLETE)**: Tube MPC foundation - nominal system decomposition, error feedback, robustness
- 🔄 **Phase 2 (IN PROGRESS)**: STL integration - belief-space robustness evaluation, budget mechanism
- 📋 **Phase 3 (PLANNED)**: Distributionally robust chance constraints - data-driven tightening
- 📋 **Phase 4 (PLANNED)**: Regret analysis - reference planning, regret transfer
- 📋 **Phase 5 (PLANNED)**: System integration - unified Safe-Regret MPC
- 📋 **Phase 6 (PLANNED)**: Experimental validation - collaborative assembly & logistics tasks

**See `PROJECT_ROADMAP.md` for detailed implementation plan and progress tracking.**

### Key Components

- **tube_mpc_ros/**: Main MPC implementation package (Phase 1 - COMPLETE)
  - Standard MPC, Tube MPC, and Pure Pursuit controllers
  - Local planner plugin for move_base
  - Multiple parameter configurations for different performance requirements

- **stl_monitor/** (Phase 2 - IN PROGRESS): STL evaluation and monitoring
  - Belief-space robustness computation with smooth surrogates
  - Robustness budget mechanism
  - Gradient computation for MPC integration

- **dr_tightening/** (Phase 3 - PLANNED): Distributionally robust constraint tightening
  - Residual collection and ambiguity set calibration
  - Data-driven margin computation
  - Chance constraint enforcement

- **reference_planner/** (Phase 4 - PLANNED): Abstract layer with regret analysis
  - No-regret trajectory planning
  - Reference generation with feasibility checking
  - Regret tracking and analysis

- **safe_regret_mpc/** (Phase 5 - PLANNED): Unified system integration
  - Complete Safe-Regret MPC solver
  - ROS node architecture
  - Parameter configuration system

- **mpc_ros/**: Original MPC ROS package (referenced implementation)

- **servingbot/**: Mobile manipulator robot packages with navigation and manipulation

- **ThirdParty libraries**: Ipopt, ASL, HSL, Mumps (optimization solvers)

## Build and Run Commands

### Build the workspace
```bash
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

### Run Tube MPC navigation
```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

### Compare different controllers
```bash
# Tube MPC
roslaunch tube_mpc_ros tube_mpc_navigation.launch controller:=tube_mpc

# Standard MPC
roslaunch tube_mpc_ros nav_gazebo.launch controller:=mpc

# DWA
roslaunch tube_mpc_ros nav_gazebo.launch controller:=dwa

# Pure Pursuit
roslaunch tube_mpc_ros nav_gazebo.launch controller:=pure_pursuit
```

### Reference trajectory tracking
```bash
roslaunch tube_mpc_ros ref_trajectory_tracking_gazebo.launch
```

## Project Architecture

### Control System Hierarchy

1. **Global Planner**: Generates high-level path (can use standard or custom global planner)
2. **Local Planner (MPC)**:
   - Receives global path and robot state
   - Solves optimization problem using CppAD + Ipopt
   - Generates velocity commands (cmd_vel)
3. **Tube MPC Extension**:
   - Decomposes system into nominal + error systems
   - Uses LQR for error feedback control
   - Applies constraint tightening for robustness
   - Adapts disturbance bounds online

### Key Files Structure

```
src/tube_mpc_ros/mpc_ros/
├── include/            # Header files for MPC classes
├── src/               # Implementation files
│   ├── TubeMPC.cpp        # Tube MPC core algorithm
│   ├── TubeMPCNode.cpp    # ROS integration
│   ├── mpc_plannner_ros.cpp  # MPC planner plugin
│   └── navMPC.cpp         # Navigation MPC
├── params/            # Parameter configuration files
│   ├── tube_mpc_params.yaml           # Current active config
│   ├── tube_mpc_params_optimized.yaml  # Balanced performance
│   ├── tube_mpc_params_aggressive.yaml # High speed
│   └── tube_mpc_params_conservative.yaml # High precision
├── launch/           # ROS launch files
└── cfg/              # Dynamic reconfigure configs
```

## Parameter Configuration

### Key MPC Parameters (in params/tube_mpc_params.yaml)

- `mpc_steps`: Prediction horizon length (default: 20)
- `mpc_ref_vel`: Reference velocity (m/s)
- `mpc_max_angvel`: Maximum angular velocity (rad/s) - **Critical for performance**
- `mpc_w_cte`: Cross-track error weight
- `mpc_w_etheta`: Heading error weight
- `mpc_w_vel`: Velocity weight
- `disturbance_bound`: Initial disturbance bound for Tube MPC

### Parameter Switching Tool

Use the provided script to switch between parameter configurations:
```bash
test/scripts/switch_mpc_params.sh
# Options: 1) Backup, 2) Optimized, 3) Aggressive, 4) Conservative
```

Apply optimized parameters (recommended):
```bash
test/scripts/quick_fix_mpc.sh
```

## Tuning and Optimization

### Common Issues

1. **Robot moving too slowly**:
   - Increase `mpc_max_angvel` (try 1.5-2.0)
   - Increase `mpc_ref_vel`
   - Decrease `mpc_w_vel` weight

2. **Poor path tracking**:
   - Increase `mpc_w_cte` and `mpc_w_etheta`
   - Use conservative parameter set

3. **Unstable motion**:
   - Increase `mpc_w_angvel` and `mpc_w_accel`
   - Decrease reference velocity

### Tuning Documentation

Refer to `test/docs/TUBEMPC_TUNING_GUIDE.md` for comprehensive tuning guidance including:
- Detailed parameter explanations
- Tuning strategies
- Performance trade-offs
- Troubleshooting methods

## Helper Scripts and Tools

Located in `test/scripts/`:

- `quick_fix_mpc.sh`: Apply optimized parameters (recommended starting point)
- `switch_mpc_params.sh`: Switch between parameter configurations
- `auto_tune_mpc.sh`: Automated parameter tuning
- `debug_tubempc.sh`: Debug helper with verbose logging

## Dependencies

### System Requirements
- Ubuntu 20.04
- ROS Noetic
- Ipopt solver (version 3.14.20 recommended)

### ROS Dependencies
```bash
sudo apt install ros-noetic-costmap-2d \
                    ros-noetic-move-base \
                    ros-noetic-global-planner \
                    ros-noetic-amcl \
                    ros-noetic-dwa-local-planner
```

### Ipopt Installation
If Ipopt issues occur, use the upgrade script:
```bash
test/scripts/upgrade_to_new_ipopt.sh
```

## Development Workflow

1. **Modify parameters**: Edit `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`
2. **Rebuild if needed**: Only needed if C++ code changes
   ```bash
   catkin_make
   source devel/setup.bash
   ```
3. **Test navigation**:
   ```bash
   roslaunch tube_mpc_ros tube_mpc_navigation.launch
   ```
4. **Monitor performance**: Check `/mpc_trajectory` and `/tube_boundaries` in RViz

## Tube MPC Specifics

### Controller Architecture
- **Total Control**: u = v + Ke
  - v: Nominal control (from MPC optimization)
  - Ke: Feedback control (from LQR)

### System Decomposition
- **Nominal system**: z[k+1] = Az[k] + Bv[k]
- **Error system**: e[k+1] = (A-BK)e[k] + w[k]
- **Actual state**: x[k] = z[k] + e[k]

### Key Features
- Adaptive disturbance estimation
- Constraint tightening for robustness
- Tube invariant set computation
- Visualization of tube boundaries

## Data Collection

MPC performance data is automatically logged to `/tmp/tube_mpc_data.csv` with columns:
- cte (cross-track error)
- etheta (heading error)
- cmd_vel (velocity commands)
- tube_radius (current tube size)

Use this data for performance analysis and parameter optimization.

## Related Documentation

- `test/docs/TUBE_MPC_README.md`: Tube MPC implementation details
- `test/docs/COMPLETE_SOLUTION_SUMMARY.md`: Problem solutions summary
- `test/docs/SYSTEM_ENVIRONMENT_DIAGNOSIS.md`: Environment setup issues
- `test/docs/TUBEMPC_CRASH_FIX_REPORT.md`: Crash troubleshooting

## Safe-Regret MPC Theory (From Paper)

### Problem Formulation

**Given**:
- Stochastic dynamics: x_{k+1} = f(x_k, u_k, w_k)
- Partial observations: y_k = g(x_k, ν_k)
- STL task specification φ over outputs z = ψ(x)
- Safety set C = {x: h(x) ≥ 0}
- Risk level δ ∈ (0,1)

**Optimization Problem** (solved at each time k):
```
min  E[Σ ℓ(x_t,u_t)] - λ·ρ^soft(φ; x_{k:k+N})
s.t. dynamics constraints
     state/input bounds
     DR chance constraints: inf_{ℙ∈𝒫_k} Pr(h(x_t) ≥ 0) ≥ 1-δ
     terminal set: x_{k+N} ∈ 𝒳_f
     robustness budget: R_{k+1} ≥ 0
```

### Four Technical Components

#### 1. Belief-Space STL Robustness (Phase 2)
- **Smooth surrogate**: Replace min/max with log-sum-exp (smin/smax)
- **Belief-space evaluation**: ρ̃_k = E_{x∼β_k}[ρ^soft(φ; x_{k:k+N})]
- **Rolling budget**: R_{k+1} = max{0, R_k + ρ̃_k - r̲}
- **Temperature parameter**: τ controls smoothness vs accuracy trade-off

#### 2. Distributionally Robust Chance Constraints (Phase 3)
- **Ambiguity set**: 𝒫_k from residuals in sliding window (size M=200)
- **Tightening**: h(z_t) ≥ σ_{k,t} + η_ℰ
  - σ_{k,t}: Data-driven margin from DR-CVaR or Chebyshev bound
  - η_ℰ: Tube error compensation (L_h·ē)
- **Risk allocation**: δ_t with Σ δ_t ≤ δ (uniform or deadline-weighted)

#### 3. Tube/Track Decomposition (Phase 1 - COMPLETE)
- **Decomposition**: x_t = z_t + e_t
- **Nominal**: z_{t+1} = f̄(z_t, v_t)
- **Error feedback**: e_{t+1} = (A+BK)e_t + Gω_t + Δ_t
- **Input split**: u_t = v_t + Ke_t
- **Terminal set**: 𝒳_f is DR control-invariant

#### 4. Regret Transfer (Phase 4)
- **Abstract regret**: R_T^ref = o(T) (from reference planner)
- **Tracking bound**: Σ ‖e_k‖ ≤ C_e·√T (Lemma 4.6)
- **Dynamic regret**: R_T^dyn = o(T) (Theorem 4.8)
- **Safe regret**: R_T^safe = o(T) (restricted to safety-feasible policies)

### Theoretical Guarantees

1. **Recursive feasibility** (Theorem 4.5): If feasible at k=0, stays feasible ∀k≥0
2. **ISS-like stability** (Theorem 4.6): Bounded disturbance → bounded state
3. **Probabilistic satisfaction** (Theorem 4.7): Pr(ρ(φ) ≥ 0) ≥ 1-δ-ε_n-η_τ
4. **Performance** (Theorem 4.8): R_T^dyn = o(T), R_T^safe = o(T)

### Experimental Tasks

#### Task 1: Collaborative Assembly
- STL specification:
  ```
  φ_asm = G_[0,T]( ¬HumanNear ∧
           F_[0,t_d] AtA ∧
           F_[0,t_d](AtB ∧ G_[0,Δ] Grasped) ∧
           F_[0,t_d](AtC ∧ G_[0,Δ] Placed) )
  ```
- Safety: δ ∈ {0.05, 0.1}
- Platform: UR5e on Clearpath Ridgeback

#### Task 2: Logistics in Partially Known Maps
- STL specification:
  ```
  φ_log = G_[0,T]( ¬Collision ∧
           F_[0,t_s] AtS1 ∧
           F_[0,t_s] AtS2 ∧
           G_[0,T] Battery ≥ b_min )
  ```
- Features: Topological belief-space planning, no-regret exploration
- Platform: Clearpath Jackal with depth camera

### Baselines for Comparison

- **B1**: Nominal STL-MPC (no chance constraints)
- **B2**: DR-MPC (no STL, quadratic tracking)
- **B3**: CBF shield + nominal MPC
- **B4**: Abstract no-regret planner + PID tracking

### Key Implementation Parameters

| Parameter | Symbol | Typical Value | Description |
|-----------|--------|---------------|-------------|
| Horizon | N | 20 (mobile), 30 (manip) | Prediction horizon |
| Risk level | δ | 0.05 - 0.1 | Target violation probability |
| Temperature | τ | 0.05 - 0.1 | Smooth robustness accuracy |
| Window size | M | 200 | Residual window for ambiguity |
| Budget baseline | r̲ | 0.1 - 0.5 | Per-step robustness requirement |
| Tube feedback | K | LQR gain | Error feedback gain |
| Solver budget | - | 8 ms | Max solve time per step |

## Development Phases Summary

| Phase | Focus | Status | Key Deliverables |
|-------|-------|--------|------------------|
| 1 | Tube MPC | ✅ Complete | Robust path tracking |
| 2 | STL Integration | 🔄 10% | Belief-space robustness |
| 3 | DR Constraints | 📋 0% | Data-driven safety |
| 4 | Regret Analysis | 📋 0% | Reference planning |
| 5 | Integration | 📋 0% | Unified Safe-Regret MPC |
| 6 | Experiments | 📋 0% | Validation & benchmarks |

**Current Priority**: Phase 2 - STL Monitoring Module
- Next steps: Implement STL parser, smooth robustness calculator, belief-space evaluator
- See `PROJECT_ROADMAP.md` for detailed task breakdown

## Git Workflow Notes

- Main development branch: `master`
- Current feature branch: `STL` (for Phase 2 development)
- Build artifacts (build/, devel/) are gitignored
- Parameter configurations are tracked in params/
- Paper source: `latex/manuscript.tex` (1372 lines, complete draft)

## References

- Main Paper: `latex/manuscript.tex` - Safe-Regret MPC theoretical foundation
- Roadmap: `PROJECT_ROADMAP.md` - Detailed implementation plan
- Tube MPC Docs: `test/docs/TUBE_MPC_README.md` - Phase 1 documentation
