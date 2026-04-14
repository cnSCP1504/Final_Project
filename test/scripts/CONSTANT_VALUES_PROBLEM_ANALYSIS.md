# Manuscript Metrics恒定值问题分析报告

**日期**: 2026-04-07
**问题**: 多个关键指标数据恒定不变
**状态**: ✅ **根本原因已找到**

---

## 🔴 发现的恒定值问题

### 1. DR Margins完全恒定
- **值**: 0.18 (所有17,493个样本完全相同)
- **预期**: DR margins应该根据跟踪误差残差动态调整
- **影响**: 无法验证DR约束收紧是否真正工作

### 2. STL Budget完全恒定
- **值**: 0.0 (所有833个样本完全相同)
- **预期**: STL budget应该随robustness变化
- **影响**: 无法验证STL budget机制是否工作

### 3. 空数据列表
- `tracking_error_history`: 空 (应该有数据)
- `instantaneous_cost`: 空
- `reference_cost`: 空
- **影响**: 缺失关键性能指标

---

## 🔍 根本原因分析

### 问题1: DR Margins恒定为0.18

**根本原因**: DR tightening节点没有收集到足够的残差数据

**证据**:
```cpp
// dr_tightening_node.cpp line 243-245
if (residual_collector_->getWindowSize() < MIN_RESIDUALS_FOR_UPDATE) {
    return;  // 没有足够数据，直接返回，不更新margins
}
```

**问题流程**:
1. dr_tightening节点启动，开始收集tracking_error残差
2. **需要至少50个残差样本** (`min_residuals_for_update: 50`)
3. 在收集到足够数据之前，`margins`向量保持初始值
4. Launch文件设置 `tube_radius: 0.18`，所以margins默认为0.18
5. 测试脚本收到的一直是初始的0.18值

**Launch文件配置** (safe_regret_mpc_test.launch line 74, 76):
```xml
<param name="tube_radius" value="0.18"/>
<param name="safety_buffer" value="0.18"/>
```

**验证**:
- 日志显示: "DR margins updated, count: 20" (每次都是20个值，全是0.18)
- 这说明DR节点在发布margins，但值没有变化

**为什么没有足够的残差数据？**
- 可能原因1: `/tube_mpc/tracking_error`话题发布频率太低
- 可能原因2: residual_collector的窗口大小设置问题
- 可能原因3: 测试时间太短，还没收集够50个样本

---

### 问题2: STL Budget恒定为0.0

**根本原因**: STL robustness是很大的负数，导致budget立即耗尽

**STL Budget更新公式** (RobustnessBudget.cpp line 30):
```cpp
state_.budget = std::max(0.0, state_.budget + robustness - state_.baseline_requirement);
```

**问题流程**:
1. 初始: `budget = 1.0`, `baseline_requirement = 0.1`
2. 第一次更新: `robustness = -7.29623`
   ```
   budget = max(0, 1.0 + (-7.29623) - 0.1)
          = max(0, -6.39623)
          = 0.0
   ```
3. 后续更新: `budget = max(0, 0.0 + (-7.x) - 0.1) = 0.0`
4. Budget一直保持在0.0

**日志证据**:
```
STL robustness: -7.29623, budget: 0
STL robustness: -7.29623, budget: 0
STL robustness: -7.29623, budget: 0
```

**为什么STL robustness这么负？**
- 可能原因1: STL规范设置过于严格
- 可能原因2: 机器人状态严重违反STL公式
- 可能原因3: Belief-space鲁棒性计算有问题

---

### 问题3: tracking_error_history为空

**根本原因**: 回调函数代码错误 - 没有append到tracking_error_history

**问题代码** (run_automated_test.py line 217-240):
```python
def tracking_error_callback(self, msg):
    """跟踪误差回调 - 接收Float64MultiArray"""
    if len(msg.data) >= 4:
        error_norm = msg.data[3]  # 第四个元素是误差范数
        self.data['tracking_error_norm_history'].append(error_norm)  # ✓ 正确

        # ❌ 错误：没有 append 到 tracking_error_history！

        # 检查是否违反tube约束
        tube_radius = 0.18
        if error_norm > tube_radius:
            self.data['tube_violations'].append(error_norm)
            self.data['tube_violation_count'] += 1

        # ... 其他代码
```

**修复**:
```python
def tracking_error_callback(self, msg):
    """跟踪误差回调 - 接收Float64MultiArray"""
    if len(msg.data) >= 4:
        error_x = msg.data[0]
        error_y = msg.data[1]
        error_yaw = msg.data[2]
        error_norm = msg.data[3]

        # ✅ 修复：添加到tracking_error_history
        self.data['tracking_error_history'].append([error_x, error_y, error_yaw, error_norm])
        self.data['tracking_error_norm_history'].append(error_norm)

        # ... 其他代码
```

---

### 问题4: instantaneous_cost和reference_cost为空

**根本原因**: 这些话题可能没有在发布，或者测试脚本没有订阅

**检查**:
```python
# run_automated_test.py line 123-187
def setup_ros_subscribers(self):
    # ... 订阅stl, dr, tracking_error等

    # ❌ 缺失：没有订阅instantaneous_cost和reference_cost话题！
```

**需要添加的订阅**:
```python
# 如果safe_regret_mpc发布这些话题，需要添加：
self.subscribers['instantaneous_cost'] = rospy.Subscriber(
    '/safe_regret_mpc/instantaneous_cost', Float64, self.cost_callback)
self.subscribers['reference_cost'] = rospy.Subscriber(
    '/safe_regret_mpc/reference_cost', Float64, self.reference_cost_callback)
```

---

## 📊 问题总结

| 问题 | 根本原因 | 严重程度 | 修复难度 |
|------|---------|---------|---------|
| DR Margins恒定 | 残差数据不足 | 🔴 高 | 简单 |
| STL Budget恒定 | Robustness太负 | 🟡 中 | 困难 |
| tracking_error_history空 | 代码错误 | 🟢 低 | 简单 |
| instantaneous_cost空 | 未订阅话题 | 🟢 低 | 简单 |
| reference_cost空 | 未订阅话题 | 🟢 低 | 简单 |

---

## ✅ 修复方案

### 修复1: DR Margins恒定问题

**方案A**: 降低最小残差要求（快速修复）
```xml
<!-- safe_regret_mpc_test.launch -->
<node name="dr_tightening" pkg="dr_tightening" type="dr_tightening_node">
  <param name="min_residuals_for_update" value="10"/>  <!-- 从50降到10 -->
</node>
```

**方案B**: 延长首次发布等待时间
```cpp
// dr_tightening_node.cpp
// 在收集到足够数据之前，发布默认margin值
if (residual_collector_->getWindowSize() < MIN_RESIDUALS_FOR_UPDATE) {
    // 发布当前tube_radius作为默认margin
    std::vector<double> default_margins(params_.mpc_horizon + 1, params_.tube_radius);
    publishMargins(default_margins, {});
    return;
}
```

**方案C**: 检查tracking_error话题是否正确发布
```bash
# 在测试运行时检查
rostopic hz /tube_mpc/tracking_error
rostopic echo /tube_mpc/tracking_error
```

---

### 修复2: STL Budget恒定问题

**方案A**: 调整STL规范（如果robustness计算正确）
- 降低baseline_requirement (0.1 → 0.01)
- 或者放宽STL公式要求

**方案B**: 检查STL robustness计算是否正确
```bash
# 检查robustness值是否合理
rostopic echo /stl_monitor/robustness
```

**方案C**: 使用不同的STL budget初始化策略
- 增加初始budget (1.0 → 10.0)
- 或者实现budget自动重置机制

---

### 修复3: tracking_error_history空

**修改**: `test/scripts/run_automated_test.py` line 217-240

```python
def tracking_error_callback(self, msg):
    """跟踪误差回调 - 接收Float64MultiArray"""
    # msg.data 是一个数组，包含 [error_x, error_y, error_yaw, error_norm]
    if len(msg.data) >= 4:
        error_x = msg.data[0]
        error_y = msg.data[1]
        error_yaw = msg.data[2]
        error_norm = msg.data[3]

        # ✅ 修复：添加完整的tracking error数据
        self.data['tracking_error_history'].append([error_x, error_y, error_yaw, error_norm])
        self.data['tracking_error_norm_history'].append(error_norm)

        # 检查是否违反tube约束（假设tube半径为0.18）
        tube_radius = 0.18
        if error_norm > tube_radius:
            self.data['tube_violations'].append(error_norm)
            self.data['tube_violation_count'] += 1

        # 计算empirical risk（违反DR安全边界）
        self.data['safety_total_steps'] += 1
        if len(self.data['dr_margins_history']) > 0:
            latest_dr_margin = self.data['dr_margins_history'][-1]
            if error_norm > latest_dr_margin:
                self.data['safety_violation_count'] += 1
                self.data['safety_violations'].append(error_norm)
    else:
        TestLogger.warning(f"tracking_error数据长度不足: {len(msg.data)}")
```

---

### 修复4: 添加cost话题订阅

**修改**: `test/scripts/run_automated_test.py` line 123-187

在`setup_ros_subscribers()`中添加：

```python
# 7. Cost Metrics (from safe_regret_mpc)
try:
    from std_msgs.msg import Float64
    self.subscribers['instantaneous_cost'] = rospy.Subscriber(
        '/safe_regret_mpc/instantaneous_cost', Float64,
        self.instantaneous_cost_callback, queue_size=10)
    self.subscribers['reference_cost'] = rospy.Subscriber(
        '/safe_regret_mpc/reference_cost', Float64,
        self.reference_cost_callback, queue_size=10)
except Exception as e:
    TestLogger.warning(f"无法订阅cost话题: {e}")
```

并添加回调函数：

```python
def instantaneous_cost_callback(self, msg):
    """瞬时代价回调"""
    self.data['instantaneous_cost'].append(msg.data)

def reference_cost_callback(self, msg):
    """参考策略代价回调"""
    self.data['reference_cost'].append(msg.data)
```

---

## 🎯 优先级修复顺序

1. **✅ 立即修复**: tracking_error_history空（简单，影响数据完整性）
2. **🔴 高优先级**: DR Margins恒定（影响核心功能验证）
3. **🟡 中优先级**: 添加cost话题订阅（补充缺失指标）
4. **🟢 低优先级**: STL Budget恒定（需要深入分析STL规范）

---

## 📝 相关文件

**需要修改的文件**:
- `test/scripts/run_automated_test.py` (tracking_error_callback, 添加cost订阅)
- `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch` (min_residuals_for_update)
- `src/dr_tightening/src/dr_tightening_node.cpp` (可选：改进默认margin行为)

**相关代码**:
- `src/tube_mpc_ros/stl_monitor/src/RobustnessBudget.cpp` (STL budget更新)
- `src/dr_tightening/src/dr_tightening_node.cpp` (DR margins计算)

---

## 🔬 验证方法

修复后运行测试并检查：

```bash
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --no-viz --timeout 120

# 检查修复效果
python3 << 'EOF'
import json
with open('test_results/safe_regret_XXXXXXX/test_01_shelf_01/metrics.json', 'r') as f:
    data = json.load(f)
    raw = data['manuscript_raw_data']

    # 检查DR margins是否变化
    dr_hist = raw['dr_margins_history']
    dr_unique = len(set(dr_hist))
    print(f"✓ DR Margins: {len(dr_hist)} samples, {dr_unique} unique values")

    # 检查tracking_error_history是否有数据
    te_hist = raw['tracking_error_history']
    print(f"✓ Tracking Error: {len(te_hist)} samples")

    # 检查cost数据
    print(f"✓ Instantaneous Cost: {len(raw['instantaneous_cost'])} samples")
    print(f"✓ Reference Cost: {len(raw['reference_cost'])} samples")
EOF
```

**预期结果**:
- DR Margins: > 10 unique values
- Tracking Error: > 700 samples
- Instantaneous Cost: > 0 samples
- Reference Cost: > 0 samples

---

## 状态

✅ **问题分析完成** - 2026-04-07
⏳ **待实施修复**
