# 参数敏感性分析实验设计

**日期**: 2026-04-17
**状态**: 📝 实验设计完成

---

## 问题描述

论文中三个关键参数未提供调参依据，也未进行敏感性分析:

| 参数 | 符号 | 当前值 | 问题 |
|------|------|--------|------|
| MPC时域 | N | 20 | 未说明为何选择20步 |
| STL温度 | τ | 0.1 | 未分析τ对robustness近似精度的影响 |
| 残差窗口 | M | 200 | 未分析M对DR margin统计可靠性的影响 |

---

## 实验设计

### 1. 参数范围选择

| 参数 | 测试范围 | 理论依据 |
|------|----------|----------|
| **MPC时域 N** | 10, 15, 20, 25, 30 | 预测步数影响优化精度和计算时间 |
| **STL温度 τ** | 0.01, 0.05, 0.1, 0.5, 1.0 | τ↓ → 近似更准确，τ↑ → 优化更平滑 |
| **残差窗口 M** | 50, 100, 200, 300, 500 | M↑ → 统计更可靠，初始化更慢 |

### 2. 评价指标

| 指标 | 说明 | 目标值 |
|------|------|--------|
| **empirical_risk** | 实际违规率 | ≈ 5% (目标δ) |
| **feasibility_rate** | MPC可行率 | 100% |
| **median_solve_time** | 求解时间 | < 100ms |
| **mean_tracking_error** | 平均跟踪误差 | 越小越好 |
| **calibration_error** | 校准误差 | ≈ 0% |
| **safety_margin** | 安全边际 | > 0 |

### 3. 敏感性指数计算

```
敏感性指数 = 指标变化范围 / 参数变化范围
```

值越大表示参数对该指标影响越大。

---

## 实验脚本

### 快速测试 (仅测试极端值)

```bash
# 测试每个参数的2个极端值 (共6次测试)
./test/scripts/quick_sensitivity_test.sh
```

**测试内容**:
- mpc_steps: 10, 30
- temperature: 0.01, 1.0
- window_size: 50, 500

### 完整敏感性分析

```bash
# 测试所有参数的所有值 (共15次测试)
python3 test/scripts/parameter_sensitivity_analysis.py
```

**测试内容**:
- mpc_steps: 10, 15, 20, 25, 30 (5次)
- temperature: 0.01, 0.05, 0.1, 0.5, 1.0 (5次)
- window_size: 50, 100, 200, 300, 500 (5次)

### 分析结果

```bash
# 生成报告和图表
python3 test/scripts/analyze_sensitivity_results.py test_results/sensitivity_analysis_YYYYMMDD_HHMMSS
```

---

## 预期输出

### 1. 汇总表格

| 参数值 | 经验风险(%) | 可行率(%) | 求解时间(ms) | 校准误差(%) |
|--------|-------------|-----------|--------------|-------------|
| N=10   | -           | -         | -            | -           |
| N=15   | -           | -         | -            | -           |
| ...    | ...         | ...       | ...          | ...         |

### 2. 敏感性曲线图

- 经验风险 vs 参数值
- 求解时间 vs 参数值
- 校准误差 vs 参数值

### 3. 敏感性指数表

| 参数 | empirical_risk | solve_time | calibration_error |
|------|----------------|------------|-------------------|
| N    | X.XXXX         | X.XXXX     | X.XXXX            |
| τ    | X.XXXX         | X.XXXX     | X.XXXX            |
| M    | X.XXXX         | X.XXXX     | X.XXXX            |

---

## 预期结论

### MPC时域 N

**理论预期**:
- N↑ → 预测更准确 → 跟踪误差↓
- N↑ → 优化变量↑ → 求解时间↑
- 存在最优N值平衡精度和实时性

**建议**: 根据实时性要求选择N=20-25

### STL温度 τ

**理论预期**:
- τ↓ (0.01-0.1) → robustness近似更准确
- τ↑ (0.5-1.0) → 优化曲面更平滑，收敛更快
- τ过小可能导致数值不稳定

**建议**: τ=0.1 作为默认值，需要精度时用τ=0.05

### 残差窗口 M

**理论预期**:
- M↑ → DR margin统计更可靠
- M↑ → 初始化时间↑
- M过小(<50) → 统计不可靠

**建议**: M=200 作为平衡点

---

## 文件清单

| 文件 | 说明 |
|------|------|
| `parameter_sensitivity_analysis.py` | 完整敏感性分析脚本 |
| `quick_sensitivity_test.sh` | 快速测试脚本 (仅测试极端值) |
| `analyze_sensitivity_results.py` | 结果分析和可视化脚本 |

---

## 使用方法

```bash
# 1. 快速测试 (约30分钟)
./test/scripts/quick_sensitivity_test.sh

# 2. 完整测试 (约75分钟)
python3 test/scripts/parameter_sensitivity_analysis.py

# 3. 分析结果
python3 test/scripts/analyze_sensitivity_results.py test_results/sensitivity_analysis_YYYYMMDD_HHMMSS
```

---

**报告生成**: 2026-04-17
