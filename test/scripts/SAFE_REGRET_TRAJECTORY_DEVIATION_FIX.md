# Safe-Regret模式大角度偏离问题修复

**日期**: 2026-04-11
**问题**: 开启safe_regret模式后出现很多意义不明的突然偏离轨道的大角度转弯

---

## 🔍 问题分析

### 用户反馈
- 开启safe_regret模式后，机器人会出现**突然偏离轨道的大角度转弯**
- **不是**触发了原地转弯程序（90°转向）
- 意义不明，行为异常

### 根本原因

**发现**: `controlTimerCallback()`在集成模式下，如果DR/STL启用，会**调用`solveMPC()`求解自己的MPC**！

```cpp
// 旧代码（有问题）
if (enable_integration_mode_) {
    if ((dr_enabled_ && dr_received_) || (stl_enabled_ && stl_received_)) {
        // ❌ 问题：自己求解MPC，而不是使用tube_mpc的命令
        solveMPC();  // 这个MPC可能产生不同的轨迹！
        publishFinalCommand(cmd_vel_);
    }
}
```

**问题所在**:
1. tube_mpc已经计算好了最优轨迹（基于全局路径）
2. safe_regret_mpc又自己求解了一个MPC（基于不同的参考轨迹）
3. **两个MPC的轨迹可能不一致**，导致突然的大角度转弯

### 为什么会出现不一致？

1. **参考轨迹不同**:
   - tube_mpc使用全局路径（经过A*优化，考虑障碍物）
   - safe_regret_mpc的`reference_trajectory_`可能来自不同的源

2. **求解器参数不同**:
   - tube_mpc有完整的环境感知（costmap）
   - safe_regret_mpc可能没有最新的环境信息

3. **约束不同**:
   - DR/STL约束可能导致MPC求解出不同的轨迹

---

## ✅ 解决方案

### 核心思路
**在集成模式下，safe_regret_mpc不应该自己求解MPC，而是应该：**
1. 使用tube_mpc的原始命令作为基础
2. 应用STL/DR调整（温和的缩放）
3. 处理原地旋转逻辑

### 代码修改

**文件**: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp`

```cpp
// 新代码（修复后）
if (enable_integration_mode_) {
    // ✅ 使用tube_mpc命令作为基础
    if (tube_mpc_cmd_received_) {
        geometry_msgs::Twist cmd_final = tube_mpc_cmd_raw_;

        // 应用STL调整（温和缩放）
        if (stl_enabled_ && stl_received_) {
            cmd_final = adjustCommandForSTL(cmd_final);
        }

        // 应用DR调整（温和限制）
        if (dr_enabled_ && dr_received_) {
            cmd_final = adjustCommandForDR(cmd_final);
        }

        // 处理原地旋转（90°大角度）
        // ... rotation logic ...

        publishFinalCommand(cmd_final);
    }
}
```

### 关键改进

1. **不再自己求解MPC**
   - 避免了轨迹不一致的问题
   - 保持tube_mpc的路径跟踪稳定性

2. **温和的STL/DR调整**
   - STL: 最多降低20%（原50%）
   - DR: 最多降低5%（原10%）

3. **保留原地旋转逻辑**
   - 90°以上角度仍会触发原地旋转
   - 确保大角度转向的正常处理

---

## 📊 行为对比

### 修改前

```
tube_mpc求解轨迹A → 发布/cmd_vel_raw
safe_regret_mpc求解轨迹B → 发布/cmd_vel（轨迹不同！）
→ 机器人突然偏离轨迹A，转向轨迹B → 异常行为！
```

### 修改后

```
tube_mpc求解轨迹A → 发布/cmd_vel_raw
safe_regret_mpc接收轨迹A → 温和调整 → 发布/cmd_vel（仍然是轨迹A）
→ 机器人稳定跟踪轨迹A → 正常行为！
```

---

## 🎯 预期效果

1. **消除突然的大角度转弯**
   - 不再因为MPC轨迹不一致导致偏离

2. **保持tube_mpc的路径跟踪稳定性**
   - tube_mpc的轨迹经过精心优化
   - safe_regret_mpc只做温和调整

3. **STL/DR约束仍然生效**
   - 通过调整命令实现，而不是重新求解

4. **原地旋转逻辑仍然有效**
   - 90°以上角度仍会触发原地旋转
   - 这是预期行为，不是问题

---

## ✅ 状态

- ✅ 代码修改完成
- ⏳ 编译中
- ⏳ 待测试验证

---

## 📋 测试命令

```bash
# 清理进程
./test/scripts/clean_ros.sh

# 运行测试
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --timeout 120 --no-viz
```

---

## 📝 相关文件

- `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp` - 主修改文件
- `test/scripts/DR_STL_WEIGHT_REDUCTION.md` - 权重降低报告
