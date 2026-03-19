# Tube MPC可视化系统使用指南

## 概述

完整的Tube MPC benchmark可视化系统，包括：
- 实时RViz可视化
- 实时数据绘图
- 后处理数据分析
- 论文级图表生成

---

## 快速开始

### 1. 带RViz的导航测试

```bash
# 基本测试（带RViz）
./src/tube_mpc_benchmark/scripts/visualize_test.sh 5.0 5.0

# 查看实时RViz可视化
# 机器人轨迹、Tube MPC路径、速度命令等
```

### 2. 实时数据绘图测试

```bash
# 启用实时matplotlib绘图
roslaunch tube_mpc_benchmark visualization_test.launch \
    goal_x:=5.0 goal_y:=5.0 \
    enable_visualizer:=true

# 将显示6个实时图表：
# - 机器人轨迹
# - 速度命令
# - 交叉跟踪误差
# - MPC求解时间
# - 位置分量
# - 统计数据
```

### 3. 数据记录和后处理

```bash
# 运行测试（自动记录数据）
./src/tube_mpc_benchmark/scripts/visualize_test.sh 4.0 4.0

# 测试完成后分析数据
./src/tube_mpc_benchmark/scripts/quick_analyze.sh
```

---

## 可视化组件详解

### 1. RViz实时可视化

**功能**：
- 机器人位置和姿态
- 全局路径规划
- Tube MPC轨迹
- 速度命令
- 目标位置

**启动方式**：
```bash
roslaunch tube_mpc_benchmark visualization_test.launch
```

**显示内容**：
- 🔵 蓝色线：机器人实际轨迹
- 🟢 绿色虚线：全局规划路径
- 🔴 红色线：局部规划路径
- 🟡 黄色区域：Tube边界
- ⭐ 红色星：目标位置
- ▲ 绿色三角：起始位置

### 2. 实时数据绘图 (realtime_visualizer.py)

**功能**：实时绘制6个图表

**图表说明**：

#### 图1：机器人轨迹（主图）
- 显示实际运动路径
- 标记起点和终点
- 显示目标位置
- 自动缩放

#### 图2：速度命令
- 线速度（蓝线）
- 角速度（红线）
- 实时更新最近100个点

#### 图3：交叉跟踪误差 (CTE)
- 路径跟踪精度
- 实时误差曲线
- 用于评估控制性能

#### 图4：MPC求解时间
- 优化求解时间
- 实时性能监控
- 计算效率评估

#### 图5：位置分量
- X位置（红线）
- Y位置（蓝线）
- 位置历史跟踪

#### 图6：统计面板
- 当前位置
- 移动距离
- 平均速度
- 平均CTE
- 平均求解时间

**启动方式**：
```bash
# 方式1：通过launch文件
roslaunch tube_mpc_benchmark visualization_test.launch enable_visualizer:=true

# 方式2：独立启动
python3 src/tube_mpc_benchmark/scripts/realtime_visualizer.py
```

### 3. 数据记录器 (data_recorder.py)

**记录数据**：
- 位置 (x, y, z, roll, pitch, yaw)
- 速度命令 (linear, angular)
- 目标位置
- 距离目标的距离
- 跟踪误差
- MPC求解时间
- 经验风险
- 可行性比率
- 路径长度

**输出格式**：CSV
**输出目录**：`/tmp/tube_mpc_data/`
**文件命名**：`navigation_data_YYYYMMDD_HHMMSS.csv`

**使用方式**：
```bash
# 自动包含在visualization_test.launch中
# 或独立运行：
python3 src/tube_mpc_benchmark/scripts/data_recorder.py
```

### 4. 后处理分析 (analyze_results.py)

**功能**：
- 从CSV文件生成分析图表
- 创建论文级质量图像
- 性能对比分析
- MPC指标可视化

**生成的图表**：

#### 1. 轨迹分析图
包含6个子图：
- 主轨迹图（实际路径 vs 规划路径）
- 位置-时间曲线
- 速度分量
- 距离起点距离
- 瞬时速度
- 统计摘要

#### 2. MPC性能图
包含4个子图：
- MPC求解时间分布
- 经验风险演化
- 优化可行性率
- 跟踪性能

#### 3. 控制器对比图
包含4个子图：
- 成功率对比
- 完成时间分布
- 位置误差箱型图
- 性能汇总表

**使用方式**：
```bash
# 基本分析
python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
    --csv /tmp/tube_mpc_data/navigation_data_20260317_020000.csv \
    --output my_trajectory.png

# MPC指标分析
python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
    --mpc-csv /tmp/tube_mpc_metrics.csv \
    --output my_mpc_metrics.png

# 控制器对比
python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
    --compare \
        tube_mpc_results.csv \
        mpc_results.csv \
        dwa_results.csv \
    --output controller_comparison.png
```

---

## 实际使用案例

### 案例1：算法开发与调优

```bash
# 1. 运行测试并查看实时性能
roslaunch tube_mpc_benchmark visualization_test.launch \
    goal_x:=5.0 goal_y:=5.0 enable_visualizer:=true

# 2. 观察实时图表，调整参数
# 3. 重新测试，比较性能

# 4. 生成对比分析
python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
    --compare test1.csv test2.csv test3.csv \
    --output parameter_comparison.png
```

### 案例2：论文实验数据收集

```bash
# 1. 收集不同场景的数据
for goal_x in 3 5 7 10; do
    for goal_y in 3 5 7 10; do
        roslaunch tube_mpc_benchmark visualization_test.launch \
            goal_x:=$goal_x goal_y:=$goal_y \
            enable_recorder:=true enable_visualizer:=false
        sleep 60  # 等待测试完成
    done
done

# 2. 批量分析所有数据
python3 <<EOF
import pandas as pd
import matplotlib.pyplot as plt
import glob

# 读取所有CSV文件
files = glob.glob('/tmp/tube_mpc_data/navigation_data_*.csv')
all_data = [pd.read_csv(f) for f in files]

# 生成论文图表
# ... (你的分析代码)
EOF
```

### 案例3：性能基准测试

```bash
# 1. 运行标准测试
./src/tube_mpc_benchmark/scripts/visualize_test.sh 5.0 5.0 false

# 2. 快速分析结果
./src/tube_mpc_benchmark/scripts/quick_analyze.sh

# 3. 查看生成的图表
eog tube_mpc_trajectory.png tube_mpc_metrics.png
```

---

## 高级功能

### 自定义可视化配置

**修改RViz配置**：
```bash
# 编辑配置文件
vim src/tube_mpc_benchmark/launch/tube_mpc_visualization.rviz

# 使用自定义配置
roslaunch tube_mpc_benchmark visualization_test.launch
```

**自定义图表样式**：
编辑`analyze_results.py`中的matplotlib参数：
```python
plt.rcParams['figure.dpi'] = 300  # 图像分辨率
plt.rcParams['font.size'] = 10      # 字体大小
plt.rcParams['font.family'] = 'serif'  # 字体类型
```

### 数据导出

**导出为PDF**：
```python
import matplotlib.pyplot as plt
# ... 生成图表 ...
plt.savefig('output.pdf', format='pdf', bbox_inches='tight')
```

**导出为矢量图**：
```python
plt.savefig('output.svg', format='svg', bbox_inches='tight')
```

### 批量处理脚本

```bash
#!/bin/bash
# batch_analyze.sh - 批量分析多个测试

for csv_file in /tmp/tube_mpc_data/navigation_data_*.csv; do
    output_name=$(basename $csv_file .csv)_analysis.png
    python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
        --csv $csv_file \
        --output $output_name
done
```

---

## 故障排除

### 问题1：实时绘图窗口不显示

**解决方案**：
```bash
# 检查DISPLAY变量
echo $DISPLAY

# 如果为空，设置显示
export DISPLAY=:0

# 或使用X11转发（SSH连接）
ssh -X username@hostname
```

### 问题2：没有数据文件生成

**检查**：
```bash
# 检查data_recorder节点
rosnode list | grep data_recorder

# 检查输出目录
ls -la /tmp/tube_mpc_data/

# 查看日志
roscd && cd ../log && ls -la
```

### 问题3：Matplotlib图表显示异常

**解决方案**：
```bash
# 更新matplotlib
pip3 install --upgrade matplotlib

# 或使用不同的后端
export MPLBACKEND=TkAgg
```

### 问题4：CSV文件格式错误

**检查CSV内容**：
```bash
head -5 /tmp/tube_mpc_data/navigation_data_*.csv

# 应该看到表头：
# timestamp,elapsed_time,x,y,z,...
```

---

## 性能提示

### 实时绘图性能

- 默认显示最近100个数据点
- 可调整以平衡性能和详细度
- 编辑`realtime_visualizer.py`中的缓冲区大小

### 大数据集处理

```python
# 对于大型CSV文件，使用chunking
import pandas as pd

# 分块读取
chunk_size = 1000
for chunk in pd.read_csv('large_file.csv', chunksize=chunk_size):
    # 处理每个chunk
    pass
```

---

## 最佳实践

### 1. 开发阶段
- 使用实时绘图快速反馈
- 调整参数后立即看到效果
- 频繁测试小场景

### 2. 实验阶段
- 使用数据记录器收集完整数据
- 运行多次测试获得统计显著性
- 使用后处理分析生成论文图表

### 3. 论文撰写阶段
- 使用高分辨率输出（300 DPI）
- 生成矢量图（SVG/PDF）
- 自定义样式符合期刊要求
- 批量处理生成一致图表

---

## 文件结构

```
tube_mpc_benchmark/
├── launch/
│   ├── tube_mpc_visualization.rviz    # RViz配置
│   └── visualization_test.launch      # 可视化测试launch
├── scripts/
│   ├── realtime_visualizer.py         # 实时绘图节点
│   ├── data_recorder.py               # 数据记录器
│   ├── analyze_results.py             # 后处理分析工具
│   ├── visualize_test.sh              # 可视化测试脚本
│   └── quick_analyze.sh               # 快速分析脚本
└── docs/
    └── VISUALIZATION_GUIDE.md         # 本文档
```

---

## 相关资源

- **RViz官方文档**: http://wiki.ros.org/rviz
- **Matplotlib文档**: https://matplotlib.org/
- **Seaborn统计图表**: https://seaborn.pydata.org/
- **Pandas数据处理**: https://pandas.pydata.org/

---

*最后更新: 2026-03-17*
*版本: 1.0*
