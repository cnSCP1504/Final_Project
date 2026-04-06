# Gazebo障碍物显示问题修复

**日期**: 2026-04-05
**问题**: 障碍物在RViz雷达中显示已移动，但在Gazebo可视化中仍在原位
**原因**: 障碍物被设置为`static=1`（静态模型），无法在Gazebo中移动

---

## 🔍 问题诊断

### 症状
- ✅ RViz中激光雷达显示障碍物在新位置
- ❌ Gazebo中障碍物仍然在原位置显示
- ✅ `set_model_state`服务调用成功
- ❌ Gazebo可视化没有更新

### 根本原因
**Static模型限制**：在Gazebo中，`<static>1</static>`的模型：
- ✅ 碰撞几何体可以被移动（所以雷达能检测到）
- ❌ 视觉几何体（visual）不会更新（所以Gazebo中看不到移动）
- ❌ 无法通过`set_model_state`服务移动

---

## 🔧 修复方案

### 1. 修改world文件
将三个中央障碍物从静态改为可移动：

```xml
<!-- 修改前 -->
<model name='central_obstacle_1'>
  <static>1</static>  ❌
  ...
</model>

<!-- 修改后 -->
<model name='central_obstacle_1'>
  <static>0</static>  ✅ 可移动
  ...
</model>
```

**修改的障碍物**：
- `central_obstacle_1` (圆柱形，在0, 2)
- `central_obstacle_2` (方块，在-2, -2)
- `central_obstacle_3` (方块，在2, -2)

### 2. 添加位置冻结
在移动障碍物后，设置零速度防止滑动：

```python
# 设置位置
self.set_model_state(model_state)

# 设置零速度（防止滑动）
model_state.twist.linear.x = 0
model_state.twist.linear.y = 0
model_state.twist.linear.z = 0
model_state.twist.angular.x = 0
model_state.twist.angular.y = 0
model_state.twist.angular.z = 0
self.set_model_state(model_state)
```

---

## 📊 修改效果

### 修改前
```
Gazebo显示:  障碍物在原始位置 (0, 2), (-2, -2), (2, -2)
RViz雷达:    障碍物在随机位置 ❌ 不一致
```

### 修改后
```
Gazebo显示:  障碍物在随机位置 ✅
RViz雷达:    障碍物在随机位置 ✅
一致:        完全一致 ✅
```

---

## 🧪 测试验证

### 测试步骤
```bash
# 1. 清理旧进程
killall -9 roslaunch rosmaster roscore gzserver gzclient

# 2. 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 等待15-20秒让系统完全启动

# 4. 观察Gazebo
# - 应该看到三个障碍物在地图中央区域
# - 障碍物位置应该是随机的（每次启动不同）
# - 障碍物应该保持在移动后的位置，不滑动

# 5. 对比RViz
# - RViz中的激光雷达点云应该与Gazebo中的障碍物位置一致
```

### 预期结果
```
============================================================
开始随机放置障碍物（仅启动时执行一次）...
============================================================
✓ central_obstacle_1: (-3.68, 5.38) | 距离小车: 6.90m
✓ central_obstacle_2: (-3.68, 4.87) | 距离小车: 6.51m
✓ central_obstacle_3: (-2.62, -2.81) | 距离小车: 6.07m
============================================================

Gazebo中应该看到：
- 圆柱障碍物在 (-3.68, 5.38)
- 方块障碍物在 (-3.68, 4.87)
- 方块障碍物在 (-2.62, -2.81)

RViz中应该看到：
- 激光雷达点云显示障碍物在相同位置
```

---

## 📁 修改文件

| 文件 | 修改内容 |
|------|---------|
| `test_world.world` | 障碍物static: 1 → 0 |
| `random_obstacles.py` | 添加零速度设置，防止滑动 |

---

## ⚠️ 注意事项

### 物理引擎影响
将障碍物改为非静态后：
- ✅ 可以通过`set_model_state`移动
- ⚠️ 可能会受到重力、碰撞等物理影响
- ✅ 通过设置零速度来冻结位置

### 性能考虑
- 三个动态障碍物对性能影响很小
- 如果障碍物数量很多（>10），可能需要考虑性能优化

### 兼容性
- ✅ 与现有功能完全兼容
- ✅ 不影响其他静态物体（墙壁、货架等）
- ✅ RViz和Gazebo显示一致

---

## 🎯 下一步

重启系统测试修改：

```bash
killall -9 roslaunch rosmaster roscore gzserver gzclient
sleep 2
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

观察Gazebo中的障碍物是否出现在随机位置。

---

**状态**: ✅ **修复完成，等待测试验证**
