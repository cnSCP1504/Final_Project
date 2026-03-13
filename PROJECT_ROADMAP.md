# Safe-Regret MPC 项目实施路线图

## 论文目标概述

基于论文 *Safe-Regret MPC for Temporal-Logic Tasks in Stochastic, Partially Observable Robots*，本项目旨在实现一个统一框架，集成时序逻辑任务规范、概率安全性和学习保证。

## 总体架构

```
┌─────────────────────────────────────────────────────────┐
│          Safe-Regret MPC 完整系统                        │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────────┐    ┌──────────────────┐          │
│  │ 抽象层           │    │ 信念空间STL      │          │
│  │ (Abstract Layer) │    │ 优化模块         │          │
│  │                  │    │                  │          │
│  │ • No-Regret规划  │◄───┤ • 可微鲁棒性     │          │
│  │ • 参考轨迹生成   │    │ • 鲁棒性预算     │          │
│  └──────────────────┘    └──────────────────┘          │
│           │                         │                   │
│           ▼                         ▼                   │
│  ┌──────────────────┐    ┌──────────────────┐          │
│  │ 管MPC控制层      │◄───┤ 分布鲁棒安全     │          │
│  │ (Tube MPC Layer) │    │ 约束模块         │          │
│  │                  │    │                  │          │
│  │ ✅ 已完成         │    │ • 数据驱动收紧   │          │
│  │ • 名义系统       │    │ • 模糊集校准     │          │
│  │ • 误差反馈       │    │ • 机会约束       │          │
│  └──────────────────┘    └──────────────────┘          │
│           │                         │                   │
│           └──────────┬──────────────┘                   │
│                      ▼                                   │
│              ┌──────────────┐                            │
│              │ 物理机器人    │                            │
│              │ 执行层        │                            │
│              └──────────────┘                            │
└─────────────────────────────────────────────────────────┘
```

## 实施阶段

### ✅ 第一阶段：Tube MPC基础（已完成）
**状态**: 完成
**时间**: 已完成
**成果**:
- ✅ Tube MPC核心算法实现
- ✅ 系统分解（名义+误差）
- ✅ LQR误差反馈控制
- ✅ ROS集成与参数配置
- ✅ 基础导航任务验证

**文件位置**:
- `src/tube_mpc_ros/mpc_ros/src/TubeMPC.cpp`
- `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`

---

### 🔄 第二阶段：STL集成模块（进行中）
**状态**: 部分完成
**预计完成**: 2-3周
**目标**: 实现信念空间STL优化

#### 任务清单
- [ ] **STL解析器**
  - [ ] 支持基本STL语法（G, F, U, until）
  - [ ] 原子谓词定义
  - [ ] 语法树构建
  - **位置**: `src/tube_mpc_ros/stl_monitor/`

- [ ] **可微鲁棒性计算**
  - [ ] log-sum-exp平滑近似（smin/smax）
  - [ ] 温度参数τ调节
  - [ ] 梯度计算
  - **位置**: `src/tube_mpc_ros/stl_monitor/src/SmoothRobustness.cpp`

- [ ] **信念空间评估**
  - [ ] 基于粒子集的期望计算
  - [ ] 无迹变换（Unscented Transform）支持
  - [ ] 与状态估计器接口
  - **位置**: `src/tube_mpc_ros/stl_monitor/src/BeliefSpaceEvaluator.cpp`

- [ ] **鲁棒性预算机制**
  - [ ] 滚动预算更新：R_{k+1} = max{0, R_k + ρ_k - r̲}
  - [ ] 预算约束集成到MPC
  - [ ] 违约检测与恢复
  - **位置**: `src/tube_mpc_ros/stl_monitor/src/RobustnessBudget.cpp`

- [ ] **ROS节点**
  - [ ] 订阅：`/belief`, `/map`
  - [ ] 发布：`/stl_robustness`, `/stl_grads`
  - [ ] YAML配置接口
  - **位置**: `src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.cpp`

**验证指标**:
- STL公式解析成功率
- 鲁棒性计算频率（目标：>100Hz）
- 梯度精度（与有限差分对比）

---

### 📋 第三阶段：分布鲁棒机会约束（待开始）
**状态**: 未开始
**预计完成**: 3-4周
**目标**: 实现数据驱动的安全约束收紧

#### 任务清单
- [ ] **残差收集与分析**
  - [ ] 滑动窗口残差缓冲（大小M=200）
  - [ ] 经验分布构建：ℙ̂_k
  - [ ] 统计量更新（均值、协方差）
  - **位置**: `src/tube_mpc_ros/dr_tightening/src/ResidualCollector.cpp`

- [ ] **模糊集校准**
  - [ ] Wasserstein球半径计算
  - [ ] 经验分位数方法
  - [ ] 浓度界支持
  - [ ] 有限样本覆盖保证
  - **位置**: `src/tube_mpc_ros/dr_tightening/src/AmbiguityCalibrator.cpp`

- [ ] **确定性边际计算**
  - [ ] Chebyshev型界：μ_t + κ_δ·σ_t
  - [ ] DR-CVaR约束（可选，SOCP）
  - [ ] 管误差补偿：η_ℰ
  - [ ] 风险分配：δ_t（均匀或截止加权）
  - **位置**: `src/tube_mpc_ros/dr_tightening/src/TighteningComputer.cpp`

- [ ] **安全函数线性化**
  - [ ] h(x)的一阶泰勒展开
  - [ ] 敏度方向：c_t = ∇h(z_t)
  - [ ] 扰动代理：Y_t = c_tᵀω_t
  - **位置**: `src/tube_mpc_ros/dr_tightening/src/SafetyLinearization.cpp`

- [ ] **ROS节点**
  - [ ] 订阅：`/ekf/residuals`
  - [ ] 发布：`/dr_margins` (ε_k, {σ_{k,t}})
  - [ ] 动态参数调节
  - **位置**: `src/tube_mpc_ros/dr_tightening/src/dr_tightening_node.cpp`

**验证指标**:
- 经验风险vs目标δ（偏差<10%）
- 边际收紧的保守性
- 计算时间（目标：<5ms）

---

### 📋 第四阶段：遗憾传递与抽象层（待开始）
**状态**: 未开始
**预计完成**: 4-5周
**目标**: 实现参考轨迹生成器与遗憾分析

#### 任务清单
- [ ] **参考轨迹规划器**
  - [ ] 拓扑信念空间规划
  - [ ] 部分已知地图处理
  - [ ] 同调类推理
  - [ ] 主动信息获取
  - **位置**: `src/tube_mpc_ros/reference_planner/`

- [ ] **无遗憾学习算法**
  - [ ] 抽象策略类定义
  - [ ] Oracle比较器
  - [ ] 遗憾计算与追踪
  - [ ] 探索-利用权衡
  - **位置**: `src/tube_mpc_ros/reference_planner/src/NoRegretPlanner.cpp`

- [ ] **遗憾传递模块**
  - [ ] 跟踪误差累积界（Lemma 4.6）
  - [ ] 动态遗憾计算：R_T^dyn
  - [ ] 安全遗憾计算：R_T^safe
  - [ ] o(T)验证
  - **位置**: `src/tube_mpc_ros/regret_tracker/src/RegretAnalyzer.cpp`

- [ ] **可行性检查器**
  - [ ] 速度/曲率边界验证
  - [ ] 管兼容性检查
  - [ ] 输入饱和处理
  - **位置**: `src/tube_mpc_ros/reference_planner/src/FeasibilityChecker.cpp`

**验证指标**:
- 动态遗憾增长率（目标：o(T)）
- 跟踪误差界：Σ‖e_k‖ ≤ C_e·√T
- 参考轨迹可行性率

---

### 📋 第五阶段：系统集成与优化（待开始）
**状态**: 未开始
**预计完成**: 2-3周
**目标**: 完整系统集成与实时性能优化

#### 任务清单
- [ ] **统一MPC求解器**
  - [ ] 集成STL目标项
  - [ ] 集成DR机会约束
  - [ ] 管终端集约束
  - [ ] 鲁棒性预算约束
  - [ ] SQP/SCVX求解
  - **位置**: `src/tube_mpc_ros/safe_regret_mpc/src/SafeRegretMPC.cpp`

- [ ] **ROS节点架构**
  - [ ] 节点图配置
  - [ ] 消息定义（.msg, .srv）
  - [ ] 时间同步与TF
  - [ ] 启动文件编写
  - **位置**: `src/tube_mpc_ros/safe_regret_mpc/launch/`

- [ ] **参数配置系统**
  - [ ] YAML配置文件
  - [ ] 动态重配置
  - [ ] 参数预设（保守/平衡/激进）
  - **位置**: `src/tube_mpc_ros/safe_regret_mpc/params/`

- [ ] **性能优化**
  - [ ] 热启动策略
  - [ ] 多线程（STL评估、DR校准）
  - [ ] 代码优化（向量化、内存池）
  - [ ] 求解器调优（OSQP/ECOS）

**验证指标**:
- 总体求解时间（目标：<10ms @ 50Hz）
- 内存占用
- ROS消息延迟

---

### 📋 第六阶段：实验验证与评估（待开始）
**状态**: 未开始
**预计完成**: 3-4周
**目标**: 论文实验复现与性能评估

#### 任务清单
- [ ] **任务T1：协作装配**
  - [ ] Gazebo仿真环境搭建
  - [ ] STL规范：φ_asm
  - [ ] 人员避障 predicates
  - [ ] 抓取/放置监控
  - **位置**: `src/tube_mpc_ros/tasks/task_assembly/`

- [ ] **任务T2：物流运输**
  - [ ] 部分已知地图生成
  - [ ] STL规范：φ_log
  - [ ] 碰撞避免
  - [ ] 电池监控
  - **位置**: `src/tube_mpc_ros/tasks/task_logistics/`

- [ ] **基线对比**
  - [ ] B1: 标称STL-MPC
  - [ ] B2: DR-MPC（无STL）
  - [ ] B3: CBF盾 + MPC
  - [ ] B4: PID跟踪

- [ ] **消融实验**
  - [ ] 无鲁棒性预算
  - [ ] 无管
  - [ ] 固定边际（无DR）
  - [ ] 无信念空间平均

- [ ] **评估指标实现**
  - [ ] 满足概率 p̂_sat（Wilson CI）
  - [ ] 经验风险 δ̂
  - [ ] 动态/安全遗憾
  - [ ] 递归可行性率
  - [ ] 计算时间统计
  - **位置**: `scripts/evaluation/`

**验证目标**:
- 满足概率 > 90% (δ=0.1)
- 风险校准误差 < 10%
- 遗憾增长率 o(T)验证
- 实时性：>50Hz稳定运行

---

## 当前优先级任务

### 高优先级（本周）
1. ✅ **回顾Tube MPC实现** - 确认基础稳固
2. 🔄 **开始STL解析器设计** - 定义STL语法和接口
3. 🔄 **搭建测试框架** - 为STL模块准备单元测试

### 中优先级（本月）
1. 完成可微鲁棒性计算模块
2. 实现鲁棒性预算机制
3. 设计ROS节点架构

### 低优先级（下月）
1. 分布鲁棒约束模块设计
2. 参考规划器初步设计

---

## 技术栈与依赖

### 核心依赖
- **ROS**: Noetic (Ubuntu 20.04)
- **优化求解器**: Ipopt 3.14.20, OSQP, ECOS
- **状态估计**: robot_localization (EKF/UKF)
- **仿真**: Gazebo 11

### 新增依赖
- **STL解析**: 可能集成 `stl_library` 或自研
- **概率计算**: Eigen + 自定义统计模块
- **分布式优化**: CVaR库（可选）

---

## 文献资源

### 核心论文
1. **Safe-Regret MPC** (本项目基础) - `latex/manuscript.tex`
2. STL-MPC相关：Yu2024Automatica, Huang2024CASE
3. DR-MPC相关：Coulson2020TAC, Zhao2023Automatica
4. No-regret规划：Zhao2025IJRR

### 参考实现
- Tube MPC: 当前 `tube_mpc_ros` 包
- STL监控：可参考 `python-stl` 或 `Taliro`

---

## 进度追踪

| 阶段 | 完成度 | 截止日期 | 状态 |
|------|--------|----------|------|
| Phase 1: Tube MPC | 100% | ✓ | ✅ 完成 |
| Phase 2: STL集成 | 10% | TBD | 🔄 进行中 |
| Phase 3: DR约束 | 0% | TBD | 📋 计划中 |
| Phase 4: 遗憾分析 | 0% | TBD | 📋 计划中 |
| Phase 5: 系统集成 | 0% | TBD | 📋 计划中 |
| Phase 6: 实验验证 | 0% | TBD | 📋 计划中 |

**总体完成度**: 15%

---

## 下次会议/检查点
- **日期**: 待定
- **目标**: STL解析器原型完成
- **交付物**:
  - STL语法规范文档
  - 解析器核心代码框架
  - 单元测试用例设计

---

*最后更新: 2026-03-13*
*维护者: Claude Code + 用户 Dixon*
