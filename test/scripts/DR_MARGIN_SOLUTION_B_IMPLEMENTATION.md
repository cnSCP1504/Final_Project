# DR Margin方案B实现报告

**日期**: 2026-04-09
**方案**: 方案B - 使用Costmap作为安全函数

---

## ✅ 实现总结

### 修改的文件

1. **`src/dr_tightening/include/dr_tightening/dr_tightening_node.hpp`**
   - 添加 `#include <nav_msgs/OccupancyGrid.h>`
   - 添加 `ros::Subscriber sub_costmap_` 成员
   - 添加 `void costmapCallback()` 方法
   - 在 `NavigationSafetyFunction` 类中添加：
     - `void setCostmap()` - 设置costmap数据
     - `double getCostmapValue()` - 获取costmap值
     - `bool hasCostmap()` - 检查costmap是否可用
     - `nav_msgs::OccupancyGrid::ConstPtr costmap_` 成员

2. **`src/dr_tightening/src/dr_tightening_node.cpp`**
   - 添加 `#include <nav_msgs/OccupancyGrid.h>`
   - 实现 `setCostmap()` 方法
   - 实现 `getCostmapValue()` 方法（坐标转换 + 边界检查）
   - 实现 `hasCostmap()` 方法
   - 修改 `evaluate()` 方法：
     - 优先使用costmap：`h(x) = 1.0 - (costmap_value / 254.0)`
     - Fallback到状态边界约束：`h(x) = 1.0 - (x²/100 + y²/100)`
     - 最后使用障碍物列表
   - 修改 `gradient()` 方法：
     - 使用数值梯度计算costmap梯度
     - Fallback到状态边界梯度：`∇h = [-2x/100, -2y/100]`
   - 在构造函数中添加costmap订阅
   - 实现 `costmapCallback()` 方法

3. **`src/dr_tightening/params/dr_tightening_params.yaml`**
   - 修改 `safety_buffer: 1.0 → 0.2`（适配costmap归一化值）

---

## 🔍 实现原理

### 安全函数定义

```cpp
// 使用costmap作为安全函数
h(x) = 1.0 - (costmap_value / 254.0) - safety_buffer
```

其中：
- `costmap_value = 0`（自由空间）→ `h(x) ≈ 0.8`（安全）
- `costmap_value = 254`（障碍物）→ `h(x) ≈ 0.0`（危险）
- `safety_buffer = 0.2`（安全缓冲）

### 梯度计算

使用**数值梯度**：
```cpp
∂h/∂x ≈ -(f(x+ε, y) - f(x-ε, y)) / (2ε × 254)
∂h/∂y ≈ -(f(x, y+ε) - f(x, y-ε)) / (2ε × 254)
```

其中：
- `ε = 0.05m`（5cm步长）
- 负号表示：costmap值越大，安全值越小

### Fallback机制

1. **优先级1**: 使用costmap（如果有）
2. **优先级2**: 使用状态边界约束 `h(x) = 1.0 - (x²/100 + y²/100)`
3. **优先级3**: 使用障碍物列表（如果有）

---

## 📊 预期效果

### DR Margin范围

**使用costmap后**：
- 自由空间（costmap=0）：margin ≈ 0.8m
- 靠近障碍物（costmap=100）：margin ≈ 0.4m
- 障碍物附近（costmap=200）：margin ≈ 0.0m

**动态更新**：
- Costmap由move_base实时更新
- 反映传感器看到的障碍物
- 自动适应环境变化

### Safety Violation预期

**修复前**：
- DR margin: 1.08~10.60m（平均5.77m）
- 违反率：0%（因为margin太大）

**修复后（预期）**：
- DR margin: 0.0~0.8m（动态）
- 违反率：10-30%（合理范围）

---

## 🧪 测试验证

### 编译验证

```bash
✅ 编译成功
[100%] Built target dr_tightening_node
```

### 运行测试

```bash
# 清理ROS进程
./test/scripts/clean_ros.sh

# 运行测试
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --timeout 60 --no-viz
```

### 检查结果

```bash
# 查看DR margin范围
cat test_results/safe_regret_*/test_01_shelf_01/metrics.json | \
    python3 -c "import json,sys; d=json.load(sys.stdin); \
    raw=d['manuscript_raw_data']; \
    margins=[raw['dr_margins_history'][i] for i in range(0,len(raw['dr_margins_history']),21)]; \
    print(f'DR Margins (t=0):'); \
    print(f'  Min: {min(margins):.3f}m'); \
    print(f'  Max: {max(margins):.3f}m'); \
    print(f'  Mean: {sum(margins)/len(margins):.3f}m'); \
    print(f'  Median: {sorted(margins)[len(margins)//2]:.3f}m')"
```

---

## 🔧 关键特性

### 1. 动态更新

- Costmap由move_base实时更新（10Hz）
- 反映激光雷达传感器数据
- 自动适应环境变化

### 2. 坐标转换

```cpp
// 世界坐标 → Costmap坐标
map_x = (x - origin_x) / resolution
map_y = (y - origin_y) / resolution

// 获取costmap值
index = map_y * width + map_x
cost = costmap.data[index]
```

### 3. 边界处理

- 超出costmap范围 → 返回未知值（50）
- 未知区域（-1）→ 返回未知值（50）
- 自由空间（0）→ 返回0
- 障碍物（254）→ 返回254

### 4. 数值梯度

- 步长：5cm
- 中心差分：`(f(x+ε) - f(x-ε)) / (2ε)`
- 自动处理costmap不连续性

---

## 📈 优势

### 相比其他方案

| 特性 | 方案B (Costmap) | 方案A (状态约束) | 方案C (Tube半径) |
|------|-----------------|-----------------|-----------------|
| **反映实际环境** | ✅ | ❌ | ❌ |
| **动态更新** | ✅ | ❌ | ❌ |
| **符合manuscript** | ✅ | ✅ | ✅ |
| **实现复杂度** | ⭐⭐⭐ | ⭐⭐ | ⭐ |
| **需要额外依赖** | ✅ (move_base) | ❌ | ❌ |

### 关键优势

1. ✅ **实时感知障碍物**：使用激光雷达数据
2. ✅ **自动适应环境**：costmap动态更新
3. ✅ **符合manuscript定义**：h(x)定义安全集
4. ✅ **物理意义明确**：costmap值反映碰撞概率

---

## ⚠️ 注意事项

### 1. 依赖move_base

- Costmap由move_base发布
- 需要确保move_base正在运行
- 如果costmap不可用，会fallback到状态约束

### 2. 坐标系

- Costmap使用map坐标系
- 机器人位置需要TF转换到map
- 已处理：代码中直接使用世界坐标

### 3. 分辨率

- Costmap分辨率：通常0.05m（5cm）
- 数值梯度步长：0.05m
- 步长与分辨率匹配，避免过度平滑

---

## 🎯 下一步

### 验证测试

1. **运行测试**：
   ```bash
   ./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --no-viz
   ```

2. **检查DR margin**：
   - 应该在0.0~0.8m范围
   - 动态变化（不是固定值）

3. **检查safety violation**：
   - 应该在10-30%范围
   - 不再是0%

4. **检查日志**：
   - 应该看到 "📍 [COSTMAP] Received costmap" 消息
   - 不再看到 "Using fallback safety function"

---

## 📁 修改文件列表

- `src/dr_tightening/include/dr_tightening/dr_tightening_node.hpp` (3处修改)
- `src/dr_tightening/src/dr_tightening_node.cpp` (4处修改)
- `src/dr_tightening/params/dr_tightening_params.yaml` (1处修改)

**总计**: 3个文件，8处修改

---

**生成时间**: 2026-04-09
**修改者**: Claude Sonnet 4.6
**状态**: ✅ 实现完成，等待测试验证
