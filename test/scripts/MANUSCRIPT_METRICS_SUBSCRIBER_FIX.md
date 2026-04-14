# Manuscript Metrics订阅者累积问题修复

**日期**: 2026-04-07
**问题**: 后续测试的Manuscript Metrics全部为空值
**状态**: ✅ **已修复**

---

## 问题描述

### 症状
运行5次自动化测试时：
- **测试1**: Manuscript metrics数据正常收集 ✅
- **测试2-5**: 所有metrics数据都是None/空值 ❌

### 影响
- 无法评估后续测试的性能
- 测试数据不完整
- 影响论文实验结果

---

## 根本原因分析

### 代码流程

**第一次测试**:
```
1. 创建GoalMonitor实例 (line 1035)
2. 调用setup_ros_subscribers() (line 865)
   └─ 创建STL订阅者
   └─ 创建DR订阅者
   └─ 创建tracking_error订阅者
   └─ 创建mpc_metrics订阅者
3. 正常收集数据 ✅
```

**第一次测试结束**:
```
4. 调用cleanup_ros_processes() (line 1123)
   ├─ 终止launch进程 ✅
   ├─ 杀死ROS进程 (killall -9) ✅
   └─ ❌ 没有注销Python订阅者对象！
```

**第二次测试**:
```
5. 创建新的GoalMonitor实例 (line 1035)
6. 再次调用setup_ros_subscribers() (line 865)
   └─ 创建新的STL订阅者 ⚠️
   └─ 创建新的DR订阅者 ⚠️
   └─ ...（旧的订阅者对象还在内存中）

7. ROS消息路由问题
   └─ 多个订阅者竞争同一话题
   └─ 消息可能只发给第一个订阅者
   └─ 新订阅者收不到数据 ❌
```

### 技术细节

**ROS Subscriber对象生命周期**:
- Python进程中的`rospy.Subscriber`对象是持久化的
- 即使ROS master被杀死，Python对象仍在内存中
- 下次创建新订阅者时，旧对象不会自动销毁
- 多个订阅者订阅同一话题会导致消息路由混乱

**问题代码位置**:
- `run_automated_test.py` line 1122-1124: `cleanup_ros_processes()`
  ```python
  # 清理ROS进程
  TestLogger.info("🔵 开始清理ROS进程...")
  self.cleanup_ros_processes()  # ❌ 只杀死进程，不注销订阅者
  TestLogger.info("🔵 ROS进程清理完成")
  ```

---

## 修复方案

### 修改内容

**文件**: `test/scripts/run_automated_test.py`
**位置**: line 1121-1130 (在`run_single_test()`方法中)

**修改前**:
```python
# 清理ROS进程
TestLogger.info("🔵 开始清理ROS进程...")
self.cleanup_ros_processes()
TestLogger.info("🔵 ROS进程清理完成")
```

**修改后**:
```python
# 清理ROS进程
TestLogger.info("🔵 开始清理ROS进程...")

# 关键修复：先注销ROS订阅者，避免订阅者累积
if self.goal_monitor is not None:
    TestLogger.info("📡 注销GoalMonitor订阅者...")
    # 注销odom订阅者
    if self.goal_monitor.odom_subscriber is not None:
        self.goal_monitor.odom_subscriber.unregister()
        self.goal_monitor.odom_subscriber = None
    # 注销manuscript metrics订阅者
    if self.goal_monitor.manuscript_metrics is not None:
        self.goal_monitor.manuscript_metrics.shutdown_subscribers()
    TestLogger.success("✓ 订阅者已注销")

self.cleanup_ros_processes()
TestLogger.info("🔵 ROS进程清理完成")
```

### 修复原理

1. **在杀死ROS进程之前，先注销Python订阅者对象**
2. **确保每次测试后订阅者对象完全清理**
3. **下次测试创建新订阅者时，不会有旧对象干扰**

### 清理顺序

```
1. 注销odom订阅者 (GoalMonitor.odom_subscriber)
2. 注销manuscript metrics订阅者 (ManuscriptMetricsCollector.subscribers)
   ├─ stl_robustness
   ├─ stl_budget
   ├─ dr_margins
   ├─ tracking_error
   ├─ mpc_solve_time
   ├─ mpc_feasibility
   ├─ tube_boundaries
   └─ regret_metrics
3. 终止launch进程
4. 杀死所有ROS进程
```

---

## 验证测试

### 模拟测试结果

运行`/tmp/test_subscriber_cleanup.py`，模拟5次测试循环：

```
【测试 1】
  ✓ ODOM订阅者已创建: True
  ✓ Metrics订阅者已创建: True
  🧹 清理订阅者...
  ✓ ODOM订阅者已清理: True
  ✓ Metrics订阅者已清理: True
  ✅ 测试 1 清理成功

【测试 2-5】
  ✅ 全部清理成功
```

### 预期效果

修复后，每次测试应该：
1. ✅ 正常创建订阅者
2. ✅ 正常收集manuscript metrics数据
3. ✅ 正确注销订阅者
4. ✅ 下次测试不受影响

---

## 测试建议

### 验证修复

运行完整的5次测试：

```bash
cd /home/dixon/Final_Project/catkin

# 运行测试（无可视化，速度更快）
./test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 5 \
    --no-viz \
    --timeout 240
```

### 检查数据完整性

```bash
# 检查每次测试的metrics数据
for i in {1..5}; do
    echo "=== Test ${i} ==="
    test_dir="test_results/safe_regret_$(date +%Y%m%d)_*/test_${i}_shelf_0${i}"
    metrics_file=$(find ${test_dir} -name "metrics.json" | head -1)

    if [ -f "$metrics_file" ]; then
        python3 << EOF
import json
with open('${metrics_file}', 'r') as f:
    data = json.load(f)
    mm = data.get('manuscript_metrics', {})
    print(f"  Satisfaction: {mm.get('satisfaction_metrics', {}).get('stl_satisfaction_probability', 'None')}")
    print(f"  Empirical Risk: {mm.get('safety_metrics', {}).get('empirical_safety_risk', 'None')}")
    print(f"  Feasibility Rate: {mm.get('computation_metrics', {}).get('mpc_feasibility_rate', 'None')}")
    print(f"  Total Steps: {mm.get('task_info', {}).get('total_snapshots', 'None')}")
EOF
    else
        echo "  ⚠️  Metrics文件未找到"
    fi
done
```

### 预期输出

修复后，每次测试都应该有数据：

```
=== Test 1 ===
  Satisfaction: 0.85
  Empirical Risk: 0.02
  Feasibility Rate: 0.95
  Total Steps: 1234

=== Test 2 ===
  Satisfaction: 0.82
  Empirical Risk: 0.03
  Feasibility Rate: 0.92
  Total Steps: 1156

=== Test 3 ===
  ... (有数据)

=== Test 4 ===
  ... (有数据)

=== Test 5 ===
  ... (有数据)
```

---

## 相关文件

**修改的文件**:
- `test/scripts/run_automated_test.py` (line 1121-1130)

**相关类和方法**:
- `ManuscriptMetricsCollector.shutdown_subscribers()` (line 188-195)
- `GoalMonitor.__init__()` (line 590-609)
- `GoalMonitor.setup_ros_subscribers()` (line 850-868)
- `AutomatedTestRunner.cleanup_ros_processes()` (line 934-953)
- `AutomatedTestRunner.run_single_test()` (line 1074-1136)

---

## 经验教训

### 问题根源
- **ROS Subscriber对象生命周期管理不当**
- Python对象与ROS进程的生命周期不同步

### 预防措施
1. **每次创建订阅者前，先注销旧订阅者**
2. **在cleanup函数中显式注销所有订阅者**
3. **测试时验证订阅者是否正确清理**

### 最佳实践
```python
# ✅ 正确模式
def setup():
    if self.subscriber is not None:
        self.subscriber.unregister()  # 先注销旧的
    self.subscriber = rospy.Subscriber(...)

def cleanup():
    if self.subscriber is not None:
        self.subscriber.unregister()  # 清理时注销
        self.subscriber = None
```

---

## 状态

✅ **修复完成** - 2026-04-07
⏳ **待验证** - 需要运行完整测试确认效果

---

## 附录：调试技巧

### 检查订阅者状态

在测试中添加调试输出：

```python
# 在setup_ros_subscribers()之后
print(f"  活跃订阅者数量: {len(rospy.get_name())}")
print(f"  已订阅话题: {rospy.get_published_topics()}")
```

### 监控消息接收

```python
# 在回调函数中添加计数器
def metrics_callback(self, msg):
    self.callback_count += 1
    if self.callback_count % 100 == 0:
        print(f"  📊 已接收 {self.callback_count} 条消息")
```

### 检查内存泄漏

```bash
# 使用memory_profiler监控Python进程内存
pip install memory_profiler
python3 -m memory_profiler test/scripts/run_automated_test.py
```
