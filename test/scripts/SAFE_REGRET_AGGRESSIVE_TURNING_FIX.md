# Safe-Regret MPC激进转弯问题修复报告

**日期**: 2026-04-12
**问题**: safe_regret_mpc比其他模型表现差，拐弯更激进且不按规划路径移动

---

## 🔍 问题根源

经过详细代码分析，发现了**三个关键问题**：

### 问题1：参考轨迹生成错误（最严重）

**原代码** (`safe_regret_mpc_node.cpp:820-832`):
```cpp
void SafeRegretMPCNode::updateReferenceTrajectory() {
    reference_trajectory_.clear();

    for (const auto& pose : global_plan_.poses) {
        Eigen::VectorXd state(6);
        state << pose.pose.position.x,
                 pose.pose.position.y,
                 tf::getYaw(pose.pose.orientation),
                 ref_vel_,
                 0.0,  // cte = 0 (强制)
                 0.0;  // etheta = 0 (强制)
        reference_trajectory_.push_back(state);
    }
}
```

**问题**：
1. ❌ 包含**整个全局路径**（可能几百个点），而不是MPC预测范围内的点
2. ❌ cte和etheta强制设为0，不准确
3. ❌ 没有根据机器人当前位置截取最近的一段路径

**后果**：
- MPC尝试跟踪整个路径，不知道当前应该跟踪哪一段
- 可能朝向终点直走，忽略中间的弯道
- 或者因为参考轨迹为空，最小化状态本身（朝向原点移动）

### 问题2：B矩阵参数不当

**原配置** (`safe_regret_mpc_params.yaml`):
```yaml
B: [0.1, 0.0,
    0.0, 0.1,
    0.0, 0.1,      # theta: 角速度影响系数太小
    0.9, 0.0,
    0.0, 0.0,
    0.0, 0.1]      # etheta: 角速度影响系数太小
```

**问题**：
- `B[2,1] = 0.1` 表示角速度每步只能改变0.01弧度（dt=0.1秒）
- MPC会使用**更大的角速度**来弥补这个"慢响应"
- 导致转弯过于激进

### 问题3：权重矩阵不平衡

**原配置**:
```yaml
Q: [1.0, 1.0, 1.0, 1.0, 10.0, 10.0]
#   ^   ^            ^     ^
#   x   y           cte  etheta (权重相同)

R: [1.0, 1.0]  # 线速度和角速度权重相同
```

**对比Tube MPC**:
```yaml
mpc_w_cte: 50.0     # cte权重
mpc_w_etheta: 30.0  # etheta权重 (cte的60%)
mpc_w_angvel: 20.0  # 角速度权重
```

**问题**：
- Safe-Regret的cte和etheta权重相同（都是10）
- Tube MPC的cte权重是etheta的1.67倍
- **Safe-Regret对etheta更敏感，优先修正朝向误差，导致激进转向**

---

## ✅ 修复内容

### 修复1：重写参考轨迹生成（核心修复）

**新代码** (`safe_regret_mpc_node.cpp`):
```cpp
void SafeRegretMPCNode::updateReferenceTrajectory() {
    reference_trajectory_.clear();

    // ✅ 找到距离机器人最近的路径点
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

    // ✅ 从最近点开始，只取mpc_steps+1个点
    size_t start_idx = closest_idx;
    size_t end_idx = std::min(start_idx + mpc_steps_ + 1, global_plan_.poses.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        const auto& pose = global_plan_.poses[i];
        Eigen::VectorXd state(6);

        // ✅ 使用路径切线方向作为参考朝向（更准确）
        double ref_theta;
        if (i + 1 < global_plan_.poses.size()) {
            double dx = global_plan_.poses[i+1].pose.position.x - pose.pose.position.x;
            double dy = global_plan_.poses[i+1].pose.position.y - pose.pose.position.y;
            ref_theta = std::atan2(dy, dx);
        } else {
            ref_theta = tf::getYaw(pose.pose.orientation);
        }

        // ✅ 估计cte和etheta
        double cte = (i == start_idx) ? min_dist : 0.0;
        double etheta = (i == start_idx) ? normalizeAngle(current_theta - ref_theta) : 0.0;

        state << pose.pose.position.x,
                 pose.pose.position.y,
                 ref_theta,
                 ref_vel_,
                 cte,
                 etheta;

        reference_trajectory_.push_back(state);
    }

    // ✅ 如果点数不足，用最后一个点填充
    while (reference_trajectory_.size() < mpc_steps_ + 1) {
        reference_trajectory_.push_back(reference_trajectory_.back());
    }
}
```

**改进**：
1. ✅ 找到距离机器人最近的路径点
2. ✅ 从最近点开始，只取mpc_steps+1个点
3. ✅ 使用路径切线方向作为参考朝向
4. ✅ 估计准确的cte和etheta值

### 修复2：调整B矩阵

**新配置** (`safe_regret_mpc_params.yaml`):
```yaml
B: [0.1, 0.0,
    0.0, 0.1,
    0.0, 1.0,      # ← 提高到1.0（完全旋转）
    0.9, 0.0,
    0.0, 0.0,
    0.0, 1.0]      # ← 提高到1.0
```

**改进**：
- 角速度对theta和etheta的影响系数提高到1.0
- MPC不再需要使用过大的角速度来补偿

### 修复3：调整权重矩阵

**新配置** (`safe_regret_mpc_params.yaml`):
```yaml
# Q矩阵：[x, y, theta, v, cte, etheta]
Q: [10.0, 10.0, 5.0, 1.0, 30.0, 15.0]
#   ^    ^            ^     ^
#   提高位置跟踪     cte:30 etheta:15 (比例2:1)

# R矩阵：[v_linear, v_angular]
R: [1.0, 5.0]  # 提高角速度成本，减少激进转向
```

**改进**：
- x,y权重提高到10（确保位置跟踪）
- cte权重提高到30（与Tube MPC的50相近）
- etheta权重提高到15（cte的50%，比Tube MPC的比例更低）
- 角速度成本提高到5（减少激进转向）

---

## 📊 预期效果对比

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| **参考轨迹** | 整个全局路径（几百个点） | 最近的mpc_steps个点 |
| **路径跟踪** | 差，不按规划路径 | 好，沿着路径移动 |
| **转弯行为** | 激进，不必要的大角速度 | 平滑，合理转向 |
| **cte/etheta平衡** | 1:1（过度修正etheta） | 2:1（优先修正cte） |

---

## 🧪 验证步骤

### 编译
```bash
cd /home/dixon/Final_Project/catkin
catkin_make --only-pkg-with-deps safe_regret_mpc
source devel/setup.bash
```

### 测试
```bash
# 运行测试
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --timeout 120 --no-viz

# 检查参考轨迹更新日志
grep "Reference trajectory updated" test_results/safe_regret_*/test_01_shelf_01/logs/launch.log
```

### 预期日志输出
```
Reference trajectory updated: 21 points, start_idx=45/120, closest_dist=0.125 m
Reference trajectory updated: 21 points, start_idx=47/120, closest_dist=0.098 m
...
```

这表明：
- 参考轨迹只有21个点（mpc_steps+1）
- 从全局路径的第45/47个点开始（机器人附近）
- 距离最近的点约0.1m（符合预期）

---

## 📝 修改文件清单

1. **src/safe_regret_mpc/src/safe_regret_mpc_node.cpp**
   - 函数：`updateReferenceTrajectory()`
   - 修改：重写参考轨迹生成逻辑

2. **src/safe_regret_mpc/params/safe_regret_mpc_params.yaml**
   - 修改：B矩阵（theta和etheta的影响系数）
   - 修改：Q矩阵（权重分配）
   - 修改：R矩阵（角速度成本）

---

## 🎯 理论依据

### MPC目标函数
```
min Σ [ (x_t - ref_t)' Q (x_t - ref_t) + u_t' R u_t ]
```

**关键点**：
1. **参考轨迹ref_t**必须是机器人当前应该跟踪的路径段
2. **权重矩阵Q**决定优先修正哪个误差
3. **动态模型B**决定控制输入对状态的影响

### 为什么之前转弯激进？

**错误链条**：
```
参考轨迹包含整个路径 → MPC不知道当前跟踪哪段 → 可能朝向错误方向
    ↓
B矩阵系数太小 → MPC以为角速度响应慢 → 使用过大的角速度
    ↓
cte和etheta权重相同 → MPC过度修正etheta → 激进转向
```

**修复后**：
```
参考轨迹只有最近的21个点 → MPC明确知道当前跟踪路径
    ↓
B矩阵系数合理 → MPC正确估计角速度响应
    ↓
cte权重是etheta的2倍 → MPC优先修正路径偏差 → 平滑转向
```

---

## 📚 相关文档

- 详细分析：`test/scripts/SAFE_REGRET_AGGRESSIVE_TURNING_ANALYSIS.md`
- 参数配置：`src/safe_regret_mpc/params/safe_regret_mpc_params.yaml`
- Tube MPC参考：`src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`

---

## ✅ 状态

- ✅ 问题诊断完成
- ✅ 修复代码编写完成
- ✅ 参数调整完成
- ✅ 编译成功
- ⏳ 待测试验证
