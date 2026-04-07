# 原地旋转方向错误修复

## 问题描述

**症状**: 原地掉头时旋转方向完全错误
- 应该左转（逆时针）时却右转（顺时针）
- 应该右转（顺时针）时却左转（逆时针）

**根本原因**: 旋转方向计算逻辑完全相反

## 问题分析

### ROS坐标系规范

在ROS标准坐标系中：
- **角速度为正（>0）** = 逆时针旋转（CCW，Counter-Clockwise）
- **角速度为负（<0）** = 顺时针旋转（CW，Clockwise）
- **etheta** = 目标角度 - 当前角度

### 错误逻辑（修复前）

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp` (line 651)

```cpp
// ❌ 错误的逻辑
_rotation_direction = (etheta > 0) ? -1.0 : 1.0;
```

**行为分析**:
- etheta > 0 → _rotation_direction = -1.0（负方向，顺时针）❌ **反了！**
- etheta < 0 → _rotation_direction = 1.0（正方向，逆时针）❌ **反了！**

**举例说明**:
1. **场景1**: 当前朝向 0°（东），目标朝向 90°（北）
   - etheta = 90° - 0° = 90° > 0
   - 需要逆时针转（0° → 90°）
   - 角速度应该为正（>0）
   - **错误代码**: _rotation_direction = -1.0（顺时针）❌
   - **结果**: 机器人朝错误方向旋转！

2. **场景2**: 当前朝向 90°（北），目标朝向 0°（东）
   - etheta = 0° - 90° = -90° < 0
   - 需要顺时针转（90° → 0°）
   - 角速度应该为负（<0）
   - **错误代码**: _rotation_direction = 1.0（逆时针）❌
   - **结果**: 机器人朝错误方向旋转！

### 正确逻辑（修复后）

```cpp
// ✅ 正确的逻辑
_rotation_direction = (etheta >= 0) ? 1.0 : -1.0;
```

**行为分析**:
- etheta > 0 → _rotation_direction = 1.0（正方向，逆时针）✅ **正确！**
- etheta < 0 → _rotation_direction = -1.0（负方向，顺时针）✅ **正确！**

**举例说明**:
1. **场景1**: 当前朝向 0°（东），目标朝向 90°（北）
   - etheta = 90° - 0° = 90° > 0
   - 需要逆时针转
   - **正确代码**: _rotation_direction = 1.0（逆时针）✅
   - **结果**: 机器人正确旋转！

2. **场景2**: 当前朝向 90°（北），目标朝向 0°（东）
   - etheta = 0° - 90° = -90° < 0
   - 需要顺时针转
   - **正确代码**: _rotation_direction = -1.0（顺时针）✅
   - **结果**: 机器人正确旋转！

## 修复详情

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

**修改行**: Line 651

**修改前**:
```cpp
// ✅ FIX: 修正方向计算，与SafeRegretMPC保持一致
// etheta > 0 表示需要逆时针旋转（负方向），etheta < 0 表示需要顺时针旋转（正方向）
_rotation_direction = (etheta > 0) ? -1.0 : 1.0;
```

**修改后**:
```cpp
// ✅ FIX: 修正方向计算错误（2026-04-06）
// 在ROS坐标系中：
//   - 角速度为正（>0）= 逆时针旋转（CCW）
//   - 角速度为负（<0）= 顺时针旋转（CW）
//   - etheta = 目标角度 - 当前角度
// 因此：
//   - etheta > 0 表示需要逆时针旋转 → 角速度为正
//   - etheta < 0 表示需要顺时针旋转 → 角速度为负
_rotation_direction = (etheta >= 0) ? 1.0 : -1.0;
```

## 测试验证

### 测试场景

**场景1**: 大角度左转
- 当前朝向: 0°（东）
- 目标朝向: 90°（北）
- 预期: 逆时针旋转（角速度为正）

**场景2**: 大角度右转
- 当前朝向: 90°（北）
- 目标朝向: 0°（东）
- 预期: 顺时针旋转（角速度为负）

**场景3**: 180°掉头
- 当前朝向: 0°（东）
- 目标朝向: 180°（西）
- 预期: 逆时针旋转到180°（角速度为正）

### 测试命令

```bash
cd /home/dixon/Final_Project/catkin

# 清理之前的ROS进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python
sleep 2

# 重新编译
catkin_make
source devel/setup.bash

# 运行测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch debug_mode:=true
```

### 预期日志输出

```
[ROTATION] ENTERING pure rotation mode (105° > 90°)
[ROTATION] Direction LOCKED: CCW (+)  # etheta > 0，应该是逆时针
[ROTATION] Speed LOCKED at 0.0 m/s | Angle: 95° | Exit at: <10° | Angular vel: 0.8 rad/s
[ROTATION] Speed LOCKED at 0.0 m/s | Angle: 85° | Exit at: <10° | Angular vel: 0.8 rad/s
...
[ROTATION] EXITING pure rotation mode (8° < 10°)
```

## 影响范围

### 受影响的功能
1. ✅ 原地旋转模式（>90°角度）
2. ✅ 方向锁定机制
3. ✅ 所有需要大角度转动的场景

### 不受影响的功能
1. ✅ 小角度调整（<90°）
2. ✅ 正常路径跟踪
3. ✅ 速度控制逻辑

## 相关文档

- `test/docs/ROTATION_DIRECTION_FIX.md` - 历史方向修复记录
- `test/docs/STRICT_ROTATION_MODE_IMPROVEMENT.md` - 严格旋转模式改进
- `CLAUDE.md` - 项目总体说明

## 修改时间

2026-04-06

## 状态

✅ **修复完成，待测试验证**

## 注意事项

1. **需要重新编译**: 修改了C++代码，必须重新编译
   ```bash
   catkin_make
   source devel/setup.bash
   ```

2. **清理旧进程**: 测试前必须清理之前的ROS进程
   ```bash
   killall -9 roslaunch rosmaster roscore gzserver gzclient python
   ```

3. **验证日志**: 观察日志中的旋转方向输出
   - `CCW (+)` = 逆时针（正方向）
   - `CW (-)` = 顺时针（负方向）

4. **边界情况**: etheta = 0 时选择正方向（使用 `>=` 而非 `>`）
   - 避免零值时的不确定性
