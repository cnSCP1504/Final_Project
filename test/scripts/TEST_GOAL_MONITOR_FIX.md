# 自动测试脚本目标监控修复

## 问题描述

**症状**: 自动测试脚本在第二次测试开始时无法正确接收到达目标点，导致第二次测试卡住无法进入到第三次测试

**根本原因**: `rospy.init_node()` 只能在整个程序中调用一次，但每次测试都会创建新的 `GoalMonitor` 并调用 `start_monitoring()`，导致第二次测试时：
1. ROS节点重复初始化错误
2. 订阅者回调函数绑定到错误的实例
3. odom回调无法正确更新机器人位置
4. 目标到达检测失效

---

## 修复方案

### 核心原则

1. **ROS节点只初始化一次**: 使用类变量跟踪是否已初始化
2. **订阅者每次重新创建**: 每次测试创建新的订阅者并绑定到当前实例
3. **监控器实例复用**: 第一次创建，后续测试重置状态

### 代码修改

#### 1. GoalMonitor类变量

**文件**: `test/scripts/run_automated_test.py` (lines 530-533)

```python
class GoalMonitor:
    """目标到达监控器（集成Manuscript Metrics收集）"""

    # 类变量：跟踪ROS节点是否已初始化（避免重复初始化）
    _node_initialized = False
```

**作用**: 确保ROS节点只初始化一次

#### 2. 实例变量odom订阅者

**文件**: `test/scripts/run_automated_test.py` (lines 547-548)

```python
def __init__(self, goals, goal_radius=0.5, timeout=240, launch_process=None):
    # ...

    # 实例变量：odom订阅者（每次测试创建新的）
    self.odom_subscriber = None
```

**作用**: 每个测试实例拥有独立的订阅者

#### 3. start_monitoring()方法

**文件**: `test/scripts/run_automated_test.py` (lines 788-807)

```python
def start_monitoring(self):
    """开始监控"""

    # 关键修复：只在第一次调用时初始化ROS节点
    if not GoalMonitor._node_initialized:
        print("📡 初始化ROS节点...")
        rospy.init_node('goal_monitor', anonymous=True)
        GoalMonitor._node_initialized = True
        print("✓ ROS节点已初始化")
    else:
        print("✓ ROS节点已存在，跳过初始化")

    # 关键修复：每次测试都创建新的订阅者（修复回调绑定问题）
    # 先注销旧的订阅者（如果存在）
    if self.odom_subscriber is not None:
        print("📡 注销旧的odom订阅者...")
        self.odom_subscriber.unregister()
        self.odom_subscriber = None

    # 创建新的订阅者（绑定到当前实例的回调）
    print("📡 创建新的odom订阅者...")
    self.odom_subscriber = rospy.Subscriber('/odom', Odometry, self.odom_callback)
    print("✓ 订阅者已创建（绑定到当前实例）")
```

**关键点**:
1. ROS节点初始化检查（避免重复初始化）
2. 注销旧订阅者（防止泄漏）
3. 创建新订阅者并绑定到当前实例的回调

#### 4. reset()方法

**文件**: `test/scripts/run_automated_test.py` (lines 578-581)

```python
def reset(self, goals, goal_radius=0.5, timeout=240, launch_process=None):
    """重置监控器状态（用于复用实例）"""

    # 关键修复：注销旧的订阅者
    if self.odom_subscriber is not None:
        self.odom_subscriber.unregister()
        self.odom_subscriber = None

    # 重置Manuscript Metrics收集器
    self.manuscript_metrics.reset()
    self.manuscript_metrics.shutdown_subscribers()

    # 重置状态变量
    self.current_goal_index = 0
    self.goals_reached = []
    # ...
```

**作用**: 在复用实例前清理所有状态和订阅

#### 5. monitor_goals()方法

**文件**: `test/scripts/run_automated_test.py` (lines 974-992)

```python
def monitor_goals(self, shelf, test_dir):
    """监控目标到达"""

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
```

**关键点**:
1. 第一次测试：创建新实例
2. 后续测试：重置现有实例（不创建新的）

---

## 修复效果对比

### 修复前

```
测试 1:
  - 创建GoalMonitor实例1
  - 初始化ROS节点 ✓
  - 创建订阅者1 → 绑定到实例1的回调 ✓
  - 正常工作 ✓

测试 2:
  - 创建GoalMonitor实例2
  - 尝试初始化ROS节点 ✗ (失败！已初始化)
  - 创建订阅者2 → 绑定到实例2的回调
  - 订阅者2的回调更新实例2的位置
  - 但监控循环检查的是实例1的位置 ✗
  - 目标到达检测失效 ✗
```

### 修复后

```
测试 1:
  - 创建GoalMonitor实例
  - 初始化ROS节点 ✓ (仅此一次)
  - 创建订阅者 → 绑定到实例的回调 ✓
  - 正常工作 ✓

测试 2:
  - 重置GoalMonitor实例 (复用同一个)
  - 跳过ROS节点初始化 ✓ (已存在)
  - 注销旧订阅者 ✓
  - 创建新订阅者 → 绑定到同一实例的回调 ✓
  - 正常工作 ✓
```

---

## 技术要点

### 1. ROS节点初始化限制

**问题**: `rospy.init_node()` 只能调用一次

**解决方案**:
- 使用类变量 `_node_initialized` 跟踪状态
- 第一次调用时初始化，后续跳过

### 2. Python回调绑定机制

**问题**: 订阅者创建时绑定的回调函数引用

```python
# 错误做法（每次创建新实例）
monitor1 = GoalMonitor(...)  # 订阅者回调 → monitor1.odom_callback
monitor2 = GoalMonitor(...)  # 订阅者回调 → monitor2.odom_callback
# 但监控循环检查的是 monitor1.robot_position

# 正确做法（复用实例）
monitor = GoalMonitor(...)   # 测试1
monitor.reset(...)           # 测试2：重置后复用
# 订阅者回调始终绑定到同一个monitor实例
```

**解决方案**:
- 复用单一的GoalMonitor实例
- 每次测试调用 `reset()` 清理状态
- 每次测试创建新的订阅者绑定到复用的实例

### 3. 订阅者生命周期管理

**问题**: ROS订阅者如果不注销会泄漏

**解决方案**:
```python
# 创建新订阅者前先注销旧的
if self.odom_subscriber is not None:
    self.odom_subscriber.unregister()
    self.odom_subscriber = None

# 创建新订阅者
self.odom_subscriber = rospy.Subscriber(...)
```

---

## 测试验证

### 验证步骤

```bash
# 运行多个货架的自动化测试
./test/scripts/run_automated_test.py --model tube_mpc --shelves 3 --no-viz
```

### 预期行为

**测试1 (shelf_1_1)**:
```
✓ 创建GoalMonitor实例
📡 初始化ROS节点...
✓ ROS节点已初始化
📡 创建新的odom订阅者...
✓ 订阅者已创建（绑定到当前实例）
✅ 目标 1/2 已到达!
✅ 目标 2/2 已到达!
🎉 所有目标已完成！
```

**测试2 (shelf_1_2)**:
```
✓ 重置GoalMonitor实例
✓ ROS节点已存在，跳过初始化
📡 注销旧的odom订阅者...
📡 创建新的odom订阅者...
✓ 订阅者已创建（绑定到当前实例）
✅ 目标 1/2 已到达!
✅ 目标 2/2 已到达!
🎉 所有目标已完成！
```

**测试3 (shelf_1_3)**:
```
✓ 重置GoalMonitor实例
✓ ROS节点已存在，跳过初始化
📡 注销旧的odom订阅者...
📡 创建新的odom订阅者...
✓ 订阅者已创建（绑定到当前实例）
✅ 目标 1/2 已到达!
✅ 目标 2/2 已到达!
🎉 所有目标已完成！
```

### 成功标志

- ✅ 所有测试都能正确检测目标到达
- ✅ 无"init_node called twice"错误
- ✅ 机器人位置实时更新
- ✅ 测试能够正常进入下一轮

---

## 修改文件列表

| 文件 | 修改内容 |
|------|---------|
| `test/scripts/run_automated_test.py` | 修复目标监控器的ROS节点和订阅者管理 |

**关键修改行**:
- Lines 530-533: 添加类变量 `_node_initialized`
- Lines 547-548: 添加实例变量 `odom_subscriber`
- Lines 565-596: 实现 `reset()` 方法
- Lines 788-807: 修改 `start_monitoring()` 方法
- Lines 974-992: 修改 `monitor_goals()` 方法

---

## 相关问题修复

此修复同时解决了以下相关问题：

### 1. 回调函数绑定错乱
- **问题**: 第二次测试的回调更新错误实例
- **解决**: 复用单一实例，确保回调绑定正确

### 2. 机器人位置不更新
- **问题**: odom回调数据未写入监控实例
- **解决**: 订阅者正确绑定到监控实例

### 3. 目标到达检测失效
- **问题**: 检查的是未更新的位置数据
- **解决**: 回调正确更新位置数据

---

## 总结

**核心修复**: 复用单一GoalMonitor实例 + ROS节点只初始化一次 + 每次测试创建新订阅者

**关键改进**:
1. ✅ 避免ROS节点重复初始化
2. ✅ 确保回调函数绑定到正确实例
3. ✅ 正确管理订阅者生命周期
4. ✅ 支持无限次数的连续测试

**测试状态**: ✅ 修复完成并验证通过

---

**创建日期**: 2026-04-06
**作者**: Claude Code
**版本**: 1.0
**状态**: ✅ 完成并已验证
