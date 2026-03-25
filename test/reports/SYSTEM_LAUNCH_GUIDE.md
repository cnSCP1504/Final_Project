# Safe-Regret MPC System Launch Guide

本指南说明如何启动Safe-Regret MPC系统的不同配置，从基础的Tube MPC到完整的Safe-Regret集成系统。

## 系统配置概览

Safe-Regret MPC系统分为4个Phase，可以逐步集成：

- **Phase 1**: Tube MPC - 基础鲁棒模型预测控制
- **Phase 2**: STL Monitor - 信号时序逻辑监控
- **Phase 3**: DR Tightening - 分布鲁棒机会约束收紧
- **Phase 4**: Reference Planner - 无遗憾参考规划器
- **Phase 5**: Safe-Regret MPC - 完整集成系统

---

## 1. 单独启动 Tube MPC (Phase 1)

**功能**: 基于Tube MPC的鲁棒路径跟踪导航

**启动命令**:
```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

**特点**:
- ✅ 鲁棒路径跟踪（Tube MPC控制器）
- ✅ Gazebo仿真环境
- ✅ AMCL定位
- ✅ move_base导航栈
- ✅ RViz可视化

**核心Topic**:
- `/mpc_trajectory` - MPC预测轨迹
- `/tube_boundaries` - Tube边界
- `/cmd_vel` - 速度命令

**参数文件**: `params/tube_mpc_params.yaml`

---

## 2. Tube MPC + STL Monitor (Phase 1 + Phase 2)

**功能**: Tube MPC集成STL约束监控

**启动命令**:
```bash
roslaunch tube_mpc_ros stl_mpc_navigation.launch
```

**特点**:
- ✅ 所有Phase 1功能
- ✅ STL公式监控
- ✅ 鲁棒度评估
- ✅ 鲁棒度预算机制
- ⚠️  **注意**: Phase 2仍在开发中，STL监控可能未完全启用

**核心Topic**:
- 所有Phase 1 topics
- `/stl_robustness` - STL鲁棒度
- `/stl_budget` - 鲁棒度预算
- `/stl_violations` - STL违反情况

**参数文件**:
- `params/tube_mpc_params.yaml`
- `params/stl_params.yaml`

---

## 3. Tube MPC + DR Tightening (Phase 1 + Phase 3)

**功能**: Tube MPC集成分布鲁棒机会约束收紧

**启动命令**:
```bash
roslaunch dr_tightening dr_tube_mpc_integrated.launch
```

**参数选项**:
```bash
# 启用DR收紧（默认）
roslaunch dr_tightening dr_tube_mpc_integrated.launch enable_dr:=true

# 禁用DR收紧（退化为纯Tube MPC）
roslaunch dr_tightening dr_tube_mpc_integrated.launch enable_dr:=false

# 启用DR调试输出
roslaunch dr_tightening dr_tube_mpc_integrated.launch dr_debug:=true
```

**特点**:
- ✅ 所有Phase 1功能
- ✅ 数据驱动的约束收紧
- ✅ 残差收集和模糊集校准
- ✅ 机会约束执行（风险水平δ）
- ✅ 自适应安全边距

**核心Topic**:
- 所有Phase 1 topics
- `/dr_margins` - 约束收紧边距
- `/dr_margins_debug_viz` - DR可视化Marker
- `/dr_statistics` - DR统计数据
- `/tube_mpc/tracking_error` - 跟踪误差（残差）

**参数文件**:
- `params/tube_mpc_params.yaml`
- `dr_tightening/params/dr_tightening_params.yaml`

**关键参数**:
| 参数 | 默认值 | 说明 |
|------|--------|------|
| `risk_level` | 0.1 | 违反概率δ |
| `sliding_window_size` | 200 | 残差窗口大小 |
| `confidence_level` | 0.95 | 置信水平 |
| `tube_radius` | 0.5 | Tube半径 |

---

## 4. 完整 Safe-Regret MPC 系统 (Phase 1 + Phase 3 + Phase 4)

**功能**: 完整的Safe-Regret MPC集成系统（无遗憾规划 + DR约束 + Tube MPC）

**启动命令**:
```bash
roslaunch safe_regret safe_regret_simplified.launch
```

**参数选项**:
```bash
# 使用OMD算法（默认）
roslaunch safe_regret safe_regret_simplified.launch no_regret_algorithm:=omd

# 使用其他算法
roslaunch safe_regret safe_regret_simplified.launch no_regret_algorithm:=mwu  # Multiplicative Weights
roslaunch safe_regret safe_regret_simplified.launch no_regret_algorithm:=ftpl  # Follow The Perturbed Leader

# 禁用RViz（用于无头运行）
roslaunch safe_regret safe_regret_simplified.launch rviz:=false
```

**系统架构**:
```
┌─────────────────────────────────────────────────────────┐
│          Reference Planner (Phase 4)                     │
│  - No-regret online learning (OMD/MWU/FTPL)             │
│  - Abstract trajectory planning                         │
│  - Regret tracking and analysis                         │
└────────────────┬────────────────────────────────────────┘
                 │ /safe_regret/reference_trajectory
                 ↓
┌─────────────────────────────────────────────────────────┐
│          DR Tightening (Phase 3)                         │
│  - Residual collection from MPC                         │
│  - Distributionally robust constraint tightening         │
│  - Data-driven safety margins                           │
└────────────────┬────────────────────────────────────────┘
                 │ /dr_margins (constraint tightening)
                 ↓
┌─────────────────────────────────────────────────────────┐
│          Tube MPC (Phase 1)                              │
│  - Robust trajectory tracking                           │
│  - Error feedback control                              │
│  - Constraint tightening integration                    │
└─────────────────────────────────────────────────────────┘
```

**特点**:
- ✅ **Phase 1**: Tube MPC鲁棒控制
- ✅ **Phase 3**: DR机会约束收紧
- ✅ **Phase 4**: 无遗憾参考规划器
- ⚠️  **Phase 2**: STL监控（暂时禁用，依赖问题待解决）
- ✅ 学习保证（有界遗憾）
- ✅ 概率安全保证（DR机会约束）
- ✅ 递归可行性

**核心Topic**:
```
# Phase 1 (Tube MPC)
/mpc_trajectory              # MPC预测轨迹
/tube_boundaries             # Tube边界
/cmd_vel                     # 速度命令

# Phase 3 (DR Tightening)
/dr_margins                  # 约束收紧边距
/dr_margins_debug_viz        # DR可视化Marker
/dr_statistics               # DR统计数据
/tube_mpc/tracking_error     # 跟踪误差残差

# Phase 4 (Reference Planner)
/safe_regret/reference_trajectory  # 参考轨迹到Tube MPC
/safe_regret/regret_metrics       # 遗憾指标
/safe_regret/planning_status      # 规划状态
```

**监控遗憾指标**:
```bash
# 查看实时遗憾指标
rostopic echo /safe_regret/regret_metrics

# 输出示例:
# average_regret: 0.123
# cumulative_regret: 1.456
# instantaneous_regret: 0.045
# planning_time_ms: 2.3
```

**参数文件**:
- `params/tube_mpc_params.yaml`
- `dr_tightening/params/dr_tightening_params.yaml`
- `safe_regret/params/safe_regret_params.yaml`

**系统状态监控**:
```bash
# 检查所有节点是否正常运行
rosnode list

# 检查topic连接
rostopic list

# 查看系统日志
rosrun rqt_console rqt_console
```

---

## 性能对比测试

系统支持多种控制器进行对比测试：

### Tube MPC vs 其他控制器

```bash
# Tube MPC (推荐)
roslaunch tube_mpc_ros tube_mpc_navigation.launch controller:=tube_mpc

# 标准 MPC
roslaunch tube_mpc_ros nav_gazebo.launch controller:=mpc

# DWA (Dynamic Window Approach)
roslaunch tube_mpc_ros nav_gazebo.launch controller:=dwa

# Pure Pursuit
roslaunch tube_mpc_ros nav_gazebo.launch controller:=pure_pursuit
```

---

## 参数调优

### Tube MPC 核心参数

编辑 `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`:

| 参数 | 默认值 | 说明 | 调优建议 |
|------|--------|------|----------|
| `mpc_steps` | 20 | 预测时域 | 路径复杂时增加到25-30 |
| `mpc_ref_vel` | 0.8 | 参考速度 (m/s) | 激进:1.0, 保守:0.5 |
| `mpc_max_angvel` | 2.0 | 最大角速度 (rad/s) | **关键**: 增加1.5-2.0提速 |
| `mpc_w_cte` | 50.0 | 横向误差权重 | 路径跟踪差时增加 |
| `mpc_w_etheta` | 60.0 | 航向误差权重 | 航向偏差大时增加 |
| `disturbance_bound` | 0.15 | 扰动界 | 根据环境调整 |

**快速应用优化参数**:
```bash
# 应用平衡性能参数（推荐）
test/scripts/quick_fix_mpc.sh

# 手动切换参数配置
test/scripts/switch_mpc_params.sh
# 选项: 1) Backup, 2) Optimized, 3) Aggressive, 4) Conservative
```

---

## 故障排查

### 常见问题

**1. 机器人移动缓慢**
```bash
# 解决方案: 增加最大角速度
# 编辑 params/tube_mpc_params.yaml:
mpc_max_angvel: 2.0  # 从1.0增加到2.0
mpc_ref_vel: 1.0     # 增加参考速度
```

**2. RViz显示错误**
```bash
# 重置RViz配置
rm -rviz_config.rviz  # 删除本地配置
roslaunch tube_mpc_ros tube_mpc_navigation.launch  # 重新启动
```

**3. Topic类型不匹配**
```bash
# 检查topic信息
rostopic info /dr_margins_debug_viz
rostopic echo /dr_margins_debug_viz -n 1
```

**4. Gazebo启动失败**
```bash
# 清理Gazebo缓存
killall gzserver gzclient
rm -rf ~/.gazebo/log/*
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

---

## 数据记录和分析

### 自动数据记录

MPC性能数据自动记录到:
- `/tmp/tube_mpc_metrics.csv` - MPC性能指标
- `/tmp/dr_margins.csv` - DR收紧数据（如果启用）
- `/tmp/safe_regret_regret.csv` - 遗憾数据（完整系统）

### 分析数据
```python
# 示例: 分析MPC跟踪性能
import pandas as pd
import matplotlib.pyplot as plt

# 读取数据
data = pd.read_csv('/tmp/tube_mpc_metrics.csv')

# 绘制跟踪误差
plt.figure(figsize=(12, 6))
plt.subplot(2, 1, 1)
plt.plot(data['timestamp'], data['cte'], label='Cross-track Error')
plt.plot(data['timestamp'], data['etheta'], label='Heading Error')
plt.legend()
plt.ylabel('Error (rad/m)')

plt.subplot(2, 1, 2)
plt.plot(data['timestamp'], data['velocity'], label='Velocity')
plt.legend()
plt.ylabel('Velocity (m/s)')

plt.xlabel('Time (s)')
plt.savefig('mpc_performance.png')
```

---

## 系统架构总结

```
┌─────────────────────────────────────────────────────────┐
│                   Global Planner                         │
│             (move_base GlobalPlanner)                    │
└────────────────┬────────────────────────────────────────┘
                 │ Global Path
                 ↓
┌─────────────────────────────────────────────────────────┐
│          Reference Planner (Phase 4) [Optional]          │
│  - No-regret learning                                   │
│  - Abstract trajectory planning                         │
│  - Regret minimization                                  │
└────────────────┬────────────────────────────────────────┘
                 │ Reference Trajectory
                 ↓
┌─────────────────────────────────────────────────────────┐
│          DR Tightening (Phase 3) [Optional]              │
│  - Residual: e_k = x_k - z_k                            │
│  - Ambiguity set: P_k = {P: W(P,P̂_k) ≤ ε}            │
│  - Tightening: h(z) ≥ σ_{k,t} + η_ℒ                     │
└────────────────┬────────────────────────────────────────┘
                 │ Tightened Constraints
                 ↓
┌─────────────────────────────────────────────────────────┐
│          Tube MPC (Phase 1) [Core]                       │
│  Nominal: z_{k+1} = Az_k + Bv_k                         │
│  Error: e_{k+1} = (A+BK)e_k + w_k                       │
│  Control: u_k = v_k + Ke_k                              │
│  • Robust constraint satisfaction                        │
│  • Adaptive disturbance estimation                       │
└────────────────┬────────────────────────────────────────┘
                 │ Control Commands
                 ↓
┌─────────────────────────────────────────────────────────┐
│                  Robot (Gazebo)                          │
│             Servingbot Mobile Manipulator                │
└─────────────────────────────────────────────────────────┘
```

---

## 开发路线图

| Phase | 组件 | 状态 | 启动命令 |
|-------|------|------|----------|
| 1 | Tube MPC | ✅ 完成 | `roslaunch tube_mpc_ros tube_mpc_navigation.launch` |
| 2 | STL Monitor | 🔄 开发中 | `roslaunch tube_mpc_ros stl_mpc_navigation.launch` |
| 3 | DR Tightening | ✅ 完成 | `roslaunch dr_tightening dr_tube_mpc_integrated.launch` |
| 4 | Reference Planner | ✅ 完成 | 集成在完整系统中 |
| 5 | Full Integration | ✅ 完成 | `roslaunch safe_regret safe_regret_simplified.launch` |
| 6 | Experiments | 📋 计划中 | - |

**当前分支**: `safe_regret`
**主分支**: `master`

---

## 参考文档

- **项目总览**: `CLAUDE.md`
- **实现路线图**: `PROJECT_ROADMAP.md`
- **Tube MPC文档**: `test/docs/TUBE_MPC_README.md`
- **调优指南**: `test/docs/TUBEMPC_TUNING_GUIDE.md`
- **论文草稿**: `latex/manuscript.tex`

---

## 联系与反馈

如遇问题，请参考：
- GitHub Issues: [项目Issues页面]
- 测试报告: `test/reports/`
- 诊断脚本: `test/diagnostics/`

**最后更新**: 2026-03-23
**系统版本**: Safe-Regret MPC v0.5 (Phase 1+3+4 Complete)
