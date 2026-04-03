# 安全半径调整说明

**日期**: 2026-04-03
**调整**: 所有安全半径参数增加10cm
**原因**: 防止机器人撞墙

---

## 修改的参数汇总

### 1. Costmap障碍物膨胀半径

**文件**: `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml`

| 参数 | 原值 | 新值 | 说明 |
|------|------|------|------|
| `obstacle_layer.inflation_radius` | 0.25m | **0.35m** | 障碍物层膨胀半径 |
| `inflation_layer.inflation_radius` | 0.25m | **0.35m** | 膨胀层半径 |

**影响**: 路径规划时会离障碍物至少35cm，而不是25cm

---

### 2. Global Costmap膨胀半径

**文件**: `src/tube_mpc_ros/mpc_ros/params/global_costmap_params.yaml`

| 参数 | 原值 | 新值 | 说明 |
|------|------|------|------|
| `inflation_radius` | 0.25m | **0.35m** | 全局路径规划的障碍物膨胀半径 |

**影响**: 全局路径会更保守，远离障碍物

---

### 3. DR约束安全参数

**文件**: `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch`

| 参数 | 原值 | 新值 | 说明 |
|------|------|------|------|
| `tube_radius` | 0.08m | **0.18m** | 管状MPC半径（+10cm） |
| `safety_buffer` | 0.08m | **0.18m** | 安全缓冲区（+10cm） |
| `obstacle_influence_radius` | 1.5m | **1.6m** | 障碍物影响半径（+10cm） |

**影响**:
- 管状MPC会保持更大的安全边界
- 分布式鲁棒约束会使用更大的安全余量
- 障碍物检测范围更大

---

### 4. Terminal Set安全边界

**文件**: `src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml`

| 参数 | 原值 | 新值 | 说明 |
|------|------|------|------|
| `safety.margin` | 0.5m | **0.6m** | 终端集安全边界（+10cm） |
| `safety.tube_compensation` | 0.1m | **0.15m** | 管误差补偿（+5cm） |

**影响**: 终端集约束会更加保守

---

### 5. Tube MPC干扰边界

**文件**: `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`

| 参数 | 原值 | 新值 | 说明 |
|------|------|------|------|
| `disturbance_bound` | 0.15 | **0.20** | 干扰边界（+0.05） |

**影响**: Tube MPC会对更大的干扰进行补偿

---

## 总体影响

### 正面影响
✅ **安全性大幅提升**：所有安全边界都增加了10cm+
✅ **减少碰撞风险**：机器人会离障碍物更远
✅ **更保守的规划**：全局和局部路径都会避开障碍物

### 可能的负面影响
⚠️ **路径可能变长**：因为需要绕过更大的安全区域
⚠️ **狭窄通道可能无法通过**：如果通道宽度 < 机器人宽度 + 2×安全半径
⚠️ **可能更频繁的 replanning**：因为路径更容易违反安全约束

---

## 测试建议

### 1. 基本导航测试
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

**观察点**:
- 机器人是否离障碍物更远
- 是否还能通过狭窄通道
- 路径是否更平滑

### 2. 碰撞测试
- 在有障碍物的环境中导航
- 观察机器人是否保持安全距离
- 检查是否还有碰撞风险

### 3. 性能评估
- 导航时间是否增加
- 路径长度是否增加
- 是否有无法到达的目标

---

## 参数回退

如果发现安全半径太大导致无法导航，可以回退到原来的值：

```bash
# 回退costmap参数
inflation_radius: 0.25  # 从0.35改回0.25

# 回退DR参数
tube_radius: 0.08       # 从0.18改回0.08
safety_buffer: 0.08     # 从0.18改回0.08

# 回退terminal set
safety.margin: 0.5      # 从0.6改回0.5
```

---

## 修改的文件列表

1. `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml`
2. `src/tube_mpc_ros/mpc_ros/params/global_costmap_params.yaml`
3. `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch`
4. `src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml`
5. `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`

---

## 版本历史

| 版本 | 日期 | 修改 |
|------|------|------|
| V1.0 | 2026-04-03 | 初始版本：所有安全半径增加10cm |

---

**修改完成！现在机器人在导航时会保持更大的安全距离。** ✅
