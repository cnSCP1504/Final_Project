# DR安全半径增加0.1米修改报告

**日期**: 2026-04-07
**修改**: 将所有DR（Distributionally Robust）相关的安全半径参数增加0.1米
**状态**: ✅ **已完成并重新编译**

---

## 修改参数总览

| 参数 | 原值 | 新值 | 增加量 | 文件位置 |
|------|------|------|--------|---------|
| `tube_radius` | 0.4 | 0.5 | +0.1 | dr_tightening_params.yaml |
| `safety_buffer` | 0.9 | 1.0 | +0.1 | dr_tightening_params.yaml |
| `dr_msg.tube_radius` | 0.5 | 0.6 | +0.1 | safe_regret_mpc_node.cpp |
| `dr_msg.safety_margin` | 1.0 | 1.1 | +0.1 | safe_regret_mpc_node.cpp |
| `tube_compensation` | 0.1 | 0.2 | +0.1 | safe_regret_mpc_params.yaml |

---

## 详细修改内容

### 1. dr_tightening参数文件

**文件**: `src/dr_tightening/params/dr_tightening_params.yaml`

```yaml
# 修改前
tube_radius: 0.4                  # ē: Tube radius (from Tube MPC, reduced by 0.2)
safety_buffer: 0.9                # Safety buffer for obstacle avoidance (reduced by 0.2)

# 修改后
tube_radius: 0.5                  # ē: Tube radius (increased by 0.1 for safety)
safety_buffer: 1.0                # Safety buffer for obstacle avoidance (increased by 0.1)
```

**影响**:
- DR模块计算的margin会使用更大的tube半径作为基础
- 安全缓冲区增加，提供更保守的避障行为

---

### 2. safe_regret_mpc_node.cpp硬编码值

**文件**: `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp` (lines 342, 348)

```cpp
// 修改前
dr_msg.tube_radius = 0.5;
dr_msg.safety_margin = 1.0;

// 修改后
dr_msg.tube_radius = 0.6;  // Increased by 0.1 for safety
dr_msg.safety_margin = 1.1;  // Increased by 0.1 for safety
```

**影响**:
- 发布的DR消息中包含更大的tube半径和安全边界
- 影响MPC求解器中的DR约束计算

---

### 3. safe_regret_mpc参数文件

**文件**: `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml` (line 69)

```yaml
# 修改前
tube_compensation: 0.1     # Tube compensation η_ℰ

# 修改后
tube_compensation: 0.2     # Tube compensation η_ℰ (increased by 0.1 for safety)
```

**影响**:
- 在MPC求解器中，DR约束的tube补偿项增加
- DR约束公式: `tracking_error ≤ margin + η_ℰ`
  - 原约束: `tracking_error ≤ σ_{k,t} + 0.1`
  - 新约束: `tracking_error ≤ σ_{k,t} + 0.2`
  - 提供更宽松的约束边界（更安全）

---

## 理论依据

根据Manuscript中的DR约束公式：

$$h(z_t) \geq \sigma_{k,t} + \eta_\mathcal{E}$$

其中：
- $\sigma_{k,t}$ = DR margin（由dr_tightening计算）
- $\eta_\mathcal{E} = L_h \cdot \bar{e}$ = Tube补偿
- $L_h$ = Lipschitz常数（1.0）
- $\bar{e}$ = Tube半径

修改后的参数：
- Tube半径增加: $\bar{e}: 0.4 \to 0.5$ (dr_tightening)
- Tube补偿增加: $\eta_\mathcal{E}: 0.1 \to 0.2$ (safe_regret_mpc)
- 总体效果: 允许更大的跟踪误差，但保持安全性

---

## 预期效果

### 安全性提升
1. **更大的Tube半径**: 允许更大的跟踪误差而不触发约束违反
2. **更大的安全缓冲**: 提供更保守的避障行为，减少碰撞风险
3. **更宽松的DR约束**: 降低MPC求解器因约束过紧而失败的概率

### 性能权衡
- ✅ **优点**: MPC可行性提高，撞墙概率降低
- ⚠️ **缺点**: 跟踪精度可能略微下降，任务时间可能增加

### 适用场景
- **推荐**: 在狭窄、复杂环境中导航
- **不推荐**: 对跟踪精度要求极高的任务

---

## 编译状态

✅ **重新编译完成** (2026-04-07)

```bash
catkin_make --only-pkg-with-deps safe_regret_mpc
```

编译输出：
- ✅ safe_regret_mpc_node: 成功编译
- ✅ safe_regret_mpc_lib: 成功编译
- ✅ 依赖包 (dr_tightening, stl_monitor): 成功编译

---

## 测试建议

### 1. 验证DR约束生效

运行测试并检查日志：
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_dr:=true debug_mode:=true 2>&1 | grep "DR constraint"
```

期望输出：
- "DR constraint: tracking_error ≤ (sigma_kt + 0.2)"  # 新的tube_compensation
- "Tube radius: 0.5"  # 新的tube_radius

### 2. 对比性能测试

测试相同场景，比较修改前后的指标：
- MPC成功率
- 平均跟踪误差
- 任务完成时间
- 安全违反次数

```bash
# 修改后测试
./test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 3 \
    --timeout 240 \
    --no-viz
```

### 3. 检查RViz可视化

启动RViz，观察：
- Tube边界是否变大（tube_mpc）
- DR margin是否增加（dr_tightening）

---

## 回滚方法

如果需要恢复原参数：

### 方法1: Git恢复
```bash
git checkout src/dr_tightening/params/dr_tightening_params.yaml
git checkout src/safe_regret_mpc/src/safe_regret_mpc_node.cpp
git checkout src/safe_regret_mpc/params/safe_regret_mpc_params.yaml
catkin_make --only-pkg-with-deps safe_regret_mpc
```

### 方法2: 手动恢复
反向执行上述修改：
- tube_radius: 0.5 → 0.4
- safety_buffer: 1.0 → 0.9
- tube_radius (msg): 0.6 → 0.5
- safety_margin (msg): 1.1 → 1.0
- tube_compensation: 0.2 → 0.1

---

## 修改的文件

1. ✅ `src/dr_tightening/params/dr_tightening_params.yaml` (lines 15, 19)
2. ✅ `src/safe_regret_mpc/src/safe_regret_mpc_node.cpp` (lines 342, 348)
3. ✅ `src/safe_regret_mpc/params/safe_regret_mpc_params.yaml` (line 69)

---

## 下一步

1. **重新source环境**:
   ```bash
   source devel/setup.bash
   ```

2. **运行测试验证**:
   ```bash
   ./test/scripts/run_automated_test.py \
       --model safe_regret \
       --shelves 1 \
       --timeout 120 \
       --no-viz
   ```

3. **监控性能指标**:
   - MPC可行性率应保持或提高
   - 安全违反率应降低
   - 跟踪误差可能略微增加

---

**状态**: ✅ 所有修改已完成，系统已重新编译，可以开始测试。
