# Manuscript Metrics - 快速开始指南

## 📋 概述

本工具链用于将ROS测试数据转换为manuscript级别的性能指标和出版质量图表。

**包含工具**:
1. `compute_manuscript_metrics.py` - 计算7类核心指标
2. `generate_manuscript_figures.py` - 生成出版级图表
3. `generate_all_metrics.sh` - 一键生成所有指标和图表

---

## 🚀 快速开始

### 前提条件

```bash
# 安装Python依赖
pip install numpy matplotlib scipy

# 或者使用requirements.txt（如果有）
pip install -r requirements.txt
```

### 一键生成（推荐）

```bash
# 完整流程：计算指标 + 生成图表 + 创建报告
./test/scripts/generate_all_metrics.sh test_results/safe_regret_20260408_120104
```

**输出**:
- `aggregated_metrics.json` - 所有7类指标的JSON数据
- `metrics/` - 目录包含7个PNG/PDF图表
- `performance_summary.md` - 人类可读的性能摘要

---

## 📊 生成的指标

根据manuscript Section V.B，计算以下7类指标：

### 1. Satisfaction Probability (满意度概率)
- **定义**: STL规范被满足的测试比例
- **单位**: 比例 (0-1) 或百分比
- **置信区间**: Wilson 95% CI
- **图表**: `satisfaction_probability.png`

### 2. Empirical Risk (经验风险)
- **定义**: 安全约束违反的比例
- **单位**: 比例 (0-1)
- **目标值**: 应接近目标风险水平 δ (通常为0.05)
- **图表**: `empirical_risk.png`

### 3. Dynamic & Safe Regret (动态与安全遗憾)
- **定义**: 相对于最优策略的累积性能损失
- **单位**: 无量纲
- **归一化**: 报告为 R_T^dyn / T
- **图表**: 在dashboard中显示

### 4. Recursive Feasibility Rate (递归可行性率)
- **定义**: MPC优化问题可行的比例
- **单位**: 比例 (0-1)
- **目标值**: >99.5% (simulation), >99% (hardware)
- **图表**: `feasibility_rate.png`

### 5. Computation Metrics (计算指标)
- **Median solve time**: 中位数求解时间
- **P90/P95 solve time**: 90/95分位数求解时间
- **Failure count**: 求解失败次数
- **目标值**: Median <8 ms (real-time)
- **图表**: `computation_metrics.png`

### 6. Tracking Error & Tube Occupancy (跟踪误差与管占用量)
- **Mean/Std/RMSE/Max**: 跟踪误差统计
- **Occupancy rate**: 状态保持在管内的比例
- **目标值**: Occupancy rate ≈ 100%
- **图表**: `tracking_error_distribution.png`

### 7. Calibration Accuracy (校准精度)
- **定义**: 观察到的违反率与目标风险水平的差异
- **单位**: 比例 (0-1)
- **目标值**: Calibration error ≈ 0
- **图表**: `calibration_accuracy.png`

---

## 🛠️ 分步使用

### Step 1: 计算聚合指标

```bash
python3 test/scripts/compute_manuscript_metrics.py \
    --input test_results/safe_regret_20260408_120104 \
    --output test_results/safe_regret_20260408_120104/aggregated_metrics.json \
    --tube-radius 0.18 \
    --target-delta 0.05
```

**参数说明**:
- `--input`: 测试结果目录（包含test_XX子目录）
- `--output`: 输出JSON文件路径
- `--tube-radius`: 管半径（默认0.18m）
- `--target-delta`: 目标风险水平（默认0.05）

**输出示例**:
```json
{
  "metadata": {
    "total_tests": 10,
    "tube_radius": 0.18,
    "target_delta": 0.05
  },
  "satisfaction_probability": {
    "p_sat": 1.0,
    "ci_lower": 0.726,
    "ci_upper": 1.0,
    "satisfied_count": 10,
    "total_count": 10
  },
  "empirical_risk": {
    "observed_delta": 0.0123,
    "target_delta": 0.05,
    "calibration_error": 0.0377,
    "relative_error": 0.754,
    ...
  },
  ...
}
```

### Step 2: 生成图表

```bash
python3 test/scripts/generate_manuscript_figures.py \
    --metrics test_results/safe_regret_20260408_120104/aggregated_metrics.json \
    --output test_results/safe_regret_20260408_120104/metrics/
```

**生成的文件**:
```
metrics/
├── satisfaction_probability.png/pdf    # 满意度概率柱状图
├── empirical_risk.png/pdf              # 经验风险柱状图
├── computation_metrics.png/pdf         # 计算时间统计图
├── feasibility_rate.png/pdf            # 可行性率柱状图
├── tracking_error_distribution.png/pdf # 跟踪误差分布图
├── calibration_accuracy.png/pdf        # 校准精度散点图
└── performance_dashboard.png/pdf       # 综合仪表板（2x4子图）
```

---

## 📈 在Manuscript中使用

### LaTeX插入图表

```latex
% 单栏图
\begin{figure}[htbp]
  \centering
  \includegraphics[width=0.8\columnwidth]{metrics/satisfaction_probability.pdf}
  \caption{STL specification satisfaction rate across methods. Wilson 95\% confidence intervals shown.}
  \label{fig:satisfaction}
\end{figure}

% 双栏图
\begin{figure*}[htbp]
  \centering
  \includegraphics[width=0.9\textwidth]{metrics/performance_dashboard.pdf}
  \caption{Comprehensive performance evaluation of Safe-Regret MPC. (a) STL satisfaction rate. (b) Empirical risk. (c) Recursive feasibility. (d) Computation time. (e) Tracking error. (f) Tube occupancy. (g) Calibration accuracy. (h) Dynamic regret. (i) Performance summary table.}
  \label{fig:dashboard}
\end{figure*}
```

### 引用指标值

```latex
The proposed Safe-Regret MPC achieves a satisfaction probability of
99.5\% [95\% CI: 97.2\%, 99.9\%], with an empirical risk of
0.012 (target: $\delta=0.05$), demonstrating well-calibrated safety
constraints (calibration error: 0.038).

The median MPC solve time is 7.2~ms (P90: 9.8~ms), meeting the
real-time budget of 8~ms. The recursive feasibility rate is 99.8\%,
and the tracking error RMSE is 4.3~cm with a tube occupancy rate of
99.2\%.
```

---

## 🔍 高级用法

### 多方法对比

如果要对比多个方法（如tube_mpc vs safe_regret），需要：

1. 分别为每个方法计算指标:
```bash
# Tube MPC baseline
python3 compute_manuscript_metrics.py \
    --input test_results/tube_mpc_20260408_120104 \
    --output aggregated_tube_mpc.json

# Safe-Regret MPC
python3 compute_manuscript_metrics.py \
    --input test_results/safe_regret_20260408_120104 \
    --output aggregated_safe_regret.json
```

2. 修改`generate_manuscript_figures.py`中的图表函数，添加多方法支持

### 自定义参数

**管半径调整**:
```bash
# 如果使用了不同的管半径
python3 compute_manuscript_metrics.py \
    --input test_results/safe_regret_20260408_120104 \
    --output aggregated_metrics.json \
    --tube-radius 0.25  # 从0.18改为0.25
```

**目标风险水平调整**:
```bash
# 如果使用了不同的风险水平
python3 compute_manuscript_metrics.py \
    --input test_results/safe_regret_20260408_120104 \
    --output aggregated_metrics.json \
    --target-delta 0.1  # 从0.05改为0.1
```

---

## 🐛 故障排除

### 问题1: 没有找到测试数据

**错误信息**:
```
❌ Error: No test data found in test_results/safe_regret_20260408_120104
```

**解决方案**:
- 检查目录结构是否正确: `test_results/safe_regret_20260408_120104/test_XX/metrics.json`
- 确保已经运行过测试并生成了metrics.json文件

### 问题2: 缺少Python依赖

**错误信息**:
```
ModuleNotFoundError: No module named 'scipy'
```

**解决方案**:
```bash
pip install numpy matplotlib scipy
```

### 问题3: 图表生成失败

**错误信息**:
```
ValueError: could not convert string to float
```

**解决方案**:
- 检查aggregated_metrics.json格式是否正确
- 确保所有数值字段都是有效的数字
- 尝试重新运行`compute_manuscript_metrics.py`

---

## 📚 相关文档

- **完整方案**: `test/scripts/MANUSCRIPT_METRICS_VISUALIZATION_GUIDE.md`
- **测试指南**: `test/scripts/AUTOMATED_TESTING_README.md`
- **Manuscript**: `latex/manuscript.tex` (Section V.B: Metrics)

---

## ✅ 质量检查清单

在使用生成的指标和图表之前，请确认：

- [ ] 所有测试都有完整的metrics.json
- [ ] Satisfaction probability使用Wilson 95% CI
- [ ] Empirical risk基于安全违反计数
- [ ] Regret已归一化（除以T）
- [ ] Feasibility rate包含所有MPC求解
- [ ] 计算时间使用正确的分位数（median, P90）
- [ ] 所有图表有清晰的标题和轴标签
- [ ] 图表分辨率≥300 DPI
- [ ] 配色方案色盲友好
- [ ] 图表尺寸符合IEEE格式要求

---

## 📧 反馈与支持

如有问题或建议，请查看：
- 项目文档: `CLAUDE.md`
- 测试脚本: `test/scripts/`
- 性能分析: `test_results/`

---

**最后更新**: 2026-04-08
**版本**: 1.0
**状态**: ✅ Production Ready
