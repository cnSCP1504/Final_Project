# 自动测试脚本 - Manuscript指标数据处理与制图集成说明

## 修改日期
2026-04-08

## 修改概述

将manuscript中定义的7种性能指标的数据处理和绘图功能完全集成到自动测试脚本中，实现：

1. **单个测试完成后**：立即处理该测试的数据并生成图表，保存在对应的shelf文件夹中
2. **所有测试结束后**：聚合所有测试的数据，生成综合报告和图表，保存在测试根目录下

---

## 修改的文件

- `test/scripts/run_automated_test.py` - 自动测试脚本主文件

---

## 新增功能

### 1. 导入必要的库

```python
import matplotlib
matplotlib.use('Agg')  # 使用非交互式后端
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from scipy import stats
```

### 2. 新增方法列表

#### 2.1 单测试处理方法

**`process_single_test_metrics(test_dir, shelf)`**
- 功能：处理单个测试的manuscript指标并生成图表
- 参数：
  - `test_dir`: 测试目录路径
  - `shelf`: 货架信息字典
- 调用时机：每个测试完成后（在 `run_single_test()` 中）

**`_generate_single_test_report(metrics, test_dir, shelf)`**
- 功能：生成单个测试的指标报告（文本格式）
- 输出文件：`{test_dir}/manuscript_metrics_report.txt`
- 包含内容：7类manuscript指标的详细统计

**`_generate_single_test_figures(metrics, figures_dir, shelf)`**
- 功能：生成单个测试的性能图表
- 输出文件：
  - `{test_dir}/figures/performance_dashboard.png`
  - `{test_dir}/figures/performance_dashboard.pdf`
- 包含子图：
  - (a) STL Robustness Over Time
  - (b) Tracking Error Over Time
  - (c) MPC Computation Time
  - (d) STL Satisfaction Rate (饼图)

#### 2.2 聚合处理方法

**`aggregate_all_tests_metrics()`**
- 功能：聚合所有测试的manuscript指标并生成综合报告和图表
- 调用时机：所有测试完成后（在 `generate_final_report()` 中）

**`_compute_aggregated_metrics(all_tests_data)`**
- 功能：计算聚合的manuscript指标
- 处理逻辑：
  - 对每个指标计算：均值、标准差、最小值、最大值、中位数
  - 对风险和可行性指标计算聚合值（总违反次数/总步数）

**`_generate_aggregated_report(metrics, output_dir)`**
- 功能：生成聚合指标报告（文本格式）
- 输出文件：`{output_dir}/aggregated_manuscript_metrics_report.txt`

**`_generate_aggregated_figures(metrics, output_dir)`**
- 功能：生成聚合图表（3×3综合仪表板）
- 输出文件：
  - `{output_dir}/aggregated_figures/aggregated_performance_dashboard.png`
  - `{output_dir}/aggregated_figures/aggregated_performance_dashboard.pdf`
- 包含子图：
  - (a) STL Satisfaction（带误差棒）
  - (b) Empirical Risk（带目标线）
  - (c) Recursive Feasibility（带目标线）
  - (d) Computation Time（多统计量）
  - (e) Tracking Error（带误差棒）
  - (f) Tube Occupancy（饼图）
  - (g) Dynamic Regret
  - (h) Calibration Accuracy
  - (i) Performance Summary（表格）

---

## Manuscript指标（7类）

### 1. Satisfaction Probability（STL满足概率）
- **指标**：STL规范满足率
- **计算**：满足步数 / 总步数
- **目标**：越高越好

### 2. Empirical Risk（经验风险）
- **指标**：安全约束违反率
- **计算**：违反次数 / 总步数
- **目标**：δ̂ ≈ 0.05（目标风险水平）

### 3. Dynamic Regret（动态遗憾）
- **指标**：归一化累积遗憾
- **计算**：R_T / T
- **目标**：越低越好

### 4. Recursive Feasibility（递归可行性）
- **指标**：MPC求解可行率
- **计算**：可行求解次数 / 总求解次数
- **目标**：≥ 99.5%

### 5. Computation Metrics（计算指标）
- **指标**：MPC求解时间统计
- **统计量**：中位数、均值、P90、P95、P99
- **目标**：中位数 < 8 ms（实时预算）

### 6. Tracking Error（跟踪误差）
- **指标**：位置误差统计
- **统计量**：均值、标准差、RMSE、最大值
- **管占用量**：在管内的步数比例
- **目标**：平均误差 < 18 cm（管半径）

### 7. Calibration Accuracy（校准精度）
- **指标**：观察风险与目标风险的偏差
- **计算**：|δ̂ - δ_target|
- **目标**：< 0.01（1%容差）

---

## 输出文件结构

```
test_results/
└── {model}_{timestamp}/
    ├── test_01_shelf_{id}/
    │   ├── metrics.json
    │   ├── test_summary.txt
    │   ├── manuscript_metrics_report.txt      # 新增：单测试指标报告
    │   └── figures/                            # 新增：图表目录
    │       ├── performance_dashboard.png
    │       └── performance_dashboard.pdf
    ├── test_02_shelf_{id}/
    │   ├── metrics.json
    │   ├── test_summary.txt
    │   ├── manuscript_metrics_report.txt      # 新增
    │   └── figures/                            # 新增
    │       ├── performance_dashboard.png
    │       └── performance_dashboard.pdf
    ├── ...
    ├── final_report.txt
    ├── aggregated_manuscript_metrics.json     # 新增：聚合指标数据
    ├── aggregated_manuscript_metrics_report.txt  # 新增：聚合报告
    └── aggregated_figures/                     # 新增：聚合图表目录
        ├── aggregated_performance_dashboard.png
        └── aggregated_performance_dashboard.pdf
```

---

## 使用方法

### 运行测试

```bash
# 基本测试（会自动进行数据处理和制图）
python3 test/scripts/run_automated_test.py --model safe_regret --shelves 5 --no-viz

# 测试完成后，自动生成：
# 1. 每个测试的指标报告和图表
# 2. 所有测试的聚合报告和综合图表
```

### 查看结果

```bash
# 单测试结果
cat test_results/safe_regret_20260408_120104/test_01_shelf_01/manuscript_metrics_report.txt
ls test_results/safe_regret_20260408_120104/test_01_shelf_01/figures/

# 聚合结果
cat test_results/safe_regret_20260408_120104/aggregated_manuscript_metrics_report.txt
ls test_results/safe_regret_20260408_120104/aggregated_figures/
```

---

## 图表样式

### 配色方案（色盲友好）

- **蓝色** (#2E86AB)：主要数据
- **红色** (#C73E1D)：警告/未达标
- **绿色** (#6A994E)：目标线/满足条件
- **灰色** (#808080)：辅助信息

### 图表参数

- 字体：Serif
- 分辨率：300 DPI（出版质量）
- 格式：PNG（预览）+ PDF（出版）
- 后端：Agg（非交互式，适合服务器环境）

---

## 错误处理

### 单测试处理失败

```python
# 如果单测试数据处理失败，会记录警告但继续执行后续测试
try:
    self.process_single_test_metrics(test_dir, shelf)
except Exception as e:
    TestLogger.warning(f"  Manuscript指标处理失败: {e}")
    TestLogger.warning(f"  继续执行后续测试...")
```

### 聚合处理失败

```python
# 如果聚合处理失败，会记录错误但不影响最终报告生成
try:
    self.aggregate_all_tests_metrics()
except Exception as e:
    TestLogger.error(f"  聚合manuscript指标失败: {e}")
    TestLogger.error(f"  请检查测试数据是否完整")
```

---

## 数据依赖

### 必需数据

单测试数据处理需要 `metrics.json` 包含以下字段：

```json
{
  "manuscript_metrics": {
    "stl_robustness_history": [...],
    "tracking_error_norm_history": [...],
    "mpc_solve_times": [...],
    "satisfaction_probability": {...},
    "empirical_risk": {...},
    "regret": {...},
    "feasibility": {...},
    "computation": {...},
    "tracking": {...},
    "calibration": {...}
  }
}
```

### 数据收集器

数据由 `ManuscriptMetricsCollector` 类自动收集（已在测试脚本中集成）。

---

## 性能考虑

### 处理时间

- **单测试处理**：约1-2秒/测试
- **聚合处理**：约5-10秒（取决于测试数量）

### 内存使用

- **单测试**：约10-20 MB
- **聚合处理**：约50-100 MB

### 优化建议

1. 如果测试数量很多（>20），可以考虑：
   - 只对成功的测试进行数据处理
   - 降低图表分辨率（300 → 150 DPI）
   - 只生成PNG格式（不生成PDF）

2. 如果服务器性能受限：
   - 设置 `skip_figures=True` 跳过图表生成
   - 只生成文本报告

---

## 示例输出

### 单测试报告片段

```
======================================================================
Manuscript性能指标报告 - 测试 1: shelf_01
======================================================================

测试时间: 2026-04-08 12:01:04
货架位置: (3.0, -7.0)

1. STL Satisfaction Probability
   满足步数: 856/1200
   最终鲁棒性: 0.1234
   平均鲁棒性: 0.0856

2. Empirical Risk (安全约束违反)
   违反次数: 62
   总步数: 1200
   观察风险: 0.0517
   目标风险: 0.05

...
```

### 聚合报告片段

```
======================================================================
聚合Manuscript性能指标报告
Aggregated Manuscript Performance Metrics Report
======================================================================

生成时间: 2026-04-08 12:35:42
模型: safe_regret
测试数量: 5

1. STL Satisfaction Probability (聚合统计)
   均值: 71.35%
   标准差: 5.23%
   中位数: 72.10%
   范围: [64.20%, 76.80%]

2. Empirical Risk (聚合统计)
   聚合风险: 0.0512
   均值: 0.0515 ± 0.0023
   总违反次数: 308
   总步数: 6000

...
```

---

## 故障排查

### 问题1：找不到manuscript_metrics

**症状**：警告 "未找到manuscript_metrics，跳过数据处理"

**原因**：
- `metrics.json` 中缺少 `manuscript_metrics` 字段
- 测试失败或未完成

**解决**：
- 检查测试是否成功完成
- 检查 `ManuscriptMetricsCollector` 是否正常工作

### 问题2：图表生成失败

**症状**：警告 "Manuscript指标处理失败"

**原因**：
- 缺少matplotlib依赖
- 数据格式不正确

**解决**：
```bash
# 安装依赖
pip install matplotlib scipy

# 检查数据格式
python3 -c "import json; print(json.load(open('metrics.json'))['manuscript_metrics'].keys())"
```

### 问题3：聚合处理失败

**症状**：错误 "聚合manuscript指标失败"

**原因**：
- 没有成功的测试
- 所有测试都缺少 `manuscript_metrics`

**解决**：
- 确保至少有一个测试成功完成
- 检查单测试数据处理是否正常

---

## 后续改进

### 可能的增强

1. **交互式可视化**
   - 添加HTML交互式报告（Plotly）
   - 添加实时监控面板

2. **更多统计方法**
   - 添加置信区间计算
   - 添加统计显著性检验

3. **性能对比**
   - 支持多模型对比图表
   - 生成论文级别的表格

4. **自动化报告**
   - 生成LaTeX表格代码
   - 生成论文插图（符合期刊要求）

---

## 作者

- 修改：Claude Code
- 日期：2026-04-08
- 版本：1.0

---

## 相关文档

- `compute_manuscript_metrics.py` - 独立的指标计算脚本
- `generate_manuscript_figures.py` - 独立的图表生成脚本
- `CLAUDE.md` - 项目总体说明
- `test/scripts/TEST_RESULTS_2026-04-07.md` - 测试结果示例
