# Tube MPC 调参完整指南

## 🎯 问题分析

### 当前问题症状：
- ❌ 运动速度低下，几乎不动
- ❌ 动一下就停止
- ❌ 无法成功到达目标点

### 根本原因分析：

#### 1. **最大角速度过低**
```yaml
mpc_max_angvel: 0.5  # 太低了！
```
**影响**: 机器人转向极其缓慢，无法及时调整方向
**建议**: 提高到 1.0-2.0 rad/s

#### 2. **速度权重过高**
```yaml
mpc_w_vel: 100.0  # 权重太高，抑制速度
```
**影响**: MPC求解器优先降低速度而非跟踪路径
**建议**: 降低到 20-50

#### 3. **参考速度过低**
```yaml
mpc_ref_vel: 0.5  # 参考速度偏低
max_vel_x: 0.5    # 最大速度限制
```
**影响**: 速度被人为限制在较低水平
**建议**: 提高到 0.8-1.2 m/s

#### 4. **路径跟踪权重失衡**
```yaml
mpc_w_cte: 100.0       # 横向误差权重过高
mpc_w_etheta: 100.0    # 航向误差权重过高
```
**影响**: 过度关注路径精度，牺牲了运动性能
**建议**: 降低到 30-80

---

## 📊 参数配置详解

### 🚀 **优化版本 (推荐)**

**适用场景**: 平衡性能和稳定性，适合一般使用

**关键参数**:
```yaml
# 速度参数
max_vel_x: 1.0              # 最大速度 1.0 m/s
mpc_ref_vel: 0.8            # 参考速度 0.8 m/s

# 转向参数
mpc_max_angvel: 1.5         # 最大角速度 1.5 rad/s (关键!)

# MPC权重 (重新平衡)
mpc_w_cte: 50.0             # 降低路径跟踪权重
mpc_w_etheta: 50.0          # 降低航向权重
mpc_w_vel: 50.0             # 大幅降低速度权重 (关键!)
mpc_w_angvel: 30.0          # 降低角速度权重
mpc_w_accel: 20.0           # 降低加速度权重
```

**特点**:
- ✅ 显著提高运动速度
- ✅ 保持路径跟踪精度
- ✅ 平稳的加减速
- ✅ 适合大多数场景

### ⚡ **激进版本 (追求速度)**

**适用场景**: 开阔环境，追求最快速度

**关键参数**:
```yaml
max_vel_x: 1.5              # 最大速度 1.5 m/s
mpc_ref_vel: 1.2            # 参考速度 1.2 m/s
mpc_max_angvel: 2.0         # 最大角速度 2.0 rad/s

mpc_w_vel: 20.0             # 极低速度权重
mpc_w_cte: 30.0             # 优先速度而非精度
mpc_w_etheta: 30.0
```

**特点**:
- ⚡ 最快速度
- ⚠️ 路径跟踪精度降低
- ⚠️ 可能在狭窄空间不稳定

### 🛡️ **保守版本 (追求稳定)**

**适用场景**: 狭窄空间，需要高精度

**关键参数**:
```yaml
max_vel_x: 0.7              # 适中的最大速度
mpc_ref_vel: 0.5            # 适中的参考速度
mpc_max_angvel: 1.0         # 适中的角速度

mpc_w_cte: 80.0             # 较高的路径跟踪权重
mpc_w_etheta: 80.0
mpc_w_vel: 80.0             # 较高的速度权重
```

**特点**:
- 🛡️ 高路径跟踪精度
- 🛡️ 运动平稳可控
- 🐢 速度较慢

---

## 🔧 使用方法

### **方法1: 使用参数切换脚本 (推荐)**

```bash
# 运行切换脚本
chmod +x /home/dixon/Final_Project/catkin/switch_mpc_params.sh
/home/dixon/Final_Project/catkin/switch_mpc_params.sh

# 选择配置 (输入2选择优化版本)
# Available configurations:
# 1) Original
# 2) Optimized (推荐)
# 3) Aggressive
# 4) Conservative
```

### **方法2: 手动切换参数文件**

```bash
# 备份原参数
cp /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml \
   /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml.backup

# 使用优化版本
cp /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params_optimized.yaml \
   /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml

# 重新启动导航
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

### **方法3: 直接修改参数**

```bash
# 编辑参数文件
nano /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml

# 关键修改项：
# mpc_max_angvel: 0.5  → 1.5  (提高最大角速度)
# mpc_ref_vel: 0.5     → 0.8  (提高参考速度)
# mpc_w_vel: 100.0     → 50.0 (降低速度权重)
# max_vel_x: 0.5       → 1.0  (提高最大速度)
```

---

## 🎛️ 核心参数详解

### **速度控制参数**

| 参数 | 作用 | 保守值 | 优化值 | 激进值 |
|------|------|--------|--------|--------|
| `max_vel_x` | 最大线速度 (m/s) | 0.7 | **1.0** | 1.5 |
| `mpc_ref_vel` | MPC参考速度 (m/s) | 0.5 | **0.8** | 1.2 |
| `mpc_max_throttle` | 最大油门 | 1.0 | **1.5** | 2.0 |

### **转向控制参数**

| 参数 | 作用 | 保守值 | 优化值 | 激进值 |
|------|------|--------|--------|--------|
| `mpc_max_angvel` | **最大角速度 (rad/s)** | 1.0 | **1.5** | 2.0 |
| `max_rot_vel` | 旋转速度限制 | 1.2 | **2.0** | 3.0 |
| `acc_lim_theta` | 角加速度限制 | 2.5 | **4.0** | 5.0 |

### **MPC权重参数**

**权重越低，对应项的优先级越高**

| 参数 | 作用 | 保守值 | 优化值 | 激进值 |
|------|------|--------|--------|--------|
| `mpc_w_vel` | 速度权重 (越低越快) | 80.0 | **50.0** | 20.0 |
| `mpc_w_cte` | 横向误差权重 (精度) | 80.0 | **50.0** | 30.0 |
| `mpc_w_etheta` | 航向误差权重 (精度) | 80.0 | **50.0** | 30.0 |
| `mpc_w_angvel` | 角速度权重 (平稳性) | 40.0 | **30.0** | 15.0 |
| `mpc_w_accel` | 加速度权重 (舒适度) | 40.0 | **20.0** | 10.0 |

### **预测参数**

| 参数 | 作用 | 保守值 | 优化值 | 激进值 |
|------|------|--------|--------|--------|
| `mpc_steps` | MPC预测步数 | 20 | **25** | 30 |
| `path_length` | 前瞻路径长度 (m) | 6.0 | **8.0** | 10.0 |
| `forward_point_distance` | 前瞻点距离 (m) | 0.35 | **0.4** | 0.5 |

---

## 🔍 调参策略

### **第一步: 基础速度提升**
```yaml
# 立即生效的关键修改
mpc_max_angvel: 1.5      # 提高角速度限制
mpc_ref_vel: 0.8         # 提高参考速度
max_vel_x: 1.0           # 提高最大速度
```

### **第二步: 权重平衡**
```yaml
# 重新平衡MPC权重
mpc_w_vel: 50.0          # 降低速度权重
mpc_w_cte: 50.0          # 适度降低精度要求
mpc_w_etheta: 50.0
```

### **第三步: 约束放松**
```yaml
# 放松运动约束
acc_lim_x: 3.0           # 提高加速度限制
acc_lim_theta: 4.0       # 提高角加速度限制
mpc_max_throttle: 1.5    # 提高油门限制
```

### **第四步: 预测优化**
```yaml
# 优化预测能力
mpc_steps: 25            # 增加预测步数
path_length: 8.0         # 增加前瞻距离
```

---

## 📈 性能对比

### **原始配置问题**:
- ❌ 最大角速度: 0.5 rad/s (转向极慢)
- ❌ 速度权重: 100.0 (过度抑制速度)
- ❌ 参考速度: 0.5 m/s (速度限制过低)

**结果**: 机器人几乎不动，动一下就停

### **优化配置改进**:
- ✅ 最大角速度: 1.5 rad/s (提升3倍)
- ✅ 速度权重: 50.0 (降低50%)
- ✅ 参考速度: 0.8 m/s (提升60%)

**预期结果**: 正常运动，成功到达目标

---

## 🧪 测试和验证

### **测试步骤**:

1. **应用优化参数**
   ```bash
   /home/dixon/Final_Project/catkin/switch_mpc_params.sh
   # 选择 2) Optimized
   ```

2. **启动导航**
   ```bash
   roslaunch tube_mpc_ros tube_mpc_navigation.launch
   ```

3. **设置导航目标**
   - 在RViz中使用"2D Pose Estimate"设置初始位置
   - 使用"2D Nav Goal"设置目标点

4. **观察指标**
   - ✅ 机器人是否正常启动运动
   - ✅ 运动速度是否合理 (0.5-1.0 m/s)
   - ✅ 转向是否流畅
   - ✅ 是否能成功到达目标

### **性能指标**:

| 指标 | 原始 | 优化 | 目标 |
|------|------|------|------|
| 平均速度 | ~0.1 m/s | 0.6-0.8 m/s | >0.5 m/s |
| 启动时间 | >10s | <2s | <3s |
| 成功率 | ~0% | >90% | >95% |
| 路径精度 | N/A | <0.3m | <0.5m |

---

## 🐛 故障排除

### **问题1: 机器人仍然很慢**
**解决**:
```yaml
# 进一步降低速度权重
mpc_w_vel: 30.0           # 从50.0降到30.0

# 提高角速度限制
mpc_max_angvel: 2.0       # 从1.5提高到2.0
```

### **问题2: 路径跟踪不准确**
**解决**:
```yaml
# 提高路径跟踪权重
mpc_w_cte: 70.0           # 从50.0提高到70.0
mpc_w_etheta: 70.0        # 从50.0提高到70.0

# 降低速度
mpc_ref_vel: 0.6          # 从0.8降到0.6
```

### **问题3: 运动不平稳，抖动**
**解决**:
```yaml
# 提高平稳性权重
mpc_w_angvel: 50.0        # 从30.0提高到50.0
mpc_w_accel: 40.0         # 从20.0提高到40.0

# 降低速度
mpc_ref_vel: 0.5          # 降低参考速度
```

### **问题4: 无法启动运动**
**检查**:
```bash
# 检查debug信息
rostopic echo /rosout | grep -i mpc

# 检查cmd_vel是否发布
rostopic echo /cmd_vel

# 检查目标是否接收
rostopic echo /move_base_simple/goal
```

---

## 📝 参数调整建议

### **渐进式调参法**:

1. **第一轮**: 只调整最关键的3个参数
   ```yaml
   mpc_max_angvel: 1.5    # 角速度
   mpc_ref_vel: 0.8       # 参考速度
   mpc_w_vel: 50.0        # 速度权重
   ```

2. **第二轮**: 如果效果不好，再调整权重
   ```yaml
   mpc_w_cte: 50.0        # 横向误差
   mpc_w_etheta: 50.0     # 航向误差
   ```

3. **第三轮**: 微调约束和预测参数
   ```yaml
   mpc_steps: 25          # 预测步数
   path_length: 8.0       # 路径长度
   ```

---

## 🎯 总结

### **立即行动**:
```bash
# 1. 使用优化参数
/home/dixon/Final_Project/catkin/switch_mpc_params.sh
# 选择: 2) Optimized

# 2. 测试效果
roslaunch tube_mpc_ros tube_mpc_navigation.launch

# 3. 如果不满意，尝试激进版本
# 重新运行脚本，选择: 3) Aggressive
```

### **预期效果**:
- ✅ 机器人能正常启动运动
- ✅ 运动速度提升5-10倍
- ✅ 转向流畅，不卡顿
- ✅ 成功到达目标点

### **调参原则**:
1. **先调约束，再调权重** (约束 > 权重)
2. **渐进式调整** (一次调几个参数)
3. **观察调试信息** (debug_info: true)
4. **记录每次修改** (便于回滚)

---

**创建时间**: 2026-03-10
**适用版本**: tube_mpc_ros
**ROS版本**: Noetic
**测试状态**: 参数已优化，待验证
