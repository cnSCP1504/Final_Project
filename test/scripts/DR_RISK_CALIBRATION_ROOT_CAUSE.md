# DR约束风险校准问题根因分析报告

**日期**: 2026-04-17
**状态**: 🔴 发现根本问题

---

## 📊 问题摘要

| 指标 | 目标值 | 实际值 | 偏差 |
|------|--------|--------|------|
| 目标风险 δ | 5% | - | - |
| 经验风险 | ~5% | **~13%** | +160% |
| 校准误差 | <5% | **~8%** | +60% |

---

## 🔍 根本原因分析

### 发现1: DR Margin恒定为0.18m

**现象**:
```
第一组DR margins (t=0 to t=20):
  t=0: margin = 0.180000
  t=1: margin = 0.180000
  ...
  t=20: margin = 0.180000
```

**问题**: DR margin应该随时间步变化（基于残差统计），但实际恒定为0.18m

### 发现2: 经验风险计算方式错误

**当前实现** (`run_automated_test.py` line 376-378):
```python
# 使用tube_violation_count计算经验风险
metrics['empirical_risk'] = (
    self.data['tube_violation_count'] / self.data['safety_total_steps']
)
```

**问题**: 使用tube_radius(0.5m)作为阈值，而非DR margin(0.18m)

### 发现3: 阈值差异导致违反率差异

| 阈值 | 违反率 |
|------|--------|
| tube_radius = 0.5m | **13.2%** |
| tube_radius = 0.7m | 11.15% |
| DR margin = 0.18m | **42.4%** |

**关键洞察**:
- DR约束的实际阈值是0.18m（非常严格）
- 测试脚本使用0.5m作为阈值（更宽松）
- 这导致报告的经验风险(13%)远低于实际DR约束违反率(42%)

---

## 🔬 深层原因

### 原因1: DR Margin计算问题

**代码位置**: `src/dr_tightening/src/TighteningComputer.cpp`

**问题**:
```cpp
double TighteningComputer::computeChebyshevMargin(...) {
    // ...
    double total_margin = tube_offset + mean_along_sensitivity +
                         cantelli_factor * std_along_sensitivity;
    return total_margin;
}
```

**margin = 0.18m 的组成**:
- `tube_offset (L_h·ē)`: Lipschitz × tube_radius = 1.0 × 0.5 = 0.5m?
- `mean_along_sensitivity`: 残差均值在梯度方向的投影
- `cantelli_factor * std`: κ_δ × σ

**待验证**: 为什么所有时间步的margin完全相同？

### 原因2: 残差数据不足或统计特性异常

**可能的问题**:
1. 残差窗口未填满，使用默认值
2. 残差协方差接近零，导致std_along_sensitivity ≈ 0
3. 残差均值接近零，导致mean_along_sensitivity ≈ 0

### 原因3: 风险分配策略问题

**当前配置** (`dr_tightening_params.yaml`):
```yaml
risk_level: 0.1
risk_allocation: "uniform"
```

**问题**: 每个时间步的δ_t = 0.1 / 21 ≈ 0.00476，导致Cantelli因子极大

```python
# Cantelli因子: κ = sqrt((1-δ)/δ)
delta_t = 0.00476
kappa = sqrt((1-0.00476)/0.00476) = sqrt(209.2) ≈ 14.5
```

**影响**: Cantelli因子14.5 × 小std = 仍然可能很大

---

## ✅ 修复方案

### 方案1: 修复经验风险计算

**修改**: `run_automated_test.py`

```python
# 使用DR margin作为阈值（而非tube_radius）
if len(self.data['dr_margins_history']) > 0:
    # 获取t=0时刻的DR margin
    horizon = self.data.get('dr_horizon', 20)
    latest_t0_margin_index = -(horizon + 1)
    latest_dr_margin = self.data['dr_margins_history'][latest_t0_margin_index]

    # 使用DR margin检查违反
    if combined_error > latest_dr_margin:
        self.data['safety_violation_count'] += 1
```

### 方案2: 调整风险分配策略

**修改**: `dr_tightening_params.yaml`

```yaml
# 选项A: 使用deadline_weighted分配（更多风险分配给早期步骤）
risk_allocation: "deadline_weighted"

# 选项B: 直接增加risk_level到接近实际违反率
risk_level: 0.15  # 或 0.20
```

### 方案3: 调整DR margin计算

**问题**: margin恒定为0.18m说明计算可能使用了固定值

**待验证**:
1. 检查残差收集器是否正常工作
2. 验证残差统计（均值、协方差）
3. 确认Cantelli因子计算

---

## 📋 后续行动

| 优先级 | 任务 | 状态 |
|--------|------|------|
| P0 | 验证DR margin计算流程 | ⬜ 待做 |
| P0 | 修复经验风险计算使用DR margin | ⬜ 待做 |
| P1 | 调整风险分配策略 | ⬜ 待做 |
| P2 | 增加DR margin调试输出 | ⬜ 待做 |

---

## 📈 预期改进

修复后的预期结果：
- 经验风险应该反映真实的DR约束违反率
- 如果DR margin=0.18m是合理的，违反率应在42%左右
- 需要重新审视DR参数设计，使margin接近tube_radius

---

**报告生成**: 2026-04-17 22:00
