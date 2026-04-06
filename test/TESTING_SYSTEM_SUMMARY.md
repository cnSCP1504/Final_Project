# 自动化Baseline测试系统 - 文件清单

## 已创建的文件

### 核心测试脚本

1. **automated_baseline_test.sh** (约16KB)
   - Bash版本的自动化测试脚本
   - 功能完整，支持所有模型和配置
   - 位置: `test/scripts/automated_baseline_test.sh`

2. **run_automated_test.py** (约19KB)
   - Python版本的自动化测试脚本（推荐）
   - 更详细的metrics收集
   - 更好的错误处理和日志
   - 位置: `test/scripts/run_automated_test.py`

### 辅助工具

3. **goal_monitor.py** (约8KB)
   - 独立的目标到达监控器
   - 可单独使用或集成到测试脚本
   - 支持JSON格式的详细metrics输出
   - 位置: `test/scripts/goal_monitor.py`

4. **quick_test.sh** (约4KB)
   - 交互式菜单启动脚本
   - 提供常用测试场景的快捷方式
   - 适合快速启动测试
   - 位置: `test/scripts/quick_test.sh`

### 配置文件

5. **shelf_locations.yaml** (约1KB)
   - 货架位置配置文件
   - 定义5个货架的坐标和属性
   - 可扩展添加更多货架
   - 位置: `test/scripts/shelf_locations.yaml`

### 文档

6. **AUTOMATED_TESTING_README.md** (约10KB)
   - 完整的使用文档
   - 包含示例、故障排除、API说明
   - 位置: `test/scripts/AUTOMATED_TESTING_README.md`

## 快速开始指南

### 方式1: 使用交互式菜单（最简单）

```bash
cd /home/dixon/Final_Project/catkin
./test/scripts/quick_test.sh
```

然后选择想要的测试场景。

### 方式2: 直接运行Python脚本（推荐）

```bash
cd /home/dixon/Final_Project/catkin

# 测试所有货架，使用tube_mpc
./test/scripts/run_automated_test.py --model tube_mpc

# 测试3个货架，使用STL增强
./test/scripts/run_automated_test.py --model stl --shelves 3

# 快速测试（无Gazebo）
./test/scripts/run_automated_test.py --model tube_mpc --no-gazebo --shelves 2
```

### 方式3: 使用Bash脚本

```bash
cd /home/dixon/Final_Project/catkin

# 测试所有货架，使用tube_mpc
./test/scripts/automated_baseline_test.sh --model tube_mpc

# 测试3个货架，使用safe_regret
./test/scripts/automated_baseline_test.sh --model safe_regret --shelves 3
```

## 支持的模型

| 模型名称 | 说明 | 启用的功能 |
|---------|------|-----------|
| `tube_mpc` | Tube MPC（基线） | 无额外功能 |
| `stl` | Tube MPC + STL | STL监控 |
| `dr` | Tube MPC + DR | DR约束收紧 |
| `safe_regret` | Safe-Regret MPC | STL + DR（完整） |

## 货架位置

| 货架ID | 名称 | 坐标 (x, y) | 说明 |
|--------|------|-------------|------|
| shelf_01 | 货架01 - 左上 | (-6.0, -7.0) | 左上区域 |
| shelf_02 | 货架02 - 左中 | (-3.0, -7.0) | 左中区域 |
| shelf_03 | 货架03 - 左下 | (0.0, -7.0) | 左下区域（默认） |
| shelf_04 | 货架04 - 中上 | (3.0, -7.0) | 中上区域 |
| shelf_05 | 货架05 - 中下 | (6.0, -7.0) | 中下区域 |

## 测试流程

1. **设置取货点**: 使用货架位置作为取货点
2. **启动系统**: 启动ROS和指定模型
3. **执行导航**:
   - 前往取货点（货架位置）
   - 等待到达确认
   - 前往卸货点 (8.0, 0.0)
   - 等待到达确认
4. **收集数据**: 记录metrics、轨迹、时间等
5. **清理重启**: 关闭ROS，切换到下一个货架

## 输出结果结构

```
test_results/
└── MODEL_YYYYMMDD_HHMMSS/          # 主结果目录
    ├── test_01_shelf_01/            # 测试1
    │   ├── rosbag/                  # ROS bag文件
    │   ├── logs/                    # 日志文件
    │   │   ├── launch.log          # 启动日志
    │   │   └── goal_monitor.log    # 监控日志
    │   ├── metrics.json            # 详细metrics（Python）
    │   ├── metrics.csv             # 简要metrics（Bash）
    │   └── test_summary.txt        # 测试摘要
    ├── test_02_shelf_02/            # 测试2
    │   └── ...
    ├── test_03_shelf_03/            # 测试3
    │   └── ...
    └── final_report.txt             # 最终报告
```

## Metrics内容

### Python脚本 (`metrics.json`)

- `test_start_time`: 测试开始时间
- `test_end_time`: 测试结束时间
- `test_status`: 测试状态（SUCCESS/TIMEOUT/ERROR）
- `total_goals`: 总目标数（通常为2）
- `goals_reached[]`: 每个目标的详细信息
  - `goal_index`: 目标索引
  - `goal_position`: 目标位置
  - `robot_position`: 到达时的机器人位置
  - `distance`: 距离误差
  - `time_elapsed`: 用时
  - `timestamp`: 时间戳
- `position_history[]`: 机器人轨迹历史
- `total_time`: 总用时

### Bash脚本 (`metrics.csv`)

```csv
timestamp,topic,value
2026-04-06 14:30:00,pickup_x,0.0
2026-04-06 14:30:00,pickup_y,-7.0
2026-04-06 14:34:20,final_position_x,7.9
2026-04-06 14:34:20,final_position_y,0.05
```

## 常用命令

### 查看测试结果

```bash
# 查看最新的测试结果
cd /home/dixon/Final_Project/catkin/test_results
ls -t | head -1
cd MODEL_*/

# 查看最终报告
cat final_report.txt

# 查看单个测试摘要
cat test_01_shelf_01/test_summary.txt

# 查看详细metrics
cat test_01_shelf_01/metrics.json | jq .
```

### 清理ROS进程（如果测试卡住）

```bash
killall -9 roslaunch rosmaster roscore gzserver gzclient python
pkill -9 -f ros
pkill -9 -f gazebo
```

### 查看实时日志

```bash
# 查看launch日志
tail -f test_results/MODEL_*/test_*/logs/launch.log

# 查看ROS日志
rostopic echo /rosout -n 100
```

## 性能优化建议

### 加速测试

1. **禁用Gazebo**: `--no-gazebo`（可减少50-70%时间）
2. **减少货架数**: `--shelves 2` 或 `--shelves 3`
3. **并行测试**: 实验性功能，谨慎使用

### 提高可靠性

1. **增加超时**: `--timeout 300`（默认240秒）
2. **添加间隔**: `--interval 10`（测试间隔）
3. **启用详细日志**: 检查logs目录

## 扩展和定制

### 添加新货架

编辑 `test/scripts/shelf_locations.yaml`:

```yaml
shelves:
  - id: "shelf_06"
    name: "货架06 - 自定义"
    x: 0.0
    y: 7.0
    yaw: 1.57
    description: "右侧货架"
```

### 添加新模型

编辑 `run_automated_test.py` 或 `automated_baseline_test.sh`，在模型配置中添加：

```python
# Python版本
'my_custom_model': {
    'name': 'My Custom Model',
    'params': '--custom_param=value',
    'description': '自定义模型'
}
```

```bash
# Bash版本
my_custom_model)
    echo "--custom_param=value"
    ;;
```

## 故障排除

### 问题1: 测试启动失败

**症状**: 脚本无法启动

**解决方案**:
```bash
# 检查权限
chmod +x test/scripts/*.sh
chmod +x test/scripts/*.py

# 检查Python依赖
pip install pyyaml rospkg
```

### 问题2: ROS进程冲突

**症状**: 测试失败，提示ROS已运行

**解决方案**:
```bash
# 清理所有ROS进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python
pkill -9 -f ros
pkill -9 -f gazebo
```

### 问题3: 测试超时

**症状**: 所有测试都超时

**解决方案**:
- 增加超时时间: `--timeout 300`
- 检查地图配置
- 检查目标点是否可达
- 查看日志文件

## 技术支持

查看详细文档:
- 使用指南: `test/scripts/AUTOMATED_TESTING_README.md`
- 项目README: `CLAUDE.md`
- 测试结果: `test_results/MODEL_*/final_report.txt`

## 版本历史

- **v1.0** (2026-04-06): 初始版本
  - 支持4种baseline模型
  - 5个货架位置配置
  - 完整自动化测试流程
  - 详细metrics收集
  - 交互式菜单系统
