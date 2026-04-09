# Dynamic Regret负数问题根本原因分析

**日期**: 2026-04-09
**问题**: Dynamic regret为什么这么大（负数）？
**根本原因**: 成本函数定义错误

---

## 🔍 问题现象

### 测试结果
```
Dynamic Regret (n=106):
  Min: -1498.69
  Max: -19.37
  Mean: -1016.02
  Median: -1467.10
```

**关键发现**: Dynamic regret是**负数**！

### 理论期望

根据manuscript定义：
```
Dynamic Regret R_T^dyn = Σ[ℓ(x_k, u_k) - ℓ(x*_k, u*_k)]
```

其中：
- `ℓ(x_k, u_k)` = 当前策略的成本
- `ℓ(x*_k, u*_k)` = 最优策略的成本

**Regret应该是非负的**：R_T ≥ 0
- 如果当前策略比最优差 → regret > 0
- 如果当前策略和最优一样 → regret = 0
- Regret不应该是负数！

---

## 🐛 根本原因

### 1. 成本函数实现错误

**文件**: `src/safe_regret/src/RegretAnalyzer.cpp` (line 247-254)

```cpp
double RegretAnalyzer::computeStageCost(
  const Eigen::VectorXd& state,
  const Eigen::VectorXd& input) const {

  // Standard quadratic cost: ℓ(x,u) = ‖x‖^2_Q + ‖u‖^2_R
  // For simplicity, use Q = I, R = I
  return state.squaredNorm() + input.squaredNorm();  // ❌ 错误！
}
```

**问题**: 成本函数使用的是**到原点(0,0)的距离**！

### 2. 比较基准设置错误

**文件**: `src/safe_regret/src/RegretAnalyzer.cpp` (line 97-106)

```cpp
// Get oracle cost if available
if (oracle && oracle->hasOracleAt(current_step_)) {
  step.comparator_cost = oracle->getOptimalCost(current_step_);
} else {
  // ❌ No oracle: use nominal cost as baseline
  step.comparator_cost = step.nominal_cost;  // 这会导致负数regret！
}

// Cost regret: ℓ(x,u) - ℓ(x*,u*)
step.cost_regret = step.instant_cost - step.comparator_cost;
```

**问题**:
- 当没有oracle时，使用`nominal_cost`作为基准
- 但`nominal_cost`可能比`instant_cost`**更大**
- 导致`cost_regret = instant_cost - nominal_cost < 0`

### 3. 为什么会负数？

**场景分析**:

1. **机器人实际状态** (actual_state):
   - 位置: (2.0, 3.0)
   - 速度: (0.5, 0.3)
   - `instant_cost = ‖(2.0, 3.0, 0.5, 0.3)‖² = 13.54`

2. **MPC标称状态** (nominal_state):
   - 位置: (5.0, 7.0) - MPC规划的路径可能远离原点
   - 速度: (0.8, 0.6)
   - `nominal_cost = ‖(5.0, 7.0, 0.8, 0.6)‖² = 75.0`

3. **Regret计算**:
   ```
   cost_regret = instant_cost - comparator_cost
               = 13.54 - 75.0
               = -61.46  ❌ 负数！
   ```

---

## 💡 为什么这是错的？

### 导航任务的正确成本函数

**应该使用**:
- ✅ **到目标的距离**: `‖x - x_goal‖²`
- ✅ **跟踪误差**: `‖x - x_ref‖²`
- ✅ **路径偏离**: `distance_to_path²`

**不应该使用**:
- ❌ **到原点的距离**: `‖x‖²` (当前实现)

### 例子说明

假设任务：从起点(-8, 0)移动到目标(3, -7)

| 状态 | 位置 | 到原点距离 | 到目标距离 |
|------|------|-----------|-----------|
| 起点 | (-8, 0) | 8.0 | 15.8 |
| 中点 | (-2, -3) | 3.6 | 5.8 |
| 目标 | (3, -7) | 7.6 | 0.0 |

**如果用"到原点距离"作为成本**（当前实现）:
- 起点: cost = 64
- 中点: cost = 13 ← 成本最低！
- 目标: cost = 58 ← 成本反而更高！

**这是错误的！** 目标点的成本应该最低，但实际却比中点高！

---

## 📊 实测数据分析

### 数据分解

```
t=0:
  dynamic_regret = -19.37
  tracking_contribution = 22.89
  nominal_contribution = 0.00

❓ 问题：为什么tracking_contribution是正数，但dynamic_regret是负数？

答案：它们计算的是不同的东西！
- tracking_contribution = Σ‖e_k‖（跟踪误差范数）
- dynamic_regret = Σ[ℓ(x_k,u_k) - ℓ(x*_k,u*_k)]（成本差）
```

### 为什么tracking_contribution ≠ |dynamic_regret|?

```
tracking_contribution = 445.85 (正数)
dynamic_regret = -1016.02 (负数)

原因：
1. tracking_contribution计算的是误差范数‖e_k‖
2. dynamic_regret计算的是成本差（基于到原点距离）
3. 两者没有直接的数量关系！
```

---

## 🔧 正确的实现

### 方案1: 修改成本函数（推荐）

```cpp
double RegretAnalyzer::computeStageCost(
  const Eigen::VectorXd& state,
  const Eigen::VectorXd& input,
  const Eigen::VectorXd& goal_state) const {  // ← 添加目标状态参数

  // ✅ Correct: Distance to goal
  Eigen::VectorXd state_error = state - goal_state;
  return state_error.squaredNorm() + input.squaredNorm();
}
```

### 方案2: 使用跟踪误差作为成本

```cpp
double RegretAnalyzer::computeStageCost(
  const Eigen::VectorXd& state,
  const Eigen::VectorXd& input,
  const Eigen::VectorXd& reference_state) const {

  // ✅ Correct: Tracking error
  Eigen::VectorXd tracking_error = state - reference_state;
  return tracking_error.squaredNorm() + input.squaredNorm();
}
```

### 方案3: 使用真实的Oracle

实现一个真正的oracle策略，计算最优成本：
```cpp
class NavigationOracle {
public:
  double getOptimalCost(int step) {
    // Compute shortest path cost from current to goal
    // Using A* or Dijkstra on known map
    return shortest_path_cost;
  }
};
```

---

## 📝 对Manuscript的影响

### 当前问题

1. **实验结果无效**: 负的regret无法与manuscript的理论对比
2. **理论保证失效**: Manuscript证明R_T = o(T)，但实验显示R_T < 0
3. **无法验证算法性能**: 不知道算法是好是坏

### Manuscript要求

根据manuscript Eq. 254-257:
```
Safe Regret R_T^safe = Σ[ℓ(x_k,u_k) - ℓ^s_k(x_k,u_k)]
                      - Σ[ℓ(z^ref_k,v^ref_k) - ℓ^s_k(z^ref_k,v^ref_k)]
```

其中：
- ℓ^s_k是safety-feasible policy的成本
- 需要正确定义成本函数（到目标的距离，不是到原点）

---

## 🎯 总结

### 问题根源
1. ❌ 成本函数使用`state.squaredNorm()`（到原点距离）
2. ❌ 基准成本`nominal_cost`可能比实际成本更大
3. ❌ 导致`cost_regret = instant_cost - nominal_cost < 0`

### 为什么这是错的
- 导航任务的成本应该是**到目标的距离**，不是到原点的距离
- 使用原点作为基准会导致**反直觉的结果**（目标点成本更高）

### 正确的做法
- ✅ 使用`‖x - x_goal‖²`作为成本
- ✅ 使用`‖x - x_ref‖²`（跟踪误差）
- ✅ 实现真实的oracle计算最优成本

### 影响
- 🔴 所有使用此成本函数的实验结果都**无效**
- 🔴 无法验证manuscript的理论保证
- 🔴 需要修复并**重新运行所有实验**

---

**生成时间**: 2026-04-09
**修改者**: Claude Sonnet 4.6
**状态**: 🔴 严重问题 - 需要修复
