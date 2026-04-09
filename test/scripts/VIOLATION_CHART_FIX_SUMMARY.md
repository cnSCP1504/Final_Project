# 图表生成修复 - Safety vs Tube Violation柱状图

**日期**: 2026-04-09
**问题**: Tube MPC和STL模式没有safety约束，但图表显示safety_violation=0，容易误解

---

## 🔍 问题分析

### 原始问题

**用户指出**: "tube和stl其实是没有safety，不是safety真的为0"

**原因**:
- Tube MPC模式：只有MPC，没有DR约束，没有safety监控
- STL模式：只有STL监控，没有DR约束，没有safety监控
- DR模式：有DR约束，有safety监控
- Safe Regret模式：有DR+STL，有safety监控

**数据验证**:

| 模式 | dr_margins_history | safety_violation_count | tube_violation_count |
|------|-------------------|----------------------|---------------------|
| **tube_mpc** | 0 items ❌ | 0 (但无意义) | 54 |
| **stl** | 0 items ❌ | 0 (但无意义) | 146 |
| **dr** | 21399 items ✅ | 34 | 158 |
| **safe_regret** | 24234 items ✅ | 203 | 390 |

**关键发现**: Tube MPC和STL的`safety_violation_count=0`不是因为没违反，而是因为没有监控！

---

## ✅ 修复方案

### 判断逻辑修改

**修改前**（错误）:
```python
has_safety = (
    'safety_violation_count' in raw_data and
    raw_data['safety_violation_count'] is not None and
    raw_data.get('safety_total_steps', 0) > 0
)
```

**问题**: 所有模式都有`safety_violation_count`字段（初始化为0），无法区分

**修改后**（正确）:
```python
has_dr_margins = (
    'dr_margins_history' in raw_data and
    len(raw_data['dr_margins_history']) > 0
)
```

**优点**: 使用`dr_margins_history`判断是否有DR约束（safety监控）

### 图表显示逻辑

```python
if has_dr_margins:
    # 有DR约束：显示实际safety violation值
    safety_label = f'Safety\n({violations}/{total} = {rate:.1f}%)'
    safety_value = safety_violations
    color = COLORS['red']
else:
    # 无DR约束：显示N/A
    safety_label = 'Safety\n(N/A - 无DR约束)'
    safety_value = 0
    color = COLORS['gray']  # 灰色表示不适用
```

### 图表效果

**Tube MPC / STL模式**:
```
柱状图:
  Safety Violation: [灰色柱子，高度0]
                    标签: "Safety (N/A - 无DR约束)"
  Tube Violation:   [蓝色柱子，高度实际值]
                    标签: "Tube (54/481 = 11.2%)"
```

**DR / Safe Regret模式**:
```
柱状图:
  Safety Violation: [红色柱子，高度实际值]
                    标签: "Safety (34/964 = 3.5%)"
  Tube Violation:   [蓝色柱子，高度实际值]
                    标签: "Tube (158/964 = 16.4%)"
```

---

## 📊 验证结果

### 逻辑验证

| 模式 | dr_margins | has_dr_margins | Safety显示 |
|------|-----------|----------------|-----------|
| tube_mpc | 0 items | False | N/A ✅ |
| stl | 0 items | False | N/A ✅ |
| dr | 21399 items | True | 34 ✅ |
| safe_regret | 24234 items | True | 203 ✅ |

**结论**: 逻辑完全正确！

### 修改的文件

**文件**: `test/scripts/run_automated_test.py`

**修改位置**: 第1535-1609行（第4个子图）

**修改内容**:
1. ✅ 替换STL Satisfaction Rate为Safety vs Tube Violations
2. ✅ 使用`dr_margins_history`判断是否有safety约束
3. ✅ 无约束模式显示"N/A - 无DR约束"
4. ✅ 有约束模式显示实际violation数据
5. ✅ 使用不同颜色区分（灰色=不适用，红色=safety，蓝色=tube）

---

## 🎯 预期效果

### Tube MPC模式图表

```
(d) Safety vs Tube Violations
  ⚠️ Safety约束未启用（无DR）

  Safety Violation  │
                    │  N/A - 无DR约束
  ──────────────────┼────────────────
                    │
  Tube Violation    │        ████
                    │        ████
  ──────────────────┼────────────────
```

### Safe Regret模式图表

```
(d) Safety vs Tube Violations

  Safety Violation  │        ████
                    │        ████
  ──────────────────┼────────────────
                    │
  Tube Violation    │        ████████
                    │        ████████
  ──────────────────┼────────────────
```

---

## 📝 关键改进

### 1. 正确区分"无约束"和"无违反"

**修改前**:
- Tube MPC: safety_violation = 0 → 误解为"无违反"
- Safe Regret: safety_violation = 203 → 真实的违反

**修改后**:
- Tube MPC: safety = N/A → 明确"无约束"
- Safe Regret: safety = 203 → 真实的违反

### 2. 避免数据误导

**修改前**:
读者可能认为Tube MPC的安全性比Safe Regret好（0 vs 203）

**修改后**:
读者明确知道Tube MPC没有safety监控，无法比较

### 3. 提供完整信息

**新图表同时显示**:
- Safety violation（如果有DR约束）
- Tube violation（所有模式都有）
- 清晰标注哪个约束启用
- 百分比显示违反率

---

## ✅ 总结

**问题**: Tube MPC和STL没有safety约束，但显示为safety_violation=0

**根本原因**: 使用`safety_violation_count`字段判断，但所有模式都初始化了该字段

**解决方案**: 使用`dr_margins_history`判断是否有DR约束

**效果**:
- ✅ Tube MPC/STL显示"N/A - 无DR约束"
- ✅ DR/Safe Regret显示实际violation数据
- ✅ 避免数据误导
- ✅ 提供完整信息

**状态**: ✅ **修复完成并验证通过**

---

**生成时间**: 2026-04-09
**修改者**: Claude Sonnet 4.6
**测试文件**: `test_results/tube_mpc_20260409_164748`, `test_results/safe_regret_20260409_165316`
