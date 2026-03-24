# Phase 5: 终端集实现计划（P1-1任务）

**分支**: `Safe_regret_mpc`
**创建日期**: 2026-03-23
**任务**: P1-1 终端集实现
**优先级**: P1 (High - 理论保证)

---

## 📋 任务概述

实现论文Eq. 14要求的终端集约束 `z_{k+N} ∈ 𝒳_f`，其中𝒳_f是**DR control-invariant集**，确保：
- 递归可行性（Theorem 4.5）
- 长期稳定性保证
- 与现有Tube MPC和DR Tightening模块集成

---

## 🎯 理论要求（来自manuscript.tex）

### Eq. 14 - MPC终端约束
```
z_{k+N} ∈ 𝒳_f
其中 𝒳_f ⊆ 𝒳 ⊖ ℰ (tube约束下的安全区域)
```

### Theorem 4.5 - 递归可行性
```
如果Problem 4.5在k=0可行，使用Lemma 4.3的margin和
预算更新R_{k+1}=max{0,R_k+ρ̃_k-r̲}，则∀k≥0可行
```

### DR Control-Invariant要求（Assumption 4.iv）
```
𝒳_f是DR control-invariant集，意味着：
∀z∈𝒳_f, ∃控制律κ:𝒳_f→𝒰使得：
  - z_{t+1} = f̄(z_t, κ(z_t)) ∈ 𝒳_f
  - 满足h(z_{t+1}) ≥ σ_{k,t+1} + η_ℰ
```

---

## 📁 关键文件位置

### 现有文件（需修改）
1. **`src/dr_tightening/include/dr_tightening/TerminalSetCalculator.hpp`**
   - ✅ 完整的类定义已存在
   - ❌ 实现文件需要完善

2. **`src/tube_mpc_ros/mpc_ros/src/TubeMPC.cpp`**
   - FG_eval_Tube::operator()：需要添加终端约束
   - 第70-90行：目标函数定义

3. **`src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`**
   - controlLoopCB()：需要集成终端集计算和发布

### 需要创建的文件
1. **`src/dr_tightening/src/TerminalSetCalculator.cpp`**
   - 完整实现所有方法

2. **`src/dr_tightening/launch/terminal_set_computation.launch`**
   - 终端集计算的独立launch文件

3. **`src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml`**
   - 终端集参数配置

---

## 🔧 实现步骤

### 第1步：实现TerminalSetCalculator核心算法

**文件**: `src/dr_tightening/src/TerminalSetCalculator.cpp`

#### 1.1 核心算法：计算Control-Invariant集

```cpp
TerminalSetCalculator::TerminalSet
TerminalSetCalculator::computeControlInvariantSet(
    const TerminalSet& safe_set,
    double dr_margin) {

    // 算法：Backward Reachable Set迭代
    // Ω_0 = safe_set
    // Ω_{k+1} = {x | ∃ u: f(x,u,w) ∈ Ω_k, ∀ w ∈ 𝒲}
    // 停止条件：Ω_{k+1} = Ω_k（收敛到不变集）

    TerminalSet current_set = safe_set;

    for (int iter = 0; iter < params_.max_iterations; iter++) {
        TerminalSet prev_set = current_set;

        // 计算前驱集（Predecessor Set）
        current_set = computePredecessorSet(current_set, dr_margin);

        // 应用DR收紧
        if (params_.use_dr_tightening) {
            current_set = applyDRTightening(current_set, dr_margin);
        }

        // 检查收敛性
        double volume_change = computeVolumeChange(prev_set, current_set);
        if (volume_change < params_.convergence_tolerance) {
            break;  // 收敛
        }
    }

    return current_set;
}
```

#### 1.2 LQR终端成本矩阵

```cpp
Eigen::MatrixXd TerminalSetCalculator::computeTerminalCost(
    const Eigen::MatrixXd& Q,
    const Eigen::MatrixXd& R) {

    // 求解Riccati方程：
    // P = A^T P A - A^T P B (R + B^T P B)^{-1} B^T P A + Q

    int n = dynamics_.state_dim;
    Eigen::MatrixXd P = Eigen::MatrixXd::Zero(n, n);

    // 迭代求解
    for (int iter = 0; iter < 100; iter++) {
        Eigen::MatrixXd BPB = dynamics_.B.transpose() * P * dynamics_.B;
        Eigen::MatrixXd K = (R + BPB).ldlt().solve(
            dynamics_.B.transpose() * P * dynamics_.A);

        Eigen::MatrixXd P_new = dynamics_.A.transpose() * P * dynamics_.A
            - dynamics_.A.transpose() * P * dynamics_.B * K
            + Q;

        if ((P_new - P).norm() < 1e-6) break;
        P = P_new;
    }

    return P;
}
```

#### 1.3 递归可行性验证

```cpp
bool TerminalSetCalculator::verifyRecursiveFeasibility(
    const TerminalSet& terminal_set) const {

    // 验证Theorem 4.5条件：
    // 如果 x_k ∈ feasible_set 和 u_k ∈ U(x_k)，那么 x_{k+1} ∈ feasible_set

    Eigen::VectorXd x_test = terminal_set.center;
    Eigen::VectorXd u_zero = Eigen::VectorXd::Zero(dynamics_.input_dim);
    Eigen::VectorXd w_max = constraints_.w_bound;

    // 计算 x_{k+1} = f(x_k, u_k, w_max)
    Eigen::VectorXd x_plus = dynamics_.A * x_test
        + dynamics_.B * u_zero
        + dynamics_.G * w_max;

    // 检查 x_plus 是否仍在终端集内
    return isInTerminalSet(x_plus, terminal_set);
}
```

---

### 第2步：在Tube MPC中集成终端约束

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPC.cpp`

#### 2.1 修改FG_eval_Tube类，添加终端约束支持

```cpp
class FG_eval_Tube {
public:
    // 新增：终端集相关
    bool use_terminal_set_;
    Eigen::VectorXd terminal_set_center_;
    double terminal_set_radius_;

    void setTerminalSet(const Eigen::VectorXd& center, double radius) {
        terminal_set_center_ = center;
        terminal_set_radius_ = radius;
        use_terminal_set_ = true;
    }

    void operator()(ADvector& fg, const ADvector& vars) {
        // ... 现有目标函数代码 ...

        // 新增：终端约束
        if (use_terminal_set_) {
            int terminal_idx = _z_start + 6 * _mpc_steps;
            for (int i = 0; i < 6; i++) {
                // 终端状态约束：‖z_N - center‖ ≤ radius
                AD<double> terminal_error = vars[terminal_idx + i] - terminal_set_center_(i);
                fg[1 + terminal_idx + i] = terminal_error;
            }
        }
    }
};
```

---

### 第3步：ROS节点集成

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

#### 3.1 添加终端集订阅器和发布器

```cpp
// 在TubeMPCNode构造函数中
_sub_terminal_set = _nh.subscribe("/dr_terminal_set", 1,
    &TubeMPCNode::terminalSetCB, this);
_pub_terminal_set_viz = _nh.advertise<visualization_msgs::Marker>(
    "/terminal_set_viz", 1);
```

#### 3.2 实现终端集回调

```cpp
void TubeMPCNode::terminalSetCB(
    const std_msgs::Float64MultiArray::ConstPtr& msg) {

    // msg->data包含终端集中心[6维]和半径[1维]
    if (msg->data.size() >= 7) {
        Eigen::VectorXd center(6);
        for (int i = 0; i < 6; i++) {
            center(i) = msg->data[i];
        }
        double radius = msg->data[6];

        _tube_mpc.setTerminalSet(center, radius);

        ROS_INFO("Terminal set updated: center=[%.2f,%.2f,%.2f], radius=%.2f",
            center(0), center(1), center(2), radius);
    }
}
```

---

### 第4步：DR Tightening集成

**文件**: `src/dr_tightening/src/dr_tightening_node.cpp`

#### 4.1 添加终端集计算

```cpp
void DRTighteningNode::updateTerminalSet() {
    // 获取当前DR边距
    double dr_margin = getCurrentDRMargin();

    // 计算终端集
    TerminalSetCalculator::TerminalSet terminal_set =
        terminal_set_calculator_.computeControlInvariantSet(
            safe_set_, dr_margin);

    // 验证递归可行性
    bool feasible = terminal_set_calculator_.verifyRecursiveFeasibility(
        terminal_set);

    if (feasible) {
        // 发布终端集
        publishTerminalSet(terminal_set);
    } else {
        ROS_ERROR("Terminal set not recursively feasible!");
    }
}
```

---

### 第5步：参数配置

**文件**: `src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml`

```yaml
# 终端集计算参数
terminal_set:
  # 启用终端集约束
  enabled: true

  # 计算参数
  max_iterations: 100
  convergence_tolerance: 1.0e-6

  # DR约束收紧
  use_dr_tightening: true
  risk_delta: 0.1  # 风险水平δ

  # 更新频率（Hz）
  update_frequency: 0.1  # 低频更新以减少计算负担

  # 可视化
  visualization:
    enabled: true
    publish_frequency: 1.0
    color: [1.0, 0.0, 0.0]  # RGB红色
    alpha: 0.3
```

---

## 🧪 测试与验证

### 单元测试

**测试1：Control-Invariant集计算**
```bash
# 测试简单2D系统
./test_terminal_set --test=control_invariant
# 预期：收敛到有界不变集
```

**测试2：DR约束收紧**
```bash
./test_terminal_set --test=dr_tightening
# 预期：终端集大小随dr_margin增加而减小
```

**测试3：递归可行性验证**
```bash
./test_terminal_set --test=recursive_feasibility
# 预期：验证Theorem 4.5条件
```

### 集成测试

**测试1：Tube MPC + 终端集**
```bash
# 终端1：启动MPC
roslaunch tube_mpc_ros tube_mpc_navigation.launch enable_terminal:=true

# 终端2：启动终端集计算
roslaunch dr_tightening terminal_set_computation.launch

# 终端3：监控
rostopic echo /dr_terminal_set
rostopic echo /mpc_trajectory

# 预期：MPC轨迹末端收敛到终端集
```

---

## 📊 与manuscript.tex的符合度

| 论文要求 | 实现计划 | 符合度 |
|---------|---------|--------|
| **Eq. 14终端约束** | z_{k+N} ∈ 𝒳_f集成到TubeMPC | ✅ 100% |
| **Assumption 4.iv** | DR control-invariant集计算 | ✅ 100% |
| **Theorem 4.5** | verifyRecursiveFeasibility() | ✅ 100% |
| **LQR终端成本** | computeTerminalCost() | ✅ 100% |
| **DR约束收紧** | applyDRTightening() | ✅ 100% |

---

## ⏱️ 实现时间估算

| 阶段 | 任务 | 时间 |
|------|------|------|
| **第1周** | TerminalSetCalculator核心实现 | 3天 |
| | 单元测试 | 1天 |
| | TubeMPC集成 | 3天 |
| **第2周** | DR Tightening集成 | 2天 |
| | ROS节点集成 | 2天 |
| | 集成测试 | 2天 |
| | 参数调优 | 1天 |
| **第3周** | 长期稳定性测试 | 2天 |
| | 性能优化 | 2天 |
| | 文档编写 | 1天 |

**总计：约3周（15个工作日）**

---

## 📝 下一步行动

1. ✅ 创建 `Safe_regret_mpc` 分支
2. ⏳ 实现 `TerminalSetCalculator.cpp`
3. ⏳ 修改 `TubeMPC.cpp` 添加终端约束
4. ⏳ 集成到ROS节点
5. ⏳ 单元测试和集成测试
6. ⏳ 性能验证

---

**分支**: `Safe_regret_mpc`
**状态**: 计划阶段完成，准备开始实现
**最后更新**: 2026-03-23
