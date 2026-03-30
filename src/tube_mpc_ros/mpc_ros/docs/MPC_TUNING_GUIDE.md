# MPC参数调优指南

## 📊 参数配置方案对比

### 方案1：平滑模式（推荐）- `tube_mpc_params_smooth.yaml`

**适用场景**：
- 日常物流运输任务
- 要求运动平滑，减少抖动
- 载货运输（避免货物损坏）

**关键参数**：
```yaml
# 速度控制
max_vel_x: 1.2              # 降低最大速度
max_rot_vel: 2.0            # 降低最大角速度
controller_freq: 20         # 提高控制频率（关键！）

# 加速度限制（重点！）
acc_lim_x: 2.0              # 降低线加速度
acc_lim_theta: 3.0          # 降低角加速度

# MPC权重
mpc_w_accel: 15.0           # 提高加速度权重
mpc_w_accel_d: 10.0         # 提高加速度变化权重（关键！）
mpc_w_angvel: 40.0          # 提高角速度权重
mpc_w_vel: 8.0              # 提高速度权重
```

**效果**：
- ✅ 运动平滑，减少急加速/急减速
- ✅ 转向稳定，减少抖动
- ⚠️ 速度较慢（0.7 m/s）

---

### 方案2：快速模式 - `tube_mpc_params_fast.yaml`

**适用场景**：
- 紧急任务，需要快速到达
- 空载运输
- 长距离直线运输

**关键参数**：
```yaml
# 速度控制
max_vel_x: 1.8              # 提高最大速度
max_rot_vel: 3.0            # 提高最大角速度
mpc_ref_vel: 1.0            # 提高参考速度

# 加速度限制
acc_lim_x: 4.0              # 提高线加速度
acc_lim_theta: 5.0          # 提高角加速度

# MPC权重
mpc_w_vel: 3.0              # 降低速度权重（优先速度）
mpc_w_angvel: 20.0          # 降低角速度权重
mpc_w_accel: 5.0            # 降低加速度权重
```

**效果**：
- ✅ 速度快（1.0 m/s）
- ✅ 到达时间短
- ⚠️ 可能有轻微抖动

---

### 方案3：精确模式 - `tube_mpc_params_precision.yaml`

**适用场景**：
- 精密对接任务
- 狭窄空间导航
- 需要精确到达目标点

**关键参数**：
```yaml
# 速度控制
max_vel_x: 0.8              # 降低最大速度
max_rot_vel: 1.5            # 降低最大角速度
controller_freq: 30         # 大幅提高控制频率
mpc_steps: 30               # 增加预测步数

# 目标容差
xy_goal_tolerance: 0.15     # 严格位置容差
yaw_goal_tolerance: 0.2     # 严格角度容差

# MPC权重
mpc_w_cte: 80.0             # 大幅提高横向误差权重
mpc_w_etheta: 100.0         # 大幅提高航向权重
mpc_w_accel: 20.0           # 大幅提高加速度权重
```

**效果**：
- ✅ 路径跟踪精确
- ✅ 到达精度高
- ⚠️ 速度慢（0.5 m/s）

---

## 🔧 关键参数详解

### 1. 控制频率（controller_freq）

**作用**：MPC求解器更新频率

| 值 | 效果 | 适用场景 |
|---|------|---------|
| 10 Hz | 基础性能 | 一般导航 |
| 20 Hz | **推荐**，平滑控制 | 日常任务 |
| 30 Hz | 高精度，计算量大 | 精密对接 |

**调优建议**：
- 抖动严重 → 提高到20Hz
- 计算负载高 → 降低到10Hz

---

### 2. 速度限制（max_vel_x, max_rot_vel）

**作用**：机器人运动速度上限

| 参数 | 范围 | 推荐值 |
|------|------|--------|
| max_vel_x | 0.5-2.0 m/s | 1.2 |
| max_rot_vel | 1.5-3.0 rad/s | 2.0 |

**调优建议**：
- Phase 6直线运输 → 1.2-1.8 m/s
- 有障碍物环境 → 0.8-1.0 m/s
- 狭窄空间 → 0.5-0.8 m/s

---

### 3. 加速度限制（acc_lim_x, acc_lim_theta）

**作用**：限制加速度变化，**这是平滑性的关键！**

| 参数 | 抖动 | 平滑 | 说明 |
|------|------|------|------|
| acc_lim_x | 4.0 | 2.0 | 线加速度 |
| acc_lim_theta | 5.0 | 3.0 | 角加速度 |

**调优建议**：
- 运动抖动 → **降低加速度**（最有效！）
- 启动/停止急 → 降低加速度
- 转向不流畅 → 降低角加速度

---

### 4. MPC代价权重

**核心权重**：

```yaml
mpc_w_cte: 40.0              # 横向误差权重
mpc_w_etheta: 80.0           # 航向误差权重
mpc_w_vel: 8.0               # 速度权重
mpc_w_angvel: 40.0           # 角速度权重
mpc_w_accel: 15.0            # 加速度权重（平滑性关键）
mpc_w_accel_d: 10.0          # 加速度变化权重（平滑性关键）
```

**权重关系**：
```
etheta > cte     : 优先转向
vel高            : 稳定速度
accel高          : 减少急加速（平滑！）
accel_d高        : 减少加速度变化（平滑！）
```

**调优建议**：
- 运动抖动 → **提高accel和accel_d**
- 转向频繁 → 提高etheta
- 速度不稳定 → 提高vel
- 路径偏差大 → 提高cte

---

### 5. MPC预测视野（mpc_steps, path_length）

**作用**：MPC预测的时域长度

| 参数 | 短视野 | 长视野 |
|------|--------|--------|
| mpc_steps | 20 | 30 |
| path_length | 8.0 | 12.0 |

**调优建议**：
- 直线运输 → 20步即可
- 复杂路径 → 增加到25-30步
- 高速运动 → 增加预测步数

---

## 🎯 Phase 6推荐配置

### S1→S2物流运输（直线16米）

**推荐：平滑模式**

```bash
# 使用切换工具
./src/tube_mpc_ros/mpc_ros/scripts/switch_mpc_params.sh
# 选择: 2) 平滑模式

# 或手动切换
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params_smooth.yaml \
   src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml
```

**预期效果**：
- 速度：0.7 m/s（平稳）
- 到达时间：约23秒
- 运动质量：平滑，无明显抖动
- 适合：载货运输

---

## 🚀 使用方法

### 方法1：交互式切换（推荐）

```bash
cd /home/dixon/Final_Project/catkin
./src/tube_mpc_ros/mpc_ros/scripts/switch_mpc_params.sh
```

### 方法2：直接切换

```bash
# 平滑模式
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params_smooth.yaml \
   src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml

# 快速模式
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params_fast.yaml \
   src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml

# 精确模式
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params_precision.yaml \
   src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml
```

### 方法3：备份与恢复

```bash
# 创建备份
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml \
   src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml.my_backup

# 恢复备份
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml.my_backup \
   src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml
```

---

## 📈 性能指标对比

| 配置 | 速度 | 抖动 | 精度 | 适用场景 |
|------|------|------|------|---------|
| 原始 | 0.8 m/s | 中 | 中 | 通用 |
| 平滑 | 0.7 m/s | 低 | 高 | **推荐** |
| 快速 | 1.0 m/s | 中 | 中 | 紧急任务 |
| 精确 | 0.5 m/s | 低 | 极高 | 精密对接 |

---

## ⚠️ 常见问题与解决

### 问题1：机器人运动抖动

**原因**：加速度过大或控制频率低

**解决**：
```yaml
controller_freq: 20        # 提高控制频率
acc_lim_x: 2.0            # 降低加速度
mpc_w_accel: 15.0         # 提高加速度权重
mpc_w_accel_d: 10.0       # 提高加速度变化权重
```

### 问题2：速度太慢

**原因**：速度权重过高或参考速度过低

**解决**：
```yaml
mpc_ref_vel: 0.8           # 提高参考速度
mpc_w_vel: 5.0            # 降低速度权重
max_vel_x: 1.5            # 提高最大速度
```

### 问题3：转向不灵敏

**原因**：航向权重过低或角速度限制过严

**解决**：
```yaml
mpc_w_etheta: 80.0        # 提高航向权重
max_rot_vel: 2.5          # 提高最大角速度
mpc_max_angvel: 2.0       # 提高MPC角速度限制
```

### 问题4：路径偏差大

**原因**：横向误差权重过低

**解决**：
```yaml
mpc_w_cte: 60.0           # 提高横向误差权重
mpc_steps: 25             # 增加预测步数
path_length: 10.0         # 增加路径长度
```

---

## 🔍 调参技巧

### 渐进式调参

1. **先调整控制频率**
   - 从10Hz → 20Hz
   - 观察平滑性改善

2. **再调整加速度限制**
   - `acc_lim_x: 3.0 → 2.0`
   - 观察急加速是否减少

3. **最后调整权重**
   - 逐步提高`mpc_w_accel`和`mpc_w_accel_d`
   - 找到平衡点

### 测试方法

```bash
# 1. 切换到新配置
./switch_mpc_params.sh

# 2. 启动测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 3. 观察指标
rostopic echo /cmd_vel          # 观察速度命令
rostopic echo /mpc_trajectory   # 观察轨迹
```

### 评估标准

- ✅ 速度曲线平滑
- ✅ 角速度无突变
- ✅ 跟踪误差稳定
- ✅ 无明显抖动

---

**最后更新**: 2026-03-30
**适用版本**: Safe-Regret MPC Phase 6
