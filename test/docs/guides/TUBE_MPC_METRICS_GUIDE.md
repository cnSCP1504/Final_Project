# Tube MPC模式下的Metrics收集指南

## ✅ **可以使用！**

Tube MPC在独立模式下**完全可以使用**Metrics收集功能，只需要使用专门的`TubeMPCMetricsCollector`。

---

## 📊 **Tube MPC模式下可收集的Metrics**

### **✅ 完全可用的Metrics**

| Metric | 来源 | 说明 |
|--------|------|------|
| **跟踪误差** | CTE, eθ | tube_mpc核心功能 |
| **跟踪误差范数** | ‖e_t‖_2 | √(CTE² + eθ²) |
| **MPC求解时间** | tube_mpc内部 | 完全可用 |
| **MPC可行性** | tube_mpc内部 | 完全可用 |
| **Tube占用** | tube_mpc发布 | 完全可用 |
| **Tube违反** | tracking error > radius | 完全可用 |
| **阶段成本** | 可从误差计算 | 完全可用 |
| **控制输入** | /cmd_vel | 完全可用 |
| **原地旋转** | tube_mpc状态 | 完全可用 |

### **⚠️ 使用替代值的Metrics**

| Metric | 替代方案 | 说明 |
|--------|----------|------|
| **STL鲁棒性** | -tracking_error_norm | 跟踪误差越小越好 |
| **安全边界** | distance_to_obstacle | 从costmap获取 |
| **经验风险** | safety_violation / total_steps | 基于违反率 |
| **遗憾** | stage_cost | 使用阶段成本 |

---

## 💻 **集成方法**

### **方法1: 在TubeMPCNode中直接集成**

修改`src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`:

```cpp
// 在头文件中添加
#include "tube_mpc/TubeMPCMetricsCollector.hpp"

class TubeMPCNode {
private:
    std::unique_ptr<tube_mpc::TubeMPCMetricsCollector> metrics_collector_;

    void collectMetrics();
};

// 在构造函数中初始化
TubeMPCNode::TubeMPCNode() {
    // ... 现有代码 ...

    // 初始化Metrics收集器
    metrics_collector_ = std::make_unique<tube_mpc::TubeMPCMetricsCollector>();
    metrics_collector_->initialize(
        "tube_mpc_navigation",     // task_name
        scenario_id,                // 从参数读取
        "tube_mpc"                  // method_name
    );
}

// 在控制循环中收集数据
void TubeMPCNode::collectMetrics() {
    tube_mpc::TubeMPCMetricsCollector::MetricsSnapshot snapshot;

    // 基本信息
    snapshot.timestamp = ros::Time::now().toSec();
    snapshot.time_step = metrics_collector_->getSnapshotCount();

    // 机器人状态
    snapshot.x = current_odom_.pose.pose.position.x;
    snapshot.y = current_odom_.pose.pose.position.y;
    snapshot.theta = tf::getYaw(current_odom_.pose.pose.orientation);

    // 控制输入
    snapshot.linear_vel = _speed;
    snapshot.angular_vel = _w_filtered;

    // 跟踪误差
    snapshot.cte = cte;
    snapshot.etheta = etheta;
    snapshot.tracking_error_norm = std::sqrt(cte*cte + etheta*etheta);

    // Tube状态
    snapshot.tube_radius = _tube_mpc.getTubeRadius();
    snapshot.tube_occupied = (snapshot.tracking_error_norm < snapshot.tube_radius);

    // MPC性能
    snapshot.mpc_feasible = mpc_feasible;
    snapshot.mpc_solve_time = solve_time_ms;
    snapshot.mpc_objective_value = cost_value;

    // 阶段成本
    snapshot.stage_cost = _w_cte * cte * cte +
                        _w_etheta * etheta * etheta +
                        _w_vel * (_speed - _ref_vel) * (_speed - _ref_vel) +
                        _w_angvel * _w_filtered * _w_filtered;

    // 原地旋转状态
    snapshot.in_place_rotation = _in_place_rotation;

    // 替代值（用于STL/DR相关的metrics）
    snapshot.stl_robustness_proxy = -snapshot.tracking_error_norm;  // 误差越小越好
    snapshot.safety_margin_proxy = getDistanceToObstacle();  // 从costmap获取

    // 记录
    metrics_collector_->recordSnapshot(snapshot);

    // 发布ROS消息（兼容SafeRegretMetrics格式）
    safe_regret_mpc::SafeRegretMetrics msg = metrics_collector_->getMetricsMessage();
    pub_metrics_.publish(msg);
}

// 在析构函数或关闭时保存数据
TubeMPCNode::~TubeMPCNode() {
    if (metrics_collector_ && metrics_collector_->getSnapshotCount() > 0) {
        std::string timestamp = getCurrentTimestamp();
        std::string logs_dir = "/home/dixon/Final_Project/catkin/logs";

        // 保存数据
        metrics_collector_->saveTimeSeriesCSV(
            logs_dir + "/tube_mpc_timeseries_" + timestamp + ".csv");
        metrics_collector_->saveSummaryCSV(
            logs_dir + "/tube_mpc_summary_" + timestamp + ".csv");
        metrics_collector_->saveMetadataJSON(
            logs_dir + "/tube_mpc_metadata_" + timestamp + ".json");
    }
}
```

---

### **方法2: 使用独立节点收集**

创建`scripts/tube_mpc_metrics_collector.py`:

```python
#!/usr/bin/env python3
import rospy
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Twist
from safe_regret_mpc.msg import SafeRegretMetrics
import csv
from datetime import datetime

class TubeMPCMetricsCollector:
    def __init__(self):
        self.data = []
        self.start_time = rospy.Time.now()

        # 订阅话题
        rospy.Subscriber('/odom', Odometry, self.odom_callback)
        rospy.Subscriber('/cmd_vel', Twist, self.cmd_callback)
        rospy.Subscriber('/mpc/tracking_error', Float64MultiArray, self.error_callback)

        # 发布metrics（统一格式）
        self.metrics_pub = rospy.Publisher(
            '/tube_mpc/metrics',
            SafeRegretMetrics,
            queue_size=10
        )

    def odom_callback(self, msg):
        # 从odom提取位置和速度
        pass

    def cmd_callback(self, msg):
        # 记录控制输入
        pass

    def error_callback(self, msg):
        # 记录跟踪误差
        pass

    def save_to_csv(self):
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filepath = f'/home/dixon/Final_Project/catkin/logs/tube_mpc_{timestamp}.csv'

        with open(filepath, 'w') as f:
            writer = csv.DictWriter(f, fieldnames=self.data[0].keys())
            writer.writeheader()
            writer.writerows(self.data)

        print(f"Saved {len(self.data)} records to {filepath}")

if __name__ == '__main__':
    rospy.init_node('tube_mpc_metrics_collector')
    collector = TubeMPCMetricsCollector()

    rospy.spin()
    collector.save_to_csv()
```

---

## 📊 **数据格式对比**

### **Tube MPC模式输出**

```csv
timestamp,time_step,cte,etheta,tracking_error_norm,mpc_solve_time,mpc_feasible,...
0.00,0,0.234,0.567,0.612,1,...
0.05,1,0.198,0.523,5.8,1,...
```

### **Safe-Regret MPC模式输出**

```csv
timestamp,time_step,stl_robustness,stl_budget,empirical_risk,tracking_error_norm,...
0.00,0,-0.523,1.0,0.0,0.034,...
0.05,1,-0.498,0.975,0.0,0.028,...
```

**注意**: 两种模式都生成兼容的CSV格式，可以用于相同的分析脚本！

---

## 🎯 **实验对比**

### **Baseline实验（Tube MPC）**

```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch \
    enable_metrics:=true \
    scenario_id:=baseline_tube_mpc
```

**输出**: `tube_mpc_baseline_*.csv`

### **完整Safe-Regret MPC实验**

```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    scenario_id:=full_safe_regret_mpc
```

**输出**: `metrics_timeseries_*.csv`

### **对比分析**

```python
import pandas as pd
import matplotlib.pyplot as plt

# 读取两种模式的数据
tube_mpc_data = pd.read_csv('logs/tube_mpc_summary_*.csv')
safe_regret_data = pd.read_csv('logs/metrics_summary_*.csv')

# 对比跟踪性能
fig, axes = plt.subplots(1, 2, figsize=(12, 5))

# CTE对比
axes[0].bar(['Tube MPC', 'Safe-Regret MPC'],
            [tube_mpc_data['cte_mean'], safe_regret_data['tracking_error_mean']])
axes[0].set_ylabel('Mean Error (m)')
axes[0].set_title('Tracking Performance Comparison')

# 求解时间对比
axes[1].bar(['Tube MPC', 'Safe-Regret MPC'],
            [tube_mpc_data['solve_time_median'], safe_regret_data['solve_time_median']])
axes[1].set_ylabel('Solve Time (ms)')
axes[1].set_title('Computation Comparison')

plt.tight_layout()
plt.savefig('logs/comparison_tube_vs_safe_regret.png')
```

---

## ⚠️ **注意事项**

### **1. STL/DR Metrics的替代值**

在tube_mpc模式下：
- `stl_robustness`: 使用`-tracking_error_norm`作为代理
- `safety_margin`: 使用距离障碍物的距离（从costmap）
- `regret`: 使用阶段成本`stage_cost`

这些是**合理的代理指标**，但与真实的STL/DR约束不同。

### **2. 数据一致性**

确保两种模式下使用相同的：
- 时间戳格式
- CSV文件结构
- Metrics命名约定

这样可以直接使用相同的分析脚本！

### **3. 实验标签**

在CSV的`method_name`字段中明确标识：
- Tube MPC: `"tube_mpc"`
- Safe-Regret MPC: `"safe_regret_mpc"`

便于后续分析和对比。

---

## 📈 **预期结果对比**

基于manuscript的实验设置：

| Metric | Tube MPC | Safe-Regret MPC | 说明 |
|--------|----------|-----------------|------|
| **Tracking Error** | 基线 | 可能更优或更差 | 取决于STL权重 |
| **Solve Time** | 快 (5-10ms) | 较慢 (10-30ms) | DR/STL增加计算量 |
| **Feasibility** | 高 (~99.5%) | 可能较低 | 约束更严格 |
| **STL Satisfaction** | N/A | 可量化 | 仅Safe-Regret有 |
| **Empirical Risk** | 未知 | 可控 | 仅Safe-Regret有 |

---

## ✅ **总结**

### **Tube MPC模式可以 Metrics收集吗？**

**✅ 完全可以！**

使用`TubeMPCMetricsCollector`，可以收集：
- ✅ 所有跟踪和性能metrics
- ✅ 兼容的CSV输出格式
- ✅ 可与Safe-Regret MPC对比的数据

### **主要区别**

| 特性 | Tube MPC | Safe-Regret MPC |
|------|----------|-----------------|
| **STL约束** | ❌ 无 | ✅ 有 |
| **DR约束** | ❌ 无 | ✅ 有 |
| **跟踪Metrics** | ✅ 完整 | ✅ 完整 |
| **计算Metrics** | ✅ 完整 | ✅ 完整 |
| **遗憾分析** | ⚠️ 代理值 | ✅ 真实值 |

### **推荐使用场景**

**Tube MPC模式**：
- 基线对比实验
- 性能基准测试
- 简单导航任务

**Safe-Regret MPC模式**：
- 完整STL任务
- 需要安全保证
- 论文主要实验

两种模式的数据可以**统一格式收集和分析**！
