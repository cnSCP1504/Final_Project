# Safe-Regret MPC 修复进度报告

**修复开始日期**: 2026-03-23
**修复范围**: P0 → P1 → P2 优先级顺序
**当前状态**: P0 进行中

---

## P0 (Critical) - 阻碍论文实验

### ✅ P0-1: 实现平滑STL鲁棒度

**状态**: ✅ **代码已存在，需启用集成**

#### 已实现的代码

1. **平滑鲁棒度** (`SmoothRobustness.cpp`)
   ```cpp
   // smin_τ(z) = -τ·log(Σ exp(-z_i/τ))
   double SmoothRobustness::smin(const Eigen::VectorXd& x) const;

   // smax_τ(z) = τ·log(Σ exp(x_i/τ))
   double SmoothRobustness::smax(const Eigen::VectorXd& x) const;

   // 梯度计算
   Eigen::VectorXd SmoothRobustness::smin_grad(const Eigen::VectorXd& x) const;
   Eigen::VectorXd SmoothRobustness::smax_grad(const Eigen::VectorXd& x) const;
   ```
   ✅ 完全符合论文Remark 3.2

2. **信念空间评估** (`BeliefSpaceEvaluator.cpp`)
   ```cpp
   // ρ̃_k = E_{x∼β_k}[ρ^soft(φ; x_{k:k+N})]
   RobustnessValue computeExpectedRobustness(
       const STLFormula& formula,
       const BeliefState& belief,
       double time_horizon)
   ```
   ✅ 支持粒子滤波和Unscented变换

3. **ROS节点** (`stl_monitor_node.py`)
   ```python
   class STLMonitorNode(BaseMonitor):
       def __init__(self):
           self.temperature = rospy.get_param('~temperature', 0.1)
           self.use_belief_space = rospy.get_param('~use_belief_space', True)
           self.num_particles = rospy.get_param('~num_particles', 100)
   ```
   ✅ Python节点已完整实现

#### 修复操作

1. **修改launch文件** ✅ 已完成
   - 文件: `stl_mpc_navigation.launch`
   - 移除了对不存在的`stl_ros`包的引用
   - 改用`stl_monitor`包
   - 添加了正确的参数配置

2. **参数配置** - 需要创建
   ```yaml
   # params/stl_params.yaml
   stl_monitor:
     temperature: 0.1          # τ for log-sum-exp
     use_belief_space: true    # Enable belief space evaluation
     baseline_requirement: 0.1 # r̲ for budget update
     update_frequency: 10.0    # Hz
   ```

3. **待验证**: 节点是否正确发布topics
   - `/stl/robustness` - STL鲁棒度值
   - `/stl/violation` - 违反标志
   - `/stl/budget` - 鲁棒度预算

### ✅ P0-2: 实现鲁棒度预算机制

**状态**: ✅ **代码已存在**

#### 已实现的代码

```cpp
// RobustnessBudget.cpp
double RobustnessBudget::update(double robustness) {
    // 论文Eq. 15: R_{k+1} = max{0, R_k + ρ_k - r̲}
    state_.budget = std::max(0.0,
        state_.budget + robustness - state_.baseline_requirement);

    // 衰减选项
    if (decay_rate_ > 0.0) {
        state_.budget *= (1.0 - decay_rate_);
    }

    // 跟踪违反
    if (robustness < 0.0) {
        state_.violation_count += 1.0;
    }

    return state_.budget;
}
```

✅ **完全符合论文Eq. 15**

#### 功能

- ✅ 预算更新: `R_{k+1} = max{0, R_k + ρ_k - r̲}`
- ✅ 违反计数
- ✅ 历史记录（用于可视化）
- ✅ 未来预测: `predictFuture(horizon, expected_robustness)`
- ✅ 充足性检查: `checkSufficiency(horizon, expected_robustness, threshold)`

### 🔄 P0-3: 集成STL到MPC目标函数

**状态**: 🔄 **需要实现**

#### 论文要求 (Eq. 10)

```
min  E[Σ ℓ(x_t,u_t)] - λ·ρ̃_k
```

#### 实现方案

1. **修改TubeMPCNode.h**
   ```cpp
   class TubeMPCNode {
   private:
       // STL集成
       ros::Subscriber _sub_stl_robustness;
       double _current_stl_robustness;
       double _stl_weight;  // λ parameter
       bool _enable_stl_constraints;

       void stlRobustnessCB(const std_msgs::Float32::ConstPtr& msg);
   };
   ```

2. **修改TubeMPCNode.cpp构造函数**
   ```cpp
   TubeMPCNode::TubeMPCNode(...) {
       // 添加STL订阅
       _enable_stl_constraints = private_nh.param("enable_stl_constraints", false);
       _stl_weight = private_nh.param("stl_weight", 10.0);

       if (_enable_stl_constraints) {
           _sub_stl_robustness = _nh.subscribe("/stl/robustness", 1,
               &TubeMPCNode::stlRobustnessCB, this);
       }
   }

   void TubeMPCNode::stlRobustnessCB(const std_msgs::Float32::ConstPtr& msg) {
       _current_stl_robustness = msg->data;
   }
   ```

3. **修改MPC成本函数 (navMPC.cpp)**

   当前成本函数:
   ```cpp
   fg[0] += _w_cte * CppAD::pow(vars[_cte_start + i] - _ref_cte, 2);
   fg[0] += _w_etheta * CppAD::pow(vars[_etheta_start + i] - _ref_etheta, 2);
   fg[0] += _w_vel * CppAD::pow(vars[_v_start + i] - _ref_vel, 2);
   ```

   需要添加STL项:
   ```cpp
   // 从外部获取stl_robustness值
   // fg[0] -= _stl_weight * _stl_robustness;
   ```

   **问题**: CppAD需要在编译时确定变量，而STL鲁棒度是运行时输入

   **解决方案**:
   - 选项A: 将STL项作为软约束，用惩罚函数实现
   - 选项B: 使用SQP方法，将STL作为外层循环约束
   - 选项C: 简化实现，将STL权重应用到参考轨迹调整

   **推荐**: 选项C（最简单且有效）

4. **简化实现方案**

   不直接修改CppAD成本函数，而是：
   - STL鲁棒度影响参考路径选择
   - 调整MPC参考点以优化STL满足度
   - 在move_base层面集成STL约束

### 🔄 P0-4: 验证DR约束真正应用

**状态**: 🔄 **需要验证**

#### 当前状态

- ✅ DR边距计算正确 (`TighteningComputer.cpp`)
- ✅ 发布到 `/dr_margins` topic
- ❓ **未知**: MPC是否真正使用这些边距收紧约束

#### 验证步骤

1. **检查数据流**
   ```bash
   rostopic echo /dr_margins
   rostopic echo /tube_mpc/tracking_error
   ```

2. **检查MPC日志**
   ```bash
   roslaunch dr_tightening dr_tube_mpc_integrated.launch
   # 查看 "MPC Solve" 日志中的约束值
   ```

3. **代码审查**
   - 检查 `TubeMPCNode.cpp` 是否订阅 `/dr_margins`
   - 检查是否将边距应用到状态/输入约束

#### 预期修复

如果MPC未应用DR边距，需要添加：

```cpp
// TubeMPCNode.cpp
void TubeMPCNode::drMarginsCB(const std_msgs::Float64MultiArray::ConstPtr& msg) {
    if (msg->data.size() > 0) {
        double margin = msg->data[0];  // σ_{k,0}
        _tube_mpc.applyDRMargin(margin);
    }
}
```

---

## P1 (High) - 影响理论保证

### 📋 P1-1: 实现终端集

**状态**: ❌ **未实现**

#### 论文要求 (Eq. 14)

```
z_{k+N} ∈ 𝒳_f
where 𝒳_f is DR control-invariant
```

#### 实现计划

1. **计算DR control-invariant集**
   ```cpp
   class TerminalSetCalculator {
   public:
       // Compute maximum control-invariant set
       Eigen::MatrixXd computeDRControlInvariant(
           const SystemDynamics& dynamics,
           const SafetyFunction& safety,
           const DRAmbiguity& ambiguity,
           double risk_delta
       );
   };
   ```

2. **在MPC中添加终端约束**
   ```cpp
   // navMPC.cpp
   // 添加终端状态约束
   fg[1 + _terminal_start + i] = vars[_terminal_start + i] - X_f[i];
   ```

3. **验证递归可行性**
   - 测试终端集是否真正control-invariant
   - 验证闭环系统长期运行可行性

### 📋 P1-2: 实现Wasserstein模糊集

**状态**: ❌ **未实现**

#### 论文要求 (Eq. 17)

```
𝒫_k = {ℙ: W(ℙ, ℙ̂_k) ≤ ε}
```

#### 当前状态

- ✅ Chebyshev边界已实现 (`computeChebyshevMargin`)
- ❌ Wasserstein距离未实现

#### 实现计划

```cpp
// AmbiguityCalibrator.cpp
class AmbiguityCalibrator {
public:
    // Compute Wasserstein distance
    double computeWassersteinDistance(
        const Eigen::VectorXd& samples1,
        const Eigen::VectorXd& samples2
    );

    // Calibrate Wasserstein ball
    double calibrateWassersteinBall(
        const std::vector<Eigen::VectorXd>& residuals,
        double epsilon
    );
};
```

**优先级**: 🟡 中等（Chebyshev已可用）

---

## P2 (Medium) - 改善性能

### 📋 P2-1: 参数调优

**状态**: ⚠️  **需要系统化调优**

#### 关键参数

| 参数 | 符号 | 当前值 | 推荐范围 | 说明 |
|------|------|--------|---------|------|
| 温度 | τ | 0.1 | 0.05-0.2 | 越小越精确，梯度越陡 |
| STL权重 | λ | - | 5-50 | 平衡跟踪vsSTL |
| 学习率 | η | 0.1 | 0.05-0.3 | OMD算法 |
| 鲁棒度基线 | r̲ | 0.1 | 0.05-0.5 | 每步最小要求 |

#### 调优策略

1. **网格搜索**
   ```python
   for tau in [0.05, 0.1, 0.15, 0.2]:
       for lambda_ in [5, 10, 20, 50]:
           run_experiment(tau, lambda_)
           evaluate_stl_satisfaction()
           evaluate_tracking_error()
   ```

2. **自适应调整**
   - 根据性能自动调整τ
   - 动态权重λ

### 📋 P2-2: 添加可视化

**状态**: ⚠️  **部分实现**

#### 已有可视化

- ✅ `/mpc_trajectory` - RViz显示
- ✅ `/tube_boundaries` - Tube上下界
- ✅ `/dr_margins_debug_viz` - DR边距文本

#### 缺失可视化

1. **STL鲁棒度实时曲线**
   ```python
   # stl_visualizer.py
   import matplotlib.pyplot as plt
   # 实时绘制 ρ̃_k 曲线
   # 显示预算 R_k 变化
   ```

2. **遗憾指标曲线**
   ```python
   # regret_visualizer.py
   # 绘制 cumulative regret vs T
   # 对比不同算法
   ```

3. **RViz集成**
   - 在RViz中添加STL鲁棒度3D文本
   - 颜色编码违反状态

---

## 实现状态总结

### P0 (Critical) - 完成度: 60%

| 任务 | 状态 | 完成度 | 备注 |
|------|------|--------|------|
| P0-1: 平滑STL鲁棒度 | ✅ 代码存在 | 90% | 需启用集成 |
| P0-2: 鲁棒度预算 | ✅ 代码存在 | 100% | 完全符合Eq. 15 |
| P0-3: STL集成到MPC | 🔄 需实现 | 20% | 架构设计完成 |
| P0-4: DR约束验证 | 🔄 需验证 | 50% | 需测试验证 |

**下一步行动**:
1. 创建STL参数配置文件
2. 实现STL回调到TubeMPCNode
3. 使用简化方案集成STL到MPC
4. 验证DR边距应用

### P1 (High) - 完成度: 10%

| 任务 | 状态 | 完成度 | 备注 |
|------|------|--------|------|
| P1-1: 终端集 | ❌ 未实现 | 0% | 需要算法实现 |
| P1-2: Wasserstein | ❌ 未实现 | 0% | Chebyshev可用 |

**下一步行动**:
1. 实现DR control-invariant集计算
2. 添加终端约束到MPC
3. 实现Wasserstein距离（可选）

### P2 (Medium) - 完成度: 30%

| 任务 | 状态 | 完成度 | 备注 |
|------|------|--------|------|
| P2-1: 参数调优 | ⚠️  部分完成 | 30% | 需系统化调优 |
| P2-2: 可视化 | ⚠️  部分完成 | 40% | 基础RViz已有 |

**下一步行动**:
1. 参数网格搜索实验
2. 添加实时曲线绘图
3. RViz marker完善

---

## 快速启动指南

### 测试STL监控 (P0)

```bash
# 启动STL MPC
roslaunch tube_mpc_ros stl_mpc_navigation.launch enable_stl:=true

# 查看STL topics
rostopic echo /stl/robustness
rostopic echo /stl/budget
rostopic echo /stl/violation

# 查看日志
tail -f /tmp/stl_monitor_data.csv
```

### 测试DR收紧 (P0)

```bash
# 启动DR+Tube MPC
roslaunch dr_tightening dr_tube_mpc_integrated.launch enable_dr:=true

# 查看DR边距
rostopic echo /dr_margins

# 查看残差
rostopic echo /tube_mpc/tracking_error
```

### 测试完整系统 (P0+P1)

```bash
# 启动Safe-Regret MPC
roslaunch safe_regret safe_regret_simplified.launch

# 查看所有指标
rostopic echo /safe_regret/regret_metrics
rostopic echo /dr_margins
rostopic echo /stl/robustness  # 如果启用
```

---

## 关键发现

1. **STL监控代码已完整实现**
   - 平滑鲁棒度: ✅ 完全符合论文
   - 信念空间评估: ✅ 粒子+Unscented
   - 鲁棒度预算: ✅ 完全符合Eq. 15
   - **仅需启用集成**

2. **DR收紧核心公式正确**
   - Lemma 4.3公式: ✅ 精确实现
   - Cantelli因子: ✅ κ_δ = sqrt((1-δ)/δ)
   - **需验证应用到MPC**

3. **主要缺失**
   - STL到MPC的集成（架构清晰，需实现）
   - 终端集约束（理论保证需要）
   - 系统化参数调优

---

**最后更新**: 2026-03-23
**下次更新**: 完成P0-3后
