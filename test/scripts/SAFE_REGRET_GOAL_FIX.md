# Safe Regret轨迹修复完成报告

**日期**: 2026-04-09
**问题**: Safe Regret轨迹所有点都在同一位置`[-0.180, 0.000, 0.000]`，导致小车不动

---

## 🚨 根本原因

### 问题发现

调试日志显示Safe Regret发布的轨迹：
```
Safe Regret trajectory (map frame, 20 points):
  Point 0: [-0.180, 0.000, 0.000]
  Point 1: [-0.180, 0.000, 0.000]  ❌ 所有点都在同一位置！
  Point 2: [-0.180, 0.000, 0.000]
  ...
```

### 代码分析

**文件**: `src/safe_regret/src/ReferencePlanner.cpp` (第269-270行)

```cpp
// ❌ 错误代码：生成从当前位置到原点的轨迹
for (int t = 0; t < horizon; ++t) {
    double alpha = static_cast<double>(t) / static_cast<double>(horizon);

    // State: move towards origin (goal)
    pt.state = (1.0 - alpha) * belief.mean;  // ❌ 目标是原点(0,0,0)！

    // Input: proportional control towards goal
    pt.input = -alpha * belief.mean;  // ❌ 向原点移动
}
```

**问题**：
1. Safe Regret没有订阅goal话题
2. 轨迹生成时使用原点(0,0,0)作为目标
3. 但实际目标可能在其他位置（如货架）
4. 结果：生成的轨迹是从机器人当前位置到原点的直线

---

## ✅ 解决方案

### 修改1：添加goal订阅

**文件**: `src/safe_regret/include/safe_regret/SafeRegretNode.hpp`

```cpp
// 订阅者
ros::Subscriber sub_goal_;  // ✅ 新增：订阅 /move_base_simple/goal

// 成员变量
Eigen::VectorXd goal_position_;  // ✅ 目标位置 [x, y, theta]
bool received_goal_;            // ✅ goal接收标志

// 回调函数
void goalCallback(const geometry_msgs::PoseStamped::ConstPtr& msg);  // ✅ 新增
```

### 修改2：实现goal回调

**文件**: `src/safe_regret/src/SafeRegretNode.cpp` (第186-191行)

```cpp
// ✅ 新增：订阅goal
sub_goal_ = nh_.subscribe(
    "/move_base_simple/goal", 1,
    &SafeRegretNode::goalCallback, this);
```

**文件**: `src/safe_regret/src/SafeRegretNode.cpp` (第303-323行)

```cpp
// ✅ 新增：Goal回调函数
void SafeRegretNode::goalCallback(const geometry_msgs::PoseStamped::ConstPtr& msg) {
  // 提取goal位置
  goal_position_ = Eigen::VectorXd(3);
  goal_position_[0] = msg->pose.position.x;
  goal_position_[1] = msg->pose.position.y;
  goal_position_[2] = 0.0;  // goal只关心位置，不关心朝向

  received_goal_ = true;

  // 更新ReferencePlanner的goal
  if (reference_planner_) {
    reference_planner_->setGoal(goal_position_);
  }

  // 日志记录
  static int goal_log_count = 0;
  if (goal_log_count++ < 3) {
    ROS_INFO("🎯 Received goal: [%.3f, %.3f]",
             goal_position_[0], goal_position_[1]);
  }
}
```

### 修改3：添加setGoal方法

**文件**: `src/safe_regret/include/safe_regret/ReferencePlanner.hpp` (第160-168行)

```cpp
/**
 * @brief ✅ 新增：Set goal position for reference trajectory
 * @param goal Goal position [x, y, theta]
 */
void setGoal(const Eigen::VectorXd& goal) { goal_ = goal; has_goal_ = true; }
```

**文件**: `src/safe_regret/include/safe_regret/ReferencePlanner.hpp` (第200-208行)

```cpp
// ✅ 新增：Goal position
Eigen::VectorXd goal_;  ///< Goal position [x, y, theta]
bool has_goal_;         ///< Flag indicating if goal has been set
```

### 修改4：修复轨迹生成逻辑

**文件**: `src/safe_regret/src/ReferencePlanner.cpp` (第94-120行)

```cpp
// ✅ 构造函数初始化goal
ReferencePlanner::ReferencePlanner(...)
  : goal_(Eigen::VectorXd::Zero(state_dim)),  // 初始化goal为零向量
    has_goal_(false)  // 初始化has_goal为false
    ...
```

**文件**: `src/safe_regret/src/ReferencePlanner.cpp` (第257-296行)

```cpp
// ✅ 修复：使用实际目标点而不是原点
ReferenceTrajectory reference;

// ✅ 如果没有设置goal，默认使用当前位置附近（避免原地不动）
Eigen::VectorXd target;
if (has_goal_) {
    target = goal_;  // ✅ 使用实际goal
} else {
    // 没有goal时，生成一个向前2米的虚拟目标
    target = belief.mean;
    target[0] += 2.0;  // 向前2米
    ROS_WARN_THROTTLE(5.0, "No goal set, using virtual target at [%.2f, %.2f]",
                      target[0], target[1]);
}

// ✅ 生成从当前位置到goal的轨迹
for (int t = 0; t < horizon; ++t) {
    double alpha = static_cast<double>(t) / static_cast<double>(horizon);

    // ✅ State: 从当前位置插值到goal
    pt.state = (1.0 - alpha) * belief.mean + alpha * target;

    // ✅ Input: 向goal方向的控制
    pt.input = alpha * (target - belief.mean);

    reference.addPoint(pt);
}
```

### 修改5：添加ROS日志头文件

**文件**: `src/safe_regret/src/ReferencePlanner.cpp` (第1-12行)

```cpp
#include "safe_regret/ReferencePlanner.hpp"
#include "safe_regret/TopologicalAbstraction.hpp"
#include <ros/ros.h>  // ✅ 添加ROS日志支持
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>
```

---

## 📊 预期效果

### 修复前

```
Safe Regret轨迹：所有点都在[-0.180, 0.000, 0.000]
→ Tube MPC认为已经在目标
→ 小车原地不动
```

### 修复后

```
Safe Regret轨迹：从机器人当前位置到goal（如货架位置）
→ Tube MPC沿着轨迹移动
→ 小车正常导航
```

### 预期轨迹示例

假设机器人在`(-8.0, 0.0)`，goal在`(3.0, -7.0)`：

```
Safe Regret trajectory (map frame, 20 points):
  Point 0:  [-8.000, 0.000, 0.000]  ← 起点（机器人位置）
  Point 1:  [-7.474, -0.368, 0.000]  ← 逐渐向目标移动
  Point 2:  [-6.947, -0.737, 0.000]
  ...
  Point 19: [ 3.000, -7.000, 0.000]  ← 终点（goal位置）
```

---

## 🔄 数据流

### 修复后的完整流程

```
1. 用户设置goal (通过RViz或代码)
   ↓
2. move_base发布 /move_base_simple/goal
   ↓
3. Safe Regret接收goal (goalCallback)
   ↓
4. Safe Regret调用 reference_planner_->setGoal(goal_position_)
   ↓
5. Safe Regret生成从机器人位置到goal的轨迹
   ↓
6. Safe Regret发布 /safe_regret/reference_trajectory
   ↓
7. Tube MPC接收轨迹 (safeRegretTrajectoryCB)
   ↓
8. Tube MPC沿着轨迹移动
```

---

## 🧪 测试验证

### 编译状态

```bash
catkin_make --only-pkg-with-deps safe_regret tube_mpc_ros
```

**结果**: ✅ 编译成功

### 测试步骤

```bash
# 清理ROS进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 运行测试
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --timeout 120 --no-viz
```

### 预期日志输出

```
🎯 Received goal: [3.000, -7.000]  ← Safe Regret接收goal
🔍 [DEBUG] Safe Regret trajectory (map frame, 20 points):
   Point 0: [-8.000, 0.000, 0.000]  ← 起点
   Point 1: [-7.474, -0.368, 0.000]  ← 移动中
   ...
🎯 Using Safe Regret trajectory: 20 waypoints  ← Tube MPC使用轨迹
```

---

## 📁 修改的文件

### 头文件 (Headers)

1. `src/safe_regret/include/safe_regret/SafeRegretNode.hpp`
   - 添加`sub_goal_`订阅者
   - 添加`goal_position_`成员变量
   - 添加`received_goal_`标志
   - 添加`goalCallback()`回调声明

2. `src/safe_regret/include/safe_regret/ReferencePlanner.hpp`
   - 添加`setGoal()`方法
   - 添加`goal_`成员变量
   - 添加`has_goal_`标志

### 实现文件 (Sources)

3. `src/safe_regret/src/SafeRegretNode.cpp`
   - 构造函数初始化goal相关变量
   - `setupROSConnections()`添加goal订阅
   - 实现`goalCallback()`回调函数

4. `src/safe_regret/src/ReferencePlanner.cpp`
   - 构造函数初始化goal和has_goal
   - 添加`ros/ros.h`头文件
   - 修改`solveAbstractPlanning()`使用goal而不是原点

---

## 📝 技术要点

### 1. Goal vs Origin

**修复前**：
- 轨迹从机器人位置到**原点(0,0,0)**
- 原点不一定是用户设置的目标

**修复后**：
- 轨迹从机器人位置到**实际goal**
- Goal来自`/move_base_simple/goal`话题

### 2. Fallback机制

如果没有收到goal：
- 生成一个向前2米的虚拟目标
- 避免小车原地不动
- 输出警告日志提醒用户

### 3. 坐标系

- Goal在**map坐标系**
- 轨迹在**map坐标系**生成
- Tube MPC负责转换到odom坐标系

---

## ✅ 完成状态

- ✅ 添加goal订阅
- ✅ 实现goal回调
- ✅ 修复轨迹生成逻辑
- ✅ 添加ROS日志支持
- ✅ 编译成功
- ⏳ 测试验证

---

**生成时间**: 2026-04-09 18:15
**修改者**: Claude Sonnet 4.6
**相关文档**:
- `test/scripts/SAFE_REGRET_TRAJECTORY_PROBLEM.md` - 问题分析
- `test/scripts/SAFE_REGRET_TRAJECTORY_INTEGRATION.md` - 集成实现
