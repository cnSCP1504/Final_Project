# DR Margin恒定0.18m问题根因分析

**日期**: 2026-04-17
**状态**: 🔴 找到根本原因

---

## 📊 问题现象

DR margin在所有时间步都恒定为0.18m，应该随残差统计变化。

```
tube_offset (L_h*ē): 0.18
mean_along_sensitivity (gradient·mean): 0
std_along_sensitivity: 0
cantelli_factor: 14.4568
cantelli_factor * std_along_sensitivity: 0
TOTAL_MARGIN: 0.18
```

---

## 🔍 根本原因分析

### 原因1: Gradient全为零

**日志数据**:
```
gradient: [-0 -0  0]
```

**代码分析** (`dr_tightening_node.cpp` line 86-108):

```cpp
Eigen::VectorXd grad = Eigen::VectorXd::Zero(state.size());  // state是3维

if (hasCostmap()) {
    // 只计算grad[0]和grad[1]
    grad[0] = -(f_x_plus - f_x_minus) / (2.0 * epsilon * 254.0);
    grad[1] = -(f_y_plus - f_y_minus) / (2.0 * epsilon * 254.0);
    return grad;  // grad[2] (theta) 保持为0
}
```

**问题**:
1. state是3维 `[x, y, theta]`
2. gradient只计算前2维 (基于costmap)
3. grad[2] (theta的梯度) 永远为0
4. 结果: `gradient = [grad_x, grad_y, 0]`

### 原因2: grad[0]和grad[1]也为零

**为什么?**

当机器人在自由空间（costmap值全为0）时:
- `f_x_plus = getCostmapValue(x + epsilon, y) = 0`
- `f_x_minus = getCostmapValue(x - epsilon, y) = 0`
- `grad[0] = -(0 - 0) / (2.0 * 0.05 * 254.0) = 0`
- 同理 `grad[1] = 0`

**结论**: 在自由空间中，costmap值恒为0，导致梯度为0！

### 原因3: tube_offset = 0.18 而非 0.5

**参数配置**:
```yaml
tube_radius: 0.5
lipschitz_constant: 1.0
```

**理论值**: `tube_offset = 1.0 * 0.5 = 0.5`

**实际值**: `tube_offset = 0.18`

**可能原因**:
- tube_radius参数未正确传递到TighteningComputer
- 或者在某处被覆盖为0.18

---

## ✅ 修复方案

### 方案1: 修复gradient计算

**问题**: costmap在自由空间中全为0，导致梯度为0

**解决**: 使用状态边界约束的梯度作为fallback，即使在自由空间也有非零梯度

```cpp
// 在gradient()函数中
if (hasCostmap()) {
    // 计算costmap梯度...

    // ✅ 新增: 如果梯度太小，使用fallback
    if (grad.head<2>().norm() < 1e-6) {
        // 使用状态边界约束的梯度
        double x_max = 10.0, y_max = 10.0;
        grad[0] = -2.0 * state[0] / (x_max * x_max);
        grad[1] = -2.0 * state[1] / (y_max * y_max);
    }
}
```

### 方案2: 修复tube_radius传递

**检查点**:
1. 参数文件 `dr_tightening_params.yaml` 中 `tube_radius: 0.5`
2. `loadParameters()` 读取 `params_.tube_radius`
3. `computeDRTightening()` 传递到 `computeTubeOffset()`

**需要验证**: tube_radius是否正确传递

### 方案3: 调整残差维度

**当前**: residual_dimension = 3, 但gradient只有前2维有效

**建议**: 要么
- 将residual_dimension改为2
- 或者为theta也计算梯度

---

## 📋 下一步行动

| 优先级 | 任务 | 状态 |
|--------|------|------|
| P0 | 验证tube_radius参数传递 | ⬜ 待做 |
| P0 | 修复gradient计算（自由空间fallback） | ⬜ 待做 |
| P1 | 调整residual维度匹配 | ⬜ 待做 |
| P2 | 增加详细调试输出 | ⬜ 待做 |

---

## 🔬 验证方法

1. **检查tube_radius传递**:
   - 在`computeTubeOffset()`中打印参数
   - 确认lipschitz_const和tube_radius的值

2. **验证costmap梯度**:
   - 打印`f_x_plus`, `f_x_minus`, `f_y_plus`, `f_y_minus`
   - 确认costmap在机器人位置附近的值

3. **测试修复后的效果**:
   - 梯度应该非零
   - DR margin应该随时间步变化

---

**报告生成**: 2026-04-17 22:30
