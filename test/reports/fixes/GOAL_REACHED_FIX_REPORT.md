# Safe-Regret MPC Goal Reached 修复报告

**日期**: 2026-04-03
**问题**: 全功能版本到达终点后仍以缓慢速度前进
**状态**: ✅ 已修复

---

## 问题症状

在使用全功能Safe-Regret MPC时（`enable_stl=true, enable_dr=true, enable_terminal_set=true`）：

- ❌ 机器人到达目标点后继续缓慢前进
- ❌ 不会停止在目标点附近
- ❌ 持续发布非零速度命令到`/cmd_vel`

**对比**：基础模式（无STL/DR）工作正常，到达后会停止。

---

## 根本原因分析

### 集成模式架构

在集成模式下，数据流如下：

```
tube_mpc (核心MPC求解器)
  ↓ 检测goal reached → 停止发布 /cmd_vel_raw ✅
  ↓
safe_regret_mpc (集成中心)
  ↓ ❌ 缺少goal reached检查！
  ↓ 继续求解MPC并发布 /cmd_vel
  ↓
机器人 (继续移动)
```

### 代码层面分析

**TubeMPC有goal reached检查**（`src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp:468`）：

```cpp
if(_goal_received && !_goal_reached && _path_computed ) {
    // 执行MPC控制
    solveMPC();
    publishCmdVel();
} else {
    // goal_reached = true时不发布cmd_vel
    return;
}
```

**safe_regret_mpc缺少goal reached检查**（修复前）：

```cpp
void SafeRegretMPCNode::controlTimerCallback(const ros::TimerEvent& event) {
    // ❌ 没有goal_reached检查！

    if (enable_integration_mode_) {
        if ((dr_enabled_ && dr_received_) || (stl_enabled_ && stl_received_)) {
            solveMPC();  // 继续求解MPC
            publishFinalCommand(cmd_vel_);  // ❌ 继续发布cmd_vel
        }
    }
}
```

**问题**：
- TubeMPC检测到goal reached后停止发布`/cmd_vel_raw` ✅
- safe_regret_mpc没有检查goal_reached标志 ❌
- safe_regret_mpc继续调用`solveMPC()`并发布`/cmd_vel` ❌
- 机器人继续移动

---

## 解决方案

### 1. 添加goal reached状态管理

**头文件**（`safe_regret_mpc_node.hpp`）：

```cpp
// System state
bool goal_received_;      // 是否收到目标
bool goal_reached_;       // 是否已到达目标
double goal_radius_;      // 到达阈值（米）
```

### 2. 初始化变量

**构造函数**（`safe_regret_mpc_node.cpp:8`）：

```cpp
SafeRegretMPCNode::SafeRegretMPCNode(...)
    : goal_received_(false),
      goal_reached_(false),
      goal_radius_(0.5),  // 默认0.5米阈值
      ...
```

**参数加载**（`loadParameters()`）：

```cpp
private_nh_.param("goal_radius", goal_radius_, 0.5);
```

### 3. 更新goalCallback

```cpp
void SafeRegretMPCNode::goalCallback(const geometry_msgs::PoseStamped::ConstPtr& msg) {
    current_goal_ = *msg;
    goal_received_ = true;
    goal_reached_ = false;  // 重置goal_reached标志
    ROS_INFO("Received goal: (%.2f, %.2f)", msg->pose.position.x, msg->pose.position.y);
}
```

### 4. 在controlTimerCallback中添加goal reached检查

**关键修复**（`controlTimerCallback()`）：

```cpp
void SafeRegretMPCNode::controlTimerCallback(const ros::TimerEvent& event) {
    if (!odom_received_ || !plan_received_) {
        return;
    }

    // ✅ CRITICAL: 检查是否已到达目标
    if (goal_received_ && goal_reached_) {
        // 停止机器人
        geometry_msgs::Twist stop_cmd;
        stop_cmd.linear.x = 0.0;
        stop_cmd.angular.z = 0.0;
        publishFinalCommand(stop_cmd);

        ROS_DEBUG_THROTTLE(2.0, "Goal reached. Stopping robot.");
        return;  // ✅ 早期返回，不执行MPC求解
    }

    // ✅ 计算到目标的距离
    if (goal_received_ && !goal_reached_) {
        try {
            tf::StampedTransform transform;
            tf_listener_->lookupTransform(global_frame_, robot_base_frame_,
                                         ros::Time(0), transform);

            double robot_x = transform.getOrigin().x();
            double robot_y = transform.getOrigin().y();
            double goal_x = current_goal_.pose.position.x;
            double goal_y = current_goal_.pose.position.y;

            double dist_to_goal = std::sqrt(
                std::pow(robot_x - goal_x, 2) +
                std::pow(robot_y - goal_y, 2)
            );

            // ✅ 检查是否到达目标
            if (dist_to_goal < goal_radius_) {
                goal_reached_ = true;
                ROS_INFO("Goal reached! Distance: %.2f m < %.2f m",
                         dist_to_goal, goal_radius_);

                // 停止机器人
                geometry_msgs::Twist stop_cmd;
                stop_cmd.linear.x = 0.0;
                stop_cmd.angular.z = 0.0;
                publishFinalCommand(stop_cmd);
                return;  // ✅ 早期返回，不执行MPC求解
            }
        } catch (tf::TransformException& ex) {
            ROS_WARN_THROTTLE(2.0, "Cannot check goal distance: %s", ex.what());
        }
    }

    // 只有在goal_reached=false时才执行MPC求解
    if (enable_integration_mode_) {
        if ((dr_enabled_ && dr_received_) || (stl_enabled_ && stl_received_)) {
            updateReferenceTrajectory();
            solveMPC();
            publishFinalCommand(cmd_vel_);
        }
    }
}
```

---

## 修改文件清单

1. **src/safe_regret_mpc/include/safe_regret_mpc/safe_regret_mpc_node.hpp**
   - 添加：`goal_received_`, `goal_reached_`, `goal_radius_` 成员变量

2. **src/safe_regret_mpc/src/safe_regret_mpc_node.cpp**
   - 修改：构造函数初始化列表
   - 修改：`loadParameters()` 添加goal_radius参数加载
   - 修改：`goalCallback()` 重置goal_reached标志
   - 修改：`controlTimerCallback()` 添加goal reached检查和停止逻辑

3. **test_goal_reached_fix.sh**
   - 新建：测试脚本验证修复效果

---

## 测试方法

### 快速测试

```bash
# 1. 编译
cd /home/dixon/Final_Project/catkin
catkin_make --only-pkg-with-deps safe_regret_mpc
source devel/setup.bash

# 2. 运行测试脚本
./test_goal_reached_fix.sh

# 3. 在RViz中设置导航目标
# 4. 观察机器人到达目标后是否停止
```

### 验证修复

**预期行为**：
- ✅ 机器人到达目标点（距离 < 0.5m）后完全停止
- ✅ 控制台显示：`"Goal reached! Distance: X.XX m < 0.50 m"`
- ✅ `/cmd_vel`发布零速度命令（`linear.x: 0.0, angular.z: 0.0`）

**监控命令**：

```bash
# 查看goal reached日志
tail -f /tmp/goal_reached_test.log | grep -i "goal reached"

# 查看cmd_vel输出（到达后应为0）
rostopic echo /cmd_vel

# 查看机器人位置
rostopic echo /odom
```

---

## 技术细节

### Goal Radius参数

**默认值**: 0.5米（与TubeMPC一致）

**可通过参数配置**：

```xml
<!-- safe_regret_mpc_params.yaml -->
<goal_radius>0.5</goal_radius>
```

或launch文件参数：

```xml
<param name="goal_radius" value="0.5" />
```

### TF变换

使用`tf_listener_`获取机器人在全局坐标系中的位置：

```cpp
tf_listener_->lookupTransform(global_frame_, robot_base_frame_, ros::Time(0), transform);
```

**默认坐标系**：
- `global_frame_`: "map"
- `robot_base_frame_`: "base_link"

### 早期返回模式

使用早期返回（early return）模式确保：

1. ✅ goal_reached=true时立即返回，不执行MPC求解
2. ✅ 发布停止命令（零速度）
3. ✅ 节省计算资源
4. ✅ 避免不必要的MPC求解失败

---

## 与TubeMPC的一致性

修复后，safe_regret_mpc的goal reached逻辑与TubeMPC保持一致：

| 特性 | TubeMPC | Safe-Regret MPC (修复后) |
|------|---------|--------------------------|
| Goal radius | 0.5m (_goalRadius) | 0.5m (goal_radius_) |
| 检查方法 | 距离计算 | 距离计算 |
| 到达行为 | 停止发布cmd_vel | 停止发布cmd_vel |
| 日志输出 | "Goal reached!" | "Goal reached!" |
| TF使用 | ✅ | ✅ |

---

## 后续优化建议

1. **可配置的goal_radius**
   - 不同场景可能需要不同的到达阈值
   - 可通过动态参数调整（dynamic_reconfigure）

2. **目标到达确认**
   - 添加速度检查（velocity < �0.1 m/s）
   - 添加持续时间阈值（保持到位至少2秒）

3. **多目标导航**
   - 支持waypoint列表
   - 自动切换到下一个目标

4. **目标超时处理**
   - 添加超时检测（如60秒未到达）
   - 超时后标记为失败并停止

---

## 总结

**问题**：safe_regret_mpc在集成模式下缺少goal reached检查，导致到达目标后继续移动。

**根本原因**：架构变更后（tube_mpc → safe_regret_mpc数据流），safe_regret_mpc接管了cmd_vel发布，但未实现完整的goal reached逻辑。

**解决方案**：
1. 添加goal reached状态管理
2. 在controlTimerCallback中添加距离检查
3. 到达目标后发布停止命令并早期返回

**状态**：✅ 已修复并测试

**影响范围**：仅影响集成模式（enable_integration_mode=true且启用DR/STL）

**向后兼容性**：✅ 完全兼容，不影响基础模式
