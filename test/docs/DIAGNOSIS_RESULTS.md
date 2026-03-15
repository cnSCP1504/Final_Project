# 机器人不动问题 - 诊断报告

## 测试时间
2026-03-13

## 问题症状
机器人完全不动，`/cmd_vel` 发布的全是 0

## 诊断结果

### 关键发现
```
Control Loop Status ===
Goal received: YES
Path computed: NO    ← 问题所在！
```

### 根本原因
路径生成代码要求至少6个路径点：
```cpp
if(odom_path.poses.size() >= 6) {
    _path_computed = true;
} else {
    cout << "Failed to path generation" << endl;
}
```

但是：
- 日志中**没有** "Failed to path generation" 消息
- 这意味着路径回调可能根本没有被调用
- 或者 `pathMsg->poses.size()` 本身就 < 6

## 次要问题（之前观察到）

### MPC输出太小
```
throttle: 0.297
Current v (odom): ~0
Computed speed = 0 + 0.297 * 0.1 = 0.0297 m/s
After min limit = 0.1 m/s
```

### 可能的原因
1. **mpc_max_throttle限制**: 当前2.0，可能需要更大
2. **加速度权重**: mpc_w_accel=20.0 惩罚大加速度
3. **速度权重**: mpc_w_vel=10.0 惩罚速度偏差

## 测试历史

| 测试 | 最小速度 | mpc_w_etheta | mpc_w_accel | mpc_ref_vel | 结果 |
|------|---------|--------------|-------------|-------------|------|
| 1 | 0.1 | 60 | 20 | 0.5 | 速度0.1 m/s |
| 2 | 0.1 | 200 | 20 | 0.5 | 不动（权重过大）|
| 3 | 0.1 | 80 | 20 | 0.5 | 不动（智能逻辑禁用）|
| 4 | 0.3 | 60 | 5 | 0.8 | **Path computed: NO** |
| 5 | 0.3 | 60 | 20 | 0.5 | **Path computed: NO** |

## 下一步调试

### 检查1: move_base是否在发布路径
```bash
rostopic echo /move_base/MPCPlannerROS/global_plan -n 1
```

### 检查2: 全局规划器是否工作
```bash
rostopic echo /move_base/GlobalPlanner/plan -n 1
```

### 检查3: 目标是否合理
检查auto_nav_goal设置的目标(3.0, -7.0)是否在地图范围内

### 检查4: TF树是否正确
```bash
rosrun tf view_frames
# 检查 map → odom → base_footprint 链
```

## 建议的修复方案

### 方案A: 降低路径点要求（临时）
```cpp
// 从 >=6 改为 >=2
if(odom_path.poses.size() >= 2) {
    _path_computed = true;
}
```

### 方案B: 增加路径点采样
```cpp
// 降低_downSampling，增加采样密度
```

### 方案C: 检查move_base配置
确保全局规划器参数设置合理：
- tolerance: 足够大
- planner_patience: 足够长
- visualize_potential: true（调试用）

## 代码位置
路径生成代码: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp` 第180-206行
