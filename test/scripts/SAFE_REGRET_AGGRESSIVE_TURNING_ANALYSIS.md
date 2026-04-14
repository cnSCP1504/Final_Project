# Safe-Regret MPC激进转弯问题分析

**日期**: 2026-04-12
**问题**: safe_regret_mpc比其他模型表现差非常多，拐弯时更激进，且不按规划路径拐弯

---

## 🔍 问题诊断

### 核心问题：MPC求解器的参考轨迹使用方式错误

经过详细代码分析，发现了一个**严重的设计缺陷**：

#### 问题1：参考轨迹存储在全局变量中，但不会实时更新

**文件**: `src/safe_regret_mpc/src/SafeRegretMPC.cpp`
```cpp
// Line 131: 参考轨迹存储在成员变量中
reference_trajectory_ = reference_trajectory;
```

**文件**: `src/safe_regret_mpc/src/SafeRegretMPCSolver.cpp`
```cpp
// Lines 174-180: 目标函数使用reference_trajectory_
if (t < mpc_->reference_trajectory_.size()) {
    tracking_error = x_t - mpc_->reference_trajectory_[t];
} else if (!mpc_->reference_trajectory_.empty()) {
    tracking_error = x_t - mpc_->reference_trajectory_.back();
} else {
    tracking_error = x_t;  // ❌ 无参考时最小化状态本身
}
```

**问题**：当`reference_trajectory_`为空或不足时，MPC会**最小化状态本身**，而不是跟踪误差！

这意味着：
- 如果参考轨迹为空：`minimize x_t' Q x_t` → 机器人会朝向(0,0)移动！
- 如果参考轨迹不足：使用最后一个点 → 所有预测步都朝向同一点

---

#### 问题2：参考轨迹更新机制存在问题

**文件**: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp`
```cpp
// Lines 820-832: updateReferenceTrajectory()
void SafeRegretMPCNode::updateReferenceTrajectory() {
    reference_trajectory_.clear();

    for (const auto& pose : global_plan_.poses) {
        Eigen::VectorXd state(6);
        state << pose.pose.position.x,
                 pose.pose.position.y,
                 tf::getYaw(pose.pose.orientation),
                 ref_vel_,
                 0.0,  // cte = 0
                 0.0;  // etheta = 0
        reference_trajectory_.push_back(state);
    }
}
```

**问题**：
1. `global_plan_`是全局路径（可能很长，包含整个路径）
2. MPC只需要预测范围内的参考点（mpc_steps=20步）
3. **没有根据当前位置截取最近的一段参考点**
4. cte和etheta在参考轨迹中强制设为0（不准确）

---

#### 问题3：集成模式下MPC求解逻辑混乱

**文件**: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp`
```cpp
// Lines 540-602: controlTimerCallback()中的集成模式逻辑
if (enable_integration_mode_) {
    if (tube_mpc_cmd_received_) {
        // ✅ 正确：使用tube_mpc的命令
        geometry_msgs::Twist cmd_final = tube_mpc_cmd_raw_;

        // ⚠️ 问题：STL/DR调整只是简单的缩放
        if (stl_enabled_ && stl_received_) {
            cmd_final = adjustCommandForSTL(cmd_final);
        }
        if (dr_enabled_ && dr_received_) {
            cmd_final = adjustCommandForDR(cmd_final);
        }

        publishFinalCommand(cmd_final);
    }
}
```

**问题**：
- 集成模式下使用tube_mpc的命令，这是对的
- **但STL/DR的调整只是简单的缩放因子（80%-100%）**
- **没有真正利用MPC优化来结合STL/DR约束**

---

#### 问题4：独立模式下MPC求解没有正确的参考

**文件**: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp`
```cpp
// Lines 603-606: 独立模式
} else {
    solveMPC();  // ❌ 调用solveMPC()但参考轨迹可能有问题
    ...
}
```

**问题**：
- 独立模式下调用`solveMPC()`
- 但`reference_trajectory_`可能包含整个全局路径
- 没有根据机器人当前位置截取合适的参考段
- MPC优化器会尝试跟踪整个路径，而不是最近的一段

---

### 为什么转弯更激进？

#### 原因1：B矩阵配置导致角速度影响过大

**文件**: `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml`
```yaml
B: [0.1, 0.0,      # x: 受v_linear影响
    0.0, 0.1,      # y: 受v_linear影响
    0.0, 0.1,      # theta: 受v_angular影响  ← 问题！
    0.9, 0.0,      # v: 受v_linear影响
    0.0, 0.0,      # cte: 不受直接输入影响
    0.0, 0.1]      # etheta: 受v_angular影响  ← 问题！
```

**问题**：
- `B[2,1] = 0.1` 和 `B[5,1] = 0.1` 表示角速度直接影响theta和etheta
- 但系数太小（0.1），导致MPC认为角速度变化不会快速改变朝向
- **MPC会使用更大的角速度来弥补**，因为dt=0.1秒，每步只能改变0.01弧度

#### 原因2：etheta权重过高但参考轨迹不准确

**文件**: `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml`
```yaml
Q: [1.0, 1.0, 1.0, 1.0, 10.0, 10.0]
#                         ^     ^
#                        cte  etheta (权重10.0)
```

**问题**：
- etheta权重是10.0（比x,y,theta都高）
- 但参考轨迹中的etheta被强制设为0
- MPC会**过矫正**etheta，导致激进转向

#### 原因3：Tube MPC参数对比

| 参数 | Tube MPC | Safe-Regret MPC | 差异 |
|------|----------|-----------------|------|
| `mpc_w_etheta` | 30.0 | 10.0 (Q[5]) | Safe-Regret低3倍 |
| `mpc_w_cte` | 50.0 | 10.0 (Q[4]) | Safe-Regret低5倍 |
| `mpc_max_angvel` | 3.0 | 2.5 | Safe-Regret低17% |
| `max_rot_vel` | 3.5 | - | Tube MPC更高 |

**分析**：
- Tube MPC的cte权重50远高于etheta权重30（约1.67倍）
- Safe-Regret MPC的cte和etheta权重相同（都是10）
- **这导致Safe-Regret MPC对etheta更敏感，会优先修正朝向误差**

---

### 为什么不按规划路径拐弯？

#### 原因：参考轨迹没有正确使用

**理想情况**：
```
参考轨迹 = 全局路径上距离机器人最近的mpc_steps个点
MPC优化 = 跟踪这mpc_steps个参考点
```

**实际情况**：
```
参考轨迹 = 整个全局路径（可能几百个点）
MPC优化 = 尝试跟踪整个路径，或者因为参考轨迹为空而最小化状态本身
```

**结果**：
- MPC不知道"当前应该跟踪哪一段路径"
- 可能会朝向路径终点直走（忽略中间的弯道）
- 或者朝向(0,0)移动（如果参考轨迹为空）

---

## 🔧 解决方案

### 方案1：修复参考轨迹生成（推荐）

**修改文件**: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp`

```cpp
void SafeRegretMPCNode::updateReferenceTrajectory() {
    reference_trajectory_.clear();

    if (global_plan_.poses.empty()) {
        ROS_WARN("Global plan is empty!");
        return;
    }

    // ✅ NEW: 找到距离机器人最近的路径点
    double robot_x = current_odom_.pose.pose.position.x;
    double robot_y = current_odom_.pose.pose.position.y;

    size_t closest_idx = 0;
    double min_dist = std::numeric_limits<double>::max();

    for (size_t i = 0; i < global_plan_.poses.size(); ++i) {
        double dist = std::hypot(
            robot_x - global_plan_.poses[i].pose.position.x,
            robot_y - global_plan_.poses[i].pose.position.y
        );
        if (dist < min_dist) {
            min_dist = dist;
            closest_idx = i;
        }
    }

    // ✅ NEW: 从最近点开始，取mpc_steps+1个点
    size_t start_idx = closest_idx;
    size_t end_idx = std::min(start_idx + mpc_steps_ + 1, global_plan_.poses.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        const auto& pose = global_plan_.poses[i];
        Eigen::VectorXd state(6);

        // 计算朝向误差（相对于路径切线方向）
        double ref_theta = tf::getYaw(pose.pose.orientation);
        double current_theta = tf::getYaw(current_odom_.pose.pose.orientation);

        // 如果有下一个点，使用切线方向更准确
        if (i + 1 < global_plan_.poses.size()) {
            double dx = global_plan_.poses[i+1].pose.position.x - pose.pose.position.x;
            double dy = global_plan_.poses[i+1].pose.position.y - pose.pose.position.y;
            ref_theta = std::atan2(dy, dx);
        }

        double cte = min_dist;  // 使用当前距离作为cte估计
        double etheta = normalizeAngle(current_theta - ref_theta);

        state << pose.pose.position.x,
                 pose.pose.position.y,
                 ref_theta,
                 ref_vel_,
                 cte,
                 etheta;

        reference_trajectory_.push_back(state);
    }

    // ✅ NEW: 如果点数不足，用最后一个点填充
    while (reference_trajectory_.size() < mpc_steps_ + 1) {
        reference_trajectory_.push_back(reference_trajectory_.back());
    }

    ROS_DEBUG("Reference trajectory updated: %zu points, starting from idx %zu",
              reference_trajectory_.size(), start_idx);
}
```

### 方案2：增加MPC求解器对参考轨迹的验证

**修改文件**: `src/safe_regret_mpc/src/SafeRegretMPCSolver.cpp`

```cpp
bool eval_f(...) {
    obj_value = 0.0;

    // ✅ NEW: 验证参考轨迹
    if (mpc_->reference_trajectory_.empty()) {
        std::cerr << "❌ CRITICAL: Reference trajectory is EMPTY! "
                  << "MPC will minimize state itself (drift to origin)!" << std::endl;
        // 使用默认目标（当前位置停止）
        for (size_t t = 0; t < mpc_->mpc_steps_; ++t) {
            obj_value += 0.0;  // 不优化，保持稳定
        }
        return true;
    }

    // ... rest of the code
}
```

### 方案3：调整权重矩阵

**修改文件**: `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml`

```yaml
# 调整权重：增加位置跟踪，减少朝向跟踪
Q: [10.0, 10.0, 5.0, 1.0, 30.0, 15.0]
#   ^     ^            ^     ^
#   x     y           cte  etheta
#
# cte权重提高到30（与Tube MPC的50相近）
# etheta权重提高到15（比Tube MPC的30低，但比原来的10高）
# x,y权重提高到10（确保位置跟踪）
```

### 方案4：修复B矩阵

**修改文件**: `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml`

```yaml
# B矩阵：控制输入对状态的影响
B: [0.1, 0.0,      # x: dt * v_linear * cos(theta)
    0.0, 0.1,      # y: dt * v_linear * sin(theta)
    0.0, 1.0,      # theta: dt * v_angular ← 提高到1.0（完全旋转）
    0.9, 0.0,      # v: v_linear decay
    0.0, 0.0,      # cte: 不受直接输入影响
    0.0, 1.0]      # etheta: dt * v_angular ← 提高到1.0
```

---

## 📊 预期效果

### 修复前
- 参考轨迹：整个全局路径（或空）
- MPC行为：不知道跟踪哪一段，可能朝向终点或原点
- 转弯行为：激进，因为etheta权重相对cte更高
- 路径跟踪：差，不按规划路径拐弯

### 修复后
- 参考轨迹：机器人附近的mpc_steps个点
- MPC行为：跟踪当前应该走的路径段
- 转弯行为：平滑，权重合理分配
- 路径跟踪：好，沿着规划路径移动

---

## 🎯 优先级

1. **🔴 高优先级**：修复参考轨迹生成（方案1）
   - 这是根本问题，必须首先解决

2. **🟡 中优先级**：调整权重矩阵（方案3）
   - 改善跟踪性能

3. **🟡 中优先级**：修复B矩阵（方案4）
   - 改善控制响应

4. **🟢 低优先级**：添加验证（方案2）
   - 防止未来问题

---

## 📝 相关文件

- `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp` - 参考轨迹更新
- `src/safe_regret_mpc/src/SafeRegretMPC.cpp` - MPC求解入口
- `src/safe_regret_mpc/src/SafeRegretMPCSolver.cpp` - 目标函数和约束
- `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml` - 参数配置
