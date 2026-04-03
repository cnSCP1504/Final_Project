# Tube MPC 到达阈值调整报告

**日期**: 2026-04-02
**调整**: 将到达目标阈值从1.0米改为0.5米（严格模式）
**状态**: ✅ 已完成

---

## 调整概述

### 修改前（宽松模式）
- **到达阈值**: 1.0米 (`_goalRadius * 2.0`)
- **优点**: 更容易判定到达，减少等待时间
- **缺点**: 精度较低，机器人可能距离目标还有较大距离就停止

### 修改后（严格模式）
- **到达阈值**: 0.5米 (`_goalRadius`)
- **优点**: 精度更高，确保机器人更接近目标
- **缺点**: 可能需要更多时间调整到精确位置

---

## 修改的文件和位置

### 1. TubeMPCNode.cpp - pathCB函数

**位置**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp` 第330-338行

**修改前**:
```cpp
// 使用更大的阈值（2.0倍goalRadius）来处理接近目标的情况
double arrival_threshold = _goalRadius * 2.0;  // 1.0米
if(dist2goal < arrival_threshold)
{
    _goal_reached = true;
    _path_computed = false;
    ROS_INFO("Goal reached! Path has only %d point(s), distance to goal: %.2f m < %.2f m",
             (int)odom_path.poses.size(), dist2goal, arrival_threshold);
```

**修改后**:
```cpp
// 使用严格的到达阈值（goalRadius）确保精确到达目标
double arrival_threshold = _goalRadius;  // 0.5米（严格模式）
if(dist2goal < arrival_threshold)
{
    _goal_reached = true;
    _path_computed = false;
    ROS_INFO("Goal reached! Path has only %d point(s), distance to goal: %.2f m < %.2f m",
             (int)odom_path.poses.size(), dist2goal, arrival_threshold);
```

### 2. TubeMPCNode.cpp - 控制循环

**位置**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp` 第489-504行

**修改前**:
```cpp
if(N < 2) {
    // 检查是否已经非常接近目标
    const double x_err = goal_pos.x - px;
    const double y_err = goal_pos.y - py;
    const double goal_err = sqrt(x_err*x_err + y_err*y_err);

    if(goal_err < _goalRadius * 2.0) {
        // 非常接近目标，标记为到达
        if(!_goal_reached) {
            _goal_reached = true;
            ROS_INFO("Goal reached! Distance: %.2f m", goal_err);
        }
    }
    return;
}
```

**修改后**:
```cpp
if(N < 2) {
    // 检查是否已经非常接近目标（严格模式：使用goalRadius）
    const double x_err = goal_pos.x - px;
    const double y_err = goal_pos.y - py;
    const double goal_err = sqrt(x_err*x_err + y_err*y_err);

    if(goal_err < _goalRadius) {
        // 非常接近目标，标记为到达
        if(!_goal_reached) {
            _goal_reached = true;
            ROS_INFO("Goal reached! Distance: %.2f m < %.2f m", goal_err, _goalRadius);
        }
    }
    return;
}
```

### 3. auto_nav_goal.py

**位置**: `src/safe_regret_mpc/scripts/auto_nav_goal.py` 第17-21行

**修改前**:
```python
# 当前目标位置
self.current_goal = None
# 从参数服务器读取到达阈值，默认为1.0米
self.goal_radius = rospy.get_param('~goal_radius', 1.0)
rospy.loginfo(f"目标到达阈值设置为: {self.goal_radius} 米")
```

**修改后**:
```python
# 当前目标位置
self.current_goal = None
# 从参数服务器读取到达阈值，默认为0.5米（严格模式）
self.goal_radius = rospy.get_param('~goal_radius', 0.5)
rospy.loginfo(f"目标到达阈值设置为: {self.goal_radius} 米（严格模式）")
```

### 4. test_two_goals.sh

**位置**: `test_two_goals.sh` 第10行和第49行

**修改前**:
```bash
echo "到达阈值: 1.0 米"
...
# 检查是否到达（距离 < 1.0米）
IS_ARRIVED=$(echo "$DIST < 1.0" | bc)
```

**修改后**:
```bash
echo "到达阈值: 0.5 米（严格模式）"
...
# 检查是否到达（距离 < 0.5米，严格模式）
IS_ARRIVED=$(echo "$DIST < 0.5" | bc)
```

---

## 统一的到达阈值

现在系统中所有判断到达目标的位置都使用相同的阈值：

| 位置 | 阈值 | 说明 |
|------|------|------|
| TubeMPC pathCB | `_goalRadius` (0.5m) | 路径点数少时检查 |
| TubeMPC amclCB | `_goalRadius` (0.5m) | AMCL位置回调检查 |
| TubeMPC 控制循环 | `_goalRadius` (0.5m) | 控制循环中检查 |
| auto_nav_goal.py | `goal_radius` (0.5m) | 自动导航脚本检查 |
| test_two_goals.sh | 0.5m | 测试脚本检查 |

**一致性**: ✅ 所有组件使用统一的0.5米到达阈值

---

## 参数配置

### _goalRadius 参数

**位置**: `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`

**当前值**:
```yaml
goal_radius: 0.5  # 米
```

**如果需要调整**:
```yaml
# 更严格：0.3米
goal_radius: 0.3

# 更宽松：0.7米
goal_radius: 0.7
```

**修改后需要重新编译**:
```bash
cd /home/dixon/Final_Project/catkin
catkin_make --only-pkg-with-deps tube_mpc_ros
source devel/setup.bash
```

### 动态调整 auto_nav_goal.py 阈值

**方法1**: 命令行参数
```bash
# 使用0.3米阈值
rosrun safe_regret_mpc auto_nav_goal.py _goal_radius:=0.3

# 使用0.7米阈值
rosrun safe_regret_mpc auto_nav_goal.py _goal_radius:=0.7
```

**方法2**: 修改代码默认值
```python
# 编辑 auto_nav_goal.py 第20行
self.goal_radius = rospy.get_param('~goal_radius', 0.3)  # 改为0.3
```

---

## 性能影响分析

### 到达时间

| 阈值 | 预计到达时间 | 精度 | 适用场景 |
|------|-------------|------|----------|
| 0.3m | 较长 | 最高 | 精密操作、狭窄空间 |
| **0.5m** | **中等** | **高** | **一般导航（推荐）** |
| 1.0m | 较短 | 中等 | 快速导航、大空间 |

### 控制稳定性

**严格模式（0.5m）**:
- ✅ 优点：更精确到达目标位置
- ✅ 优点：适合需要精确定位的任务
- ⚠️ 缺点：可能在目标附近花费更多时间调整
- ⚠️ 缺点：对局部最小值更敏感

**宽松模式（1.0m）**:
- ✅ 优点：更快完成导航任务
- ✅ 优点：减少目标附近的调整时间
- ⚠️ 缺点：精度较低
- ⚠️ 缺点：可能不适合需要精确停靠的任务

---

## 测试验证

### 测试步骤

```bash
# 1. 清理进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 2. 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 运行自动导航（新终端）
rosrun safe_regret_mpc auto_nav_goal.py
```

### 预期日志输出

```
[INFO] 目标到达阈值设置为: 0.5 米（严格模式）
[INFO] 导航目标已发送: 第一个目标 - 位置(3.0, -7.0)
[INFO] 等待机器人到达目标...
[INFO] 正在导航... 已等待 10 秒, 距离目标: 5.23m, 已移动: 2.15m
[INFO] 正在导航... 已等待 20 秒, 距离目标: 3.45m, 已移动: 4.02m
[INFO] 正在导航... 已等待 30 秒, 距离目标: 1.82m, 已移动: 5.67m
[INFO] 正在导航... 已等待 40 秒, 距离目标: 0.95m, 已移动: 6.54m
[INFO] 正在导航... 已等待 50 秒, 距离目标: 0.58m, 已移动: 6.91m
[INFO] 正在导航... 已等待 55 秒, 距离目标: 0.42m, 已移动: 7.07m
[INFO] ✓ 目标已到达！距离: 0.42 m < 0.5 m
[INFO] ✓ 目标已到达!
```

### 关键检查点

- ✅ 机器人能够在距离目标 < 0.5m 时停止
- ✅ TubeMPC日志显示正确的距离和阈值
- ✅ auto_nav_goal.py能够正确检测到达
- ✅ 自动发送下一个目标

---

## 故障排查

### 问题1: 机器人无法在0.5米内停止

**症状**: 机器人在目标附近震荡，无法稳定在0.5米内

**原因**:
- MPC控制参数可能需要调整
- 局部规划器精度限制
- 机器人运动学约束

**解决方案**:
1. 检查MPC参数配置
   ```bash
   # 查看 tube_mpc_params.yaml
   cat src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml | grep -A 5 "mpc_w_cte\|mpc_w_etheta"
   ```

2. 适当放宽阈值到0.7米
   ```bash
   rosrun safe_regret_mpc auto_nav_goal.py _goal_radius:=0.7
   ```

### 问题2: 到达判定不准确

**症状**: 机器人距离目标明显超过0.5米就停止

**原因**:
- odom坐标系与map坐标系存在偏差
- 定位精度问题

**解决方案**:
1. 检查坐标变换
   ```bash
   rosrun tf tf_echo map odom
   ```

2. 使用amcl_pose代替odom（修改auto_nav_goal.py）
   ```python
   self.amcl_sub = rospy.Subscriber('/amcl_pose', PoseWithCovarianceStamped, self.amcl_callback)
   ```

---

## 总结

**修改内容**: 将到达目标阈值从1.0米统一改为0.5米
**影响范围**: TubeMPC控制、自动导航脚本、测试脚本
**测试状态**: ✅ 已编译，待测试验证
**预期效果**: 提高导航精度，确保机器人更接近目标位置

**相关文档**:
- 目标到达修复: `test/docs/GOAL_ARRIVAL_FIX_REPORT.md`
- 自动目标发布修复: `test/docs/AUTO_GOAL_PUBLISHER_FIX_REPORT.md`
- 快速测试指南: `TEST_AUTO_GOAL_README.md`
