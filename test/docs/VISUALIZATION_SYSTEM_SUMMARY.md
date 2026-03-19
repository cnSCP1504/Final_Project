# Tube MPC可视化系统 - 完整总结

## 系统概述

已为Tube MPC benchmark系统创建了完整的可视化解决方案，包括：
- ✅ 实时RViz 3D可视化
- ✅ 实时数据绘图（6个图表）
- ✅ 数据记录（CSV格式）
- ✅ 后处理分析（论文级图表）
- ✅ 性能对比工具
- ✅ MPC指标可视化

---

## 🎯 已创建的组件

### 1. RViz配置文件
**文件**: `src/tube_mpc_benchmark/launch/tube_mpc_visualization.rviz`

**显示内容**：
- 🔵 机器人实际轨迹
- 🟢 全局规划路径
- 🔴 局部规划路径
- 🟡 Tube MPC边界
- 🎯 目标位置
- 📊 速度命令
- 🤖 机器人模型

### 2. 实时数据可视化节点
**文件**: `src/tube_mpc_benchmark/scripts/realtime_visualizer.py`

**功能**：实时显示6个图表
1. **主轨迹图**：机器人运动路径
2. **速度命令图**：线速度和角速度
3. **CTE图**：交叉跟踪误差
4. **求解时间图**：MPC优化时间
5. **位置分量图**：X/Y位置历史
6. **统计面板**：实时统计数据

### 3. 数据记录器
**文件**: `src/tube_mpc_benchmark/scripts/data_recorder.py`

**记录数据**：
- 位置和姿态 (x, y, z, roll, pitch, yaw)
- 速度命令 (linear, angular)
- 目标信息
- MPC指标 (tracking_error, solve_time_ms, empirical_risk, feasibility_rate)
- 时间戳

**输出**：CSV格式，`/tmp/tube_mpc_data/navigation_data_YYYYMMDD_HHMMSS.csv`

### 4. 后处理分析工具
**文件**: `src/tube_mpc_benchmark/scripts/analyze_results.py`

**生成图表**：
- **轨迹分析图**（6子图）：完整导航分析
- **MPC指标图**（4子图）：优化性能分析
- **控制器对比图**（4子图）：多算法比较

**特点**：
- 300 DPI论文级质量
- 可自定义样式
- 支持批量处理
- 导出PDF/SVG矢量图

### 5. 测试脚本
- `visualize_test.sh`：可视化测试启动
- `quick_analyze.sh`：快速数据分析
- `demo_visualization.sh`：完整演示

---

## 🚀 快速使用指南

### 方式1：RViz可视化（最简单）

```bash
# 启动带RViz的测试
roslaunch tube_mpc_benchmark visualization_test.launch

# 或使用便捷脚本
./src/tube_mpc_benchmark/scripts/visualize_test.sh 5.0 5.0
```

**显示**：3D环境、机器人轨迹、速度命令

### 方式2：实时数据绘图（推荐调试用）

```bash
# 启用实时绘图
roslaunch tube_mpc_benchmark visualization_test.launch \
    enable_visualizer:=true
```

**显示**：6个实时更新的matplotlib图表

### 方式3：数据记录和分析（推荐实验用）

```bash
# 1. 运行测试（自动记录）
./src/tube_mpc_benchmark/scripts/visualize_test.sh 4.0 4.0

# 2. 分析数据
./src/tube_mpc_benchmark/scripts/quick_analyze.sh
```

**输出**：
- `tube_mpc_trajectory.png`：轨迹分析
- `tube_mpc_metrics.png`：MPC指标

### 方式4：完整演示（所有功能）

```bash
# 运行交互式演示
./demo_visualization.sh
```

---

## 📊 可视化图表示例

### 轨迹分析图

```
┌─────────────────────────────────────────┐
│         Robot Trajectory                 │
│  ┌─────────────────────────────────┐    │
│  │  B─┐                           │    │
│  │   │  Path                     │    │
│  │   ▼    (Actual trajectory)    │    │
│  │  Start ⋯⋯⋯⋯⋯⋯⋯⋯⋯⋯ Goal      │    │
│  │   ▲                          │    │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│ Position vs Time │ Velocity Commands    │
├─────────────────────────────────────────┤
│ Distance │ Instantaneous Vel │ Stats   │
└─────────────────────────────────────────┘
```

### 控制器对比图

```
┌──────────────┬──────────────┬──────────────┐
│ Success Rate │ Time Dist    │ Pos Error    │
│   ▇▇▇▇▇     │  ▃▃▃▃        │  │││││││     │
│ Tube MPC    │  ┄┄┄┄        │  ┅┅┅┅┅       │
│ Standard MPC│              │              │
└──────────────┴──────────────┴──────────────┘
```

---

## 💡 使用场景

### 场景1：算法开发

```bash
# 快速迭代测试
for param in 0.1 0.2 0.3 0.4; do
    # 修改参数
    sed -i "s/mpc_w_cte: .*/mpc_w_cte: $param/" \
        src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml

    # 测试并可视化
    roslaunch tube_mpc_benchmark visualization_test.launch \
        enable_visualizer:=true &
    PID=$!

    sleep 30
    kill $PID

    # 查看效果
    # 调整下一个参数...
done
```

### 场景2：论文实验

```bash
# 1. 收集数据
for i in {1..50}; do
    roslaunch tube_mpc_benchmark visualization_test.launch \
        goal_x:=$RANDOM goal_y:=$RANDOM \
        enable_recorder:=true
    sleep 60
done

# 2. 批量分析
python3 <<EOF
import pandas as pd
import glob
import matplotlib.pyplot as plt

# 读取所有数据
files = glob.glob('/tmp/tube_mpc_data/*.csv')
dfs = [pd.read_csv(f) for f in files]

# 生成论文图表
# ...
EOF
```

### 场景3：性能基准

```bash
# 对比不同控制器
for controller in tube_mpc mpc dwa; do
    # 修改配置
    # 运行测试
    # 记录结果
done

# 生成对比图
python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
    --compare tube_mpc.csv mpc.csv dwa.csv
```

---

## 📁 文件结构

```
tube_mpc_benchmark/
├── launch/
│   ├── tube_mpc_visualization.rviz      # RViz配置
│   └── visualization_test.launch        # 可视化测试
├── scripts/
│   ├── realtime_visualizer.py           # 实时绘图
│   ├── data_recorder.py                 # 数据记录
│   ├── analyze_results.py               # 后处理分析
│   ├── visualize_test.sh                # 测试脚本
│   └── quick_analyze.sh                 # 快速分析
└── docs/
    └── VISUALIZATION_GUIDE.md           # 完整指南
```

---

## 🎨 自定义可视化

### 修改图表样式

编辑`analyze_results.py`：
```python
plt.rcParams['figure.dpi'] = 300        # 分辨率
plt.rcParams['font.size'] = 12          # 字体大小
plt.rcParams['font.family'] = 'serif'   # 字体类型
plt.rcParams['figure.figsize'] = [12, 8] # 图表大小
```

### 添加新的指标

在`data_recorder.py`中添加：
```python
# 添加新话题
rospy.Subscriber('/new_topic', MessageType, self.new_callback)

# 在record_callback中记录
self.current_data['new_metric'] = value
```

### 自定义RViz显示

使用RViz GUI：
1. 启动RViz：`rosrun rviz rviz`
2. 添加显示（Add → 选择类型）
3. 调整属性
4. 保存配置：File → Save Config As

---

## 📈 数据分析示例

### Python分析脚本

```python
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 读取数据
df = pd.read_csv('/tmp/tube_mpc_data/navigation_data_latest.csv')

# 计算指标
total_distance = np.sqrt(np.diff(df['x'])**2 + np.diff(df['y'])**2).sum()
avg_velocity = total_distance / df['elapsed_time'].iloc[-1]
max_cte = df['tracking_error'].abs().max()

# 生成图表
fig, axes = plt.subplots(2, 2, figsize=(12, 8))

# 轨迹图
axes[0, 0].plot(df['x'], df['y'])
axes[0, 0].set_title('Trajectory')
axes[0, 0].axis('equal')

# 速度图
axes[0, 1].plot(df['elapsed_time'], df['linear_vel'])
axes[0, 1].set_title('Velocity')

# 误差图
axes[1, 0].plot(df['elapsed_time'], df['tracking_error'])
axes[1, 0].set_title('Tracking Error')

# 统计
axes[1, 1].axis('off')
axes[1, 1].text(0.1, 0.5,
    f'Distance: {total_distance:.2f}m\n'
    f'Avg Vel: {avg_velocity:.2f}m/s\n'
    f'Max CTE: {max_cte:.3f}m',
    fontsize=12)

plt.tight_layout()
plt.savefig('my_analysis.png', dpi=300)
```

---

## 🔧 故障排除

### 问题1：实时绘图窗口不显示

```bash
# 检查显示
echo $DISPLAY
export DISPLAY=:0

# 或使用X11转发
ssh -X user@host
```

### 问题2：没有数据文件

```bash
# 检查记录器节点
rosnode list | grep data_recorder
rostopic echo /odom  # 应该有数据

# 检查输出目录
ls -la /tmp/tube_mpc_data/
```

### 问题3：分析脚本错误

```bash
# 安装依赖
pip3 install matplotlib pandas seaborn numpy

# 检查CSV格式
head -5 /tmp/tube_mpc_data/navigation_data_*.csv
```

---

## 📚 相关资源

- **完整指南**: `src/tube_mpc_benchmark/docs/VISUALIZATION_GUIDE.md`
- **测试报告**: `TUBE_MPC_NAV_TEST_REPORT.md`
- **系统文档**: `src/tube_mpc_benchmark/README.md`

---

## ✅ 验证清单

- [x] RViz配置创建
- [x] 实时可视化节点
- [x] 数据记录器
- [x] 后处理分析工具
- [x] 测试脚本
- [x] 使用文档
- [x] 演示脚本
- [x] 权限设置

**状态**: ✅ 可视化系统完全就绪

---

*创建时间: 2026-03-17*
*版本: 1.0*
*作者: Claude Code*
