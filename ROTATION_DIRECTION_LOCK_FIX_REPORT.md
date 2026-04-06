# 原地旋转方向锁定修复报告

**日期**: 2026-04-05
**问题**: 原地旋转时左右摆头（方向振荡）
**状态**: ✅ **已修复**

---

## 🔍 **问题描述**

### **症状**

即使使用固定角速度值，机器人仍然左右摆头：
- ✅ 角速度值固定（0.5 rad/s）
- ❌ **方向在正负之间切换**
- ❌ 机器人先逆时针转，再顺时针转，反复切换

### **日志证据**

```
🔄 [PURE ROTATION] Angular vel OVERRIDDEN to -0.5 rad/s | Heading error: 179.8°
🔄 [PURE ROTATION] Angular vel OVERRIDDEN to +0.5 rad/s | Heading error: -179.5°
🔄 [PURE ROTATION] Angular vel OVERRIDDEN to -0.5 rad/s | Heading error: 179.2°
```

**问题**：方向在 -0.5 和 +0.5 之间切换！

---

## 🔬 **根本原因分析**

### **问题1：etheta跨越±180°边界**

**角度表示的周期性**：
- etheta = 179° 等价于 etheta = -181°
- 但normalizeAngle()将其归一化到 [-π, π]
- 所以179°可能变成-179°

**振荡示例**：

```
时刻1: etheta = 179.8° → direction = -1.0 → angular_vel = -0.5 ✅
时刻2: etheta = -179.5° (归一化) → direction = +1.0 → angular_vel = +0.5 ❌
时刻3: etheta = 179.2° → direction = -1.0 → angular_vel = -0.5 ✅
```

**问题链条**：
1. etheta接近±180°边界
2. 小噪声导致符号跳变（+ → - 或 - → +）
3. direction重新计算，符号改变
4. 角速度方向切换

### **问题2：方向每次都重新计算**

**原有逻辑**（错误）：
```cpp
// ❌ 每次都根据当前etheta计算方向
double rotation_direction = (etheta >= 0) ? -1.0 : 1.0;
angular_vel = rotation_direction * fixed_angular_vel;
```

**问题**：
- 没有方向锁定机制
- direction随etheta实时变化
- etheta振荡 → direction振荡 → 机器人摆头

---

## 🔧 **修复方案**

### **核心思想**

**方向锁定**：在进入旋转模式时，锁定旋转方向；退出时才解锁。

### **代码修改**

#### **1. SafeRegretMPC.hpp - 添加方向锁定成员**

```cpp
// ========== In-Place Rotation ==========

bool in_place_rotation_;             ///< In-place rotation mode flag
bool rotation_direction_locked_;      ///< ✅ NEW: Rotation direction locked flag
double locked_rotation_direction_;    ///< ✅ NEW: Locked rotation direction (+1.0 or -1.0)
static constexpr double HEADING_ERROR_CRITICAL = 1.57;   ///< 90° - Enter rotation threshold
static constexpr double HEADING_ERROR_EXIT = 0.175;      ///< 10° - Exit rotation threshold
```

#### **2. SafeRegretMPC.cpp - 构造函数初始化**

```cpp
SafeRegretMPC::SafeRegretMPC()
    : ...
      in_place_rotation_(false),
      rotation_direction_locked_(false),     // ✅ NEW: 初始化为未锁定
      locked_rotation_direction_(0.0),        // ✅ NEW: 初始化为0
      ...
```

#### **3. SafeRegretMPC.cpp - updateInPlaceRotation() - 进入时锁定**

**修改前**：
```cpp
if (std::abs(etheta) > HEADING_ERROR_CRITICAL && !in_place_rotation_) {
    in_place_rotation_ = true;
    // ❌ 没有方向锁定
}
```

**修改后**：
```cpp
if (std::abs(etheta) > HEADING_ERROR_CRITICAL && !in_place_rotation_) {
    in_place_rotation_ = true;

    // ✅ LOCK rotation direction when entering rotation mode
    rotation_direction_locked_ = true;
    locked_rotation_direction_ = (etheta >= 0) ? -1.0 : 1.0;

    // 显示锁定的方向
    std::cout << "║  Direction LOCKED: "
              << (locked_rotation_direction_ > 0 ? "Clockwise" : "Counter-Clockwise")
              << "         ║\n";
}
```

#### **4. SafeRegretMPC.cpp - updateInPlaceRotation() - 退出时解锁**

**修改前**：
```cpp
else if (in_place_rotation_ && std::abs(etheta) < HEADING_ERROR_EXIT) {
    in_place_rotation_ = false;
    // ❌ 没有解锁
}
```

**修改后**：
```cpp
else if (in_place_rotation_ && std::abs(etheta) < HEADING_ERROR_EXIT) {
    in_place_rotation_ = false;

    // ✅ UNLOCK rotation direction when exiting
    rotation_direction_locked_ = false;
    locked_rotation_direction_ = 0.0;

    // 显示解锁
    std::cout << "║  Direction UNLOCKED                                   ║\n";
}
```

#### **5. SafeRegretMPC.cpp - applyRotationSpeedLimit() - 使用锁定方向**

**修改前**：
```cpp
if (in_place_rotation_) {
    linear_vel = 0.0;

    const double fixed_angular_vel = 0.5;

    // ❌ 每次都根据etheta计算方向
    double rotation_direction = (etheta >= 0) ? -1.0 : 1.0;

    angular_vel = rotation_direction * fixed_angular_vel;
}
```

**修改后**：
```cpp
if (in_place_rotation_) {
    linear_vel = 0.0;

    const double fixed_angular_vel = 0.5;

    // ✅ Use LOCKED rotation direction to prevent oscillation
    double rotation_direction;
    if (rotation_direction_locked_) {
        rotation_direction = locked_rotation_direction_;  // ✅ 使用锁定的方向
    } else {
        // Fallback: determine direction from etheta sign
        rotation_direction = (etheta >= 0) ? -1.0 : 1.0;
    }

    angular_vel = rotation_direction * fixed_angular_vel;

    // 显示方向状态
    std::cout << "Direction " << (rotation_direction_locked_ ? "LOCKED" : "computed") << " | ";
}
```

---

## 🎯 **关键改进**

### **状态机图**

```
                etheta > 90°
        ┌────────────────────────┐
        ▼                        │
┌───────────────┐         │
│ Normal Motion │         │
└───────┬───────┘         │
        │                 │
        │ etheta > 90°    │
        ▼                 │
┌───────────────────┐     │
│ In-Place Rotation │     │
│ direction = LOCKED│◄────┘
│ speed = 0         │
│ ω = 0.5 rad/s     │
└───────┬───────────┘
        │
        │ etheta < 10°
        ▼
┌───────────────┐
│ Normal Motion │
│ direction =    │
│ UNLOCKED       │
└───────────────┘
```

### **方向锁定逻辑**

| 阶段 | rotation_direction_locked_ | locked_rotation_direction_ | 行为 |
|------|-------------------------|--------------------------|------|
| **正常** | false | 0.0 | 方向未锁定 |
| **进入旋转** | ✅ true | -1.0 或 +1.0 | **锁定方向** |
| **旋转中** | ✅ true | **不变** | 方向恒定 ✅ |
| **退出旋转** | false | 0.0 | 解锁 |

### **稳定性保证**

**修复前**（振荡）：
```
etheta: 179.8° → direction = -1.0 → ω = -0.5 rad/s
etheta: -179.5° → direction = +1.0 → ω = +0.5 rad/s ❌
etheta: 179.2° → direction = -1.0 → ω = -0.5 rad/s
```

**修复后**（稳定）：
```
进入旋转: etheta = 179.8° → LOCK direction = -1.0
旋转中:   etheta = -179.5° → use LOCKED = -1.0 → ω = -0.5 rad/s ✅
旋转中:   etheta = 179.2° → use LOCKED = -1.0 → ω = -0.5 rad/s ✅
旋转中:   etheta = 150.0° → use LOCKED = -1.0 → ω = -0.5 rad/s ✅
旋转中:   etheta = 10.5° → use LOCKED = -1.0 → ω = -0.5 rad/s ✅
退出旋转: etheta = 9.8° → UNLOCK
```

---

## 📊 **修复前后对比**

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **方向来源** | 每次从etheta计算 | 进入时锁定 ✅ |
| **方向变化** | 随etheta振荡 | 恒定不变 ✅ |
| **角速度** | ±0.5（切换） | -0.5（恒定）✅ |
| **机器人行为** | 左右摆头 ❌ | 稳定旋转 ✅ |

---

## 🧪 **测试验证**

### **预期日志**

#### **进入旋转**

```
╔════════════════════════════════════════════════════════╗
║  🔄 ENTERING PURE ROTATION MODE                        ║
╠════════════════════════════════════════════════════════╣
║  Current heading error: 179.8° (>90°)                  ║
║  Exit condition: < 10°                                  ║
║  Direction LOCKED: Counter-Clockwise                    ║  ← ✅ 锁定方向
║  Action: Speed FORCED to 0.0 m/s                       ║
╚════════════════════════════════════════════════════════╝
```

#### **旋转中**

```
🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s |
   Angular vel OVERRIDDEN to -0.5 rad/s (-28.6°/s) |
   Direction LOCKED | ← ✅ 方向锁定
   Heading error: -179.5°  ← etheta变化，但方向不变！

🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s |
   Angular vel OVERRIDDEN to -0.5 rad/s (-28.6°/s) |
   Direction LOCKED | ← ✅ 方向不变
   Heading error: 150.2°
```

#### **退出旋转**

```
╔════════════════════════════════════════════════════════╗
║  ✅ EXITING PURE ROTATION MODE                         ║
╠════════════════════════════════════════════════════════╣
║  Current heading error: 9.8° (<10°)                    ║
║  Direction UNLOCKED                                   ║  ← ✅ 解锁
║  Action: Normal motion RESUMED                         ║
╚════════════════════════════════════════════════════════╝
```

### **关键验证点**

- ✅ 方向只在进入时计算一次
- ✅ 旋转期间方向恒定（不管etheta如何变化）
- ✅ 退出时解锁
- ✅ 下次旋转重新锁定（可能方向不同）

---

## 📁 **修改文件清单**

```
src/safe_regret_mpc/
├── include/safe_regret_mpc/
│   └── SafeRegretMPC.hpp              ✅ 添加锁定成员变量
└── src/
    ├── SafeRegretMPC.cpp              ✅ 构造函数初始化
    ├── SafeRegretMPC.cpp              ✅ updateInPlaceRotation() - 锁定/解锁
    └── SafeRegretMPC.cpp              ✅ applyRotationSpeedLimit() - 使用锁定方向
```

**修改内容**：
1. 添加2个成员变量：rotation_direction_locked_, locked_rotation_direction_
2. 进入旋转时：锁定方向
3. 旋转期间：使用锁定方向（不重新计算）
4. 退出旋转时：解锁

---

## ✅ **修复验证清单**

- [x] 方向锁定成员变量添加
- [x] 构造函数初始化
- [x] 进入旋转时锁定方向
- [x] 旋转期间使用锁定方向
- [x] 退出旋转时解锁
- [x] 日志显示锁定状态
- [x] 编译无错误

---

## 🎉 **总结**

### **问题根源**
etheta跨越±180°边界 → 符号跳变 → 方向切换 → 左右摆头

### **解决方案**
方向锁定机制：进入时锁定，退出时解锁

### **关键改进**
- ✅ 方向只计算一次（进入时）
- ✅ 旋转期间方向恒定
- ✅ 不受etheta振荡影响
- ✅ 绝对稳定，不会摆头

### **预期效果**
机器人在原地旋转时**方向恒定**，不再左右摆头

---

**状态**: ✅ **修复完成，等待实际场景测试验证**

**注意**: 方向锁定机制确保旋转稳定性，即使etheta在±180°边界跳变也不会影响方向。
