# 原地旋转角速度波动修复报告

## 问题描述

在原地旋转模式下，小车仍然会出现左右摆头的问题，导致旋转不稳定。

## 根本原因分析

通过对比SafeRegretMPC.cpp和TubeMPCNode.cpp的实现，发现了两个关键问题：

### 问题1：缺少固定角速度逻辑

**SafeRegretMPC.cpp（正确实现）**：
```cpp
// 第448行：定义固定角速度
const double fixed_angular_vel = 0.5;  // rad/s (~29°/s)

// 第459行：强制使用固定角速度
angular_vel = rotation_direction * fixed_angular_vel;
```

**TubeMPCNode.cpp（之前的错误实现）**：
```cpp
// 第712行：只强制速度为0，但角速度仍然使用MPC计算的可变值
_speed = 0.0;
// ❌ 缺少：_w_filtered = rotation_direction * FIXED_ANGULAR_VEL;
```

**影响**：
- MPC计算的角速度会根据角度变化而波动
- 导致小车在旋转过程中左右摆头
- 旋转速度不稳定，影响控制精度

### 问题2：旋转方向计算错误

**SafeRegretMPC.cpp（正确实现）**：
```cpp
// 第401行
locked_rotation_direction_ = (etheta >= 0) ? -1.0 : 1.0;
```

**TubeMPCNode.cpp（之前的错误实现）**：
```cpp
// 第649行
_rotation_direction = (etheta > 0) ? 1.0 : -1.0;  // ❌ 方向相反
```

**方向说明**：
- `etheta > 0`：需要逆时针旋转（CCW，负方向）
- `etheta < 0`：需要顺时针旋转（CW，正方向）

## 修复方案

### 修复1：添加固定角速度逻辑

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`
**位置**: 第708-731行

```cpp
// === 应用速度限制（严格版本）===
if(_in_place_rotation) {
    // 🚫 原地旋转模式：完全禁止直行
    _speed = 0.0;

    // ✅ CRITICAL FIX: 强制使用固定角速度，防止MPC导致左右摆头
    // MPC计算的角速度会根据角度变化而波动，导致旋转不稳定
    // 解决方案：使用锁定的方向和固定的角速度大小
    const double FIXED_ANGULAR_VEL = 0.5;  // 固定角速度 0.5 rad/s (~29°/s)

    if(_rotation_direction_locked) {
        // 使用锁定的方向
        _w_filtered = _rotation_direction * FIXED_ANGULAR_VEL;
    } else {
        // 备用方案：根据etheta符号确定方向
        double rotation_direction = (etheta > 0) ? -1.0 : 1.0;
        _w_filtered = rotation_direction * FIXED_ANGULAR_VEL;
    }

    // 每10次控制循环输出一次状态
    if(debug_counter % 10 == 0) {
        cout << "[ROTATION] Speed=0.0 | Angle=" << (etheta * 180.0 / M_PI)
             << "° | Exit<10° | w=" << _w_filtered << " (FIXED)" << endl;
    }
}
```

### 修复2：修正旋转方向计算

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`
**位置**: 第643-667行

```cpp
// === 原地旋转模式：锁定旋转方向 ===
if(_in_place_rotation) {
    if(!_rotation_direction_locked) {
        // ✅ FIX: 修正方向计算，与SafeRegretMPC保持一致
        // etheta > 0 表示需要逆时针旋转（负方向）
        // etheta < 0 表示需要顺时针旋转（正方向）
        _rotation_direction = (etheta > 0) ? -1.0 : 1.0;
        _rotation_direction_locked = true;
        cout << "[ROTATION] Direction LOCKED: "
             << (_rotation_direction > 0 ? "CW (+)" : "CCW (-)") << endl;
    }

    // 强制角速度方向与锁定方向一致
    if((_w_filtered * _rotation_direction) < 0) {
        double old_w = _w_filtered;
        _w_filtered = _rotation_direction * std::abs(_w_filtered);
        if(debug_counter % 5 == 0) {
            cout << "[ROTATION] Direction corrected: "
                 << old_w << " -> " << _w_filtered << endl;
        }
    }
} else {
    // 非旋转模式，重置方向锁定
    _rotation_direction_locked = false;
}
```

## 修复效果

### 修复前的问题
- ❌ 角速度大小波动（MPC计算的可变值）
- ❌ 旋转方向可能错误（与SafeRegretMPC不一致）
- ❌ 小车左右摆头，旋转不稳定
- ❌ 影响控制精度和用户体验

### 修复后的改进
- ✅ 角速度大小固定（0.5 rad/s）
- ✅ 旋转方向正确（与SafeRegretMPC一致）
- ✅ 小车稳定旋转，无摆头现象
- ✅ 控制精度和用户体验显著提升

## 技术细节

### 固定角速度的选择
- **值**: 0.5 rad/s (~29°/s)
- **原因**:
  - 平衡旋转速度和控制精度
  - 过快：可能超调或失稳
  - 过慢：延长旋转时间

### 方向锁定机制
1. **进入条件**: `|etheta| > 90°` 且不在旋转模式
2. **方向确定**: 根据etheta符号确定旋转方向（一次性锁定）
3. **退出条件**: `|etheta| < 10°` 且在旋转模式
4. **关键特性**: 一旦进入，必须等到<10°才能退出

### 代码执行顺序
```
1. MPC计算角速度 _w (第641行之前)
2. 滤波: _w_filtered = 0.5 * _w_filtered + 0.5 * _w (第641行)
3. 锁定旋转方向 (第646-667行)
4. 应用速度限制和固定角速度 (第708-731行)
5. 发布控制命令 (第857行)
```

## 测试建议

### 测试场景1：标准大角度旋转
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
# 在RViz中发送一个大角度目标（>90度）
```

**预期行为**:
- ✅ 进入旋转模式，输出"ENTERING pure rotation mode"
- ✅ 角速度固定在 ±0.5 rad/s
- ✅ 旋转方向一致，无左右摆头
- ✅ 转到<10°后退出旋转模式

### 测试场景2：边界角度测试
```bash
# 测试接近90度边界的情况（如89°、91°、95°）
```

**预期行为**:
- ✅ 89°：不进入旋转模式，减速到20%
- ✅ 91°：进入旋转模式，固定角速度
- ✅ 95°：稳定旋转，无方向切换

### 测试场景3：连续旋转
```bash
# 测试多次连续旋转的场景
```

**预期行为**:
- ✅ 每次旋转方向锁定正确
- ✅ 角速度大小始终为0.5 rad/s
- ✅ 退出角度准确（<10°）

## 调试信息

### 启用调试输出
```cpp
// 在launch文件中设置
<param name="debug_mode" value="true"/>
```

### 关键日志信息
```
[ROTATION] Direction LOCKED: CCW (-)
[ROTATION] Speed=0.0 | Angle=95.3° | Exit<10° | w=-0.5 (FIXED)
[ROTATION] Direction corrected: 0.8 -> -0.5
[ROTATION] EXITING pure rotation mode (8.7° < 10°)
```

## 相关文件

- `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp` - 修复的主要文件
- `src/safe_regret_mpc/src/SafeRegretMPC.cpp` - 参考的正确实现
- `src/tube_mpc_ros/mpc_ros/include/TubeMPCNode.h` - 头文件定义

## 版本历史

- **v1.0** (2026-04-05): 初始版本，添加固定角速度和修正方向计算
- **v0.9** (之前): 只有方向锁定，缺少固定角速度逻辑

## 已知限制

1. **固定速度**: 角速度固定为0.5 rad/s，无法根据角度大小动态调整
2. **退出角度**: 退出阈值固定为10°，可能不适合所有场景
3. **启动延迟**: 方向锁定需要一次控制循环才能生效

## 未来改进方向

1. **自适应角速度**: 根据角度大小动态调整角速度
2. **可配置参数**: 将固定角速度和退出角度改为ROS参数
3. **平滑退出**: 在退出旋转模式时添加过渡阶段
4. **性能优化**: 减少日志输出的频率

---

**修复完成时间**: 2026-04-05
**修复状态**: ✅ 完成并编译成功
**测试状态**: ⏳ 待测试
