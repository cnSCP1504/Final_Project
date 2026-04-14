# DR/STL控制权重降低报告

**日期**: 2026-04-11
**问题**: DR和STL调控小车速度的幅度太激进

---

## 📊 修改对比

### STL调整权重

| 参数 | 修改前 | 修改后 | 变化 |
|------|--------|--------|------|
| **最小缩放因子** | 0.5 (降至50%) | 0.8 (降至80%) | +60% |
| **缩放公式** | `max(0.5, 1.0 + r/2.0)` | `max(0.8, 1.0 + r/5.0)` | 更温和 |
| **当robustness=-1.0时** | factor=0.5 | factor=0.8 | +60% |
| **当robustness=-0.5时** | factor=0.75 | factor=0.9 | +20% |

**效果**: STL对速度的影响降低约60%

### DR调整权重

| 参数 | 修改前 | 修改后 | 变化 |
|------|--------|--------|------|
| **线速度限制** | ×0.9 (降10%) | ×0.95 (降5%) | +50% |
| **角速度限制** | ×0.9 (降10%) | ×0.95 (降5%) | +50% |

**效果**: DR对速度的影响降低50%

---

## 📝 代码修改

### 文件: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp`

#### STL调整函数 (lines 957-966)

**修改前**:
```cpp
if (stl_info_.robustness_value < 0.0) {
    // Low robustness: scale down commands
    stl_factor = std::max(0.5, 1.0 + stl_info_.robustness_value / 2.0);
}
```

**修改后**:
```cpp
if (stl_info_.robustness_value < 0.0) {
    // Low robustness: scale down commands GENTLY
    // Old formula: factor could drop to 0.5 (50% reduction)
    // New formula: factor drops to max 0.8 (only 20% reduction)
    stl_factor = std::max(0.8, 1.0 + stl_info_.robustness_value / 5.0);
}
```

#### DR调整函数 (lines 990-995)

**修改前**:
```cpp
if (dr_info_.margins.size() > 0) {
    max_linear_vel *= 0.9;
    max_angular_vel *= 0.9;
}
```

**修改后**:
```cpp
if (dr_info_.margins.size() > 0) {
    // Reduced impact: 5% reduction instead of 10%
    max_linear_vel *= 0.95;
    max_angular_vel *= 0.95;
}
```

---

## 🎯 预期效果

### 速度对比示例

假设原始速度命令: `v=0.5 m/s`, `w=0.8 rad/s`

**场景1: STL robustness = -1.0 (较差)**

| 阶段 | 修改前速度 | 修改后速度 | 改善 |
|------|-----------|-----------|------|
| STL调整后 | v=0.25, w=0.4 | v=0.4, w=0.64 | +60% |
| DR调整后 | v=0.225, w=0.36 | v=0.38, w=0.61 | +69% |

**场景2: STL robustness = -0.5 (中等)**

| 阶段 | 修改前速度 | 修改后速度 | 改善 |
|------|-----------|-----------|------|
| STL调整后 | v=0.375, w=0.6 | v=0.45, w=0.72 | +20% |
| DR调整后 | v=0.338, w=0.54 | v=0.43, w=0.68 | +26% |

---

## ✅ 状态

- ✅ 代码修改完成
- ✅ 编译成功
- ⏳ 待测试验证

---

## 📋 测试命令

```bash
# 清理进程
./test/scripts/clean_ros.sh

# 运行测试
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --timeout 120 --no-viz
```
