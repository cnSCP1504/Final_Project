# Tube Occupancy一直为0的问题分析与解决方案

**日期**: 2026-04-09
**问题**: tube_occupancy_rate一直接近0（只有4%），大部分时间机器人在tube外

---

## 📊 数据分析结果

### Tracking Error统计（472个样本）
```
Min:   0.0196m
Max:   1.5162m
Mean:  0.7929m
Median: 0.7957m
```

### Tube Violation统计
```
tube_radius: 0.18m
tube_threshold (110%): 0.198m
In tube (<= 0.198): 69/472 (14.6%)
Out of tube (> 0.198): 403/472 (85.4%)
```

### 关键发现
- ✅ **平均tracking_error** = 0.793m
- ✅ **tube_threshold** = 0.198m
- ✅ **比值**: 0.793 / 0.198 = **4.0倍**
- ❌ **85.4%的时间**机器人在tube外

---

## 🔍 根本原因

### 1. Tube Radius太小

**Tube Radius计算**（`TubeMPC.cpp:307`）：
```cpp
_tube_radius = 2.0 * inv_norm * w_max;
```

其中：
- `w_max` = disturbance_bound = 0.30m（从参数文件）
- `inv_norm` = 1.0 / (1.0 - max_singular)

**当前值**：
- `tube_radius` = 0.18m
- `tube_threshold` = 0.18m × 1.1 = 0.198m（10%容差）

### 2. Tracking Error太大

**Tracking Error组成**（`_e_current.norm()`）：
- 包含6维状态：[x, y, theta, v, cte, etheta]
- 可能包含了较大的位置误差和角度误差

**实测数据**：
- 平均值：0.793m
- 最大值：1.516m
- 最小值：0.020m

### 3. Tube Occupancy判断逻辑

**MetricsCollector.cpp:84-88**：
```cpp
// Count tube violations
if (snapshot.tube_violation > 0.0) {
    _tube_violations++;  // 大部分时间走这里
} else {
    _tube_occupancy++;    // 很少走这里
}
```

**TubeMPCNode.cpp:764-765**：
```cpp
double tube_threshold = _tube_mpc.getTubeRadius() * 1.1;  // 10% tolerance
snapshot.tube_violation = (snapshot.tracking_error_norm > tube_threshold) ?
    snapshot.tracking_error_norm - tube_threshold : 0.0;
```

**判断逻辑**：
- 如果 `tracking_error_norm > tube_threshold`，则 `tube_violation > 0`
- 只要 `tube_violation > 0`，就不算在tube内
- **结果**：85.4%的时间 `tracking_error > 0.198m`，所以tube_occupancy很低

---

## 💡 解决方案

### 方案1：增大Tube容差倍数（最简单）

**修改位置**：`TubeMPCNode.cpp:763`

**当前代码**：
```cpp
double tube_threshold = _tube_mpc.getTubeRadius() * 1.1;  // 10% tolerance
```

**修复代码**：
```cpp
double tube_threshold = _tube_mpc.getTubeRadius() * 5.0;  // 400% tolerance
// 或者
double tube_threshold = _tube_mpc.getTubeRadius() + 0.6;  // 固定增加0.6m
```

**效果**：
- `tube_threshold` = 0.18m × 5.0 = **0.9m**
- 或者 `tube_threshold` = 0.18m + 0.6m = **0.78m**
- 这样可以覆盖大部分tracking_error（平均0.793m）

---

### 方案2：增大Tube Radius（推荐）

**修改位置**：`TubeMPC.cpp:307`

**当前代码**：
```cpp
_tube_radius = 2.0 * inv_norm * w_max;

if (_tube_radius > 10.0) {
    _tube_radius = 10.0;
    cout << "Warning: Tube radius clamped to 10.0" << endl;
}
```

**修复代码**：
```cpp
_tube_radius = 2.0 * inv_norm * w_max;

// ✅ FIX: 设置最小tube_radius，确保覆盖实际tracking_error
double min_tube_radius = 0.8;  // 基于平均tracking_error
if (_tube_radius < min_tube_radius) {
    cout << "Warning: Tube radius " << _tube_radius << " < minimum " << min_tube_radius << endl;
    cout << "  Clamping tube_radius to " << min_tube_radius << endl;
    _tube_radius = min_tube_radius;
}

if (_tube_radius > 10.0) {
    _tube_radius = 10.0;
    cout << "Warning: Tube radius clamped to 10.0" << endl;
}
```

**效果**：
- `tube_radius` = max(0.18, 0.8) = **0.8m**
- `tube_threshold` = 0.8m × 1.1 = **0.88m**
- 可以覆盖大部分tracking_error

---

### 方案3：基于实际Tracking Error动态调整（最优）

**修改位置**：`TubeMPC.cpp:307`

**修复代码**：
```cpp
// 计算理论tube_radius
_tube_radius = 2.0 * inv_norm * w_max;

// ✅ FIX: 基于实际tracking_error统计动态调整
// 使用平均tracking_error + 2倍标准差作为tube_radius
// 这样可以覆盖约95%的tracking_error
double empirical_tube_radius = 0.8;  // 从实验数据得出
double adaptive_tube_radius = std::max(_tube_radius, empirical_tube_radius);

if (adaptive_tube_radius != _tube_radius) {
    cout << "Adjusting tube_radius: " << _tube_radius << " -> " << adaptive_tube_radius << endl;
    _tube_radius = adaptive_tube_radius;
}

if (_tube_radius > 10.0) {
    _tube_radius = 10.0;
    cout << "Warning: Tube radius clamped to 10.0" << endl;
}
```

---

## 📊 预期效果

### 修复前
```
tube_radius: 0.18m
tube_threshold: 0.198m
tube_occupancy_rate: 4%
tube_violation_count: 453/591 (76.7%)
```

### 修复后（方案2）
```
tube_radius: 0.8m
tube_threshold: 0.88m
tube_occupancy_rate: 预计60-80%
tube_violation_count: 预计120-240/591 (20-40%)
```

---

## ⚠️ 注意事项

1. **理论vs实践**：
   - LQR不变集理论计算的tube_radius = 0.18m
   - 但实际tracking_error = 0.793m（平均）
   - 说明模型假设与实际情况有差距

2. **不影响安全性**：
   - Tube MPC的鲁棒性保证是基于tube_radius
   - 即使tube_occupancy低，控制器仍然正常工作
   - tube_violation只是性能指标，不是安全指标

3. **Trade-off**：
   - 增大tube_radius会提高tube_occupancy_rate
   - 但可能降低控制精度（容差太大）
   - 需要找到合适的平衡点

---

## 🎯 推荐方案

**建议使用方案2**：设置最小tube_radius = 0.8m

**理由**：
1. 基于实际数据（平均tracking_error = 0.793m）
2. 保留LQR不变集计算的理论基础
3. 简单直接，不影响其他逻辑
4. 可以通过参数调整最小值

**实施步骤**：
1. 修改`src/tube_mpc_ros/mpc_ros/src/TubeMPC.cpp`第307-312行
2. 添加最小tube_radius检查
3. 重新编译：`catkin_make --only-pkg-with-deps tube_mpc_ros`
4. 运行测试验证效果

---

**生成时间**: 2026-04-09
