# Safe-Regret MPC Metrics收集系统使用指南

## 📊 **系统概述**

基于manuscript Section VI.B.1的Metrics要求，实现了完整的实验数据收集系统。

---

## 📁 **文件结构**

```
src/safe_regret_mpc/
├── include/safe_regret_mpc/
│   └── MetricsCollector.hpp          # Metrics收集器头文件
├── src/
│   └── MetricsCollector.cpp           # Metrics收集器实现
├── msg/
│   └── SafeRegretMetrics.msg          # ROS消息定义
logs/                                 # 日志输出目录（自动创建）
├── metrics_timeseries_YYYYMMDD_HHMMSS.csv    # 时间序列数据
├── metrics_summary_YYYYMMDD_HHMMSS.csv        # 汇总统计
└── metrics_metadata_YYYYMMDD_HHMMSS.json      # 元数据
```

---

## 🎯 **收集的Metrics（符合manuscript要求）**

### **1. 满足度概率 (Satisfaction Probability)**
- `stl_satisfaction_prob`: 满足STL的运行比例
- `stl_satisfaction_ci_lower/upper`: Wilson 95%置信区间

### **2. 经验风险 (Empirical Risk)**
- `empirical_risk`: 观测到的违反率 δ̂
- `target_risk`: 目标风险 δ
- `safety_violation_count`: 违反次数
- `safety_margin_min/mean`: 安全边界统计

### **3. 遗憾 (Regret)**
- `dynamic_regret_cumulative`: 累计动态遗憾 R_T^dyn
- `dynamic_regret_normalized`: 归一化动态遗憾 R_T^dyn/T
- `safe_regret_cumulative`: 累计安全遗憾 R_T^safe
- `safe_regret_normalized`: 归一化安全遗憾 R_T^safe/T

### **4. MPC可行性和计算**
- `feasibility_rate`: 可行性率（应>99.5%）
- `mpc_feasible`: 当前是否可行
- `mpc_solve_time`: 求解时间（毫秒）
- `solve_time_median/p90/mean`: 统计量

### **5. 跟踪误差和Tube占用**
- `tracking_error_norm`: ‖e_t‖_2
- `tube_occupied`: 是否在Tube内
- `tube_violation_count`: Tube违反次数（应为0）

### **6. 校准准确性**
- `calibration_error`: |δ̂ - δ|
- `calibration_ratio`: δ̂ / δ
- `well_calibrated`: 是否|δ̂ - δ| < 0.05

---

## 💻 **使用方法**

### **方法1: 在safe_regret_mpc_node中集成**

在`safe_regret_mpc_node.hpp`中添加：

```cpp
#include "safe_regret_mpc/MetricsCollector.hpp"

class SafeRegretMPCNode {
private:
    std::unique_ptr<MetricsCollector> metrics_collector_;

    // 在控制循环中收集metrics
    void collectMetrics();
};
```

在`safe_regret_mpc_node.cpp`中初始化：

```cpp
SafeRegretMPCNode::SafeRegretMPCNode() {
    // 初始化MetricsCollector
    metrics_collector_ = std::make_unique<MetricsCollector>();
    metrics_collector_->initialize(
        "logistics",              // task_name
        "scenario_001",           // scenario_id
        "safe_regret_mpc",       // method_name
        0.1,                     // target_risk_delta
        20                       // horizon_n
    );
}
```

在`controlTimerCallback()`中收集数据：

```cpp
void SafeRegretMPCNode::controlTimerCallback(...) {
    // ... MPC求解代码 ...

    // 收集metrics
    MetricsCollector::MetricsSnapshot snapshot;
    snapshot.timestamp = ros::Time::now().toSec();
    snapshot.time_step = metrics_collector_->getSnapshotCount();

    // STL metrics
    snapshot.stl_robustness = mpc_solver_->getSTLRobustness();
    snapshot.stl_budget = mpc_solver_->getSTLBudget();

    // Safety metrics
    snapshot.empirical_risk = current_empirical_risk;
    snapshot.safety_margin = current_safety_margin;
    snapshot.safety_violation = (current_safety_margin < 0);

    // Regret metrics
    snapshot.dynamic_regret = computeDynamicRegret();
    snapshot.safe_regret = computeSafeRegret();

    // MPC metrics
    snapshot.mpc_feasible = mpc_feasible;
    snapshot.mpc_solve_time = solve_time_ms;
    snapshot.mpc_objective_value = cost_value;

    // Tracking metrics
    snapshot.tracking_error_norm = std::sqrt(cte*cte + etheta*etheta);
    snapshot.cte = cte;
    snapshot.etheta = etheta;
    snapshot.tube_occupied = (tracking_error_norm < tube_radius);

    // Control metrics
    snapshot.linear_vel = cmd_vel_.linear.x;
    snapshot.angular_vel = cmd_vel_.angular.z;
    snapshot.stage_cost = current_stage_cost;

    // Calibration metrics
    snapshot.calibration_error = std::abs(empirical_risk - target_risk);
    snapshot.well_calibrated = (snapshot.calibration_error < 0.05);

    // Metadata
    snapshot.in_place_rotation = mpc_solver_->isInPlaceRotation();

    // 记录snapshot
    metrics_collector_->recordSnapshot(snapshot);

    // 发布metrics消息
    safe_regret_mpc::SafeRegretMetrics msg = metrics_collector_->getMetricsMessage();
    pub_metrics_.publish(msg);
}
```

实验结束时保存数据：

```cpp
void SafeRegretMPCNode::onExperimentEnd() {
    // 生成时间戳
    std::string timestamp = getCurrentTimestamp();
    std::string logs_dir = "/home/dixon/Final_Project/catkin/logs";

    // 保存时间序列数据
    std::string timeseries_file = logs_dir + "/metrics_timeseries_" + timestamp + ".csv";
    metrics_collector_->saveTimeSeriesCSV(timeseries_file);

    // 保存汇总统计
    std::string summary_file = logs_dir + "/metrics_summary_" + timestamp + ".csv";
    metrics_collector_->saveSummaryCSV(summary_file);

    // 保存元数据
    std::string metadata_file = logs_dir + "/metrics_metadata_" + timestamp + ".json";
    metrics_collector_->saveMetadataJSON(metadata_file);

    ROS_INFO("Metrics saved to: %s", logs_dir.c_str());
}
```

---

### **方法2: 使用独立脚本收集**

创建`scripts/collect_metrics.py`：

```python
#!/usr/bin/env python3
import rospy
from safe_regret_mpc.msg import SafeRegretMetrics
import csv
from datetime import datetime

class MetricsCollector:
    def __init__(self):
        self.metrics_data = []

        # 订阅metrics话题
        rospy.Subscriber('/safe_regret_mpc/metrics',
                        SafeRegretMetrics,
                        self.metrics_callback)

    def metrics_callback(self, msg):
        # 记录metrics数据
        row = {
            'timestamp': msg.header.stamp.to_sec(),
            'time_step': msg.time_step,
            'stl_robustness': msg.stl_robustness,
            'stl_budget': msg.stl_robustness_budget,
            'empirical_risk': msg.empirical_risk,
            'tracking_error_norm': msg.tracking_error_norm,
            'mpc_solve_time': msg.mpc_solve_time,
            'mpc_feasible': msg.mpc_feasible,
            'cmd_linear_vel': msg.cmd_linear_vel,
            'cmd_angular_vel': msg.cmd_angular_vel,
        }
        self.metrics_data.append(row)

    def save_to_csv(self, filepath):
        keys = self.metrics_data[0].keys()
        with open(filepath, 'w') as f:
            writer = csv.DictWriter(f, fieldnames=keys)
            writer.writeheader()
            writer.writerows(self.metrics_data)
        print(f"Saved {len(self.metrics_data)} records to {filepath}")

if __name__ == '__main__':
    rospy.init_node('metrics_collector')
    collector = MetricsCollector()

    # 运行直到节点关闭
    rospy.spin()

    # 保存数据
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filepath = f'/home/dixon/Final_Project/catkin/logs/metrics_collected_{timestamp}.csv'
    collector.save_to_csv(filepath)
```

---

## 📊 **数据格式示例**

### **时间序列CSV (metrics_timeseries_*.csv)**

```csv
timestamp,time_step,stl_robustness,stl_budget,empirical_risk,tracking_error_norm,mpc_solve_time,mpc_feasible,cmd_linear_vel,cmd_angular_vel
0.00,0,-0.523,1.0,0.0,0.034,5.2,1,0.5,0.0
0.05,1,-0.498,0.975,0.0,0.028,4.8,1,0.52,0.1
0.10,2,-0.465,0.95,0.0,0.025,5.1,1,0.55,0.15
...
```

### **汇总统计CSV (metrics_summary_*.csv)**

```csv
metric,value,unit
satisfaction_prob,0.93,ratio
satisfaction_ci_lower,0.82,ratio
satisfaction_ci_upper,0.98,ratio
empirical_risk,0.048,ratio
target_risk,0.1,ratio
dynamic_regret_normalized,0.023,cost_per_step
safe_regret_normalized,0.019,cost_per_step
feasibility_rate,0.998,ratio
solve_time_median,6.8,ms
solve_time_p90,9.2,ms
tracking_error_mean,0.042,m
tube_violations,0,count
well_calibrated_ratio,0.95,ratio
```

### **元数据JSON (metrics_metadata_*.json)**

```json
{
  "task_name": "logistics",
  "scenario_id": "scenario_001",
  "method_name": "safe_regret_mpc",
  "total_steps": 1200,
  "total_time": 60.0,
  "configuration": {
    "horizon_n": 20,
    "target_risk_delta": 0.1
  },
  "results": {
    "satisfaction_prob": 0.93,
    "empirical_risk": 0.048,
    "dynamic_regret_normalized": 0.023,
    "feasibility_rate": 0.998,
    "solve_time_median": 6.8
  }
}
```

---

## 📈 **可视化建议**

### **Python脚本示例**

```python
import pandas as pd
import matplotlib.pyplot as plt

# 读取时间序列数据
df = pd.read_csv('logs/metrics_timeseries_20260404_120000.csv')

# 绘制STL鲁棒性
plt.figure(figsize=(12, 6))
plt.subplot(2, 2, 1)
plt.plot(df['timestamp'], df['stl_robustness'])
plt.axhline(y=0, color='r', linestyle='--', label='Satisfaction threshold')
plt.xlabel('Time (s)')
plt.ylabel('STL Robustness')
plt.title('STL Robustness Over Time')
plt.legend()

# 绘制跟踪误差
plt.subplot(2, 2, 2)
plt.plot(df['timestamp'], df['tracking_error_norm'])
plt.xlabel('Time (s)')
plt.ylabel('Tracking Error (m)')
plt.title('Tracking Error Over Time')

# 绘制MPC求解时间
plt.subplot(2, 2, 3)
plt.plot(df['timestamp'], df['mpc_solve_time'])
plt.axhline(y=8, color='r', linestyle='--', label='Real-time threshold')
plt.xlabel('Time (s)')
plt.ylabel('Solve Time (ms)')
plt.title('MPC Solve Time')
plt.legend()

# 绘制速度
plt.subplot(2, 2, 4)
plt.plot(df['timestamp'], df['cmd_linear_vel'], label='Linear (m/s)')
plt.plot(df['timestamp'], df['cmd_angular_vel'], label='Angular (rad/s)')
plt.xlabel('Time (s)')
plt.ylabel('Velocity')
plt.title('Control Inputs')
plt.legend()

plt.tight_layout()
plt.savefig('logs/metrics_visualization.png', dpi=300)
plt.show()
```

---

## 🧪 **测试验证**

### **单元测试**

创建`test/test_metrics_collector.cpp`：

```cpp
#include <gtest/gtest.h>
#include "safe_regret_mpc/MetricsCollector.hpp"

TEST(MetricsCollector, Initialization) {
    MetricsCollector collector;
    EXPECT_TRUE(collector.initialize("test_task", "scenario_0", "test_method", 0.1, 20));
    EXPECT_EQ(collector.getSnapshotCount(), 0);
}

TEST(MetricsCollector, RecordSnapshot) {
    MetricsCollector collector;
    collector.initialize("test", "s0", "method", 0.1, 20);

    MetricsCollector::MetricsSnapshot snapshot;
    snapshot.timestamp = 0.0;
    snapshot.time_step = 0;
    snapshot.stl_robustness = 1.0;
    snapshot.mpc_feasible = true;

    collector.recordSnapshot(snapshot);
    EXPECT_EQ(collector.getSnapshotCount(), 1);
}

TEST(MetricsCollector, EpisodeSummary) {
    MetricsCollector collector;
    collector.initialize("test", "s0", "method", 0.1, 20);

    // 记录多个snapshot
    for (int i = 0; i < 100; i++) {
        MetricsCollector::MetricsSnapshot snapshot;
        snapshot.timestamp = i * 0.05;
        snapshot.time_step = i;
        snapshot.stl_robustness = (i % 2 == 0) ? 1.0 : -1.0;  // 50% satisfaction
        snapshot.mpc_feasible = true;
        collector.recordSnapshot(snapshot);
    }

    auto summary = collector.getEpisodeSummary();
    EXPECT_EQ(summary.total_steps, 100);
    EXPECT_FLOAT_EQ(summary.satisfaction_prob, 0.5);
    EXPECT_FLOAT_EQ(summary.feasibility_rate, 1.0);
}
```

---

## 📝 **实验协议**

### **仿真实验（30个种子）**

```bash
# 运行实验脚本
for i in {1..30}; do
    echo "Running seed $i..."

    # 设置随机种子
    export RANDOM_SEED=$i

    # 启动实验
    roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
        scenario_id:=seed_$i \
        enable_stl:=true \
        enable_dr:=true &

    LAUNCH_PID=$!

    # 运行60秒
    sleep 60

    # 停止实验
    kill $LAUNCH_PID

    # 保存数据（通过ROS服务）
    rosservice call /safe_regret_mpc/save_metrics \
        "filepath: '/home/dixon/Final_Project/catkin/logs/seed_$i'"
done
```

### **物理实验（3次重复）**

```bash
for i in {1..3}; do
    echo "Physical trial $i..."

    roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
        scenario_id:=physical_trial_$i \
        enable_stl:=true \
        enable_dr:=true
done
```

---

## 🔧 **配置参数**

在`params/metrics_params.yaml`中配置：

```yaml
# Metrics收集配置
metrics:
  enabled: true
  save_on_shutdown: true
  save_interval: 10.0  # 每10秒保存一次

  # 日志路径
  log_dir: "/home/dixon/Final_Project/catkin/logs"

  # 实验信息
  task_name: "logistics"
  scenario_id: "default"
  method_name: "safe_regret_mpc"

  # 目标参数
  target_risk_delta: 0.1
  horizon_n: 20

  # 发布频率
  publish_frequency: 10.0  # Hz
```

---

## 📊 **与Manuscript对应关系**

| Manuscript Section | Metrics字段 | 单位 | 目标值 |
|-------------------|-------------|------|--------|
| VI.B.1 Satisfaction | `stl_satisfaction_prob` | ratio | >0.9 |
| VI.B.1 Empirical Risk | `empirical_risk` | ratio | ≈δ (0.1) |
| VI.B.1 Regret | `dynamic_regret_normalized` | cost/step | o(T) |
| VI.B.1 Feasibility | `feasibility_rate` | ratio | >0.995 |
| VI.B.1 Computation | `solve_time_median` | ms | <8 |
| VI.B.1 Tracking | `tracking_error_mean` | m | 最小化 |
| VI.B.1 Calibration | `calibration_error` | ratio | <0.05 |

---

## ✅ **验证清单**

实验前检查：
- [ ] logs文件夹存在并可写
- [ ] MetricsCollector正确初始化
- [ ] ROS话题正确连接
- [ ] 时间戳正常生成

实验中监控：
- [ ] metrics消息正常发布（10 Hz）
- [ ] 时间序列数据正常增长
- [ ] 无内存泄漏

实验后验证：
- [ ] CSV文件正确生成
- [ ] 数据完整性检查
- [ ] 可视化图表正常

---

## 📚 **相关文档**

- Manuscript Section VI: Experimental Evaluation
- `IN_PLACE_ROTATION_PORT_REPORT.md`: 原地旋转功能报告
- `IMPLEMENTATION_VS_MANUSCRIPT_ANALYSIS.md`: 实现vs手稿对比分析

---

**状态**: ✅ Metrics收集系统已完成实现，待集成测试
