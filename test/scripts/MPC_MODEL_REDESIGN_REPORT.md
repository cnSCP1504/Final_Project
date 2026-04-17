# 纯MPC基准测试功能重新设计报告

**日期**: 2026-04-15
**功能**: 重新设计`run_automated_test.py`的纯MPC模型测试支持

---

## 🎯 设计理念

### 核心问题
mpc_ros包的运行逻辑与tube_mpc为基础的其他模型完全不同：
- **mpc_ros**: 使用独立的`nav_gazebo.launch`，参数为`x_pos`, `y_pos`, `yaw`
- **tube_mpc等**: 使用`safe_regret_mpc_test.launch`，参数为`pickup_x`, `pickup_y`, `pickup_yaw`

### 设计原则
1. **不修改现有功能** - tube_mpc等模型的行为保持不变
2. **清晰的模型区分** - 通过`is_mpc_ros`标志区分不同的launch文件
3. **灵活的参数映射** - 支持不同的参数名称映射
4. **禁用不可用话题** - 纯MPC没有tube/STL/DR相关话题

---

## ✅ 实现方案

### 1. ModelConfig扩展

```python
MODELS = {
    'mpc': {
        'name': 'Pure MPC (Baseline)',
        'description': '纯MPC控制器（论文基准对照）',
        'launch_package': 'mpc_ros',
        'launch_file': 'nav_gazebo.launch',
        'params': 'controller:=mpc',
        'is_mpc_ros': True,  # 标记为mpc_ros包
        # mpc_ros的参数映射（不同于safe_regret_mpc）
        'position_args': {
            'x': 'x_pos',
            'y': 'y_pos',
            'yaw': 'yaw'
        }
    },
    'tube_mpc': {
        'name': 'Tube MPC',
        'params': '',
        'description': '仅使用Tube MPC（基线模型）',
        'is_mpc_ros': False
    },
    # ... stl, dr, safe_regret配置
}
```

**关键设计点**:
- `is_mpc_ros=True`: 标记为mpc_ros包
- `launch_package`, `launch_file`: 指定独立的launch文件
- `position_args`: 参数名称映射，支持不同命名约定

### 2. launch_ros_system()双模式支持

```python
# 构建launch命令 - 根据模型类型选择不同的launch文件
is_mpc_ros = self.model_config.get('is_mpc_ros', False)

if is_mpc_ros:
    # mpc_ros包（纯MPC基准）使用独立的launch文件
    launch_package = self.model_config.get('launch_package', 'mpc_ros')
    launch_file = self.model_config.get('launch_file', 'nav_gazebo.launch')
    pos_args = self.model_config.get('position_args', {'x': 'x_pos', 'y': 'y_pos', 'yaw': 'yaw'})

    launch_cmd_str = (
        f"source {self.workspace_dir}/devel/setup.bash && "
        f"roslaunch {launch_package} {launch_file} "
        f"{pos_args['x']}:={shelf['x']} "
        f"{pos_args['y']}:={shelf['y']} "
        f"{pos_args['yaw']}:={shelf['yaw']} "
        f"gui:={str(self.args.visualization).lower()} "
        f"{self.model_config['params']}"
    )
else:
    # safe_regret_mpc包使用统一的launch文件
    launch_cmd_str = (
        f"source {self.workspace_dir}/devel/setup.bash && "
        f"roslaunch safe_regret_mpc safe_regret_mpc_test.launch "
        f"pickup_x:={shelf['x']} "
        f"pickup_y:={shelf['y']} "
        f"pickup_yaw:={shelf['yaw']} "
        f"use_gazebo:={str(self.args.use_gazebo).lower()} "
        f"enable_visualization:={str(self.args.visualization).lower()} "
        f"debug_mode:=true "
        f"{self.model_config['params']}"
    )
```

**关键区别**:

| 特性 | mpc_ros模式 | safe_regret_mpc模式 |
|------|------------|---------------------|
| Launch包 | `mpc_ros` | `safe_regret_mpc` |
| Launch文件 | `nav_gazebo.launch` | `safe_regret_mpc_test.launch` |
| X坐标参数 | `x_pos` | `pickup_x` |
| Y坐标参数 | `y_pos` | `pickup_y` |
| 偏航角参数 | `yaw` | `pickup_yaw` |
| Gazebo参数 | 内置 | `use_gazebo` |
| 可视化参数 | `gui` | `enable_visualization` |

### 3. ManuscriptMetricsCollector智能订阅

```python
def setup_ros_subscribers(self, model_name='tube_mpc'):
    """设置所有需要的ROS话题订阅

    Args:
        model_name: 模型名称，用于判断哪些话题可用
            - mpc: 纯MPC，只发布基础话题（/mpc_trajectory, /cmd_vel）
            - tube_mpc, stl, dr, safe_regret: 发布完整的metrics话题
    """
    self.model_name = model_name

    # 1. STL Robustness - 只在stl和safe_regret模式下启用
    if model_name in ['stl', 'safe_regret']:
        # 订阅 /stl_monitor/robustness 和 /stl_monitor/budget
    else:
        TestLogger.info(f"  STL话题禁用（模型: {model_name}）")

    # 2. DR Margins - 只在dr和safe_regret模式下启用
    if model_name in ['dr', 'safe_regret']:
        # 订阅 /dr_margins
    else:
        TestLogger.info(f"  DR话题禁用（模型: {model_name}）")

    # 3. Tracking Error - 纯MPC没有这个话题
    if model_name in ['tube_mpc', 'stl', 'dr', 'safe_regret']:
        # 订阅 /tube_mpc/tracking_error
    else:
        TestLogger.info(f"  Tracking Error话题禁用（模型: {model_name}，纯MPC无此功能）")

    # 4-6: MPC Metrics, Tube Boundaries, Regret同理
```

**智能订阅逻辑**:
- 根据模型名称判断话题可用性
- 只订阅该模型支持的话题
- 清晰的禁用信息提示

### 4. GoalMonitor模型感知

```python
def __init__(self, goals, goal_radius=0.5, timeout=240, launch_process=None, test_dir=None, model_name='tube_mpc'):
    # ...
    self.model_name = model_name  # 添加模型名称
```

**数据流**:
```
main() --model_name--> AutomatedTestRunner --model_name--> GoalMonitor --model_name--> ManuscriptMetricsCollector
```

---

## 📊 功能对比

### 模型功能对比

| 功能 | mpc | tube_mpc | stl | dr | safe_regret |
|------|-----|----------|-----|-----|-------------|
| **Launch包** | mpc_ros | safe_regret_mpc | safe_regret_mpc | safe_regret_mpc | safe_regret_mpc |
| **参数命名** | x_pos, y_pos, yaw | pickup_x, pickup_y, pickup_yaw | pickup_x, pickup_y, pickup_yaw | pickup_x, pickup_y, pickup_yaw | pickup_x, pickup_y, pickup_yaw |
| **Tube MPC** | ❌ | ✅ | ✅ | ✅ | ✅ |
| **STL监控** | ❌ | ❌ | ✅ | ❌ | ✅ |
| **DR约束** | ❌ | ❌ | ❌ | ✅ | ✅ |
| **参考规划器** | ❌ | ❌ | ❌ | ❌ | ✅ |

### 可收集指标对比

| 指标类型 | mpc | tube_mpc | stl | dr | safe_regret |
|---------|-----|----------|-----|-----|-------------|
| 位置历史 | ✅ | ✅ | ✅ | ✅ | ✅ |
| 目标到达 | ✅ | ✅ | ✅ | ✅ | ✅ |
| STL鲁棒性 | ❌ | ❌ | ✅ | ❌ | ✅ |
| DR边界 | ❌ | ❌ | ❌ | ✅ | ✅ |
| 跟踪误差 | ❌ | ✅ | ✅ | ✅ | ✅ |
| MPC求解时间 | ❌ | ✅ | ✅ | ✅ | ✅ |
| 可行性率 | ❌ | ✅ | ✅ | ✅ | ✅ |
| Tube占用率 | ❌ | ✅ | ✅ | ✅ | ✅ |
| 遗憾指标 | ❌ | ❌ | ❌ | ❌ | ✅ |

---

## 🚀 使用方法

### 单独测试纯MPC

```bash
# 测试5个货架
python3 test/scripts/run_automated_test.py --model mpc --shelves 5

# 快速测试（无可视化）
python3 test/scripts/run_automated_test.py --model mpc --shelves 1 --no-viz

# 使用Gazebo但无RViz
python3 test/scripts/run_automated_test.py --model mpc --shelves 3 --no-viz
```

### 测试所有模型（包含纯MPC）

```bash
# 完整测试（每个模型10个货架）
python3 run_automated_test.py --model all --shelves 10

# 快速测试所有模型
python3 run_automated_test.py --model all --shelves 1 --no-viz
```

### 测试顺序（all模式）

当使用`--model all`时，测试顺序为：
1. **mpc** - 纯MPC基准（论文对照）
2. **tube_mpc** - Tube MPC基线
3. **stl** - Tube MPC + STL
4. **dr** - Tube MPC + DR
5. **safe_regret** - 完整Safe-Regret MPC

---

## ⚠️ 重要注意事项

### 1. 纯MPC的限制

mpc_ros包是独立实现的，不具备以下功能：
- ❌ 没有Tube MPC功能
- ❌ 没有STL监控功能
- ❌ 没有DR约束收紧功能
- ❌ 没有参考规划器功能
- ❌ 不发布tracking_error、tube_boundaries等话题

### 2. 可收集指标差异

**纯MPC可以收集**:
- ✅ 位置历史（/odom）
- ✅ 目标到达时间
- ✅ 基本导航统计

**纯MPC无法收集**:
- ❌ STL鲁棒性（没有stl_monitor节点）
- ❌ DR边界（没有dr_tightening节点）
- ❌ 跟踪误差（没有tube_mpc/tracking_error话题）
- ❌ MPC求解时间（没有mpc_metrics话题）
- ❌ Tube占用率（没有tube_boundaries话题）
- ❌ 遗憾指标（没有reference_planner节点）

### 3. 论文基准对照价值

纯MPC作为基准，主要对比指标：
- **任务完成率** - 是否成功到达目标
- **任务完成时间** - 导航效率
- **路径平滑度** - 通过速度变化判断
- **计算开销** - 系统整体响应（虽然没有MPC求解时间数据）

---

## 🔧 技术实现细节

### 参数映射机制

```python
# mpc_ros的参数映射
position_args = {
    'x': 'x_pos',    # shelf['x'] -> x_pos:=...
    'y': 'y_pos',    # shelf['y'] -> y_pos:=...
    'yaw': 'yaw'     # shelf['yaw'] -> yaw:=...
}

# safe_regret_mpc的参数映射（隐式）
# shelf['x'] -> pickup_x:=...
# shelf['y'] -> pickup_y:=...
# shelf['yaw'] -> pickup_yaw:=...
```

### Launch命令生成

**mpc模式**:
```bash
roslaunch mpc_ros nav_gazebo.launch x_pos:=1.0 y_pos:=2.0 yaw:=0.5 gui:=true controller:=mpc
```

**tube_mpc模式**:
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch pickup_x:=1.0 pickup_y:=2.0 pickup_yaw:=0.5 use_gazebo:=true enable_visualization:=true debug_mode:=true
```

### 话题订阅智能判断

```python
# 根据模型名称动态判断话题可用性
if model_name in ['tube_mpc', 'stl', 'dr', 'safe_regret']:
    # 订阅tube_mpc相关话题
    # /tube_mpc/tracking_error
    # /mpc_metrics/solve_time_ms
    # /tube_boundaries
else:
    # 纯MPC模式下跳过这些话题
    TestLogger.info(f"  Topic disabled for model: {model_name}")
```

---

## 📁 修改文件列表

1. `test/scripts/run_automated_test.py`
   - ModelConfig类：添加mpc配置，包含完整的参数映射
   - launch_ros_system()：双模式支持，根据is_mpc_ros选择launch文件
   - ManuscriptMetricsCollector.setup_ros_subscribers()：智能话题订阅
   - GoalMonitor：添加model_name参数传递
   - argparse：更新模型choices
   - all模式：包含mpc模型

---

## ✅ 测试验证

### ModelConfig测试

```bash
python3 -c "
import sys
sys.path.insert(0, 'test/scripts')
from run_automated_test import ModelConfig

print('Available models:', list(ModelConfig.MODELS.keys()))
mpc_config = ModelConfig.get_model_params('mpc')
print('MPC config:', mpc_config)
"
```

**预期输出**:
```
Available models: ['mpc', 'tube_mpc', 'stl', 'dr', 'safe_regret']
MPC config: {
    'name': 'Pure MPC (Baseline)',
    'launch_file': 'nav_gazebo.launch',
    'launch_package': 'mpc_ros',
    'params': 'controller:=mpc',
    'is_mpc_ros': True,
    'position_args': {'x': 'x_pos', 'y': 'y_pos', 'yaw': 'yaw'},
    'description': '纯MPC控制器（论文基准对照）'
}
```

### 功能测试建议

```bash
# 快速测试纯MPC功能
python3 test/scripts/run_automated_test.py --model mpc --shelves 1 --no-viz --timeout 60

# 检查测试结果
ls -la test_results/mpc_*/
cat test_results/mpc_*/test_01_shelf_01/test_summary.txt
```

---

## 🎯 设计优势

### 1. 清晰的分离
- mpc_ros和safe_regret_mpc完全分离
- 不修改tube_mpc等现有功能
- 参数映射清晰明了

### 2. 可扩展性
- 支持添加更多独立模型
- 参数映射机制可复用
- 话题订阅逻辑可扩展

### 3. 可维护性
- 代码结构清晰
- 注释完整
- 易于理解和修改

### 4. 向后兼容
- 现有模型功能不变
- 现有测试脚本兼容
- 现有参数文件兼容

---

## 总结

✅ **已完成**：
- 重新设计纯MPC模型集成方案
- 实现双模式launch文件支持
- 实现智能话题订阅
- 支持all模式测试

✅ **设计优势**：
- 清晰分离mpc_ros和safe_regret_mpc
- 不修改现有功能
- 参数映射灵活
- 代码可维护性高

✅ **论文需求**：
- 提供纯MPC基准对照
- 支持性能对比分析
- 满足论文实验要求
