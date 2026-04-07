# 安全半径增加 0.1 米

## 修改时间

2026-04-06

## 修改目的

提高系统安全性，扩大所有安全半径参数 0.1 米（10cm）

## 修改详情

### 1. Tube MPC 参数

**文件**: `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`

| 参数 | 原值 | 新值 | 变化 |
|------|------|------|------|
| `disturbance_bound` | 0.20 | 0.30 | +0.10 (+50%) |

**说明**: 扰动边界用于 Tube MPC 的鲁棒控制，增加后可以容忍更大的扰动，提高系统稳定性。

---

### 2. Costmap 参数

**文件**: `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml`

| 参数 | 原值 | 新值 | 变化 |
|------|------|------|------|
| `inflation_radius` (obstacle_layer) | 0.45 | 0.55 | +0.10 (+22%) |
| `inflation_radius` (inflation_layer) | 0.45 | 0.55 | +0.10 (+22%) |

**说明**: 障碍物膨胀半径，增加后机器人在路径规划时会保持更大的距离，避免碰撞。

**历史记录**:
- 原始值: 0.25m
- 第一次增加: 0.35m (+0.10)
- 第二次增加: 0.45m (+0.10)
- **本次增加: 0.55m (+0.10)**

---

### 3. DR Tightening 参数

**文件**: `src/dr_tightening/params/dr_tightening_params.yaml`

| 参数 | 原值 | 新值 | 变化 |
|------|------|------|------|
| `tube_radius` | 0.5 | 0.6 | +0.1 (+20%) |
| `safety_buffer` | 1.0 | 1.1 | +0.1 (+10%) |

**说明**:
- `tube_radius`: 管状半径 ē，用于 DR 约束收紧计算
- `safety_buffer`: 安全缓冲区，用于障碍物避免

---

### 4. Safe-Regret 参数

**文件**: `src/safe_regret/params/safe_regret_params.yaml`

| 参数 | 原值 | 新值 | 变化 |
|------|------|------|------|
| `tube_radius` | 0.5 | 0.6 | +0.1 (+20%) |

**说明**: 管状半径 ē，用于可行集计算 𝒳 ⊖ ℰ

---

## 影响分析

### 预期改进

1. **提高安全性**: 所有安全相关参数增加 10cm，降低碰撞风险
2. **更保守的路径**: 机器人会保持更大的障碍物距离
3. **更强的鲁棒性**: 可以容忍更大的扰动和不确定性

### 可能的副作用

1. **路径可行性**: 更大的安全半径可能导致某些狭窄通道无法通过
2. **任务时间**: 机器人可能需要更长时间完成任务（更保守的路径）
3. **速度限制**: 为了满足更严格的安全约束，速度可能需要降低

### 建议的后续调整

如果发现以下问题，可能需要调整其他参数：

1. **无法通过狭窄通道**
   - 考虑降低速度
   - 增加路径规划的灵活性
   - 使用更精细的地图

2. **任务时间过长**
   - 适当提高参考速度（`mpc_ref_vel`）
   - 调整路径权重（`mpc_w_cte`, `mpc_w_etheta`）
   - 优化目标到达阈值

3. **过度保守的行为**
   - 检查实际安全需求
   - 考虑动态调整安全半径
   - 使用自适应安全边界

---

## 测试验证

### 测试命令

```bash
# 测试 tube_mpc
python3 test/scripts/run_automated_test.py --model tube_mpc --shelves 1 --no-viz

# 测试 safe_regret
python3 test/scripts/run_automated_test.py --model safe_regret --shelves 1 --no-viz

# 完整测试
python3 test/scripts/run_automated_test.py --model tube_mpc --shelves 3 --no-viz
```

### 验证要点

1. ✅ 系统正常启动
2. ✅ 机器人能够正常导航
3. ✅ 没有碰撞发生
4. ✅ 路径保持合理距离
5. ✅ 任务能够完成

### 性能指标

- **碰撞次数**: 应该为 0
- **任务完成率**: 应该 ≥ 90%
- **平均任务时间**: 预计增加 10-20%
- **最小障碍物距离**: 预计增加 10cm

---

## 修改文件列表

1. `src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml`
2. `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml`
3. `src/dr_tightening/params/dr_tightening_params.yaml`
4. `src/safe_regret/params/safe_regret_params.yaml`

---

## 相关文档

- `test/scripts/SAFETY_RADIUS_INCREASE.md` - 历史安全半径调整记录
- `CLAUDE.md` - 项目总体说明

---

## 状态

✅ **修改完成，待测试验证**
