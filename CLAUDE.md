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

### Implementation Status: ✅ Phase 1 Complete | 🔄 Phase 2-5 Complete | 📊 Phase 6 Testing

- ✅ **Phase 1 (COMPLETE)**: Tube MPC foundation - nominal system decomposition, error feedback, robustness
- ✅ **Phase 2 (COMPLETE)**: STL integration - belief-space robustness evaluation, budget mechanism
- ✅ **Phase 3 (COMPLETE)**: Distributionally robust chance constraints - data-driven tightening
- ✅ **Phase 4 (COMPLETE)**: Regret analysis - reference planning, regret transfer
- ✅ **Phase 5 (COMPLETE)**: System integration - unified Safe-Regret MPC
- 🔄 **Phase 6 (IN TESTING)**: Experimental validation - collaborative assembly & logistics tasks

**See `PROJECT_ROADMAP.md` for detailed implementation plan and progress tracking.**

### Key Components

- **tube_mpc_ros/**: Core MPC solver and path tracking controller (Phase 1 - COMPLETE)
  - **完整实现非线性MPC优化求解器**（CppAD + Ipopt）
  - Tube MPC算法：标称系统 + 误差系统分解
  - LQR反馈控制、管状约束收紧
  - 负责实际的路径跟踪和速度命令生成
  - ⚠️ **这是系统的核心执行层**

- **stl_monitor/** (Phase 2 - COMPLETE): STL evaluation and monitoring
  - Belief-space robustness computation with smooth surrogates
  - Robustness budget mechanism
  - Gradient computation for MPC integration
  - **功能模块**：评估时序逻辑任务满足度

- **dr_tightening/** (Phase 3 - COMPLETE): Distributionally robust constraint tightening
  - Residual collection and ambiguity set calibration
  - Data-driven margin computation
  - Chance constraint enforcement
  - **功能模块**：计算概率安全约束收紧边界

- **reference_planner/** (Phase 4 - COMPLETE): Abstract layer with regret analysis
  - No-regret trajectory planning
  - Reference generation with feasibility checking
  - Regret tracking and analysis
  - **功能模块**：抽象层规划和遗憾分析

- **safe_regret_mpc/** (Phase 5 - COMPLETE): Integration framework and data hub
  - ⚠️ **不是独立的MPC求解器**
  - **数据集成中心**：收集tube_mpc的状态和控制数据
  - **协调器**：将数据传递给STL、DR、Reference Planner等模块
  - **参数管理**：统一配置各组件参数
  - **监控和可视化**：发布系统状态、性能指标
  - **注意**：实际的MPC求解由tube_mpc完成，本包只负责集成高级功能

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

### Run Safe-Regret MPC Test (Primary)
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

**⚠️ Architecture Note:**
This launch file starts:
1. **tube_mpc** (core MPC solver) - Performs actual path tracking and control
2. **safe_regret_mpc** (integration hub) - Collects data and coordinates STL/DR modules
3. Optional modules: STL monitor, DR constraint tightening, Reference planner

**Data flow:** tube_mpc → safe_regret_mpc → STL/DR modules → optional feedback to tube_mpc

**Optional parameters**:
- `enable_visualization:=true` - Enable RViz (default: false)
- `enable_stl:=true` - Enable STL monitoring (default: false)
- `enable_dr:=true` - Enable DR constraints (default: false)
- `debug_mode:=true` - Enable debug output (default: true)
- `use_gazebo:=true` - Use Gazebo simulation (default: true)

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

### System Component Collaboration

```
┌─────────────────────────────────────────────────────────────────┐
│                    Safe-Regret MPC 系统架构                      │
└─────────────────────────────────────────────────────────────────┘

数据流向:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Global Planner (move_base)
   └─> 输出: 全局路径 (/move_base/GlobalPlanner/plan)
       └─> 传递给 tube_mpc

2. tube_mpc_ros (核心MPC求解器)
   ├─> 接收: 全局路径 + 机器人状态 (odom)
   ├─> 求解: 非线性MPC优化问题 (CppAD + Ipopt)
   ├─> 输出: 速度命令 (/cmd_vel) → 机器人
   └─> 发布: MPC状态数据 → safe_regret_mpc

3. safe_regret_mpc (数据集成中心)
   ├─> 订阅: tube_mpc的状态数据
   ├─> 收集数据:
   │   ├─ 机器人状态 (odom)
   │   ├─ 跟踪误差 (tracking_error)
   │   ├─ MPC轨迹 (mpc_trajectory)
   │   └─ 管状边界 (tube_boundaries)
   ├─> 分发给功能模块:
   │   ├─> STL监控 (stl_monitor)
   │   ├─> DR约束收紧 (dr_tightening)
   │   └─> 参考规划器 (reference_planner)
   ├─> 接收处理结果:
   │   ├─ STL鲁棒性值
   │   ├─ DR安全边界
   │   └─ 参考轨迹调整建议
   ├─> 可选: 将调整参数反馈给tube_mpc
   └─> 发布: 系统状态、性能指标

4. stl_monitor (STL监控模块)
   ├─> 接收: 状态数据 from safe_regret_mpc
   ├─> 计算: STL鲁棒性 (ρ(φ; x))
   ├─> 更新: 鲁棒性预算 (R_{k+1})
   └─> 输出: STL约束建议 → safe_regret_mpc

5. dr_tightening (DR约束模块)
   ├─> 接收: 跟踪残差 from safe_regret_mpc
   ├─> 构建: 模糊集 (Wasserstein球)
   ├─> 计算: 约束收紧边界 (σ_{k,t})
   └─> 输出: DR安全边界 → safe_regret_mpc

6. reference_planner (参考规划器)
   ├─> 接收: 当前状态 and 性能数据
   ├─> 规划: No-regret参考轨迹
   ├─> 分析: 遗憾界 (R_T^dyn, R_T^safe)
   └─> 输出: 参考调整 → safe_regret_mpc

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

关键点:
✓ tube_mpc_ros: 实际的MPC求解和路径跟踪执行者
✓ safe_regret_mpc: 数据集成和协调中心，不进行MPC求解
✓ STL/DR/Reference: 独立的功能模块，通过safe_regret_mpc协调
✓ 数据流: 单向为主 (tube_mpc → safe_regret_mpc → 功能模块)
✓ 控制流: tube_mpc独立决策，可选择性接受高级功能的建议
```

### Control System Hierarchy

1. **Global Planner**: Generates high-level path (can use standard or custom global planner)
2. **Local Planner (tube_mpc)**:
   - ⚠️ **实际的MPC求解器** (完整的CppAD + Ipopt实现)
   - Receives global path and robot state
   - Solves optimization problem using CppAD + Ipopt
   - Generates velocity commands (cmd_vel)
3. **Tube MPC Extension**:
   - Decomposes system into nominal + error systems
   - Uses LQR for error feedback control
   - Applies constraint tightening for robustness
   - Adapts disturbance bounds online
4. **Integration Layer (safe_regret_mpc)**:
   - ⚠️ **不是控制器**，而是数据集成和协调框架
   - Collects data from tube_mpc
   - Coordinates STL, DR, and Reference Planner modules
   - Manages parameters and system configuration
   - Publishes monitoring and visualization data

### ⚠️ Critical Architecture Clarification

**safe_regret_mpc的正确角色定义：**

| 功能 | tube_mpc_ros | safe_regret_mpc |
|------|--------------|-----------------|
| **MPC求解** | ✅ 完整实现（CppAD+Ipopt） | ❌ 无求解能力 |
| **路径跟踪** | ✅ 核心功能 | ❌ 无跟踪功能 |
| **速度控制** | ✅ 发布cmd_vel | ❌ 不发布cmd_vel |
| **数据收集** | 发布状态数据 | ✅ 收集并分发数据 |
| **STL集成** | ❌ 无 | ✅ 协调STL模块 |
| **DR约束** | ❌ 无 | ✅ 协调DR模块 |
| **参数管理** | 独立参数文件 | ✅ 统一参数配置 |
| **系统监控** | 基础日志 | ✅ 完整监控和可视化 |

**数据流（正确理解）：**
```
tube_mpc (实际控制器)
  ↓ 发布状态数据
safe_regret_mpc (数据集成中心)
  ↓ 分发给各模块
  ├─> STL监控
  ├─> DR约束收紧
  └─> 参考规划器
  ↓ 可选反馈调整建议
tube_mpc (选择性接受建议，保持独立决策权)
```

**关键要点：**
1. ✅ tube_mpc是**核心执行层**，负责所有实际控制
2. ✅ safe_regret_mpc是**集成框架**，提供高级功能的协调接口
3. ✅ 功能模块（STL/DR/Reference）是**独立组件**，通过safe_regret_mpc接入
4. ❌ safe_regret_mpc**不替代**tube_mpc的任何核心控制功能
5. ❌ safe_regret_mpc**不是MPC求解器**，只是数据中转和协调节点

### 🔄 Integration Mode Architecture (集成模式设计)

**设计理念：** tube_mpc通过参数判断是否启用safe_regret_mpc集成，实现向后兼容的双模式架构

#### 模式A：独立模式（Standalone Mode）

```yaml
enable_safe_regret_integration: false  # 默认值
```

```
tube_mpc (独立工作)
  ├─ 求解MPC
  ├─ 发布 /cmd_vel → 机器人 ✅
  ├─ 发布 /mpc_trajectory → 可视化
  └─ 独立决策，无外部干预
```

**特点：**
- ✅ tube_mpc完全独立工作
- ✅ 无需safe_regret_mpc
- ✅ 零额外开销
- ✅ 向后兼容现有系统

#### 模式B：集成模式（Integration Mode）

```yaml
enable_safe_regret_integration: true
```

```
tube_mpc (核心控制器)
  ├─ 求解MPC
  ├─ 发布 /cmd_vel_raw → safe_regret_mpc ⚠️ (原始命令)
  ├─ 发布 /mpc_trajectory → safe_regret_mpc
  ├─ 发布 /tube_mpc/tracking_error → safe_regret_mpc
  └─ 订阅 /safe_regret_mpc/parameter_adjustments (反馈)
       ↑
       └─ 接收STL/DR处理的参数调整

safe_regret_mpc (集成中心)
  ├─ 订阅 /cmd_vel_raw (tube_mpc原始命令)
  ├─ 订阅 /mpc_trajectory
  ├─ 订阅 /tube_mpc/tracking_error
  ├─ 转发给STL模块 → 获取鲁棒性评估
  ├─ 转发给DR模块 → 获取安全边界
  ├─ 数据处理和整合
  ├─ 发布 /cmd_vel → 机器人 ✅ (最终命令)
  └─ 发布 /safe_regret_mpc/parameter_adjustments → tube_mpc
```

**数据流详解：**

1. **tube_mpc → safe_regret_mpc (原始数据)**
   - `/cmd_vel_raw`: tube_mpc求解的原始速度命令
   - `/mpc_trajectory`: MPC预测轨迹
   - `/tube_mpc/tracking_error`: 跟踪误差（用于DR模块）

2. **safe_regret_mpc内部处理**
   - STL模块：评估时序逻辑满足度，计算鲁棒性调整因子
   - DR模块：检查命令安全性，必要时进行约束
   - 数据整合：综合各模块处理结果

3. **safe_regret_mpc → 机器人 (最终命令)**
   - `/cmd_vel`: 经过STL/DR处理的最终速度命令

4. **safe_regret_mpc → tube_mpc (可选反馈)**
   - `/safe_regret_mpc/parameter_adjustments`: 参数调整建议
   - tube_mpc可根据建议调整下一步求解参数

#### 实现要点

**tube_mpc端修改：**
```cpp
// 添加参数判断
bool enable_safe_regret_integration_;

// 根据模式决定发布话题
if (enable_safe_regret_integration_) {
    pub_cmd_vel_ = nh_.advertise<geometry_msgs::Twist>("/cmd_vel_raw", 1);

    // 订阅safe_regret_mpc的反馈（可选）
    sub_param_adjustment_ = nh_.subscribe(
        "/safe_regret_mpc/parameter_adjustments", 1,
        &TubeMPCNode::parameterAdjustmentCallback, this);
} else {
    pub_cmd_vel_ = nh_.advertise<geometry_msgs::Twist>("/cmd_vel", 1);
}
```

**safe_regret_mpc端修改：**
```cpp
// 订阅tube_mpc的原始命令
sub_tube_mpc_cmd_ = nh_.subscribe("/cmd_vel_raw", 1,
    &SafeRegretMPCNode::tubeMPCCmdCallback, this);

// 处理并发布最终命令
void SafeRegretMPCNode::processTubeMPCCommand() {
    geometry_msgs::Twist cmd_final = tube_mpc_cmd_raw_;

    // STL处理
    if (stl_enabled_) {
        cmd_final = adjustCommandForSTL(cmd_final);
    }

    // DR处理
    if (dr_enabled_) {
        cmd_final = adjustCommandForDR(cmd_final);
    }

    // 发布最终命令
    pub_cmd_vel_final_.publish(cmd_final);

    // 发布参数调整（可选）
    if (needParameterAdjustment()) {
        publishParameterAdjustment();
    }
}
```

#### 方案优势

| 优势 | 说明 |
|------|------|
| **向后兼容** | 独立模式完全兼容现有系统 |
| **灵活切换** | 仅需调整参数，无需修改代码 |
| **渐进集成** | 可选择性启用功能（STL/DR/Feedback） |
| **性能可控** | 独立模式零开销，集成模式按需启用 |
| **故障安全** | safe_regret_mpc崩溃时，tube_mpc可检测并切换模式 |

#### 话题命名规范

| 话题 | 发布者 | 订阅者 | 说明 |
|------|--------|--------|------|
| `/cmd_vel_raw` | tube_mpc | safe_regret_mpc | 原始速度命令（仅集成模式） |
| `/cmd_vel` | safe_regret_mpc 或 tube_mpc | 机器人 | 最终速度命令 |
| `/mpc_trajectory` | tube_mpc | safe_regret_mpc | MPC预测轨迹 |
| `/tube_mpc/tracking_error` | tube_mpc | safe_regret_mpc | 跟踪误差 |
| `/safe_regret_mpc/parameter_adjustments` | safe_regret_mpc | tube_mpc | 参数调整反馈（可选） |

#### 配置示例

**独立模式配置：**
```xml
<!-- launch file -->
<arg name="enable_safe_regret_integration" default="false"/>
```

**集成模式配置：**
```xml
<!-- launch file -->
<arg name="enable_safe_regret_integration" default="true"/>
<arg name="enable_stl" default="true"/>
<arg name="enable_dr" default="true"/>
```

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

- **`clean_ros.sh`**: ⚠️ **CRITICAL** - Clean all ROS processes before launching (REQUIRED!)
- `quick_fix_mpc.sh`: Apply optimized parameters (recommended starting point)
- `switch_mpc_params.sh`: Switch between parameter configurations
- `auto_tune_mpc.sh`: Automated parameter tuning
- `debug_tubempc.sh`: Debug helper with verbose logging

### Process Management Script

**Use the clean_ros.sh script before EVERY launch:**
```bash
# Clean all ROS processes (REQUIRED!)
./test/scripts/clean_ros.sh

# Now launch your test
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

**Or create a convenient alias:**
```bash
# Add to ~/.bashrc
alias cleanros='./test/scripts/clean_ros.sh'
```

Usage:
```bash
cleanros  # Clean all ROS processes before launching
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

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

### ⚠️ CRITICAL: Pre-Launch Safety Check

**BEFORE running ANY launch file, you MUST stop all previous ROS processes:**

```bash
# Step 1: Check for running ROS processes
ps aux | grep roslaunch
ps aux | grep rosmaster
ps aux | grep gazebo

# Step 2: Kill all ROS processes (IMPORTANT!)
killall -9 roslaunch rosmaster roscore gzserver gzclient
killall -9 python  # Kill python ROS nodes

# Step 3: Wait for clean shutdown
sleep 2

# Step 4: Verify no processes remain
ps aux | grep ros

# Step 5: Clean up any zombies
pkill -f ros
pkill -f gazebo
```

**Why?** Running multiple instances of:
- `move_base` (conflicting path planners)
- `safe_regret_mpc_node` (conflicting velocity commands)
- `gazebo` (port conflicts and resource issues)

This causes unpredictable behavior, trajectory corruption, and system instability!

### Development Steps

1. **Clean previous processes** (CRITICAL!): See "Pre-Launch Safety Check" above
2. **Modify parameters**: Edit appropriate params files:
   - `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml` (Safe-Regret MPC)
   - `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml` (Tube MPC)
2. **Rebuild if needed**: Only needed if C++ code changes
   ```bash
   catkin_make
   source devel/setup.bash
   ```
3. **Test navigation**:
   ```bash
   roslaunch safe_regret_mpc safe_regret_mpc_test.launch
   ```
4. **Monitor performance**: Check `/mpc_trajectory` and `/tube_boundaries` in RViz

### ⚠️ CRITICAL: Git Reset Safety

**After ANY git reset operation, you MUST clean and rebuild:**

```bash
# Step 1: Reset code
git reset --hard <commit-hash>

# Step 2: Clean build artifacts (CRITICAL!)
rm -rf build/safe_regret_mpc devel/lib/safe_regret_mpc

# Step 3: Rebuild
catkin_make --only-pkg-with-deps safe_regret_mpc

# Step 4: Source updated environment
source devel/setup.bash

# Step 5: Clean any running processes (CRITICAL!)
killall -9 roslaunch rosmaster roscore gzserver gzclient python
```

**Why?** Git reset only changes source code, but compiled binaries in `devel/lib/` remain from the old version. Running old binaries with new parameters causes unpredictable behavior!

```bash
# Step 1: Reset code
git reset --hard <commit-hash>

# Step 2: Clean build artifacts (CRITICAL!)
rm -rf build/safe_regret_mpc devel/lib/safe_regret_mpc

# Step 3: Rebuild
catkin_make --only-pkg-with-deps safe_regret_mpc

# Step 4: Source updated environment
source devel/setup.bash
```

**Why?** Git reset only changes source code, but compiled binaries in `devel/lib/` remain from the old version. Running old binaries with new parameters causes unpredictable behavior!

### ⚠️ Important: Git Workflow Preferences

**DO NOT automatically commit or push changes to git**

- Code changes should be tested thoroughly before committing
- Wait for explicit user approval before creating git commits
- Do NOT run `git commit` or `git push` automatically
- User will manually commit when ready
- Focus on implementation and testing, not version control operations

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
- `test/docs/MPC_DEBUG_REPORT.md`: Latest debugging reports and solutions
- `test/docs/MPC_LOCAL_PATH_FIX.md`: Local trajectory fixes
- `test/docs/MPC_TRAJECTORY_FIX_SUMMARY.md`: Trajectory optimization summary

## RViz Configuration

### TF Display Settings
RViz configuration files are located in `src/safe_regret_mpc/rviz/`:
- `safe_regret_navigation.rviz`: Main navigation configuration (TF names disabled by default)
- `safe_regret_mpc.rviz`: Safe-Regret MPC specific configuration

**To enable/disable TF link names**:
- In RViz: Displays → TF → Show Names checkbox
- In config file: Set `Show Names: true/false` in the TF section

## Current Known Issues

### Local Trajectory Display Issue
**Status**: Under investigation (as of 2026-04-01)

**Symptom**: Local trajectory occasionally appears fixed to robot front instead of guiding navigation properly

**Troubleshooting Steps**:
1. **CRITICAL**: Kill all previous ROS processes:
   ```bash
   killall -9 roslaunch rosmaster roscore gzserver gzclient python
   sleep 2
   ```
2. Ensure clean rebuild after git reset:
   ```bash
   rm -rf build/safe_regret_mpc devel/lib/safe_regret_mpc
   catkin_make --only-pkg-with-deps safe_regret_mpc
   source devel/setup.bash
   ```
3. Check parameter configuration in `safe_regret_mpc_params.yaml`
3. Verify topic connections: `rostopic echo /mpc_trajectory`
4. Check global planner output: `rostopic echo /move_base/GlobalPlanner/plan`
5. Review debug output in launch file (set `debug_mode:=true`)

**Recent Fixes**:
- Fixed controller mode conflicts (commit 060bd8c)
- Fixed intermittent robot movement (commit 87c9a30)
- Optimized RViz display settings (commit b5cc076)

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
| 1 | Tube MPC | ✅ Complete | Robust path tracking & MPC solver |
| 2 | STL Integration | ✅ Complete | Belief-space robustness monitoring |
| 3 | DR Constraints | ✅ Complete | Data-driven safety constraint tightening |
| 4 | Regret Analysis | ✅ Complete | Reference planning & regret tracking |
| 5 | Integration | ✅ Complete | Data integration framework (safe_regret_mpc) |
| 6 | Experiments | 🔄 In Progress | Validation & benchmarks |

**Current Priority**: Phase 6 - Experimental Validation
- Active testing: Safe-Regret MPC navigation performance
- Debugging: Local trajectory display optimization
- Next: Comprehensive performance benchmarking against baselines
- See `PROJECT_ROADMAP.md` for detailed task breakdown

## Git Workflow Notes

- Main development branch: `master`
- **Current branch**: `test` (Phase 6 experimental validation)
- Previous feature branches: `STL` (Phase 2), completed and merged
- Build artifacts (build/, devel/) are gitignored
- Parameter configurations are tracked in params/
- RViz configurations are tracked in rviz/
- Paper source: `latex/manuscript.tex` (1372 lines, complete draft)

### Recent Commit History
```
b5cc076 fix: RViz配置调整 (2026-04-01)
060bd8c fix: 修复Safe-Regret MPC控制器冲突与系统启动问题
87c9a30 修复了小车移动断断续续的问题
453172c feat: Phase 5系统完善与实验环境准备
9d5675e feat: Phase 5核心完成 - Safe-Regret MPC统一系统实现
```

## References

- Main Paper: `latex/manuscript.tex` - Safe-Regret MPC theoretical foundation
- Roadmap: `PROJECT_ROADMAP.md` - Detailed implementation plan
- Tube MPC Docs: `test/docs/TUBE_MPC_README.md` - Phase 1 documentation
- Test Scripts: `test/scripts/` - Helper scripts for testing and debugging

## Quick Start Guide (New Users)

### 1. Build the System
```bash
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

### 2. Run Safe-Regret MPC Test

**⚠️ CRITICAL: Close all previous ROS/Gazebo processes first!**
```bash
# Kill all ROS processes
killall -9 roslaunch rosmaster roscore gzserver gzclient python
sleep 2
```

**Now run the test:**
```bash
# Basic test (no visualization)
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# With RViz visualization
roslaunch safe_regret_mpc safe_regret_mpc_test.launch enable_visualization:=true
```

### 3. Monitor Performance
- **Topics to watch**:
  - `/mpc_trajectory`: Local MPC trajectory
  - `/tube_boundaries`: Tube MPC boundaries
  - `/cmd_vel`: Velocity commands
  - `/odom`: Robot odometry

- **RViz Displays**:
  - Global Path (green)
  - MPC Trajectory (yellow)
  - Tube Boundaries (blue)

### 4. Troubleshooting
If problems occur after git operations:
```bash
rm -rf build/safe_regret_mpc devel/lib/safe_regret_mpc
catkin_make --only-pkg-with-deps safe_regret_mpc
source devel/setup.bash
```
