# STL Robustness值修复最终报告

**日期**: 2026-04-09
**状态**: ✅ **完全解决**

---

## 🎯 问题描述

STL的robustness值持续增大（越变越负），从-7.84603增大到-8.28828，不符合预期。

**目标**: 使robustness值稳定在[-1.5, 1.5]范围内。

---

## 🔍 问题分析过程

### 第一轮调查：MPC轨迹坐标更新问题

**假设**: MPC轨迹没有实时更新，导致距离计算错误。

**调查结果**:
- ✅ MPC轨迹实时更新（msg_time: 0.000 sec ago）
- ✅ 轨迹点数正常（20个点）
- ✅ 轨迹坐标在变化（从[-7.759, -0.052] → [-7.515, -0.298]）

**结论**: MPC轨迹更新正常，问题不在这里。

---

### 第二轮调查：机器人位置更新延迟问题

**假设**: STL模块使用的机器人位置过期（amcl_pose更新太慢）。

**调查结果**:
- ❌ 机器人位置一直不变：[-7.995, 0.011]
- ❌ 位置时间戳延迟：age: **4.029s**（4秒！）
- ✅ MPC轨迹在移动：traj_first从[-7.759, -0.052] → [-7.515, -0.298]

**根本原因**:
```
距离 = 机器人位置（4秒前的旧位置）到 MPC轨迹起点（当前位置）
      = 过期的机器人位置到新的轨迹起点
      → 距离持续增大
```

**修复方案**: 改用TF直接查询机器人位置（不依赖amcl_pose/odom发布频率）

**修复文件**: `src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.cpp`
- 移除`pose_sub_`订阅（不再订阅/amcl_pose）
- 在`evaluateSTL()`中直接使用TF查询：
  ```cpp
  geometry_msgs::TransformStamped transform = tf_buffer_->lookupTransform(
      "map", "base_footprint", ros::Time(0), ros::Duration(0.1));
  double x = transform.transform.translation.x;
  double y = transform.transform.translation.y;
  ```

**修复效果**:
- ✅ 机器人位置实时更新：[-7.982, 0.011] → [-7.715, -0.072] → [-7.388, -0.437]
- ✅ 距离很小：0.002m → 0.005m → 0.003m
- ✅ 位置延迟消失（使用TF查询最新位置）

---

### 第三轮调查：Safe-Regret MPC内部STL计算错误

**问题**: 虽然机器人位置修复了，但robustness值还是-7到-8。

**发现**: 有两个地方在计算STL robustness：
1. `stl_monitor`节点（我们刚刚修复的）
2. `Safe-Regret MPC`求解器内部（`STLConstraintIntegrator`）

**日志显示**:
```
STL robustness: -7.84603, budget: 0
STL robustness: -7.89215, budget: 0
STL robustness: -8.07126, budget: 0
```

这些日志来自`SafeRegretMPC.cpp`第143-144行，不是`stl_monitor`节点！

**根本原因**: `STLConstraintIntegrator.cpp`第72-73行：
```cpp
// Simple robustness: negative distance to goal
double distance = trajectory[0].norm();  // ❌ 错误！计算到原点的距离
return baseline_ - distance;
```

**问题**:
- 计算的是`trajectory[0].norm()`（轨迹第一个点到原点的距离）
- 而不是机器人到轨迹的距离！
- 机器人在(-8, 0)，`trajectory[0].norm()` ≈ 8m
- `baseline_ - distance` ≈ 0.5 - 8 = **-7.5** ✅

**修复方案**: 修改`computeBeliefRobustness()`函数，计算机器人到轨迹的最小距离

**修复文件**: `src/safe_regret_mpc/src/STLConstraintIntegrator.cpp`
- 修改第63-74行的`computeBeliefRobustness()`函数
- 计算机器人位置（belief_mean）到轨迹（trajectory）的最小距离
- 使用点到线段距离算法（与`stl_monitor`一致）

**修复代码**:
```cpp
// ✅ CRITICAL FIX: Calculate distance from robot to path
double robot_x = belief_mean(0);
double robot_y = belief_mean(1);

// Find minimum distance from robot to any segment in the trajectory
double min_dist = std::numeric_limits<double>::max();

for (size_t i = 0; i < trajectory.size() - 1; ++i) {
    // ... 点到线段距离计算 ...
}

// Robustness: threshold - distance
return baseline_ - min_dist;
```

---

## ✅ 最终结果

### 修复前
```
STL robustness: -7.84603, budget: 0
STL robustness: -7.89215, budget: 0
STL robustness: -8.07126, budget: 0
STL robustness: -8.17536, budget: 0
STL robustness: -8.28828, budget: 0
```
- **范围**: [-8.29, -7.79]
- **趋势**: 持续增大（越变越负）
- **状态**: ❌ 不符合目标

### 修复后
```
STL robustness: 0.0719498, budget: 0.97195
STL robustness: 0.0738219, budget: 0.945772
STL robustness: 0.0361382, budget: 0.88191
STL robustness: 0.0295814, budget: 0.811491
STL robustness: 0.0276153, budget: 0.739107
...
STL robustness: 0.0987758, budget: ...
STL robustness: 0.099268, budget: ...
STL robustness: 0.0993355, budget: ...
STL robustness: 0.0997029, budget: ...
STL robustness: 0.0998544, budget: ...
```
- **范围**: [-0.033, 0.100]
- **趋势**: 稳定，在0附近波动
- **状态**: ✅ **完全符合目标** [-1.5, 1.5]

### 统计数据
- 总共181个robustness值
- 最小值：-0.033
- 最大值：0.100
- 平均值：约0.06
- 标准差：约0.03

---

## 📁 修改的文件

### 1. STL Monitor节点（机器人位置更新修复）
**文件**: `src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.cpp`

**修改**:
- 移除`pose_sub_`订阅（第65行）
- 在`evaluateSTL()`中添加TF查询（第249-255行）
- 移除`poseCallback()`函数
- 移除`current_pose_`成员变量

**关键代码**:
```cpp
// ✅ CRITICAL FIX: Use TF to get LATEST robot position
geometry_msgs::TransformStamped transform = tf_buffer_->lookupTransform(
    "map", "base_footprint", ros::Time(0), ros::Duration(0.1));
double x = transform.transform.translation.x;
double y = transform.transform.translation.y;
```

### 2. Safe-Regret MPC求解器（STL距离计算修复）
**文件**: `src/safe_regret_mpc/src/STLConstraintIntegrator.cpp`

**修改**:
- 重写`computeBeliefRobustness()`函数（第63-74行）
- 计算机器人到轨迹的最小距离（而不是轨迹到原点的距离）

**关键代码**:
```cpp
// ✅ CRITICAL FIX: Calculate distance from robot to path
double robot_x = belief_mean(0);
double robot_y = belief_mean(1);

// Find minimum distance from robot to any segment
double min_dist = std::numeric_limits<double>::max();
for (size_t i = 0; i < trajectory.size() - 1; ++i) {
    // 点到线段距离计算
    ...
}

return baseline_ - min_dist;
```

---

## 🎯 验证方法

### 运行测试
```bash
# 清理ROS进程
./test/scripts/clean_ros.sh

# 运行测试
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --timeout 60 --no-viz
```

### 检查结果
```bash
# 查看robustness值
grep "STL robustness:" test_results/safe_regret_YYYYMMDD_HHMMSS/test_01_shelf_01/logs/launch.log

# 统计范围
grep "STL robustness:" test_results/safe_regret_YYYYMMDD_HHMMSS/test_01_shelf_01/logs/launch.log | \
  awk '{print $3}' | sort -n | head -5  # 最小值
grep "STL robustness:" test_results/safe_regret_YYYYMMDD_HHMMSS/test_01_shelf_01/logs/launch.log | \
  awk '{print $3}' | sort -n | tail -5  # 最大值
```

---

## 📊 技术总结

### 问题根源
1. **机器人位置过期**: 使用`amcl_pose`（2-5Hz）导致位置延迟4秒
2. **错误的距离计算**: Safe-Regret MPC内部计算的是轨迹到原点的距离，而不是机器人到轨迹的距离

### 解决方案
1. **使用TF查询最新位置**: 不依赖amcl_pose/odom发布频率
2. **修复距离计算算法**: 计算机器人到轨迹的最小距离

### 关键技术点
- **TF变换**: 使用`ros::Time(0)`查询最新变换
- **点到线段距离**: 投影参数t，clamped to [0, 1]
- **STL robustness**: `robustness = threshold - distance`

---

## ✅ 验证状态

- ✅ STL robustness值稳定在[-0.033, 0.100]范围内
- ✅ 完全符合目标[-1.5, 1.5]
- ✅ 机器人位置实时更新（无延迟）
- ✅ 距离计算正确（机器人到轨迹）
- ✅ Budget机制正常工作

---

## 🎉 结论

**问题已完全解决！** STL robustness值现在稳定在目标范围内，系统运行正常。

**关键改进**:
1. 机器人位置更新延迟从4秒降到接近0秒（使用TF）
2. 距离计算从错误（轨迹到原点）修复为正确（机器人到轨迹）
3. Robustness值从[-8.29, -7.79]修复为[-0.033, 0.100]

**下一步**:
- 继续进行长时间测试，验证稳定性
- 可以调整`baseline_`参数来改变robustness的偏移量
- 可以调整距离阈值来改变robustness的敏感度

---

**生成时间**: 2026-04-09
**测试目录**: `test_results/safe_regret_20260409_005106/`
