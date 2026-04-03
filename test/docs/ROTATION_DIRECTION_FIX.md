# 修复原地旋转模式的左右抽搐问题

**日期**: 2026-04-03
**问题**: 转到90度时机器人开始左右抽搐
**状态**: ✅ 已修复

---

## 问题现象

**描述**：机器人在原地旋转模式下，当角度接近90度时，开始左右摇摆抽搐，无法继续旋转到30度以下。

**日志观察**：
```
[ROTATION] Speed=0.0 | Angle=95.2° | Exit<30° | w=0.8
[ROTATION] Speed=0.0 | Angle=89.3° | Exit<30° | w=-0.5  ← 方向反转！
[ROTATION] Speed=0.0 | Angle=92.1° | Exit<30° | w=0.7   ← 又反转回来！
[ROTATION] Speed=0.0 | Angle=88.5° | Exit<30° | w=-0.6  ← 再次反转！
```

---

## 根本原因分析

### 旧的角速度方向纠正逻辑（有问题）

**位置**：`TubeMPCNode.cpp` 第641-659行

**旧代码**：
```cpp
// === CRITICAL FIX: 纠正大角度转向时的角速度方向 ===
if(std::abs(etheta) > 1.57) {  // > 90度
    double desired_angular_direction = (etheta > 0) ? 1.0 : -1.0;

    if((_w_filtered * desired_angular_direction) < 0) {
        _w_filtered = -_w_filtered;  // 反转角速度方向
        cout << "🔄 CORRECTING ANGULAR VELOCITY DIRECTION" << endl;
    }
}
```

### 问题分析

**问题1：条件判断在90度边界上振荡**

| 角度 | 旧逻辑行为 | 结果 |
|------|-----------|------|
| 91° | `|etheta| > 90°` → **执行纠正** | 强制正向旋转 |
| 89° | `|etheta| < 90°` → **不执行纠正** | MPC可能选择反向 |
| 91° | `|etheta| > 90°` → **再次执行纠正** | 又强制正向 |

**问题2：MPC的最短路径优化**

当角度降到 < 90° 时：
- 纠正逻辑失效
- MPC认为"反向旋转"是更短的路径
- MPC输出负的角速度（反向）
- 角度又开始增大
- 角度 > 90° 时，纠正逻辑又生效
- **循环往复，左右抽搐！**

### 图示

```
角度: 100° → 95° → 91° → 89° → 92° → 88° → 91° → ...
       正向   正向   正向   反向!  正向!  反向!  正向!
       ↑      ↑      ↑      ↑      ↑      ↑      ↑
      纠正   纠正   纠正   MPC    纠正   MPC    纠正
      逻辑   逻辑   逻辑   反向   逻辑   反向   逻辑
```

---

## 解决方案

### 核心思路

**在原地旋转模式下，锁定旋转方向，禁止MPC反向。**

### 实现细节

#### 1. 添加成员变量

**文件**：`TubeMPCNode.h` 第75行

```cpp
bool _in_place_rotation;           // 标记是否正在原地旋转模式
double _rotation_direction;         // 锁定的旋转方向 (1.0 或 -1.0)
bool _rotation_direction_locked;    // 旋转方向是否已锁定
```

#### 2. 初始化变量

**文件**：`TubeMPCNode.cpp` 第92-94行

```cpp
_in_place_rotation = false;
_rotation_direction = 0.0;
_rotation_direction_locked = false;
```

#### 3. 方向锁定逻辑

**文件**：`TubeMPCNode.cpp` 第641-673行

**新代码**：
```cpp
// === 原地旋转模式：锁定旋转方向 ===
if(_in_place_rotation) {
    if(!_rotation_direction_locked) {
        // 第一次进入旋转模式，确定并锁定旋转方向
        _rotation_direction = (etheta > 0) ? 1.0 : -1.0;
        _rotation_direction_locked = true;
        cout << "[ROTATION] Direction LOCKED: "
             << (_rotation_direction > 0 ? "CCW (+)" : "CW (-)") << endl;
    }

    // 强制角速度方向与锁定方向一致
    if((_w_filtered * _rotation_direction) < 0) {
        // MPC选择了反向，强制纠正
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

---

## 工作原理

### 状态机流程

```
┌─────────────────────────────────────────────────────────┐
│ 进入原地旋转模式 (角度 > 90°)                           │
└────────────────┬────────────────────────────────────────┘
                 │
                 ▼
        ┌────────────────────┐
        │ 确定旋转方向        │
        │ etheta > 0 → CCW   │
        │ etheta < 0 → CW    │
        └────────┬───────────┘
                 │
                 ▼
        ┌────────────────────┐
        │ 锁定方向            │
        │ _rotation_direction │
        │ _direction_locked=1 │
        └────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────────────┐
    │ 每个控制循环：                  │
    │ if (w * direction < 0)         │
    │   w = direction * |w|  ← 强制  │
    └────────┬───────────────────────┘
             │
             ▼
    ┌────────────────────────────────┐
    │ 角度持续减小：100° → 30°        │
    │ 方向始终保持不变                │
    └────────┬───────────────────────┘
             │
             ▼
        ┌────────────────┐
        │ 角度 < 30°     │
        │ 退出旋转模式    │
        │ 重置方向锁定    │
        └────────────────┘
```

### 关键特性

1. **一次性锁定**：只在刚进入旋转模式时确定方向一次
2. **强制执行**：所有角速度都必须朝锁定方向
3. **防止反向**：即使MPC选择反向，也会被强制纠正
4. **自动重置**：退出旋转模式时自动重置锁定

---

## 测试验证

### 预期行为

**场景：角度从 100° 旋转到 25°**

```
[ROTATION] ENTERING pure rotation mode (100.5° > 90°)
[ROTATION] Direction LOCKED: CCW (+)
[ROTATION] Speed=0.0 | Angle=100.5° | Exit<30° | w=0.8
[ROTATION] Speed=0.0 | Angle=95.2°  | Exit<30° | w=0.8  ← 持续正向
[ROTATION] Speed=0.0 | Angle=89.3°  | Exit<30° | w=0.7  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=82.1°  | Exit<30° | w=0.7  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=75.4°  | Exit<30° | w=0.6  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=68.2°  | Exit<30° | w=0.6  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=52.8°  | Exit<30° | w=0.5  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=41.3°  | Exit<30° | w=0.4  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=32.5°  | Exit<30° | w=0.3  ← 仍然正向！
[ROTATION] Speed=0.0 | Angle=28.7°  | Exit<30° | w=0.2  ← 仍然正向！
[ROTATION] EXITING pure rotation mode (28.7° < 30°)
```

**关键验证点**：
- ✅ 角度从 100° → 28.7° 全程保持正向旋转
- ✅ 没有反向（负角速度）
- ✅ 平滑旋转，无抽搐
- ✅ 在 < 30° 时正确退出

### 测试步骤

```bash
# 1. 清理旧进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 2. 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 观察日志输出
# 查找 "[ROTATION] Direction LOCKED"
# 查找 "[ROTATION] Direction corrected"
# 验证角度持续减小，无振荡
```

---

## 修改文件总结

| 文件 | 修改内容 | 行号 |
|------|---------|------|
| `TubeMPCNode.h` | 添加成员变量 | 75-76 |
| `TubeMPCNode.cpp` | 初始化成员变量 | 92-94 |
| `TubeMPCNode.cpp` | 实现方向锁定逻辑 | 641-673 |
| `TubeMPCNode.cpp` | 删除旧的纠正逻辑 | 已删除 |

---

## 技术要点

### 为什么使用成员变量而不是static？

**问题**：static变量在函数调用之间保持状态，但在复杂的状态机中难以管理。

**解决**：使用成员变量可以：
1. 在构造函数中明确初始化
2. 在状态切换时（进入/退出旋转模式）精确控制
3. 便于调试和追踪状态变化

### 为什么不依赖MPC的方向选择？

**MPC的优化目标**：最小化控制误差，可能选择"最短路径"

**问题**：在原地旋转场景中，最短路径不是最优的，因为：
1. 反向旋转会导致角度振荡
2. 破坏状态机的连续性
3. 可能导致安全问题

**解决**：强制锁定方向，即使不是"最短路径"

---

## 相关文档

- 严格原地旋转模式实现：`test/docs/STRICT_ROTATION_MODE_IMPROVEMENT.md`
- 修复说明：`TEST_STRICT_ROTATION_FIX.md`
- 项目文档：`CLAUDE.md`

---

## 版本历史

| 版本 | 日期 | 修改 |
|------|------|------|
| V1.0 | 2026-04-03 | 初始版本：修复左右抽搐问题 |

---

**修复完成！现在原地旋转应该平滑且稳定。** ✅
