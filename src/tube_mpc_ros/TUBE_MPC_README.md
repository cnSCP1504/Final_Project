# Tube MPC 实现说明

## 概述

本实现将Tube MPC（管状模型预测控制）添加到mpc_ros包中，用于处理系统不确定性，提供鲁棒性保证。

## Tube MPC 核心概念

### 1. 系统分解

实际系统被分解为：
- **名义系统**: z[k+1] = Az[k] + Bv[k]
- **误差系统**: e[k+1] = (A-BK)e[k] + w[k]

其中：
- x[k] = z[k] + e[k] (实际状态 = 名义状态 + 误差)
- K 是LQR反馈增益
- w[k] 是扰动，属于扰动集W

### 2. 管状不变集

管状不变集 E = {e: ||P^0.5 e|| ≤ α}，保证误差始终在管内。

### 3. 约束收紧

原始约束: h(x) ≤ 0
收紧约束: h(z) - d ≤ 0

其中d是扰动对约束的影响。

### 4. 双层控制

总控制: u = v + Ke
- v: 名义控制（由MPC优化）
- Ke: 反馈控制（由LQR计算）

## 文件结构

### 新增文件

```
tube_mpc_ros/mpc_ros/
├── include/
│   └── TubeMPC.h          # Tube MPC核心类
│   └── TubeMPCNode.h      # Tube MPC ROS节点
├── src/
│   ├── TubeMPC.cpp          # Tube MPC实现
│   └── TubeMPCNode.cpp      # ROS节点实现
├── params/
│   └── tube_mpc_params.yaml  # Tube MPC参数配置
├── launch/
│   └── tube_mpc_navigation.launch  # Tube MPC启动文件
└── CMakeLists.txt              # 更新了Tube MPC节点编译
```

## 编译

```bash
cd ~/catkin_ws
catkin_make
source devel/setup.bash
```

## 使用方法

### 1. 启动Tube MPC导航

```bash
roslaunch mpc_ros tube_mpc_navigation.launch controller:=tube_mpc
```

### 2. 比较不同控制器

```bash
# Tube MPC
roslaunch mpc_ros tube_mpc_navigation.launch controller:=tube_mpc

# 标准MPC
roslaunch mpc_ros nav_gazebo.launch controller:=mpc

# DWA
roslaunch mpc_ros nav_gazebo.launch controller:=dwa

# Pure Pursuit
roslaunch mpc_ros nav_gazebo.launch controller:=pure_pursuit
```

### 3. 在RViz中查看

Tube MPC发布以下可视化话题：

- `/mpc_trajectory` - MPC预测轨迹
- `/tube_boundaries` - 管状边界可视化
- `/mpc_reference` - 参考路径

## 参数配置

### Tube MPC 特有参数

在 `params/tube_mpc_params.yaml` 中配置：

```yaml
# 扰动参数
disturbance_bound: 0.1              # 初始扰动界
enable_adaptive_disturbance: true    # 启用自适应扰动估计
disturbance_window_size: 100          # 扰动估计窗口大小

# Tube MPC 参数
mpc_steps: 20                       # 预测步数
mpc_ref_vel: 0.5                    # 参考速度
mpc_w_cte: 1000.0                  # 横向误差权重
mpc_w_etheta: 1000.0               # 航向误差权重
mpc_max_angvel: 1.0                 # 最大角速度

# 可视化
tube_visualization: true             # 启用管状可视化
```

### 参数调优建议

| 参数 | 增加效果 | 减少效果 |
|------|---------|---------|
| `disturbance_bound` | 更保守，鲁棒性更好 | 可能过于保守，性能下降 |
| `mpc_w_cte` | 路径跟踪更精确 | 可能过于僵硬 |
| `mpc_w_etheta` | 航向控制更准确 | 可能过度调整 |
| `mpc_w_vel` | 更快达到期望速度 | 速度控制可能不稳定 |

## 数据记录

Tube MPC节点将数据保存到 `/tmp/tube_mpc_data.csv`：

```csv
idx,cte,etheta,cmd_vel.linear.x,cmd_vel.angular.z,tube_radius
```

可用于性能分析和参数调优。

## 性能对比

### Tube MPC vs 标准MPC

| 特性 | 标准MPC | Tube MPC |
|------|---------|----------|
| **不确定性处理** | ❌ | ✅ |
| **鲁棒性保证** | ❌ | ✅ |
| **约束满足** | 无保证 | 有保证 |
| **计算复杂度** | O(N³) | O(N³) × 2 |
| **保守性** | 低 | 中等 |
| **适用场景** | 确定性环境 | 不确定性环境 |

## 实现细节

### 1. LQR增益计算

使用迭代方法求解离散代数Riccati方程：

```cpp
P = A'PA - A'PB(B'R+PB)^(-1)B'PA + Q
K = -(R + B'PB)^(-1)B'PA
```

### 2. 管状不变集计算

```
α ≥ ||P^0.5(I-A_cl)^(-1)|| × ||w||
```

其中 A_cl = A - BK

### 3. 扰动自适应估计

使用模型残差在线估计扰动界：

```cpp
w[k] = x[k+1] - (Ax[k] + Bu[k])
```

每N步更新扰动界。

### 4. 代价函数

```
J = Σ [w_cte(cte-ref_cte)² + w_etheta(etheta-ref_etheta)² + w_vel(v-ref_v)²]
    + Σ [w_angvel·v² + w_accel·a²]
    + Σ [w_angvel_d(v[i+1]-v[i])² + w_accel_d(a[i+1]-a[i])²]
    + Σ ||e[i]||²  (误差惩罚)
```

## 故障排除

### 问题1: Tube半径过大

**症状**: 机器人过于保守，移动缓慢

**解决方案**: 
- 减小 `disturbance_bound`
- 检查扰动估计是否合理
- 增加参考速度

### 问题2: Tube半径过小

**症状**: 约束频繁违反，优化失败

**解决方案**:
- 增大 `disturbance_bound`
- 检查系统模型准确性
- 减小预测步数 `mpc_steps`

### 问题3: 编译错误

**症状**: CppAD或Ipopt链接错误

**解决方案**:
```bash
# 检查Ipopt安装
pkg-config --cflags --libs ipopt

# 重新安装Ipopt（如果需要）
sudo apt-get install coinor-libipopt-dev
```

## 参考文献

1. Rawlings, J. B., & Mayne, D. Q. (2009). "Model Predictive Control: Theory and Design". John Wiley & Sons.

2. Bemporad, A., & Morari, M. (1999). "Robust Model Predictive Control: A Survey". Automatica, 35(8), 849-865.

3. MATLAB TubeMPC实现参考: `safe_regret_mpc/TubeMPC.m`

## 贡献者

- 基于原始mpc_ros包
- Tube MPC理论参考MATLAB实现
- ROS集成和C++实现

## 许可证

Apache 2.0 License（继承自原始mpc_ros包）
