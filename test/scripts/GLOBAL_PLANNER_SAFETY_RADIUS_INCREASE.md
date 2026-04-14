# Global Planner安全半径增加0.1米

**日期**: 2026-04-07
**修改**: 将Global Planner的inflation_radius增加0.1米
**状态**: ✅ **已完成**

---

## 修改总结

| 参数 | 原值 | 新值 | 增加量 | 文件位置 |
|------|------|------|--------|---------|
| **obstacle_layer inflation_radius** | 0.55m | 0.65m | +0.1m | costmap_common_params.yaml:18 |
| **inflation_layer inflation_radius** | 0.55m | 0.65m | +0.1m | costmap_common_params.yaml:29 |
| **global_costmap inflation_radius** | 0.45m | 0.55m | +0.1m | global_costmap_params.yaml:16 |

---

## 详细修改内容

### 1. costmap_common_params.yaml - obstacle_layer

**文件**: `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml` (line 18)

```yaml
# 修改前
inflation_radius: 0.55  # 增加到55cm，提高安全性（原25cm→35cm→45cm→55cm）

# 修改后
inflation_radius: 0.65  # 增加到65cm，提高安全性（原25cm→35cm→45cm→55cm→65cm）
```

**影响**:
- 影响local_costmap和global_costmap
- 障碍物周围inflation半径增加
- Global Planner规划的路径会更远离障碍物

---

### 2. costmap_common_params.yaml - inflation_layer

**文件**: `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml` (line 29)

```yaml
# 修改前
inflation_radius: 0.55  # max. distance from an obstacle at which costs are incurred for planning paths (increased to 55cm for better safety)

# 修改后
inflation_radius: 0.65  # max. distance from an obstacle at which costs are incurred for planning paths (increased to 65cm for better safety)
```

**影响**:
- inflation层的inflation半径增加
- 影响路径规划时的障碍物代价计算
- 机器人会更早地开始避开障碍物

---

### 3. global_costmap_params.yaml - inflation_layer

**文件**: `src/tube_mpc_ros/mpc_ros/params/global_costmap_params.yaml` (line 16)

```yaml
# 修改前
inflation_radius: 0.45  # 增加到45cm，提高安全性（原25cm→35cm→45cm）

# 修改后
inflation_radius: 0.55  # 增加到55cm，提高安全性（原25cm→35cm→45cm→55cm）
```

**影响**:
- **仅影响global_costmap**
- Global Planner（如NavFn、A*）会使用更大的安全半径
- 全局路径会更保守，远离障碍物

---

## 理论依据

### Inflation Radius的作用

在ROS costmap中，`inflation_radius`定义了：

1. **障碍物膨胀距离**
   - 在inflation_radius内的所有单元格都被标记为"有代价"
   - 距离障碍物越近，代价越高（ lethal cost 254）

2. **路径规划影响**
   - Global Planner会避免高代价区域
   - 路径会尽量保持在inflation_radius之外

3. **安全保证**
   - 机器人中心与障碍物保持至少inflation_radius的距离
   - 考虑机器人footprint，实际安全距离 = inflation_radius - robot_radius

### 安全距离计算

**机器人尺寸**：
- Footprint: 54cm × 54cm
- 机器人半径: ~27cm (对角线的一半)

**实际安全距离**：
```
修改前:
  Local Costmap: 0.65m - 0.27m = 0.38m (38cm)
  Global Costmap: 0.45m - 0.27m = 0.18m (18cm) ⚠️ 过小！

修改后:
  Local Costmap: 0.65m - 0.27m = 0.38m (38cm) ✅
  Global Costmap: 0.55m - 0.27m = 0.28m (28cm) ✅ 更合理
```

---

## 预期效果

### ✅ 优点

1. **撞墙概率降低**
   - Global Planner规划的路径更保守
   - 路径不会过于接近障碍物或墙壁

2. **导航安全性提高**
   - 全局路径考虑更大的安全余量
   - 在狭窄通道中更谨慎

3. **与DR约束一致性更好**
   - DR安全半径: 0.5m (tube_radius) + 0.2m (tube_compensation) = 0.7m
   - Global Planner inflation: 0.55m
   - **一致性**: 0.55m < 0.7m ✅

### ⚠️ 缺点

1. **路径可能不够优化**
   - 在宽阔空间中，路径可能会绕远
   - 不会贴着障碍物走最短路径

2. **某些狭窄空间可能无法通过**
   - 如果走廊宽度 < 2 × inflation_radius + robot_width
   - 通道宽度 < 2 × 0.55 + 0.54 = 1.64m 时可能无法规划路径

3. **任务时间可能略微增加**
   - 路径更保守 = 路径更长
   - 预计增加5-10%的任务时间

---

## Costmap层次结构

ROS costmap有多个层次，每层都有自己的inflation_radius：

| Costmap类型 | inflation_radius | 作用 |
|------------|------------------|------|
| **local_costmap** (inflation_layer) | 0.65m | 局部规划避障（DWA/TrajectoryPlanner） |
| **global_costmap** (inflation_layer) | 0.55m | 全局规划路径（GlobalPlanner） |
| **common_params** (obstacle_layer) | 0.65m | 障碍物检测和膨胀 |

**关键**: global_costmap的inflation_radius (0.55m) < local_costmap (0.65m) 是合理的，因为：
- 全局路径可以更激进一些（稍微靠近障碍物）
- 局部规划会负责最终的安全避障

---

## 配合DR约束的调整

之前已经修改的DR安全半径：
- `dr_tightening/params/dr_tightening_params.yaml`:
  - `tube_radius: 0.5m` (原0.4m)
  - `safety_buffer: 1.0m` (原0.9m)

现在修改的Global Planner半径：
- `inflation_radius: 0.55m` (global_costmap)

**一致性检查**：
```
层次关系（从保守到激进）:
1. DR安全边界: 1.0m (最保守)
2. Local Costmap inflation: 0.65m
3. DR tube_radius: 0.5m
4. Global Costmap inflation: 0.55m ← 新增
5. 机器人radius: 0.27m (最激进)
```

这个层次结构是合理的：
- DR约束提供最终的硬安全保证
- Costmap inflation提供路径规划的软约束
- 机器人实际控制可以在tube内灵活调整

---

## 修改的文件

1. ✅ `src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml` (2处修改)
2. ✅ `src/tube_mpc_ros/mpc_ros/params/global_costmap_params.yaml` (1处修改)

---

## 测试建议

### 1. 验证路径规划

```bash
# 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_visualization:=true

# 在RViz中观察：
# 1. 全局路径（绿色）是否更远离墙壁
# 2. Local costmap的inflation区域（蓝色/红色）是否变大
# 3. 机器人是否保持在路径中间
```

### 2. 检查撞墙情况

运行自动化测试，比较修改前后的撞墙次数：

```bash
# 修改后的测试
./test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 3 \
    --timeout 240 \
    --no-viz

# 检查日志中的撞墙次数
grep "collision" test_results/*/logs/* | wc -l
```

### 3. 性能对比

| 指标 | 修改前 | 修改后（预期） |
|------|--------|---------------|
| 撞墙次数 | 较多 | 减少 |
| 平均路径长度 | 较短 | 略微增加 |
| 任务完成时间 | 较短 | 略微增加 (+5-10%) |
| 安全性 | 一般 | 提高 |

---

## 回滚方法

如果需要恢复原参数：

### 方法1: Git恢复
```bash
git checkout src/tube_mpc_ros/mpc_ros/params/costmap_common_params.yaml
git checkout src/tube_mpc_ros/mpc_ros/params/global_costmap_params.yaml
```

### 方法2: 手动恢复
反向执行上述修改：
- obstacle_layer: 0.65 → 0.55
- inflation_layer: 0.65 → 0.55
- global_costmap: 0.55 → 0.45

---

## 注意事项

1. **无需重新编译**
   - 这些是YAML配置文件
   - 修改后直接生效（重启ROS节点）

2. **需要重启ROS节点**
   - move_base节点需要重启才能读取新配置
   - 或者重启整个测试系统

3. **与DR参数的配合**
   - DR安全半径 (0.5m + 0.2m) > Global Planner inflation (0.55m) ✅
   - 这个顺序是正确的：DR提供最终安全保证

---

## 下一步

**状态**: ✅ 所有修改已完成

**建议测试顺序**:
1. 简单的2点导航测试（验证路径是否合理）
2. 单货架测试（验证基本功能）
3. 多货架测试（验证性能和安全性）

**预期**:
- ✅ 撞墙次数减少
- ✅ 路径更加保守安全
- ⚠️  任务时间可能略微增加
