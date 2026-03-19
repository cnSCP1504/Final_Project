# Tube MPC可视化配置指南

## 📋 概述

这个配置提供了完整的Tube MPC导航可视化，包括MPC预测轨迹、Tube边界、参考路径等所有关键元素的显示。

## 🎨 可视化元素说明

### Tube MPC核心可视化

1. **MPC预测轨迹 (🔴 红色线)**
   - 话题: `/mpc_trajectory`
   - 说明: 显示未来20步的MPC预测轨迹
   - 颜色: 红色
   - 线宽: 0.05

2. **Tube边界 (🔵 蓝色线)**
   - 话题: `/tube_boundaries`
   - 说明: 显示4条Tube边界线（上下左右）
   - 颜色: 蓝色 (RGB: 0, 100, 255)
   - 线宽: 0.05
   - **这是Tube MPC的核心可视化，显示不确定性边界**

3. **MPC参考路径 (🟡 黄色线)**
   - 话题: `/mpc_reference`
   - 说明: 全局规划的参考路径
   - 颜色: 黄色

### 导航相关可视化

4. **全局路径 (🟢 绿色线)**
   - 话题: `/move_base/GlobalPlanner/plan`
   - 说明: 全局规划器生成的完整路径

5. **机器人足迹 (🟣 紫色)**
   - 话题: `/move_base/local_costmap/footprint`
   - 说明: 机器人的当前占用空间

6. **代价地图 (⬜ 灰色区域)**
   - 全局代价地图: `/move_base/global_costmap/costmap`
   - 局部代价地图: `/move_base/local_costmap/costmap`
   - 说明: 显示障碍物和可导航区域

## 🚀 使用方法

### 方法1: 使用快速启动脚本（推荐）

```bash
# 启动完整的Tube MPC导航和可视化
./src/tube_mpc_ros/scripts/launch_tube_mpc_with_rviz.sh
```

这个脚本会：
- 启动ROS master（如果需要）
- 启动Tube MPC导航
- 使用新的完整RViz配置

### 方法2: 手动启动

```bash
# 1. 启动导航
roslaunch tube_mpc_ros tube_mpc_navigation.launch

# 2. 在另一个终端启动RViz（使用新配置）
rviz -d src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz
```

## ✅ 验证可视化

使用验证脚本检查所有话题是否正常发布：

```bash
# 在导航运行时执行
./src/tube_mpc_ros/scripts/verify_tube_visualization.sh
```

这个脚本会检查：
- 所有可视化话题是否存在
- 数据类型是否正确
- 是否有数据在发布

## 🎯 正常使用流程

1. **启动系统**
   ```bash
   ./src/tube_mpc_ros/scripts/launch_tube_mpc_with_rviz.sh
   ```

2. **等待RViz启动**
   - 你会看到地图和机器人模型
   - 绿色的全局路径应该可见

3. **设置导航目标**
   - 在RViz工具栏点击 **"2D Nav Goal"**
   - 在地图上点击目标位置
   - 拖动设置目标方向

4. **观察Tube MPC可视化**
   - 🟡 黄色线: 参考路径出现
   - 🔴 红色线: MPC预测轨迹出现（20步）
   - 🔵 蓝色线: **Tube边界出现（4条线围绕预测轨迹）**

## 🔍 Tube边界说明

Tube边界是Tube MPC的核心概念：

```
        ↑ Tube上边界
        |
    ←---+---→  Tube左边界和右边界
        |
        ↓ Tube下边界
```

- **红线**: 机器人应该跟随的标称轨迹
- **蓝线**: 允许的不确定性边界
- 如果机器人状态偏离红线但保持在蓝色边界内，说明系统正常
- 如果机器人超出蓝色边界，会触发警告

## ⚠️ 故障排除

### 问题1: 只看到红线，没有蓝线

**原因**: RViz配置没有包含Tube边界显示

**解决方案**:
```bash
# 使用新的完整配置
rviz -d src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz
```

### 问题2: Tube边界话题不存在

**检查**: 话题是否正在发布
```bash
rostopic list | grep tube
```

**如果没有输出**: 确认使用的是Tube MPC控制器
```bash
# 检查launch文件中controller参数
roslaunch tube_mpc_ros tube_mpc_navigation.launch controller:=tube_mpc
```

### 问题3: 话题存在但没有数据

**原因**: 导航可能还没开始

**解决方案**: 设置一个导航目标，Tube边界会在导航开始后出现

### 问题4: 可视化更新很慢

**原因**: RViz配置或计算负载

**解决方案**:
1. 减少RViz显示的元素
2. 调整MPC参数（减少预测步数）
3. 关闭不需要的显示项

## 📊 可视化数据含义

### MPC预测轨迹 (红线)
- 显示未来20步（默认）的预测
- 每步之间的距离由时间步长DT决定
- 轨迹应该平滑且合理

### Tube边界 (蓝线)
- 4条线组成一个矩形管道
- 距离红线的距离 = 当前tube_radius
- tube_radius会根据扰动估计自适应调整

### Tube半径的含义
- 小半径: 系统对扰动估计小，更精确
- 大半径: 系统感知到较大扰动，更保守
- 半径过大(>10.0)会被限制，防止数值问题

## 🛠️ 自定义可视化

### 修改Tube边界颜色

编辑RViz配置文件，找到Tube_Boundaries部分：
```python
Color: 0; 100; 255  # R; G; B格式
```

### 调整线宽
```python
Line Width: 0.05  # 增大使线更粗
```

### 添加轨迹点样式
```python
Pose Style: Axes  # 在每个轨迹点显示坐标系
```

## 📈 性能监控

可视化系统会记录数据到：
- `/tmp/tube_mpc_data.csv` - 实时MPC数据
- `/tmp/tube_mpc_metrics.csv` - 性能指标

可以用Python分析：
```python
import pandas as pd
df = pd.read_csv('/tmp/tube_mpc_data.csv')
print(df['tube_radius'].describe())
```

## 🎓 学习资源

- **Tube MPC理论**: 见 `latex/manuscript.tex`
- **实现细节**: 见 `test/docs/TUBE_MPC_README.md`
- **参数调优**: 见 `test/docs/TUBEMPC_TUNING_GUIDE.md`

## 📞 快速参考

```bash
# 启动完整可视化
./src/tube_mpc_ros/scripts/launch_tube_mpc_with_rviz.sh

# 验证可视化
./src/tube_mpc_ros/scripts/verify_tube_visualization.sh

# 检查Tube边界话题
rostopic echo /tube_boundaries -n 1

# 查看MPC数据
cat /tmp/tube_mpc_data.csv

# 只启动RViz（新配置）
rviz -d src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz
```

---

**作者**: Claude Code (Tube MPC可视化配置)
**日期**: 2026-03-18
**版本**: 1.0
