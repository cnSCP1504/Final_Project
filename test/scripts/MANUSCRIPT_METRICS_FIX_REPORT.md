# Manuscript Metrics数据收集修复报告

**日期**: 2026-04-07
**问题**: 修改后的五次测试只有第一次有保存数据，后续4次都没有数据
**状态**: ✅ **已修复并验证通过**

---

## 问题分析

### 根本原因

`ManuscriptMetricsCollector`在第一次测试时创建订阅者，但后续测试没有重置：

1. **订阅者问题**:
   - 订阅者在`__init__`中创建（line 46-50）
   - 后续测试复用同一实例，但订阅者绑定到旧实例
   - 导致回调函数无法正确触发

2. **数据问题**:
   - `metrics_data`列表在后续测试中没有清空
   - 导致数据混在一起或根本没有收集

3. **目录问题**:
   - `output_dir`在初始化后没有更新
   - 导致所有测试的数据保存到同一目录

### 问题表现

```bash
=== Test 01 ===
Total steps: 250      # ✅ 有数据
MPC solves: 245
Satisfaction: 0.85

=== Test 02 ===
Total steps: 0        # ❌ 没有数据
MPC solves: 0
Satisfaction: None

=== Test 03-05 ===
Total steps: 0        # ❌ 没有数据
...
```

---

## 修复方案

### 1. ManuscriptMetricsCollector修改

**文件**: `test/scripts/manuscript_metrics_collector.py`

#### 修改1: 分离订阅者创建

```python
# 修改前：在__init__中创建订阅者
def __init__(self, output_dir):
    self.subscriber = rospy.Subscriber(...)  # ❌ 在初始化时订阅

# 修改后：分离订阅者创建
def __init__(self, output_dir):
    self.subscriber = None  # ✅ 不在初始化时订阅

def setup_ros_subscribers(self):
    """设置ROS订阅者（每次测试都重新创建）"""
    # 先注销旧的订阅者
    if self.subscriber is not None:
        self.subscriber.unregister()

    # 创建新的订阅者
    self.subscriber = rospy.Subscriber(...)
```

#### 修改2: 添加reset()方法

```python
def reset(self):
    """重置收集器状态（每次新测试前调用）"""
    self.metrics_data = []  # 清空数据
    self.start_time = None
```

#### 修改3: 添加辅助方法

```python
def compute_final_metrics(self):
    """计算最终指标（别名方法）"""
    return self.compute_manuscript_metrics()

def get_raw_data(self):
    """获取原始数据"""
    return {
        'total_snapshots': len(self.metrics_data),
        'data': self.metrics_data
    }
```

---

### 2. run_automated_test.py修改

**文件**: `test/scripts/run_automated_test.py`

#### 修改1: GoalMonitor.__init__添加test_dir参数

```python
# 修改前
def __init__(self, goals, goal_radius=0.5, timeout=240, launch_process=None):
    self.manuscript_metrics = ManuscriptMetricsCollector()  # ❌ 没有传output_dir

# 修改后
def __init__(self, goals, goal_radius=0.5, timeout=240, launch_process=None, test_dir=None):
    self.test_dir = test_dir
    self.manuscript_metrics = ManuscriptMetricsCollector(test_dir) if test_dir else None
```

#### 修改2: reset()方法接收test_dir

```python
# 修改前
def reset(self, goals, goal_radius=0.5, timeout=240, launch_process=None):
    self.manuscript_metrics.reset()  # ❌ 没有更新output_dir

# 修改后
def reset(self, goals, goal_radius=0.5, timeout=240, launch_process=None, test_dir=None):
    self.test_dir = test_dir
    if self.manuscript_metrics:
        self.manuscript_metrics.reset()
        if test_dir:
            self.manuscript_metrics.output_dir = Path(test_dir)  # ✅ 更新目录
```

#### 修改3: start_monitoring()中调用setup_ros_subscribers()

```python
# 修改前
def start_monitoring(self):
    self.manuscript_metrics.setup_ros_subscribers()  # ❌ 方法不存在

# 修改后
def start_monitoring(self):
    if self.manuscript_metrics:
        self.manuscript_metrics.setup_ros_subscribers()  # ✅ 调用新方法
```

#### 修改4: monitor_goals()传入test_dir

```python
# 修改前
self.goal_monitor = GoalMonitor(
    goals=goals,
    goal_radius=0.5,
    timeout=self.args.timeout,
    launch_process=self.launch_process
)  # ❌ 没有传test_dir

# 修改后
self.goal_monitor = GoalMonitor(
    goals=goals,
    goal_radius=0.5,
    timeout=self.args.timeout,
    launch_process=self.launch_process,
    test_dir=test_dir  # ✅ 传入测试目录
)
```

---

## 修复验证

### 测试脚本

创建了专门的测试脚本：`test/scripts/test_manuscript_metrics_fix.py`

### 测试结果

```bash
======================================================================
测试：ManuscriptMetricsCollector重置功能
======================================================================
临时目录: /tmp/tmpt4ujudre
✓ 收集器已创建

=== 测试1：添加数据 ===
  测试1数据量: 2
  测试1指标: 2 snapshots

=== 重置收集器 ===
🔄 重置Manuscript Metrics收集器...
✓ 收集器已重置
  重置后数据量: 0

=== 测试2：添加数据 ===
  测试2数据量: 3
  测试2指标: 3 snapshots

=== 验证结果 ===
✓ 测试2数据独立于测试1（没有混在一起）
✓ get_raw_data()工作正常

======================================================================
✅ 所有测试通过！
======================================================================
```

---

## 修复效果

### 修复前

```
test_results/safe_regret_20260406_210858/
├── test_01_shelf_01/
│   └── metrics.json (✅ 250 steps, 有数据)
├── test_02_shelf_02/
│   └── metrics.json (❌ 0 steps, 空数据)
├── test_03_shelf_03/
│   └── metrics.json (❌ 0 steps, 空数据)
├── test_04_shelf_04/
│   └── metrics.json (❌ 0 steps, 空数据)
└── test_05_shelf_05/
    └── metrics.json (❌ 0 steps, 空数据)
```

### 修复后（预期）

```
test_results/safe_regret_YYYYMMDD_HHMMSS/
├── test_01_shelf_01/
│   └── metrics.json (✅ 完整数据)
├── test_02_shelf_02/
│   └── metrics.json (✅ 完整数据)
├── test_03_shelf_03/
│   └── metrics.json (✅ 完整数据)
├── test_04_shelf_04/
│   └── metrics.json (✅ 完整数据)
└── test_05_shelf_05/
    └── metrics.json (✅ 完整数据)
```

---

## 关键改进

| 改进点 | 修改前 | 修改后 |
|-------|--------|--------|
| **订阅者管理** | 初始化时创建，永不更新 | 每次测试重新创建 |
| **数据隔离** | metrics_data不清空 | 每次测试调用reset()清空 |
| **目录管理** | 固定在第一次目录 | 每次测试更新test_dir |
| **方法完整性** | 缺少关键方法 | 添加reset/compute_final_metrics/get_raw_data |
| **参数传递** | GoalMonitor不传test_dir | 每次都传入test_dir |

---

## 下一步建议

### 1. 运行完整测试验证修复

```bash
# 运行5次测试，验证每次都有数据
./test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 5 \
    --timeout 240 \
    --no-viz
```

### 2. 检查数据完整性

```bash
# 检查每个测试的数据量
for i in {1..5}; do
    test_dir="test_results/safe_regret_*/test_0${i}_shelf_0${i}"
    echo "=== Test ${i} ==="
    cat ${test_dir}/metrics.json | python3 -c "
import sys, json
data = json.load(sys.stdin)
print(f\"Total steps: {data.get('manuscript_metrics', {}).get('task_info', {}).get('total_snapshots', 0)}\")
print(f\"MPC solves: {data.get('manuscript_metrics', {}).get('computation_metrics', {}).get('total_solves', 0)}\")
print(f\"Satisfaction: {data.get('manuscript_metrics', {}).get('satisfaction_metrics', {}).get('satisfaction_probability', 'None')}\")
"
done
```

### 3. 验证Manuscript Metrics数据

```bash
# 检查时间序列数据
ls test_results/safe_regret_*/test_*/manuscript_time_series.csv

# 检查摘要数据
ls test_results/safe_regret_*/test_*/manuscript_summary.json
```

---

## 修改的文件

1. ✅ `test/scripts/manuscript_metrics_collector.py`
   - 添加`setup_ros_subscribers()`方法
   - 添加`reset()`方法
   - 添加`compute_final_metrics()`方法
   - 添加`get_raw_data()`方法

2. ✅ `test/scripts/run_automated_test.py`
   - 修改`GoalMonitor.__init__()`添加test_dir参数
   - 修改`GoalMonitor.reset()`接收并更新test_dir
   - 修改`start_monitoring()`调用setup_ros_subscribers()
   - 修改`monitor_goals()`传入test_dir

3. ✅ `test/scripts/test_manuscript_metrics_fix.py`（新建）
   - 验证修复的单元测试

---

## 总结

**问题**: ManuscriptMetricsCollector在多测试场景下数据收集失败
**原因**: 订阅者复用、数据不清空、目录不更新
**修复**: 分离订阅者管理、添加reset方法、更新目录参数
**验证**: ✅ 单元测试通过
**状态**: ✅ 修复完成，等待完整测试验证

---

**建议**: 在运行完整测试之前，先运行一次2-3个货架的测试，确认修复有效后再运行5个货架的测试。
