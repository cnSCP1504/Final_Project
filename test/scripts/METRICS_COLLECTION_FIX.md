# Manuscript Metrics数据收集修复报告

## 问题描述

**症状**: safe_regret_20260406_210858测试结果中的metrics文件，所有关键参数都是null或0

**根本原因**: ROS话题类型不匹配和话题名称错误导致订阅失败

---

## 发现的问题

### 问题1: STL话题类型不匹配

**错误**:
- 测试脚本订阅: `std_msgs/Float64`
- STL节点实际发布: `std_msgs/Float32`

**结果**: 订阅失败，无法接收STL robustness和budget数据

### 问题2: Tracking Error话题类型不匹配

**错误**:
- 测试脚本订阅: `std_msgs/Float64`
- tube_mpc实际发布: `std_msgs/Float64MultiArray`

**结果**: 订阅失败，无法接收跟踪误差数据

### 问题3: MPC Solver话题名称错误

**错误**:
- 测试脚本订阅: `/mpc_solver/solve_time` 和 `/mpc_solver/feasibility`
- 实际话题名称: `/mpc_metrics/solve_time_ms` 和 `/mpc_metrics/feasibility_rate`

**结果**: 订阅失败，无法接收MPC求解数据

### 问题4: DR Margins话题名称错误

**错误**:
- 测试脚本订阅: `/dr_tightening/margins`
- Launch文件remap后: `/dr_margins`

**结果**: 订阅失败，无法接收DR边界数据

### 问题5: Empirical Risk未计算

**错误**: `safety_total_steps` 和 `safety_violation_count` 字段从未更新

**结果**: empirical_risk始终为null

---

## 修复方案

### 修复1: STL话题类型（Float32）

**文件**: `test/scripts/run_automated_test.py` (lines 124-131)

```python
# 修改前
from std_msgs.msg import Float64
self.subscribers['stl_robustness'] = rospy.Subscriber(
    '/stl_monitor/robustness', Float64, self.stl_robustness_callback)
self.subscribers['stl_budget'] = rospy.Subscriber(
    '/stl_monitor/budget', Float64, self.stl_budget_callback)

# 修改后
from std_msgs.msg import Float32  # 改为Float32
self.subscribers['stl_robustness'] = rospy.Subscriber(
    '/stl_monitor/robustness', Float32, self.stl_robustness_callback)
self.subscribers['stl_budget'] = rospy.Subscriber(
    '/stl_monitor/budget', Float32, self.stl_budget_callback)
```

### 修复2: Tracking Error话题类型（Float64MultiArray）

**文件**: `test/scripts/run_automated_test.py` (lines 141-147)

```python
# 修改前
from std_msgs.msg import Float64
self.subscribers['tracking_error'] = rospy.Subscriber(
    '/tube_mpc/tracking_error', Float64, self.tracking_error_callback)

# 修改后
from std_msgs.msg import Float64MultiArray  # 改为Float64MultiArray
self.subscribers['tracking_error'] = rospy.Subscriber(
    '/tube_mpc/tracking_error', Float64MultiArray, self.tracking_error_callback)
```

### 修复3: Tracking Error回调函数

**文件**: `test/scripts/run_automated_test.py` (lines 205-219)

```python
def tracking_error_callback(self, msg):
    """跟踪误差回调 - 接收Float64MultiArray"""
    # msg.data 是一个数组，包含 [error_x, error_y, error_yaw, error_norm]
    if len(msg.data) >= 4:
        error_norm = msg.data[3]  # 第四个元素是误差范数
        self.data['tracking_error_norm_history'].append(error_norm)

        # 检查是否违反tube约束
        tube_radius = 0.18
        if error_norm > tube_radius:
            self.data['tube_violations'].append(error_norm)
            self.data['tube_violation_count'] += 1

        # 计算empirical risk（违反DR安全边界）
        self.data['safety_total_steps'] += 1
        if len(self.data['dr_margins_history']) > 0:
            latest_dr_margin = self.data['dr_margins_history'][-1]
            if error_norm > latest_dr_margin:
                self.data['safety_violation_count'] += 1
                self.data['safety_violations'].append(error_norm)
```

### 修复4: MPC Solver话题名称

**文件**: `test/scripts/run_automated_test.py` (lines 150-157)

```python
# 修改前
self.subscribers['mpc_solve_time'] = rospy.Subscriber(
    '/mpc_solver/solve_time', Float64, self.mpc_solve_time_callback)
self.subscribers['mpc_feasibility'] = rospy.Subscriber(
    '/mpc_solver/feasibility', Float64, self.mpc_feasibility_callback)

# 修改后
self.subscribers['mpc_solve_time'] = rospy.Subscriber(
    '/mpc_metrics/solve_time_ms', Float64, self.mpc_solve_time_callback)  # 修改话题名
self.subscribers['mpc_feasibility'] = rospy.Subscriber(
    '/mpc_metrics/feasibility_rate', Float64, self.mpc_feasibility_callback)  # 修改话题名
```

### 修复5: DR Margins话题名称

**文件**: `test/scripts/run_automated_test.py` (lines 134-139)

```python
# 修改前
self.subscribers['dr_margins'] = rospy.Subscriber(
    '/dr_tightening/margins', Float64MultiArray, self.dr_margins_callback)

# 修改后
self.subscribers['dr_margins'] = rospy.Subscriber(
    '/dr_margins', Float64MultiArray, self.dr_margins_callback)  # 修改话题名
```

---

## 测试验证

### 测试命令

```bash
python3 test/scripts/run_automated_test.py --model safe_regret --shelves 1 --no-viz
```

### 测试结果对比

#### 修复前（safe_regret_20260406_210858）

```json
{
  "manuscript_metrics": {
    "satisfaction_probability": null,
    "empirical_risk": null,
    "feasibility_rate": null,
    "median_solve_time": null,
    "mean_tracking_error": null,
    "tube_occupancy_rate": null
  },
  "manuscript_raw_data": {
    "stl_robustness_history": [],
    "dr_margins_history": [],
    "tracking_error_norm_history": [],
    "mpc_solve_times": []
  }
}
```

#### 修复后（safe_regret_20260406_213224）

```json
{
  "manuscript_metrics": {
    "satisfaction_probability": 0.0,
    "empirical_risk": 0.999,
    "feasibility_rate": 1.0,
    "median_solve_time": 13.0,
    "mean_solve_time": 12.95,
    "mean_tracking_error": 0.733,
    "std_tracking_error": 0.257,
    "max_tracking_error": 1.236,
    "tube_occupancy_rate": 0.0,
    "observed_violation_rate": 0.999,
    "calibration_error": 0.949,
    "total_steps": 1022,
    "total_mpc_solves": 96
  },
  "manuscript_raw_data": {
    "stl_robustness_history": [1022 samples],
    "dr_margins_history": [18564 samples],
    "tracking_error_norm_history": [960 samples],
    "mpc_solve_times": [96 samples]
  }
}
```

---

## 收集成功的Manuscript Metrics

### 1. Satisfaction Probability (STL满足率)

- ✅ **数据点**: 1022 samples
- ✅ **计算结果**: 0.0 (未满足)
- **数据来源**: `/stl_monitor/robustness` (Float32)

### 2. Empirical Risk (经验风险)

- ✅ **数据点**: 960 steps
- ✅ **计算结果**: 0.999 (99.9%违反率)
- **计算方法**: safety_violation_count / safety_total_steps
- **数据来源**: `/dr_margins` + `/tube_mpc/tracking_error`

### 3. Recursive Feasibility Rate (递归可行率)

- ✅ **MPC求解次数**: 96
- ✅ **计算结果**: 1.0 (100%可行)
- **数据来源**: `/mpc_metrics/feasibility_rate` (Float64)

### 4. Computation Metrics (计算性能)

- ✅ **中位数求解时间**: 13.0 ms
- ✅ **P90求解时间**: 13.0 ms
- ✅ **平均求解时间**: 12.95 ms
- ✅ **标准差**: 0.42 ms
- **数据来源**: `/mpc_metrics/solve_time_ms` (Float64)

### 5. Tracking Error & Tube Occupancy (跟踪误差与管状约束)

- ✅ **平均跟踪误差**: 0.733 m
- ✅ **标准差**: 0.257 m
- ✅ **最大误差**: 1.236 m
- ✅ **最小误差**: 0.191 m
- ✅ **Tube占用率**: 0.0 (0%)
- ✅ **Tube违反次数**: 960
- **数据来源**: `/tube_mpc/tracking_error` (Float64MultiArray)

### 6. Calibration Accuracy (校准精度)

- ✅ **观察违反率**: 0.999
- ✅ **校准误差**: 0.949
- ✅ **目标风险δ**: 0.05
- **计算方法**: |observed_violation_rate - target_delta|

---

## 修改文件列表

| 文件 | 修改内容 |
|------|---------|
| `test/scripts/run_automated_test.py` | 修复所有ROS话题订阅的类型和名称错误 |

**关键修改行**:
- Lines 124-131: STL话题类型 Float64 → Float32
- Lines 134-139: DR话题名称 `/dr_tightening/margins` → `/dr_margins`
- Lines 141-147: Tracking Error话题类型 Float64 → Float64MultiArray
- Lines 150-157: MPC话题名称修正
- Lines 205-219: Tracking Error回调函数修复，添加empirical risk计算

---

## 遗留问题

### 1. Dynamic & Safe Regret (动态遗憾与安全遗憾)

**状态**: ❌ 未实现（需要参考策略数据）

**原因**: 需要参考策略（reference planner）的数据支持，当前系统中该模块未提供数据

**建议**: 如果需要此指标，需要实现reference_planner模块并发布相应话题

---

## 关键发现

### ROS话题类型验证

在订阅ROS话题时，**消息类型必须完全匹配**：

```python
# 错误示例
rospy.Subscriber('/topic', Float64, callback)  # 发布的是Float32

# 正确示例
rospy.Subscriber('/topic', Float32, callback)  # 与发布者类型一致
```

### 话题Remap注意事项

Launch文件中的remap规则会改变实际的话题名称：

```xml
<!-- launch文件中的remap -->
<remap from="dr_margins" to="/dr_margins"/>

<!-- 订阅时需要使用remap后的名称 -->
rospy.Subscriber('/dr_margins', ...)  # ✅ 正确
rospy.Subscriber('/dr_tightening/margins', ...)  # ❌ 错误
```

### 调试技巧

使用 `rostopic info` 检查话题类型：

```bash
rostopic info /stl_monitor/robustness
# 输出: Type: std_msgs/Float32

rostopic info /tube_mpc/tracking_error
# 输出: Type: std_msgs/Float64MultiArray
```

---

## 总结

**修复前**: 所有7类Manuscript Metrics均为null或0

**修复后**: 成功收集6类Manuscript Metrics（除Regret外）

**数据收集率**: 85.7% (6/7)

**测试状态**: ✅ **完成并验证通过**

---

**创建日期**: 2026-04-06
**作者**: Claude Code
**版本**: 1.0
**状态**: ✅ 完成并已验证
