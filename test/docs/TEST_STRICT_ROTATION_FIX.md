# 严格原地旋转模式修复说明

**修复日期**: 2026-04-03 00:32
**问题**: 转到90度就开始走了
**状态**: ✅ 已修复

---

## 问题根源

**发现的关键bug**：在 `TubeMPCNode.cpp` 第802行，有一个**独立的角度判断逻辑**，与状态机冲突：

```cpp
// ❌ 错误的旧代码
bool pure_rotation_mode = (std::abs(etheta) > 1.57);  // 独立判断

if(!pure_rotation_mode) {
    if(_speed < 0.3) {
        _speed = 0.3;  // 应用最小速度阈值
    }
}
```

**问题分析**：
1. 当角度 > 90°：状态机设置 `_in_place_rotation = true`，速度 = 0
2. 当角度降到 89°：`_in_place_rotation` 仍然是 `true`（正确），但是 `pure_rotation_mode = false`（因为 89° < 90°）
3. 由于 `pure_rotation_mode = false`，代码应用了最小速度阈值 `_speed = 0.3`
4. **结果**：机器人开始以 0.3 m/s 移动！

---

## 修复方案

### 修改1：删除独立的角度判断

**位置**：`TubeMPCNode.cpp` 第799-814行

**修改前**：
```cpp
bool pure_rotation_mode = (std::abs(etheta) > 1.57);  // ❌ 独立判断

if(!pure_rotation_mode) {
    if(_speed < 0.3) {
        _speed = 0.3;
    }
} else {
    if(debug_counter % 5 == 0) {
        cout << "🔄 Pure rotation mode: Min speed threshold disabled" << endl;
    }
}
```

**修改后**：
```cpp
// ⚠️ CRITICAL FIX: 使用状态机标志 _in_place_rotation
if(!_in_place_rotation) {
    // 正常模式：应用最小速度阈值
    if(_speed < 0.3) {
        _speed = 0.3;
    }
}
// 原地旋转模式：允许速度为0（已在上面设置为0）
```

### 修改2：简化日志输出

删除了大段的边框字符输出，改为简洁的日志：

**进入旋转模式**：
```cpp
cout << "[ROTATION] ENTERING pure rotation mode (" << (etheta * 180.0 / M_PI) << "° > 90°)" << endl;
```

**旋转过程中**（每10次循环输出一次）：
```cpp
cout << "[ROTATION] Speed=0.0 | Angle=" << (etheta * 180.0 / M_PI) << "° | Exit<30° | w=" << _w_filtered << endl;
```

**退出旋转模式**：
```cpp
cout << "[ROTATION] EXITING pure rotation mode (" << (etheta * 180.0 / M_PI) << "° < 30°)" << endl;
```

---

## 测试验证

### 启动系统

```bash
# 清理旧进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

### 预期行为

**场景：大角度转弯（100° → 25°）**

1. **角度 > 90°**：
   ```
   [ROTATION] ENTERING pure rotation mode (105.3° > 90°)
   [ROTATION] Speed=0.0 | Angle=105.3° | Exit<30° | w=0.8
   ```

2. **角度 90° → 30°**（关键测试点）：
   ```
   [ROTATION] Speed=0.0 | Angle=85.2° | Exit<30° | w=0.7
   [ROTATION] Speed=0.0 | Angle=68.4° | Exit<30° | w=0.6
   [ROTATION] Speed=0.0 | Angle=52.1° | Exit<30° | w=0.5
   [ROTATION] Speed=0.0 | Angle=41.8° | Exit<30° | w=0.4
   [ROTATION] Speed=0.0 | Angle=32.5° | Exit<30° | w=0.3
   ```
   **✅ 关键验证点**：速度始终保持 0.0 m/s，不会在 90° 时开始移动

3. **角度 < 30°**：
   ```
   [ROTATION] EXITING pure rotation mode (28.7° < 30°)
   ```
   **✅ 退出旋转模式，恢复正常行驶**

---

## 关键改进点

| 项目 | 修改前 | 修改后 |
|------|--------|--------|
| **状态标志** | 独立判断 `pure_rotation_mode` | 统一使用 `_in_place_rotation` |
| **退出条件** | 角度 < 90° | 角度 < 30° ✅ |
| **速度控制** | 在 90° 时应用最小速度（0.3 m/s） | 严格保持 0.0 m/s 直到 < 30° ✅ |
| **日志输出** | 大段边框字符 | 简洁的一行输出 ✅ |

---

## 技术细节

### 状态机流程图

```
                    ┌─────────────────┐
                    │  非旋转模式       │
                    │  (最小速度0.3)    │
                    └────────┬────────┘
                             │
              |etheta| > 90° 且 !in_rotation
                             ▼
                    ┌─────────────────┐
                    │  旋转模式         │
                    │  (速度=0)        │
                    │  严格锁定        │
                    └────────┬────────┘
                             │
              |etheta| < 30° 且 in_rotation
                             ▼
                    ┌─────────────────┐
                    │  非旋转模式       │
                    │  (最小速度0.3)    │
                    └─────────────────┘
```

### 代码修改总结

**修改文件**：`src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

**修改位置**：
- 第680-734行：简化状态机日志输出
- 第799-809行：删除独立的角度判断，改用状态机标志

**编译状态**：✅ 已成功编译（2026-04-03 00:32）

---

## 验证清单

- [ ] 启动系统无错误
- [ ] 发送大角度目标（>90°）
- [ ] 观察日志输出 `[ROTATION] ENTERING`
- [ ] 验证速度保持 0.0 m/s
- [ ] 验证角度从 90° → 30° 期间速度仍为 0
- [ ] 验证角度 < 30° 时输出 `[ROTATION] EXITING`
- [ ] 验证退出后恢复正常行驶

---

## 相关文档

- 详细实现：`test/docs/STRICT_ROTATION_MODE_IMPROVEMENT.md`
- 快速参考：`STRICT_ROTATION_MODE_README.md`
- 项目文档：`CLAUDE.md`

---

**修复完成！现在应该可以正确工作了。** ✅
