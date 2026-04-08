# MPC轨迹接收和保存问题分析

**日期**: 2026-04-08
**问题**: 距离持续增大，MPC轨迹正常更新，但STL模块接收/保存有问题

---

## ✅ MPC轨迹更新正常

实测数据：
```
MPC Traj received (count=1): 20 poses, msg_time: 0.001 sec ago, frame: base_footprint
MPC Traj received (count=2): 20 poses, msg_time: 0.000 sec ago, frame: base_footprint
MPC Traj received (count=3): 20 poses, msg_time: 0.000 sec ago, frame: base_footprint
```

**结论**：
- ✅ MPC轨迹实时更新（延迟<0.001秒）
- ✅ 轨迹点数正常（20个点）
- ✅ 使用base_footprint坐标系

---

## ❌ 距离持续增大

实测数据：
```
dist=0.257 m, to_first=0.257 m, traj_points=20
dist=0.685 m, to_first=0.685 m, traj_points=20
dist=1.044 m, to_first=1.044 m, traj_points=20
...
dist=1.948 m, to_first=1.948 m, traj_points=20
```

**关键发现**：
- ❌ `to_first ≈ dist` - 最小距离就是到轨迹起点的距离
- ❌ 机器人逐渐远离轨迹起点（0.26m → 1.95m）

---

## 🔍 可能的原因

### 1. TF变换时间戳问题 ✅ 已修复

**修复前**：
```cpp
tf_buffer_->lookupTransform("map", msg->header.frame_id, msg->header.stamp, ...);
```
- 使用msg的timestamp（可能是预测时间）
- 导致查询到旧的TF变换

**修复后**：
```cpp
tf_buffer_->lookupTransform("map", msg->header.frame_id, ros::Time(0), ...);
```
- 使用ros::Time(0)查询最新TF变换
- ✅ 修复完成

### 2. 订阅队列问题 ✅ 已修复

**修复前**：队列大小=10，可能积压旧消息

**修复后**：队列大小=1，始终使用最新轨迹

### 3. 机器人位置时间戳延迟 ⚠️ 待验证

**可能问题**：
- `current_pose_` (amcl_pose)的时间戳可能是旧的
- 导致机器人位置不更新
- 与最新轨迹的起点不匹配

**验证方法**：
```cpp
ros::Time robot_time = current_pose_->header.stamp;
ros::Time now = ros::Time::now();
double robot_time_diff = (now - robot_time).toSec();
```

---

## 📊 需要验证的假设

### 假设1：MPC轨迹起点不是机器人当前位置

MPC预测轨迹的第一个点可能是：
- 下一时间步的位置（t+Δt）
- 而不是当前位置（t）

**验证方法**：对比机器人位置和轨迹起点坐标

### 假设2：amcl_pose发布频率太低

**可能情况**：
- amcl_pose: 2-5 Hz
- MPC轨迹: 10 Hz
- 导致机器人位置更新滞后

**解决方案**：
- 增加amcl_pose发布频率
- 或者使用odom（更频繁）

### 假设3：坐标系不匹配

**frame_id对比**：
- MPC轨迹: base_footprint
- 机器人定位: map（amcl_pose）
- TF查询: base_footprint → map

**可能问题**：TF树中base_footprint → map的变换不准确

---

## 🔧 下一步调试

1. **检查机器人位置时间戳**：
   - 验证`current_pose_`的时间戳是否是当前时间
   - 如果延迟很大，说明amcl_pose更新慢

2. **对比位置坐标**：
   - 机器人位置 vs 轨迹起点
   - 看是否存在系统性偏移

3. **检查odom vs amcl_pose**：
   - 尝试使用odom而不是amcl_pose
   - odom更新频率更高（通常50Hz）

---

**分析者**: Claude Code
**日期**: 2026-04-08
**状态**: 🔍 **调查中**
