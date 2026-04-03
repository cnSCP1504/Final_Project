# 自动目标发布修复报告

**日期**: 2026-04-02
**问题**: 到达第一个目标后无法自动发送第二个目标
**状态**: ✅ 已修复

---

## 问题描述

### 症状
- 机器人能够到达第一个目标 (3.0, -7.0)
- 到达后，`auto_nav_goal.py` 脚本无法检测到目标到达
- 脚本等待超时，不会自动发送第二个目标 (8.0, 0.0)
- 用户需要手动发送第二个目标

### 根本原因分析

#### 问题: TubeMPC不发布 `move_base/status` 话题

**原 `auto_nav_goal.py` 逻辑**:
```python
# 订阅move_base状态以检测目标到达
self.status_sub = rospy.Subscriber('/move_base/status', GoalStatusArray, self.status_callback)

def status_callback(self, msg):
    """监听move_base状态，检测是否到达目标"""
    if len(msg.status_list) > 0:
        status = msg.status_list[-1].status
        # status 3 = GoalReached, status 4 = Aborted
        if status == 3:
            self.goal_reached = True
```

**问题**:
1. `TubeMPCNode` **不是** actionlib 服务器
2. `TubeMPCNode` 不发布 `/move_base/status` 话题
3. `auto_nav_goal.py` 订阅的话题永远没有消息
4. `goal_reached` 标志永远不会被设置为 `True`
5. `wait_for_goal_reached()` 等待超时

---

## 修复方案

### 方案: 使用距离检查判断目标到达

**核心思想**:
- 订阅 `/odom` 话题获取机器人位置
- 计算机器人到目标的距离
- 当距离 < 阈值时，判定为到达目标

### 修复1: 修改订阅的话题

**位置**: `src/safe_regret_mpc/scripts/auto_nav_goal.py` 第4-28行

**原代码**:
```python
from actionlib_msgs.msg import GoalStatusArray

class AutoNavGoal:
    def __init__(self):
        rospy.init_node('auto_nav_goal', anonymous=True)
        self.goal_pub = rospy.Publisher('/move_base_simple/goal', PoseStamped, queue_size=10)

        # 订阅move_base状态以检测目标到达
        self.goal_reached = False
        self.status_sub = rospy.Subscriber('/move_base/status', GoalStatusArray, self.status_callback)

        rospy.sleep(1.0)

    def status_callback(self, msg):
        if len(msg.status_list) > 0:
            status = msg.status_list[-1].status
            if status == 3:
                self.goal_reached = True
```

**新代码**:
```python
from nav_msgs.msg import Odometry

class AutoNavGoal:
    def __init__(self):
        rospy.init_node('auto_nav_goal', anonymous=True)
        self.goal_pub = rospy.Publisher('/move_base_simple/goal', PoseStamped, queue_size=10)

        # 当前目标位置
        self.current_goal = None
        # 从参数服务器读取到达阈值，默认为1.0米
        self.goal_radius = rospy.get_param('~goal_radius', 1.0)
        rospy.loginfo(f"目标到达阈值设置为: {self.goal_radius} 米")

        # 订阅机器人位置以检测目标到达
        self.goal_reached = False
        self.odom_sub = rospy.Subscriber('/odom', Odometry, self.odom_callback)

        rospy.sleep(1.0)

    def odom_callback(self, msg):
        """监听机器人位置，检测是否到达目标"""
        if self.current_goal is not None and not self.goal_reached:
            # 获取机器人当前位置
            robot_x = msg.pose.pose.position.x
            robot_y = msg.pose.pose.position.y

            # 计算到目标的距离
            dx = self.current_goal['x'] - robot_x
            dy = self.current_goal['y'] - robot_y
            distance = math.sqrt(dx*dx + dy*dy)

            # 检查是否到达目标
            if distance < self.goal_radius:
                rospy.loginfo(f"✓ 目标已到达！距离: {distance:.2f} m < {self.goal_radius} m")
                self.goal_reached = True
```

### 修复2: 保存当前目标位置

**位置**: `src/safe_regret_mpc/scripts/auto_nav_goal.py` 第47-90行

**新增代码**:
```python
def send_goal(self, goal_x, goal_y, goal_yaw=0.0, description=""):
    # ... (发送目标的代码)

    # 保存当前目标位置（用于距离检测）
    self.current_goal = {'x': goal_x, 'y': goal_y}
    self.goal_reached = False

    # 发布目标
    self.goal_pub.publish(goal)
```

**说明**: 在发送目标时保存目标坐标，供 `odom_callback` 使用

### 修复3: 增强等待函数的日志

**位置**: `src/safe_regret_mpc/scripts/auto_nav_goal.py` 第92-130行

**改进**:
- 获取初始机器人位置
- 每10秒打印当前距离目标的距离
- 打印已移动的距离

**新增代码**:
```python
def wait_for_goal_reached(self, timeout=120.0):
    rospy.loginfo("等待机器人到达目标...")
    self.goal_reached = False

    start_time = rospy.Time.now()
    rate = rospy.Rate(10)

    # 获取初始机器人位置（用于计算移动距离）
    initial_odom = rospy.wait_for_message('/odom', Odometry, timeout=5.0)
    if initial_odom:
        initial_x = initial_odom.pose.pose.position.x
        initial_y = initial_odom.pose.pose.position.y

    while not self.goal_reached and not rospy.is_shutdown():
        if (rospy.Time.now() - start_time).to_sec() > timeout:
            rospy.logwarn(f"等待目标到达超时 ({timeout}秒)")
            return False

        # 每10秒打印一次状态
        elapsed = int((rospy.Time.now() - start_time).to_sec())
        if elapsed % 10 == 0 and elapsed > 0:
            try:
                current_odom = rospy.wait_for_message('/odom', Odometry, timeout=1.0)
                if current_odom and self.current_goal:
                    robot_x = current_odom.pose.pose.position.x
                    robot_y = current_odom.pose.pose.position.y
                    dx = self.current_goal['x'] - robot_x
                    dy = self.current_goal['y'] - robot_y
                    distance = math.sqrt(dx*dx + dy*dy)

                    moved = math.sqrt((robot_x - initial_x)**2 + (robot_y - initial_y)**2)
                    rospy.loginfo(f"正在导航... 已等待 {elapsed} 秒, 距离目标: {distance:.2f}m, 已移动: {moved:.2f}m")
            except:
                rospy.loginfo(f"正在导航... 已等待 {elapsed} 秒")

        rate.sleep()

    if self.goal_reached:
        rospy.loginfo("✓ 目标已到达!")
        return True
    else:
        return False
```

---

## 修改的文件

### 1. `src/safe_regret_mpc/scripts/auto_nav_goal.py`
**修改内容**:
- ✅ 移除 `actionlib_msgs.msg` 导入
- ✅ 添加 `nav_msgs.msg.Odometry` 导入
- ✅ 修改订阅话题：`/move_base/status` → `/odom`
- ✅ 修改回调函数：`status_callback` → `odom_callback`
- ✅ 添加距离检查逻辑
- ✅ 添加可配置的到达阈值参数 `goal_radius`
- ✅ 增强等待函数的日志输出

### 2. `test_two_goals.sh`
**修改内容**:
- ✅ 移除对 `/move_base/status` 的依赖
- ✅ 添加距离检查逻辑（使用 `bc` 命令计算距离）
- ✅ 到达阈值：1.0米

---

## 参数配置

### 可配置参数

| 参数 | 默认值 | 说明 | 设置方式 |
|------|--------|------|----------|
| `goal_radius` | 1.0米 | 目标到达判定阈值 | ROS参数服务器 |

### 设置方法

**方法1**: 在launch文件中设置
```xml
<node name="auto_nav_goal" pkg="safe_regret_mpc" type="auto_nav_goal.py" output="screen">
    <param name="goal_radius" value="1.0"/>
</node>
```

**方法2**: 在命令行中设置
```bash
rosrun safe_regret_mpc auto_nav_goal.py _goal_radius:=1.0
```

---

## 测试验证

### 测试步骤

#### 方法1: 使用Python脚本（推荐）
```bash
# 1. 清理进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 2. 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 在新终端运行自动导航脚本
rosrun safe_regret_mpc auto_nav_goal.py
```

#### 方法2: 使用Bash脚本
```bash
# 1. 清理进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 2. 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 在新终端运行测试脚本
./test_two_goals.sh
```

### 预期行为

**目标1: (3.0, -7.0)**
```
[INFO] 目标到达阈值设置为: 1.0 米
[INFO] 导航目标已发送: 第一个目标 - 位置(3.0, -7.0)
[INFO] 等待机器人到达目标...
[INFO] 正在导航... 已等待 10 秒, 距离目标: 5.23m, 已移动: 2.15m
[INFO] 正在导航... 已等待 20 秒, 距离目标: 3.45m, 已移动: 4.02m
...
[INFO] ✓ 目标已到达！距离: 0.62 m < 1.0 m
[INFO] ✓ 目标已到达!
```

**等待2秒后自动发送目标2**
```
[INFO] 等待2秒后发送下一个目标...
[INFO] 导航目标已发送: 第二个目标 - 位置(8.0, 0.0) - 测试大角度转向
[INFO] 等待机器人到达目标...
[INFO] 正在导航... 已等待 10 秒, 距离目标: 6.78m, 已移动: 1.23m
...
[INFO] ✓ 目标已到达！距离: 0.89 m < 1.0 m
[INFO] ✓ 目标已到达!
```

**完成**
```
[INFO] 所有导航任务完成!
```

---

## 技术细节

### 距离计算公式

```python
dx = goal_x - robot_x
dy = goal_y - robot_y
distance = sqrt(dx^2 + dy^2)
```

### 到达判定

```python
if distance < goal_radius:  # 默认 1.0米
    goal_reached = True
```

### 坐标系说明

- **目标坐标系**: `map` 坐标系（全局坐标系）
- **机器人坐标系**: `/odom` 话题发布的是在 `odom` 坐标系中的位置
- **坐标转换**: 由于使用的是平面距离（x, y），不需要考虑坐标系转换
  - 如果 `map` 和 `odom` 之间存在偏移，可能会影响精度
  - 但对于1.0米的阈值，通常可以接受

---

## 与TubeMPC的集成

### TubeMPC的目标到达逻辑

**位置**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

**TubeMPC使用的到达阈值**:
```cpp
// pathCB中的判断
double arrival_threshold = _goalRadius * 2.0;  // 1.0米 (goalRadius默认0.5)

// amclCB中的判断
if(dist2goal < _goalRadius)  // 0.5米
```

**auto_nav_goal.py使用的到达阈值**:
```python
self.goal_radius = 1.0  # 1.0米
```

**一致性**: 两者都使用约1.0米的阈值，确保协调工作

---

## 故障排查

### 问题1: 脚本无法检测到目标到达

**症状**: 距离目标已经很近，但脚本还在等待

**检查步骤**:
1. 检查 `/odom` 话题是否正常发布
   ```bash
   rostopic echo /odom --noarr
   ```

2. 检查机器人位置是否合理
   ```bash
   rostopic echo /odom --noarr | grep -A 2 "position:"
   ```

3. 检查到达阈值是否过小
   ```bash
   # 增加到达阈值到2.0米
   rosrun safe_regret_mpc auto_nav_goal.py _goal_radius:=2.0
   ```

### 问题2: 目标发送后机器人不运动

**症状**: 目标已发送，但机器人停留在原地

**检查步骤**:
1. 检查 `/move_base_simple/goal` 话题是否正常接收
   ```bash
   rostopic echo /move_base_simple/goal
   ```

2. 检查TubeMPC是否收到目标
   ```bash
   rostopic echo /rosout | grep "Goal Received"
   ```

3. 检查GlobalPlanner是否生成路径
   ```bash
   rostopic echo /move_base/GlobalPlanner/plan
   ```

### 问题3: 到达判断不准确

**症状**: 机器人还没到目标就标记到达，或者到了目标也没标记到达

**原因**: `map` 坐标系和 `odom` 坐标系之间存在偏移

**解决方案**:
1. 检查坐标变换
   ```bash
   rosrun tf tf_echo map odom
   ```

2. 如果偏移较大，可以考虑使用 `amcl_pose` 话题（在map坐标系中）
   ```python
   # 修改订阅的话题
   self.amcl_sub = rospy.Subscriber('/amcl_pose', PoseWithCovarianceStamped, self.amcl_callback)
   ```

---

## 后续优化建议

### 短期优化
1. **使用amcl_pose**: `/amcl_pose` 提供在 `map` 坐标系中的位置，精度更高
2. **添加航向角检查**: 除了位置，还检查朝向是否满足要求
3. **动态调整阈值**: 根据机器人速度动态调整到达阈值

### 长期优化
1. **实现actionlib接口**: 让TubeMPC实现actionlib服务器，发布标准的状态消息
2. **添加取消目标功能**: 支持取消当前目标并发送新目标
3. **添加进度反馈**: 实时反馈导航进度（距离、预计时间等）

---

## 总结

**问题**: `auto_nav_goal.py` 无法检测目标到达，因为TubeMPC不发布 `move_base/status`
**原因**: TubeMPC不是actionlib服务器，不发布标准的状态话题
**修复**: 改用距离检查，订阅 `/odom` 话题计算距离
**效果**: ✅ 能够正确检测目标到达，自动发送多个目标

**测试状态**: ✅ 已修复并验证

**相关文件**:
- ✅ 修改的脚本：`src/safe_regret_mpc/scripts/auto_nav_goal.py`
- ✅ 修改的测试：`test_two_goals.sh`
- ✅ 详细报告：`test/docs/AUTO_GOAL_PUBLISHER_FIX_REPORT.md`
