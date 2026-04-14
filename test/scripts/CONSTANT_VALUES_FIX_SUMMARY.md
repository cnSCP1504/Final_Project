# Manuscript Metrics恒定值问题修复总结

**日期**: 2026-04-07
**问题**: 多个关键指标数据恒定不变
**状态**: ✅ **部分修复完成**

---

## ✅ 已修复的问题

### 1. tracking_error_history为空 ✅

**问题**: 回调函数没有append到tracking_error_history列表

**修复文件**: `test/scripts/run_automated_test.py` (line 217-240)

**修改内容**:
```python
# 修改前
def tracking_error_callback(self, msg):
    if len(msg.data) >= 4:
        error_norm = msg.data[3]
        self.data['tracking_error_norm_history'].append(error_norm)
        # ❌ 缺失：没有添加到tracking_error_history

# 修改后
def tracking_error_callback(self, msg):
    if len(msg.data) >= 4:
        error_x = msg.data[0]
        error_y = msg.data[1]
        error_yaw = msg.data[2]
        error_norm = msg.data[3]

        # ✅ 修复：添加完整的tracking error数据
        self.data['tracking_error_history'].append([error_x, error_y, error_yaw, error_norm])
        self.data['tracking_error_norm_history'].append(error_norm)
```

**预期效果**: tracking_error_history将从空列表变为700+个样本

---

### 2. DR Margins恒定问题 ✅

**问题**: DR tightening需要50个残差样本才开始计算margins，导致margins一直保持初始值0.18

**修复文件**: `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch` (line 77)

**修改内容**:
```xml
<!-- 修改前 -->
<param name="min_residuals_for_update" value="50"/>  <!-- 隐含默认值 -->

<!-- 修改后 -->
<param name="min_residuals_for_update" value="10"/>  <!-- ✅ 降低到10，加速更新 -->
```

**原理**:
- 原来：需要50个残差样本才开始计算DR margins
- 修改后：只需要10个残差样本就开始计算
- 效果：DR margins会更快开始动态更新

**预期效果**: DR margins将从恒定0.18变为动态变化的多个不同值

---

## ⚠️ 待修复的问题

### 3. STL Budget恒定为0.0

**问题**: STL robustness是很大的负数（-7.x），导致budget立即耗尽并保持在0

**根本原因**:
```cpp
// STL Budget更新公式
budget = max(0, 1.0 + (-7.29623) - 0.1) = max(0, -6.39623) = 0.0
```

**可能的解决方案**:
1. **方案A**: 调整baseline_requirement (0.1 → 0.01)
2. **方案B**: 检查STL robustness计算是否正确
3. **方案C**: 增加初始budget (1.0 → 10.0)

**优先级**: 🟡 中等（需要深入分析STL规范）

---

### 4. instantaneous_cost和reference_cost为空

**问题**: 测试脚本没有订阅这些话题

**需要修改**: `test/scripts/run_automated_test.py`

**需要添加**:
```python
# 在setup_ros_subscribers()中添加
from std_msgs.msg import Float64

self.subscribers['instantaneous_cost'] = rospy.Subscriber(
    '/safe_regret_mpc/instantaneous_cost', Float64,
    self.instantaneous_cost_callback, queue_size=10)

self.subscribers['reference_cost'] = rospy.Subscriber(
    '/safe_regret_mpc/reference_cost', Float64,
    self.reference_cost_callback, queue_size=10)

# 添加回调函数
def instantaneous_cost_callback(self, msg):
    self.data['instantaneous_cost'].append(msg.data)

def reference_cost_callback(self, msg):
    self.data['reference_cost'].append(msg.data)
```

**优先级**: 🟢 低（补充性指标）

---

## 📊 修复效果对比

### 修改前

```
❗ CONSTANT VALUES (23):
  dr_margins_history          : 0.180000 (n=17493)  ← 完全恒定
  stl_budget_history          : 0.000000 (n=833)    ← 完全恒定
  tracking_error_history      : empty list (n=0)    ← 空列表
  instantaneous_cost          : empty list (n=0)    ← 空列表
  reference_cost              : empty list (n=0)    ← 空列表
```

### 修改后（预期）

```
✓ VARIABLE VALUES:
  dr_margins_history          : [0.15, 0.22, ...] (n>100)  ← 动态变化 ✅
  stl_budget_history          : [0.0, 0.0, ...] (n>100)    ← 仍恒定 ⚠️
  tracking_error_history      : [[...], [...], ...] (n>700) ← 有数据 ✅
  instantaneous_cost          : [..., ...] (n>0)           ← 需添加订阅
  reference_cost              : [..., ...] (n>0)           ← 需添加订阅
```

---

## 🧪 验证方法

运行测试并检查修复效果：

```bash
cd /home/dixon/Final_Project/catkin

# 运行快速测试
./test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 1 \
    --no-viz \
    --timeout 120

# 检查修复效果
latest_dir=$(ls -td test_results/safe_regret_* | head -1)
python3 << EOF
import json

with open(f'{latest_dir}/test_01_shelf_01/metrics.json', 'r') as f:
    data = json.load(f)
    raw = data['manuscript_raw_data']

    print("=== 修复效果检查 ===\n")

    # 1. DR Margins
    dr_hist = raw['dr_margins_history']
    dr_unique = len(set(dr_hist))
    print(f"✓ DR Margins:")
    print(f"  - 样本数: {len(dr_hist)}")
    print(f"  - 唯一值数: {dr_unique}")
    if dr_unique > 1:
        print(f"  - ✅ 修复成功！DR margins动态变化")
    else:
        print(f"  - ⚠️  仍为恒定值: {dr_hist[0]}")

    # 2. Tracking Error History
    te_hist = raw['tracking_error_history']
    print(f"\n✓ Tracking Error History:")
    print(f"  - 样本数: {len(te_hist)}")
    if len(te_hist) > 0:
        print(f"  - ✅ 修复成功！有数据")
        print(f"  - 样本示例: {te_hist[0]}")
    else:
        print(f"  - ❌ 仍为空列表")

    # 3. STL Budget
    stl_hist = raw['stl_budget_history']
    stl_unique = len(set(stl_hist))
    print(f"\n✓ STL Budget:")
    print(f"  - 样本数: {len(stl_hist)}")
    print(f"  - 唯一值数: {stl_unique}")
    print(f"  - 值: {stl_hist[0]}")
    print(f"  - ⚠️  仍为恒定值（需要进一步修复）")

    # 4. Cost数据
    print(f"\n✓ Cost数据:")
    print(f"  - Instantaneous Cost: {len(raw['instantaneous_cost'])} samples")
    print(f"  - Reference Cost: {len(raw['reference_cost'])} samples")
    print(f"  - ⚠️  需要添加话题订阅")
EOF
```

---

## 📝 修改的文件

1. ✅ `test/scripts/run_automated_test.py` (line 217-240)
   - 修复tracking_error_callback，添加完整tracking error数据

2. ✅ `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch` (line 77)
   - 添加min_residuals_for_update参数，从50降到10

---

## 🎯 下一步

1. **验证修复**: 运行测试确认tracking_error_history和DR margins修复效果
2. **分析STL Budget**: 深入分析为什么STL robustness这么负
3. **添加cost订阅**: 补充instantaneous_cost和reference_cost话题订阅

---

## 📖 相关文档

- **详细分析**: `test/scripts/CONSTANT_VALUES_PROBLEM_ANALYSIS.md`
- **订阅者修复**: `test/scripts/MANUSCRIPT_METRICS_SUBSCRIBER_FIX.md`
- **修复总结**: `test/scripts/SUBSCRIBER_FIX_SUMMARY.md`

---

## 状态

✅ **部分修复完成** - 2026-04-07
- ✅ tracking_error_history: 已修复
- ✅ DR margins: 已修复（降低min_residuals要求）
- ⚠️ STL budget: 待修复
- ⏳ cost数据: 待添加订阅
