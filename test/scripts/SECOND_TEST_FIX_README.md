# 第二次测试无法自动停止问题修复

## 问题描述

**症状**：
- 自动测试脚本在第一次测试运行正常
- 第二次测试开始后无法正确检测目标到达
- 测试卡住无法进入第三次测试

**根本原因**：

`rospy.init_node()` 只能在整个程序中调用一次，但测试脚本在每次测试时都会创建新的 `GoalMonitor` 实例。

**问题链条**：
1. 第一次测试：创建 `GoalMonitor` 实例1 → 初始化ROS节点 → 创建订阅者（绑定实例1的回调）
2. 第二次测试：创建 `GoalMonitor` 实例2 → **跳过**ROS节点初始化（正确） → **跳过**创建订阅者（复用现有的）
3. **问题**：订阅者的回调函数还是指向实例1的回调，而不是实例2的回调
4. **结果**：第二次测试时，odom消息被实例1处理（而不是实例2），导致目标检测失败

## 解决方案

**核心思路**：使用单一的 `GoalMonitor` 实例，每次测试时重置其状态，而不是创建新实例。

### 修改1：添加单一的GoalMonitor实例

**文件**: `test/scripts/run_automated_test.py`

**位置**: `AutomatedTestRunner.__init__()`

```python
def __init__(self, args):
    self.args = args
    # 关键修复：使用单一的GoalMonitor实例，避免回调函数绑定问题
    self.goal_monitor = None
```

### 修改2：添加reset()方法

**文件**: `test/scripts/run_automated_test.py`

**位置**: `GoalMonitor` 类

```python
def reset(self, goals, goal_radius=0.5, timeout=240, launch_process=None):
    """重置监控器状态（用于复用实例）"""
    self.goals = goals
    self.goal_radius = goal_radius
    self.timeout = timeout
    self.launch_process = launch_process

    self.current_goal_index = 0
    self.goals_reached = []
    self.robot_position = None
    self.start_time = None
    self.monitoring = False

    # 重置Metrics
    self.metrics = {
        'test_start_time': None,
        'test_end_time': None,
        'test_status': 'RUNNING',
        'total_goals': len(goals),
        'goals_reached': [],
        'position_history': [],
        'ros_died': False
    }
```

### 修改3：复用GoalMonitor实例

**文件**: `test/scripts/run_automated_test.py`

**位置**: `AutomatedTestRunner.monitor_goals()`

```python
def monitor_goals(self, shelf, test_dir):
    """监控目标到达"""
    pickup_goal = (shelf['x'], shelf['y'], shelf['yaw'])
    delivery_goal = (8.0, 0.0, 0.0)  # 固定卸货点

    goals = [pickup_goal, delivery_goal]

    # 关键修复：复用单一的GoalMonitor实例，避免回调函数绑定问题
    if self.goal_monitor is None:
        # 第一次创建
        TestLogger.info("创建GoalMonitor实例...")
        self.goal_monitor = GoalMonitor(
            goals=goals,
            goal_radius=0.5,
            timeout=self.args.timeout,
            launch_process=self.launch_process
        )
    else:
        # 后续测试：重置现有实例
        TestLogger.info("重置GoalMonitor实例...")
        self.goal_monitor.reset(
            goals=goals,
            goal_radius=0.5,
            timeout=self.args.timeout,
            launch_process=self.launch_process
        )

    # ... 其余代码保持不变
```

## 修复效果

### 修复前

```
测试1:
  - 创建 GoalMonitor 实例1
  - 初始化ROS节点 ✓
  - 创建订阅者（绑定实例1的回调） ✓
  - 检测目标到达 ✓
  - 测试完成 ✓

测试2:
  - 创建 GoalMonitor 实例2
  - 跳过ROS节点初始化 ✓
  - 跳过创建订阅者（复用现有） ✓
  - **问题**：订阅者的回调还是实例1的回调 ❌
  - odom消息被实例1处理 ❌
  - 实例2无法检测目标到达 ❌
  - 测试卡住 ❌
```

### 修复后

```
测试1:
  - 创建 GoalMonitor 实例1（保存在 self.goal_monitor）
  - 初始化ROS节点 ✓
  - 创建订阅者（绑定实例1的回调） ✓
  - 检测目标到达 ✓
  - 测试完成 ✓

测试2:
  - 复用 GoalMonitor 实例1（不创建新实例） ✓
  - 调用 reset() 重置状态 ✓
  - 重新设置 goals, monitoring 等状态 ✓
  - 订阅者的回调还是实例1的回调 ✓（但实例1已经被重置为新测试的状态）
  - 检测目标到达 ✓
  - 测试完成 ✓

测试3, 4, 5...:
  - 同测试2，复用实例并重置 ✓
```

## 关键要点

1. **ROS节点只能初始化一次**：`rospy.init_node()` 在整个程序中只能调用一次
2. **订阅者回调绑定问题**：订阅者在创建时绑定了特定实例的回调函数，后续创建新实例不会更新回调绑定
3. **解决方案**：复用单一的 GoalMonitor 实例，每次测试时重置其状态
4. **状态重置**：`reset()` 方法重置所有状态变量（goals, monitoring, metrics等）

## 测试验证

运行测试验证修复效果：

```bash
cd /home/dixon/Final_Project/catkin
python3 test/scripts/run_automated_test.py --model tube_mpc --shelves 3 --no-viz
```

**预期结果**：
- ✅ 测试1正常完成
- ✅ 测试2正常完成（不再卡住）
- ✅ 测试3正常完成
- ✅ 所有测试都能正确检测目标到达

## 相关文件

- `test/scripts/run_automated_test.py` - 自动化测试脚本（已修改）
- `test/scripts/SECOND_TEST_FIX_README.md` - 本文档

## 修改时间

2026-04-06

## 状态

✅ 修复完成，待测试验证
