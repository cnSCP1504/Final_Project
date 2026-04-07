# Manuscript性能指标收集指南

## 概述

自动测试脚本现在收集manuscript中定义的**7类关键性能指标**，这些指标直接对应论文的实验评估要求。

## 支持的性能指标

根据manuscript第1251-1267行的定义，系统收集以下指标：

### 1. Satisfaction Probability (STL满足率)

**定义**: $\widehat{p}_{\mathrm{sat}}$ - 满足STL规格的运行比例

**计算**:
```
p_sat = (steps with ρ(φ) ≥ 0) / total_steps
```

**数据源**: `/stl_monitor/robustness` 话题

**输出指标**:
- `satisfaction_probability`: 满足概率 (0-1)
- `stl_satisfaction_count`: 满足步数
- `stl_total_steps`: 总评估步数

**用途**: 评估系统完成时序逻辑任务的能力

---

### 2. Empirical Risk (经验风险)

**定义**: $\widehat{\delta}$ - 违反安全约束的步骤比例

**计算**:
```
empirical_risk = (steps with h(x) < 0) / total_steps
```

**数据源**:
- `/dr_tightening/margins` - DR安全边界
- `/tube_mpc/tracking_error` - 跟踪误差

**输出指标**:
- `empirical_risk`: 观察到的违反率
- `target_delta`: 目标风险水平 (默认0.05)
- `safety_violation_count`: 安全违反次数

**用途**: 验证distributionally robust chance constraints的有效性

---

### 3. Dynamic & Safe Regret (动态遗憾与安全遗憾)

**定义**:
- $R_T^{\mathrm{dyn}}/T$: 平均动态遗憾
- $R_T^{\mathrm{safe}}/T$: 平均安全遗憾

**计算**:
```
dynamic_regret = J_T(π) - J_T(π^*_dyn)
safe_regret = J_T(π) - min_{π^*∈Π_safe} J_T(π^*)
```

**数据源**: 需要参考策略数据（当前为占位符）

**输出指标**:
- `avg_dynamic_regret`: 平均动态遗憾
- `avg_safe_regret`: 平均安全遗憾

**用途**: 评估学习算法的收敛性和性能保证

---

### 4. Recursive Feasibility Rate (递归可行率)

**定义**: MPC保持可行的步骤比例

**计算**:
```
feasibility_rate = feasible_solves / total_solves
```

**数据源**: `/mpc_solver/feasibility` 话题

**输出指标**:
- `feasibility_rate`: 可行率 (0-1)
- `mpc_feasible_count`: 可行求解次数
- `mpc_infeasible_count`: 不可行求解次数
- `mpc_total_solves`: 总求解尝试次数

**用途**: 验证Tube MPC的递归可行性保证

---

### 5. Computation Metrics (计算性能)

**定义**: MPC求解时间和失败次数

**数据源**: `/mpc_solver/solve_time` 话题

**输出指标**:
- `median_solve_time`: 中位数求解时间 (ms)
- `p90_solve_time`: 90th百分位求解时间 (ms)
- `mean_solve_time`: 平均求解时间 (ms)
- `std_solve_time`: 求解时间标准差 (ms)
- `mpc_failure_count`: 求解失败次数

**用途**: 评估实时性能和计算开销

---

### 6. Tracking Error & Tube Occupancy (跟踪误差与管状占用)

**定义**:
- $\|e_t\|_2$: 误差向量的2-范数
- Tube occupancy: 误差在管状集合$\mathcal{E}$内的比例

**计算**:
```
tracking_error = ||x_t - z_t||_2
tube_occupancy_rate = 1 - (tube_violations / total_steps)
```

**数据源**:
- `/tube_mpc/tracking_error` - 跟踪误差
- `/tube_boundaries` - Tube边界

**输出指标**:
- `mean_tracking_error`: 平均跟踪误差 (m)
- `std_tracking_error`: 跟踪误差标准差 (m)
- `max_tracking_error`: 最大跟踪误差 (m)
- `min_tracking_error`: 最小跟踪误差 (m)
- `tube_occupancy_rate`: Tube占用率 (0-1)
- `tube_violation_count`: Tube违反次数

**用途**: 验证Tube MPC的跟踪性能和鲁棒性

---

### 7. Calibration Accuracy (校准精度)

**定义**: 观察到的违反率与名义风险$\delta$的差异

**计算**:
```
calibration_error = |observed_violation_rate - target_delta|
```

**输出指标**:
- `observed_violation_rate`: 观察到的违反率
- `calibration_error`: 校准误差
- `target_delta`: 目标风险水平

**用途**: 评估distributionally robust约束的校准质量

---

## 使用方法

### 基本测试命令

```bash
# 测试tube_mpc（基线）
./test/scripts/run_automated_test.py --model tube_mpc --shelves 1 --no-viz

# 测试safe_regret（完整系统）
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --no-viz

# 测试多个货架
./test/scripts/run_automated_test.py --model safe_regret --shelves 5 --no-viz
```

### 查看收集的指标

测试完成后，指标保存在：
```
test_results/<timestamp>/metrics.json
```

**Metrics JSON结构**:
```json
{
  "test_start_time": "2026-04-06 18:30:00",
  "test_end_time": "2026-04-06 18:35:00",
  "test_status": "SUCCESS",
  "total_time": 300.5,
  "total_goals": 2,
  "goals_reached": [...],
  "manuscript_metrics": {
    "satisfaction_probability": 0.95,
    "empirical_risk": 0.02,
    "avg_dynamic_regret": 0.001,
    "feasibility_rate": 0.98,
    "median_solve_time": 15.3,
    "p90_solve_time": 25.7,
    "mean_tracking_error": 0.08,
    "tube_occupancy_rate": 0.99,
    "calibration_error": 0.03,
    ...
  },
  "manuscript_raw_data": {
    "stl_robustness_history": [...],
    "tracking_error_norm_history": [...],
    "mpc_solve_times": [...],
    ...
  }
}
```

### 控制台输出

测试过程中会实时打印Manuscript Metrics摘要：

```
======================================================================
📊 MANUSCRIPT性能指标摘要 / MANUSCRIPT PERFORMANCE METRICS SUMMARY
======================================================================

1️⃣  Satisfaction Probability (STL满足率):
   - ρ(φ) ≥ 0: 950/1000 steps
   - 概率: 0.950

2️⃣  Empirical Risk (经验风险):
   - 违反次数: 20/1000
   - 违反率: 0.0200
   - 目标风险 δ: 0.05

3️⃣  Regret (遗憾):
   - Dynamic Regret/T: 0.0010

4️⃣  Recursive Feasibility Rate (递归可行率):
   - 可行/总求解: 980/1000
   - 可行率: 0.980

5️⃣  Computation Metrics (计算性能):
   - 中位数求解时间: 15.30 ms
   - P90求解时间: 25.70 ms
   - 失败次数: 20

6️⃣  Tracking Error & Tube (跟踪误差与管状约束):
   - 平均误差: 0.0800 m
   - 标准差: 0.0300 m
   - 最大误差: 0.1800 m
   - Tube占用率: 0.990
   - Tube违反次数: 10

7️⃣  Calibration Accuracy (校准精度):
   - 观察违反率: 0.0200
   - 目标风险: 0.05
   - 校准误差: 0.0300

======================================================================
```

---

## ROS话题要求

为了收集完整的manuscript指标，系统需要以下ROS话题：

| 话题名称 | 消息类型 | 用途 | 必需 |
|---------|---------|------|------|
| `/stl_monitor/robustness` | `std_msgs/Float64` | STL鲁棒性值 | 可选 |
| `/stl_monitor/budget` | `std_msgs/Float64` | STL预算值 | 可选 |
| `/dr_tightening/margins` | `std_msgs/Float64MultiArray` | DR安全边界 | 可选 |
| `/tube_mpc/tracking_error` | `std_msgs/Float64` | 跟踪误差 | 推荐 |
| `/mpc_solver/solve_time` | `std_msgs/Float64` | MPC求解时间 | 推荐 |
| `/mpc_solver/feasibility` | `std_msgs/Float64` | MPC可行性 | 推荐 |
| `/tube_boundaries` | `geometry_msgs/PolygonStamped` | Tube边界 | 可选 |

**注意**: 如果某些话题不可用，相应的指标将为`None`或默认值。

---

## 数据分析

### 导出数据用于论文图表

```python
import json
import pandas as pd
import numpy as np

# 读取metrics
with open('test_results/20260406_183000/metrics.json', 'r') as f:
    data = json.load(f)

# 提取manuscript指标
metrics = data['manuscript_metrics']

# 创建DataFrame
df = pd.DataFrame([metrics])

# 导出为CSV（用于Excel/图表）
df.to_csv('manuscript_metrics.csv', index=False)
```

### 批量测试对比

```bash
# 运行所有货架的测试
./test/scripts/run_automated_test.py --model tube_mpc --shelves 10 --no-viz
./test/scripts/run_automated_test.py --model safe_regret --shelves 10 --no-viz

# 合并结果
python3 test/scripts/merge_metrics.py test_results/
```

---

## 常见问题

### Q1: 某些指标显示"无数据"

**原因**: 相应的ROS话题未发布或话题名称不匹配

**解决**:
1. 检查话题是否发布: `rostopic list | grep stl_monitor`
2. 检查话题类型: `rostopic info /stl_monitor/robustness`
3. 确认launch文件中启用了相应模块: `enable_stl:=true`

### Q2: 指标值异常（如NaN或Inf）

**原因**: 数据收集不完整或计算时除零

**解决**:
1. 检查原始数据: `metrics['manuscript_raw_data']`
2. 确认测试时间足够长（至少60秒）
3. 查看ROS日志是否有错误

### Q3: 如何添加自定义指标？

**方法**: 修改`ManuscriptMetricsCollector`类：

```python
class ManuscriptMetricsCollector:
    def __init__(self):
        # 添加新的数据字段
        self.data['my_custom_metric'] = []

    def my_callback(self, msg):
        # 处理消息
        self.data['my_custom_metric'].append(msg.data)

    def compute_final_metrics(self):
        # 计算指标
        metrics['my_custom_result'] = np.mean(self.data['my_custom_metric'])
```

---

## 与论文的对应关系

| 论文指标 | 代码字段 | Manuscript行号 |
|---------|---------|---------------|
| $\widehat{p}_{\mathrm{sat}}$ | `satisfaction_probability` | 1253 |
| $\widehat{\delta}$ | `empirical_risk` | 1255 |
| $R_T^{\mathrm{dyn}}/T$ | `avg_dynamic_regret` | 1257 |
| $R_T^{\mathrm{safe}}/T$ | `avg_safe_regret` | 1257 |
| Feasibility rate | `feasibility_rate` | 1259 |
| Median solve time | `median_solve_time` | 1262 |
| P90 solve time | `p90_solve_time` | 1262 |
| $\|e_t\|_2$ | `mean_tracking_error` | 1263 |
| Tube occupancy | `tube_occupancy_rate` | 1264 |
| Calibration error | `calibration_error` | 1266 |

---

## 参考文档

- Manuscript: `latex/manuscript.tex` (Section VI: Experimental Evaluation)
- 测试脚本: `test/scripts/run_automated_test.py`
- Metrics收集器: `ManuscriptMetricsCollector`类

---

**最后更新**: 2026-04-06
**作者**: Claude Code
**版本**: 1.0
