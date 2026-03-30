# Safe-Regret MPC参数调优指南

## 🎯 快速开始

### Phase 6 推荐配置（平滑模式）

```bash
# 方法1：使用交互式工具
cd /home/dixon/Final_Project/catkin
./src/safe_regret_mpc/scripts/switch_safe_regret_params.sh
# 选择: 2) 平滑模式

# 方法2：直接切换
cp src/safe_regret_mpc/params/safe_regret_mpc_params_smooth.yaml \
   src/safe_regret_mpc/params/safe_regret_mpc_params.yaml

# 方法3：启动时指定参数（无需切换）
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    reference_velocity:=0.7 \
    controller_frequency:=20
```

---

## 📊 配置方案对比

### 平滑模式（推荐）

**文件**: `safe_regret_mpc_params_smooth.yaml`

```yaml
system:
  controller_frequency: 20.0  # 提高控制频率

mpc:
  ref_vel: 0.7                # 适中速度
  max_angvel: 1.8             # 降低最大角速度
  horizon: 25                 # 增加预测视野

  # 关键权重调整
  w_cte: 15.0                 # 提高横向误差权重
  w_etheta: 15.0              # 提高航向权重
  w_vel: 8.0                  # 提高速度权重（稳定！）
  w_angvel: 15.0              # 提高角速度权重
  w_accel: 12.0               # 提高加速度权重（关键！）
```

**效果**:
- ✅ 运动平滑
- ✅ 减少抖动
- ✅ 速度: 0.7 m/s
- ✅ 适合: Phase 6物流运输

---

### 快速模式

**文件**: `safe_regret_mpc_params_fast.yaml`

```yaml
mpc:
  ref_vel: 1.0                # 提高速度
  max_angvel: 2.2             # 提高角速度

  # 优化速度
  w_vel: 3.0                  # 降低速度权重
  w_angvel: 8.0               # 降低角速度权重
  w_accel: 5.0                # 降低加速度权重
```

**效果**:
- ✅ 高速运输
- ✅ 快速到达
- ⚠️ 可能有轻微抖动

---

## 🔧 关键调参点

### 1. 控制频率（最重要！）

```yaml
system:
  controller_frequency: 20.0  # Hz
```

| 频率 | 效果 | 计算 |
|------|------|------|
| 10 Hz | 基础 | 低 |
| **20 Hz** | **推荐** | 中 |
| 30 Hz | 高精度 | 高 |

**抖动问题** → 提高到20Hz

---

### 2. 参考速度

```yaml
mpc:
  ref_vel: 0.7  # m/s
```

| 场景 | 推荐速度 |
|------|---------|
| Phase 6直线 | 0.7-1.0 m/s |
| 有障碍物 | 0.5-0.7 m/s |
| 精密对接 | 0.3-0.5 m/s |

---

### 3. 最大角速度

```yaml
mpc:
  max_angvel: 1.8  # rad/s
```

| 值 | 效果 |
|---|------|
| 1.5 | 转向平滑 |
| **1.8** | **推荐** |
| 2.2 | 转向快速 |

---

### 4. 代价权重（核心！）

```yaml
mpc:
  w_cte: 15.0          # 横向误差
  w_etheta: 15.0       # 航向误差
  w_vel: 8.0           # 速度权重
  w_angvel: 15.0       # 角速度权重
  w_accel: 12.0        # 加速度权重（平滑关键！）
```

**权重关系**:
- `w_vel`高 → 速度稳定
- `w_accel`高 → 减少急加速（平滑！）
- `w_etheta`高 → 优先转向
- `w_cte`高 → 路径精确

---

## 🎮 使用示例

### 启动Phase 6测试（平滑模式）

```bash
# 1. 切换到平滑模式
./src/safe_regret_mpc/scripts/switch_safe_regret_params.sh
# 选择: 2

# 2. 启动测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 观察改善
# - 运动更平滑
# - 抖动减少
# - 速度稳定
```

### 启动Phase 6测试（快速模式）

```bash
# 1. 切换到快速模式
./src/safe_regret_mpc/scripts/switch_safe_regret_params.sh
# 选择: 3

# 2. 启动测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

### 临时调参（不修改文件）

```bash
# 临时提高速度
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    reference_velocity:=1.0

# 临时提高控制频率
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    controller_frequency:=30

# 组合使用
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    reference_velocity:=0.8 \
    controller_frequency:=25 \
    mpc_steps:=25
```

---

## 📈 性能指标

| 配置 | 速度 | 平滑性 | 精度 | 适用场景 |
|------|------|--------|------|---------|
| 默认 | 0.5 m/s | 中 | 中 | 通用 |
| 平滑 | 0.7 m/s | **高** | 高 | **Phase 6推荐** |
| 快速 | 1.0 m/s | 中 | 中 | 紧急任务 |

---

## ⚠️ 常见问题

### 问题1：机器人运动抖动

**原因**: 控制频率低或加速度权重低

**解决**:
```yaml
system:
  controller_frequency: 20.0  # 提高频率

mpc:
  w_accel: 12.0               # 提高加速度权重
```

### 问题2：速度太慢

**原因**: 参考速度低或速度权重高

**解决**:
```yaml
mpc:
  ref_vel: 0.8                # 提高参考速度
  w_vel: 5.0                  # 降低速度权重
```

### 问题3：转向不灵敏

**原因**: 航向权重低或角速度限制严

**解决**:
```yaml
mpc:
  w_etheta: 20.0              # 提高航向权重
  max_angvel: 2.0             # 提高角速度限制
```

---

## 🔍 调参技巧

### 渐进式调参

1. **先调控制频率**
   ```yaml
   controller_frequency: 20.0  # 从默认提高到20
   ```

2. **再调参考速度**
   ```yaml
   ref_vel: 0.7  # 逐步提高
   ```

3. **最后调权重**
   ```yaml
   w_accel: 12.0  # 重点调整这个！
   w_vel: 8.0
   ```

### 测试流程

```bash
# 1. 切换配置
./switch_safe_regret_params.sh

# 2. 启动测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 观察指标
rostopic echo /cmd_vel          # 速度命令
rostopic echo /mpc_trajectory   # MPC轨迹
rostopic echo /safe_regret_mpc/metrics  # 性能指标
```

### 评估标准

- ✅ 速度曲线平滑
- ✅ 无明显抖动
- ✅ 转向流畅
- ✅ 到达目标稳定

---

## 📁 文件结构

```
src/safe_regret_mpc/
├── params/
│   ├── safe_regret_mpc_params.yaml          # 当前活动配置
│   ├── safe_regret_mpc_params.yaml.backup   # 自动备份
│   ├── safe_regret_mpc_params_smooth.yaml   # 平滑模式
│   └── safe_regret_mpc_params_fast.yaml     # 快速模式
└── scripts/
    └── switch_safe_regret_params.sh         # 切换工具
```

---

**最后更新**: 2026-03-30
**适用**: Safe-Regret MPC Phase 6
