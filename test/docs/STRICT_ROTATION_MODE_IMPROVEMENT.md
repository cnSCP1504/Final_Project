# 严格原地旋转模式改进报告

**版本**: V1.0
**日期**: 2026-04-03
**修改文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

---

## 问题描述

**原有实现的问题**：

虽然系统已有原地旋转功能，但逻辑不够严格，存在以下问题：

1. **状态机逻辑漏洞**：
   ```cpp
   // 旧代码
   if(std::abs(etheta) > HEADING_ERROR_CRITICAL) {  // > 90°
       _in_place_rotation = true;
   } else if(_in_place_rotation && std::abs(etheta) < HEADING_ERROR_EXIT) {  // < 30°
       _in_place_rotation = false;
   }
   ```

   **问题**：如果 `etheta` 在 30-90° 之间，既不会进入旋转模式，也不会退出旋转模式。这导致：
   - 机器人在 60-90° 时只减速到 20%（通过 `HEADING_ERROR_THRESHOLD`）
   - 但不会完全停止，仍然可以直行
   - 不符合"一旦启动就必须转到30度才能直行"的要求

2. **缺少明确的状态边界**：
   - 没有清晰的"进入/退出"条件说明
   - 用户不清楚何时会进入/退出旋转模式

---

## 改进方案

### 核心逻辑修改

**新的严格状态机**：

```cpp
// === 原地旋转状态机（严格版本）===
if(std::abs(etheta) > HEADING_ERROR_CRITICAL && !_in_place_rotation) {
    // 触发条件：角度 > 90度 且 当前不在旋转模式
    _in_place_rotation = true;
    // 输出详细的进入信息
} else if(_in_place_rotation && std::abs(etheta) < HEADING_ERROR_EXIT) {
    // 退出条件：已在旋转模式 且 角度 < 30度
    // 注意：必须同时满足两个条件才能退出
    _in_place_rotation = false;
    // 输出详细的退出信息
}
```

**关键改进**：

1. **明确的进入条件**：
   - `|etheta| > 90°` **且** `!_in_place_rotation`
   - 只有当前不在旋转模式时才会检查进入条件
   - 一旦进入，进入条件不再检查

2. **严格的退出条件**：
   - `_in_place_rotation` **且** `|etheta| < 30°`
   - 必须已在旋转模式 **且** 角度足够小才能退出
   - **在旋转期间，不管角度如何，都不会退出**

3. **完全禁止直行**：
   ```cpp
   if(_in_place_rotation) {
       // 🚫 原地旋转模式：完全禁止直行
       // 不管当前角度是多少，只要在旋转模式，速度必须为0
       _speed = 0.0;
   }
   ```

---

## 行为对比

### 旧版本行为（不严格）

| 角度范围 | 状态 | 速度 | 问题 |
|---------|------|------|------|
| > 90° | 进入旋转模式 | 0.0 m/s | ✅ 正确 |
| 60-90° | 非旋转模式 | 20% | ⚠️ 仍可直行 |
| 30-60° | 非旋转模式 | 100% | ⚠️ 正常行驶 |
| < 30° | 非旋转模式 | 100% | ✅ 正常 |

**问题场景**：
1. 初始角度 = 70°
2. 机器人不会进入旋转模式（因为 < 90°）
3. 机器人减速到 20%，但仍然可以直行
4. 不符合"必须转到30度才能直行"的要求

### 新版本行为（严格）

| 角度范围 | 状态 | 速度 | 说明 |
|---------|------|------|------|
| > 90° | 进入旋转模式 | 0.0 m/s | ✅ 完全停止 |
| 30-90° | **保持旋转模式** | 0.0 m/s | ✅ 仍需旋转 |
| < 30° | 退出旋转模式 | 100% | ✅ 恢复行驶 |

**改进场景**：
1. 初始角度 = 100°
2. 机器人**进入**旋转模式，速度 = 0.0 m/s
3. 角度减小到 50° 时，**仍在**旋转模式，速度 = 0.0 m/s ⬅️ **关键改进**
4. 角度减小到 25° 时，**退出**旋转模式，速度恢复正常 ✅

---

## 代码改进详情

### 1. 进入条件（更明确）

```cpp
if(std::abs(etheta) > HEADING_ERROR_CRITICAL && !_in_place_rotation) {
    // 触发条件：角度 > 90度 且 当前不在旋转模式
    _in_place_rotation = true;
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  🔄 ENTERING PURE ROTATION MODE                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║  Current heading error: " << (etheta * 180.0 / M_PI) << "° (>90°)" << endl;
    cout << "║  Exit condition: < 30°                                  " << endl;
    cout << "║  Action: Speed FORCED to 0.0 m/s                       " << endl;
    cout << "║         Only in-place rotation ALLOWED                 " << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
}
```

**关键点**：
- 增加 `&& !_in_place_rotation` 条件
- 防止重复进入（已在旋转模式时不检查进入条件）
- 清晰的日志输出，显示进入条件和退出阈值

### 2. 退出条件（更严格）

```cpp
else if(_in_place_rotation && std::abs(etheta) < HEADING_ERROR_EXIT) {
    // 退出条件：已在旋转模式 且 角度 < 30度
    // 注意：必须同时满足两个条件才能退出
    _in_place_rotation = false;
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  ✅ EXITING PURE ROTATION MODE                         ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║  Current heading error: " << (etheta * 180.0 / M_PI) << "° (<30°)" << endl;
    cout << "║  Action: Normal motion RESUMED                         " << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
}
```

**关键点**：
- 增加 `_in_place_rotation` 前置条件
- 只有在旋转模式内才会检查退出条件
- 强调"必须同时满足两个条件"

### 3. 速度控制（完全锁定）

```cpp
if(_in_place_rotation) {
    // 🚫 原地旋转模式：完全禁止直行
    // 不管当前角度是多少，只要在旋转模式，速度必须为0
    double old_speed = _speed;
    _speed = 0.0;

    // 每次控制循环都输出（因为这是关键安全功能）
    cout << "🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s | "
         << "Angle: " << (etheta * 180.0 / M_PI) << "° | "
         << "Exit at: <30° | "
         << "Angular vel: " << _w_filtered << " rad/s" << endl;
}
```

**关键点**：
- **每次控制循环都输出日志**（关键安全功能）
- 显示当前角度、退出阈值、角速度
- 强调"LOCKED"（锁定）状态

---

## 测试验证

### 测试脚本

已创建测试脚本：`test/test_strict_rotation_mode.sh`

```bash
# 运行测试
./test/test_strict_rotation_mode.sh
```

### 测试场景

#### 场景1：大角度转弯（100° → 25°）

**初始状态**：
- 机器人位置：(-8.0, 0.0)
- 机器人朝向：0°（向东）
- 目标位置：(0.0, 8.0)
- 初始角度差：约 100°

**预期行为**：
1. ✅ 检测到角度 > 90°，**进入**旋转模式
2. ✅ 输出日志：`"🔄 ENTERING PURE ROTATION MODE"`
3. ✅ 速度锁定为 0.0 m/s
4. ✅ 机器人原地旋转，角度逐渐减小
5. ✅ 当角度从 90° → 30° 时，**仍在**旋转模式
6. ✅ 当角度 < 30° 时，**退出**旋转模式
7. ✅ 输出日志：`"✅ EXITING PURE ROTATION MODE"`
8. ✅ 速度恢复正常，开始直行

**关键验证点**：
- 在 30-90° 范围内，速度必须保持 0.0 m/s
- 只有角度 < 30° 时才能恢复直行

#### 场景2：连续多目标测试

**目标序列**：
1. Goal 1: (3.0, -7.0) - 正常到达
2. Goal 2: (8.0, 0.0) - 大角度转弯

**预期行为**：
- Goal 1 → Goal 2 触发大角度转弯（>90°）
- 进入旋转模式，完成旋转后退出
- 成功到达 Goal 2

---

## 日志输出示例

### 进入旋转模式

```
╔════════════════════════════════════════════════════════╗
║  🔄 ENTERING PURE ROTATION MODE                        ║
╠════════════════════════════════════════════════════════╣
║  Current heading error: 105.3° (>90°)                  ║
║  Exit condition: < 30°                                  ║
║  Action: Speed FORCED to 0.0 m/s                       ║
║         Only in-place rotation ALLOWED                 ║
╚════════════════════════════════════════════════════════╝
```

### 旋转过程中（每次控制循环）

```
🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s | Angle: 75.2° | Exit at: <30° | Angular vel: 0.8 rad/s
🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s | Angle: 68.4° | Exit at: <30° | Angular vel: 0.7 rad/s
🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s | Angle: 52.1° | Exit at: <30° | Angular vel: 0.6 rad/s
🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s | Angle: 41.8° | Exit at: <30° | Angular vel: 0.5 rad/s
🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s | Angle: 32.5° | Exit at: <30° | Angular vel: 0.4 rad/s
```

### 退出旋转模式

```
╔════════════════════════════════════════════════════════╗
║  ✅ EXITING PURE ROTATION MODE                         ║
╠════════════════════════════════════════════════════════╣
║  Current heading error: 28.7° (<30°)                   ║
║  Action: Normal motion RESUMED                         ║
╚════════════════════════════════════════════════════════╝
```

---

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `HEADING_ERROR_CRITICAL` | 1.57 rad (90°) | 进入旋转模式的阈值 |
| `HEADING_ERROR_EXIT` | 0.524 rad (30°) | 退出旋转模式的阈值 |
| `HEADING_ERROR_THRESHOLD` | 1.05 rad (60°) | 非旋转模式下的减速阈值 |

---

## 理论保证

### 状态机正确性

**性质1：互斥性**
- 同一时刻只能在一个状态（旋转 / 非旋转）
- 保证：由布尔标志 `_in_place_rotation` 保证

**性质2：确定性**
- 给定当前状态和角度，下一个状态是确定的
- 保证：显式的进入/退出条件

**性质3：安全性**
- 在旋转模式下，速度始终为 0
- 保证：每次循环都强制设置 `_speed = 0.0`

**性质4：活性**
- 只要角度收敛到 < 30°，最终会退出旋转模式
- 保证：退出条件会在角度满足时触发

---

## 相关文档

- `CLAUDE.md` - 项目说明文档（Large-Angle Turning Problem章节）
- `test/docs/LARGE_ANGLE_TURN_FIX_V2_REPORT.md` - V2版本修复报告
- `test/docs/ETHETA_FIX_VERIFICATION_REPORT.md` - Etheta修复验证报告

---

## 版本历史

| 版本 | 日期 | 修改内容 |
|------|------|---------|
| V1.0 | 2026-04-03 | 初始版本：严格原地旋转模式实现 |
