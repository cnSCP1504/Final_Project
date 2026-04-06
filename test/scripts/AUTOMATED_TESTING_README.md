# 自动化Baseline测试系统使用指南

## 目录结构

```
test/scripts/
├── automated_baseline_test.sh    # 主测试脚本（Bash版本）
├── goal_monitor.py               # 目标监控器（Python）
├── shelf_locations.yaml          # 货架配置文件
├── run_automated_test.py         # 高级测试脚本（Python版本）
└── AUTOMATED_TESTING_README.md   # 本文档
```

## 快速开始

### 1. 基本测试（Bash脚本）

测试所有5个货架，使用tube_mpc模型：

```bash
cd /home/dixon/Final_Project/catkin
./test/scripts/automated_baseline_test.sh --model tube_mpc
```

### 2. 高级测试（Python脚本）

使用Python脚本获得更详细的metrics：

```bash
cd /home/dixon/Final_Project/catkin
python3 test/scripts/run_automated_test.py \
    --model tube_mpc \
    --shelves 5 \
    --viz
```

## 支持的模型

| 模型 | 说明 | 启用参数 |
|------|------|----------|
| `tube_mpc` | 仅Tube MPC（基线） | 无额外参数 |
| `stl` | Tube MPC + STL监控 | `--enable_stl=true` |
| `dr` | Tube MPC + DR约束收紧 | `--enable_dr=true` |
| `safe_regret` | 完整Safe-Regret MPC (STL+DR) | `--enable_stl=true --enable_dr=true` |

## 命令行选项

### Bash脚本 (`automated_baseline_test.sh`)

```
--model MODEL          模型类型: tube_mpc, stl, dr, safe_regret
--shelves NUM          测试货架数量 (1-5)
--timeout SEC          单次测试超时时间（秒，默认240）
--no-gazebo            禁用Gazebo可视化（加速测试）
--viz                  启用RViz可视化
--help                 显示帮助信息
```

### Python脚本 (`run_automated_test.py`)

```
--model MODEL          模型类型
--shelves NUM          测试货架数量
--timeout SEC          超时时间
--no-gazebo            禁用Gazebo
--viz                  启用RViz
--interval SEC         测试间隔时间（秒）
--parallel             并行运行测试（实验性功能）
```

## 货架位置配置

货架位置定义在 `shelf_locations.yaml` 中：

```yaml
shelves:
  - id: "shelf_01"
    name: "货架01 - 左上"
    x: -6.0
    y: -7.0
    yaw: 0.0

  - id: "shelf_02"
    name: "货架02 - 左中"
    x: -3.0
    y: -7.0
    yaw: 0.0

  # ... 更多货架
```

## 测试流程

### 单次测试流程

1. **启动ROS系统**
   - 加载指定模型配置
   - 设置取货点坐标
   - 启动Gazebo（可选）

2. **执行导航任务**
   - 前往取货点（货架位置）
   - 等待到达确认
   - 前往卸货点 (8.0, 0.0)
   - 等待到达确认

3. **数据收集**
   - 记录机器人轨迹
   - 记录到达时间和位置
   - 收集ROS topics数据

4. **清理和重启**
   - 关闭所有ROS进程
   - 等待系统清理
   - 切换到下一个货架

### 测试完成条件

- **成功**: 两次目标均到达（取货点 + 卸货点）
- **超时**: 在指定时间内未完成（默认240秒）
- **失败**: ROS系统启动失败或崩溃

## 输出结果

### 目录结构

```
test_results/
└── MODEL_YYYYMMDD_HHMMSS/
    ├── test_01_shelf_01/
    │   ├── rosbag/
    │   ├── logs/
    │   │   ├── launch.log
    │   │   └── goal_monitor.log
    │   ├── metrics.json
    │   ├── metrics.csv
    │   └── test_summary.txt
    ├── test_02_shelf_02/
    │   └── ...
    ├── test_03_shelf_03/
    │   └── ...
    └── final_report.txt
```

### Metrics文件说明

#### `metrics.json` (Python脚本)

```json
{
  "test_start_time": "2026-04-06 14:30:00",
  "test_end_time": "2026-04-06 14:34:20",
  "test_status": "SUCCESS",
  "total_goals": 2,
  "goals_reached": [
    {
      "goal_index": 0,
      "goal_position": {"x": 0.0, "y": -7.0, "yaw": 0.0},
      "robot_position": {"x": 0.1, "y": -6.95, "yaw": 0.05},
      "distance": 0.25,
      "time_elapsed": 45.2,
      "timestamp": "2026-04-06 14:30:45"
    },
    {
      "goal_index": 1,
      "goal_position": {"x": 8.0, "y": 0.0, "yaw": 0.0},
      "robot_position": {"x": 7.9, "y": 0.05, "yaw": 0.02},
      "distance": 0.18,
      "time_elapsed": 256.3,
      "timestamp": "2026-04-06 14:34:16"
    }
  ],
  "position_history": [
    {"time": 0.0, "x": -8.0, "y": 0.0, "yaw": 0.0},
    {"time": 1.0, "x": -7.95, "y": 0.02, "yaw": 0.01},
    ...
  ],
  "total_time": 256.3
}
```

#### `metrics.csv` (Bash脚本)

```csv
timestamp,topic,value
2026-04-06 14:30:00,pickup_x,0.0
2026-04-06 14:30:00,pickup_y,-7.0
2026-04-06 14:34:20,final_position_x,7.9
2026-04-06 14:34:20,final_position_y,0.05
```

#### `test_summary.txt`

```
=================================================================
              测试摘要 / Test Summary
=================================================================

测试时间: 2026-04-06 14:30:00

模型配置:
  - 模型类型: tube_mpc
  - 货架ID: shelf_01
  - 货架名称: 货架01 - 左上

测试结果: SUCCESS

测试参数:
  - 超时时间: 240秒
  - Gazebo启用: true
  - 可视化: false

=================================================================
```

## 使用示例

### 示例1: 快速测试（无可视化）

```bash
# 测试3个货架，无Gazebo，使用tube_mpc
./test/scripts/automated_baseline_test.sh \
    --model tube_mpc \
    --shelves 3 \
    --no-gazebo
```

### 示例2: 完整测试（带可视化）

```bash
# 测试所有货架，启用RViz，使用safe_regret模型
python3 test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 5 \
    --viz
```

### 示例3: 模型对比测试

```bash
# 测试tube_mpc
./test/scripts/automated_baseline_test.sh --model tube_mpc

# 测试stl
./test/scripts/automated_baseline_test.sh --model stl

# 测试dr
./test/scripts/automated_baseline_test.sh --model dr

# 测试safe_regret
./test/scripts/automated_baseline_test.sh --model safe_regret
```

### 示例4: 自定义超时时间

```bash
# 增加超时时间到5分钟
./test/scripts/automated_baseline_test.sh \
    --model tube_mpc \
    --timeout 300
```

## 故障排除

### 问题1: ROS进程未清理

**症状**: 测试失败，提示ROS进程仍在运行

**解决方案**:
```bash
# 手动清理所有ROS进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python
pkill -9 -f ros
pkill -9 -f gazebo
```

### 问题2: 测试超时

**症状**: 所有测试都超时

**解决方案**:
- 增加超时时间: `--timeout 300`
- 检查地图配置是否正确
- 检查目标点是否可达
- 查看日志: `test_results/MODEL_*/test_*/logs/launch.log`

### 问题3: Gazebo启动失败

**症状**: 测试无法启动Gazebo

**解决方案**:
- 使用 `--no-gazebo` 选项（如果不需要可视化）
- 检查Gazebo模型路径
- 重启计算机释放资源

### 问题4: 权限错误

**症状**: 脚本无法执行

**解决方案**:
```bash
chmod +x test/scripts/automated_baseline_test.sh
chmod +x test/scripts/goal_monitor.py
```

## 性能优化建议

### 1. 加速测试

- 使用 `--no-gazebo` 禁用Gazebo（可减少50-70%测试时间）
- 减少测试货架数量: `--shelves 2`
- 并行运行多个测试实例（实验性功能）

### 2. 提高可靠性

- 增加超时时间: `--timeout 300`
- 在测试之间添加间隔: `--interval 10`
- 启用详细日志查看问题

### 3. 数据收集优化

- 使用Python脚本获得更详细的metrics
- 记录rosbag用于后续分析
- 监控系统资源使用情况

## 扩展和定制

### 添加新的货架位置

编辑 `shelf_locations.yaml`:

```yaml
shelves:
  - id: "shelf_06"
    name: "货架06 - 自定义"
    x: 0.0
    y: 7.0
    yaw: 1.57  # 90度
```

### 添加新的模型配置

编辑测试脚本，在 `get_model_params()` 函数中添加：

```bash
my_custom_model)
    echo "--enable_safe_regret_integration=true --custom_param=value"
    ;;
```

### 自定义Metrics收集

修改 `goal_monitor.py` 或 `collect_test_data()` 函数，添加需要收集的ROS topics。

## 技术支持

如有问题，请查看：
- 日志文件: `test_results/MODEL_*/test_*/logs/`
- 测试摘要: `test_results/MODEL_*/test_*/test_summary.txt`
- 最终报告: `test_results/MODEL_*/final_report.txt`

## 版本历史

- **v1.0** (2026-04-06): 初始版本
  - 支持4种baseline模型
  - 5个货架位置配置
  - 自动化测试流程
  - Metrics收集和保存
