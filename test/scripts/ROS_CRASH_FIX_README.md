# ROS进程崩溃检测修复说明

## 问题描述

测试过程中ROS进程（roslaunch、gazebo等）会神秘崩溃，但测试脚本不知道，导致监控一直运行到超时。

## 根本原因

`GoalMonitor.start_monitoring()` 在监控循环中只检查：
1. 超时（`check_timeout()`）
2. ROS shutdown状态（`rospy.is_shutdown()`）

**但没有检查launch进程是否还活着！**

如果launch进程中途崩溃（内存不足、段错误等），monitor会继续运行直到超时，浪费大量时间。

## 修复方案

### 1. 添加launch进程引用

```python
class GoalMonitor:
    def __init__(self, goals, goal_radius=0.5, timeout=240, launch_process=None):
        self.launch_process = launch_process  # 新增：launch进程引用
        self.metrics['ros_died'] = False  # 新增：ROS死亡标志
```

### 2. 添加进程健康检查

```python
def check_launch_process(self):
    """检查launch进程是否还活着"""
    if not self.launch_process:
        return True

    poll_result = self.launch_process.poll()
    if poll_result is not None:
        # 进程已经死了
        self.metrics['test_status'] = 'ROS_DIED'
        self.metrics['ros_died'] = True
        self.metrics['launch_exit_code'] = poll_result

        print("❌ ROS进程已崩溃！")
        print(f"   Launch进程退出码: {poll_result}")

        self.monitoring = False
        return False

    return True
```

### 3. 在监控循环中检查

```python
def start_monitoring(self):
    rate = rospy.Rate(10)
    while not rospy.is_shutdown() and self.monitoring:
        if self.check_timeout():
            break

        # 关键修复：每次循环都检查launch进程健康
        if not self.check_launch_process():
            print("❌ 检测到ROS进程崩溃，停止监控")
            break

        rate.sleep()
```

### 4. 传递launch进程引用

```python
def monitor_goals(self, shelf, test_dir):
    monitor = GoalMonitor(
        goals=goals,
        goal_radius=0.5,
        timeout=self.args.timeout,
        launch_process=self.launch_process  # 传递引用
    )
```

## 改进效果

### 修复前：
```
时间 0s:   ROS启动成功
时间 30s:  ROS进程崩溃（无人知晓）
时间 60s:  监控继续运行...
时间 120s: 监控继续运行...
时间 240s: ⏰ 测试超时（浪费了210秒）
```

### 修复后：
```
时间 0s:   ROS启动成功
时间 30s:  ❌ 检测到ROS进程崩溃（退出码: 139）
           立即停止监控，报告错误
时间 31s:  清理并开始下一个测试
```

## 退出码含义

常见launch进程退出码：
- **0**: 正常退出
- **1**: 一般错误
- **139**: 段错误（Segmentation fault）- 可能是内存问题或代码bug
- **255**: 异常终止
- **其他**: 具体查看launch.log

## 增强的测试摘要

当检测到ROS崩溃时，测试摘要会包含：

```
❌ ROS进程崩溃详情 / ROS Process Crash Details:
  - 退出码 / Exit Code: 139
  - 运行时间 / Runtime: 28.3秒
  - 已完成目标 / Goals Reached: 1/2

  - Launch日志最后20行 / Last 20 lines of launch.log:
    [INFO] Launching safe_regret_mpc_node...
    [INFO] MPC solver initialized
    ...
    [ERROR] process has died [pid 12345, exit code 139]
```

## 使用方法

无需任何额外操作，修复会自动生效：

```bash
# 运行测试（自动监控ROS健康）
./test/scripts/run_automated_test.py --model safe_regret --num-shelves 5
```

如果ROS中途崩溃，你会立即看到：
```
❌ ROS进程已崩溃！
   Launch进程退出码: 139
   已完成目标: 1/2
   运行时间: 28.3 秒
❌ 检测到ROS进程崩溃，停止监控
```

## 测试验证

运行测试并检查是否正确检测到崩溃：

```bash
# 正常测试
./test/scripts/run_automated_test.py --model tube_mpc --num-shelves 1

# 检查测试摘要
cat test_results/tube_mpc_*/test_01_*/test_summary.txt

# 如果看到"ROS_DIED"状态，说明检测成功
```

## 文件修改

- `test/scripts/run_automated_test.py`:
  - `GoalMonitor.__init__()`: 添加launch_process参数
  - `GoalMonitor.check_launch_process()`: 新增方法
  - `GoalMonitor.start_monitoring()`: 在循环中检查进程健康
  - `monitor_goals()`: 传递launch_process引用
  - `run_single_test()`: 处理ROS_DIED状态
  - `generate_test_summary()`: 增强崩溃报告

## 日期

2026-04-06
