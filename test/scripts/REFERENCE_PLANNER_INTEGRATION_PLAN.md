# Reference Planner集成计划

## 目标

实现Dynamic Regret和Safe Regret的数据收集，将Manuscript Metrics完整度从85.7%提升到100%

## 当前状态

### ✅ 已完成

1. **RegretAnalyzer实现** (`src/safe_regret/src/RegretAnalyzer.cpp`)
   - Oracle比较器
   - 动态遗憾计算
   - 安全遗憾计算
   - 遗憾传递定理验证

2. **SafeRegretNode实现** (`src/safe_regret/src/SafeRegretNode.cpp`)
   - 发布regret数据到 `/safe_regret/regret_metrics`
   - Float64MultiArray格式
   - 包含safe_regret, dynamic_regret, tracking_contribution, nominal_contribution

### ❌ 未完成

1. **SafeRegretNode未启动**
   - launch文件中没有包含safe_regret节点
   - 需要添加到launch文件

2. **测试脚本未订阅**
   - run_automated_test.py没有订阅regret话题
   - 需要添加订阅和数据处理

## 实施计划

### 第1步: 添加SafeRegretNode到launch文件

**文件**: `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch`

**修改内容**:
```xml
<!-- ========== Reference Planner & Regret Analysis ========== -->
<group if="$(arg enable_reference_planner)">
  <node name="safe_regret" pkg="safe_regret" type="safe_regret_node"
        output="screen" required="false"
        respawn="false" respawn_delay="5">
    <!-- Parameters -->
    <param name="enable_regret_logging" value="true"/>
    <param name="regret_log_file" value="/tmp/safe_regret_data.csv"/>
    <param name="regret_metrics_topic" value="/safe_regret/regret_metrics"/>
    <param name="reference_trajectory_topic" value="/safe_regret/reference_trajectory"/>
    <param name="no_regret_algorithm" value="omd"/>
    <param name="tube_radius" value="0.18"/>
  </node>
</group>
```

**添加参数**:
```xml
<arg name="enable_reference_planner" default="false"/>
```

### 第2步: 修改测试脚本订阅regret数据

**文件**: `test/scripts/run_automated_test.py`

**修改位置**: `ManuscriptMetricsCollector.setup_ros_subscribers()`

**添加代码**:
```python
# 6. Regret Metrics (from safe_regret)
try:
    from std_msgs.msg import Float64MultiArray
    self.subscribers['regret_metrics'] = rospy.Subscriber(
        '/safe_regret/regret_metrics', Float64MultiArray, self.regret_metrics_callback)
except Exception as e:
    TestLogger.warning(f"无法订阅regret_metrics话题: {e}")
```

**添加回调函数**:
```python
def regret_metrics_callback(self, msg):
    """Regret指标回调"""
    if len(msg.data) >= 2:
        # msg.data[0] = safe_regret
        # msg.data[1] = dynamic_regret
        # msg.data[2] = tracking_contribution
        # msg.data[3] = nominal_contribution
        self.data['safe_regret'].append(msg.data[0])
        self.data['dynamic_regret'].append(msg.data[1])
        self.data['tracking_contribution'].append(msg.data[2])
        self.data['nominal_contribution'].append(msg.data[3])
```

**修改ModelConfig**:
```python
'safe_regret': {
    'name': 'Safe-Regret MPC',
    'params': 'enable_stl:=true enable_dr:=true enable_reference_planner:=true',
    'description': '完整Safe-Regret MPC (STL+DR+Reference)'
}
```

### 第3步: 更新compute_final_metrics()

**确保计算平均regret**:
```python
# 3. Dynamic & Safe Regret (平均值)
if len(self.data['dynamic_regret']) > 0:
    metrics['avg_dynamic_regret'] = np.mean(self.data['dynamic_regret'])
else:
    metrics['avg_dynamic_regret'] = None

if len(self.data['safe_regret']) > 0:
    metrics['avg_safe_regret'] = np.mean(self.data['safe_regret'])
else:
    metrics['avg_safe_regret'] = None
```

### 第4步: 测试验证

**测试命令**:
```bash
python3 test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 1 \
    --no-viz
```

**预期结果**:
```json
{
  "avg_dynamic_regret": 0.123,  // ✅ 有数据
  "avg_safe_regret": 0.456,     // ✅ 有数据
  "total_regret_samples": 100    // ✅ 有数据
}
```

## 数据流

```
SafeRegretNode
  ├─ 收集: 系统状态、MPC轨迹、跟踪误差
  ├─ 计算: RegretAnalyzer.analyze()
  │   ├─ Oracle比较器
  │   ├─ 动态遗憾: R_T^dyn
  │   └─ 安全遗憾: R_T^safe
  └─ 发布: /safe_regret/regret_metrics (Float64MultiArray)
       └─ 订阅: ManuscriptMetricsCollector
           └─ 存储: safe_regret[], dynamic_regret[]
           └─ 计算: avg_safe_regret, avg_dynamic_regret
```

## 验证指标

- ✅ safe_regret数据收集: >0 samples
- ✅ dynamic_regret数据收集: >0 samples
- ✅ avg_safe_regret计算成功: 非null
- ✅ avg_dynamic_regret计算成功: 非null
- ✅ Manuscript Metrics完整度: 100% (7/7)

## 时间估算

- 第1步（launch文件）: 5分钟
- 第2步（测试脚本）: 10分钟
- 第3步（计算逻辑）: 5分钟
- 第4步（测试验证）: 10分钟

**总计**: ~30分钟

## 风险与注意事项

1. **SafeRegretNode依赖**:
   - 需要确认safe_regret包已编译
   - 检查依赖项是否满足

2. **Oracle轨迹**:
   - RegretAnalyzer需要oracle轨迹作为参考
   - 可能需要使用离线最优轨迹或预设参考

3. **话题匹配**:
   - 确认话题名称: `/safe_regret/regret_metrics`
   - 确认消息类型: `std_msgs/Float64MultiArray`

4. **性能影响**:
   - Regret计算可能增加计算负担
   - 监控CPU使用率

## 后续优化

1. **在线Oracle更新**:
   - 实现在线学习oracle轨迹
   - 自适应参考策略

2. **多策略比较**:
   - 支持多个oracle策略
   - 计算相对不同策略的regret

3. **可视化**:
   - 添加regret曲线可视化
   - 实时regret监控

---

**创建日期**: 2026-04-06
**作者**: Claude Code
**优先级**: 高（完成最后15%的指标收集）
**状态**: 📋 计划中
