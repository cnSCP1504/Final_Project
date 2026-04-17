# MPC纯基准测试 - 最终修复方案

**日期**: 2026-04-15
**状态**: 🔍 问题定位完成

---

## 问题定位

**核心问题**：原始 `mpc_ros` 包工作正常，但 `mpc_baseline_test.launch` 无法工作

**差异对比**：

| 配置项 | 原始nav_gazebo.launch | mpc_baseline_test.launch |
|--------|----------------------|-------------------------|
| 世界文件 | `mpc_ros/worlds/sq_world.world` | `tube_mpc_ros/worlds/test_world.world` |
| 地图文件 | `mpc_ros/map/sq_world.yaml` | `tube_mpc_ros/map/test_world.yaml` |
| costmap参数 | `mpc_ros/params/costmap_*.yaml` | 已改为mpc_ros参数 ✅ |
| AMCL参数 | `mpc_ros/params/amcl_params.yaml` | `tube_mpc_ros/params/amcl_params.yaml` |
| MPC参数 | `mpc_ros/params/mpc_last_params.yaml` | `mpc_ros/params/mpc_last_params.yaml` ✅ |
| 初始位置 | (0.0, 0.0, yaw=-1.507) | (-8.0, 0.0, yaw=0.0) |

**关键发现**：
1. costmap参数已改回mpc_ros原始参数 ✅
2. global_path_topic已正确设置 ✅
3. 问题可能出在：
   - 世界文件/地图文件不兼容
   - 初始位置导致路径规划失败
   - AMCL参数差异

---

## 解决方案

### 方案A：直接使用原始nav_gazebo.launch（推荐）

最简单的方案是直接使用 `mpc_ros` 原始的 `nav_gazebo.launch`，然后在测试脚本中处理差异：

```python
# run_automated_test.py
if model == 'mpc':
    # 使用原始mpc_ros launch文件
    launch_cmd = f"roslaunch mpc_ros nav_gazebo.launch controller:=mpc"
```

**优点**：
- 确保兼容性
- 无需维护两份配置
- 避免参数不一致问题

**缺点**：
- 与其他模型使用不同的世界/地图
- 初始位置不同

### 方案B：完全复制原始配置

将 `mpc_baseline_test.launch` 完全改为与 `nav_gazebo.launch` 一致：

1. 世界文件改为 `mpc_ros/worlds/sq_world.world`
2. 地图改为 `mpc_ros/map/sq_world.yaml`
3. AMCL参数改为 `mpc_ros/params/amcl_params.yaml`
4. 初始位置改为 (0.0, 0.0, yaw=-1.507)

**优点**：
- 与原始包完全一致
- 保证工作

**缺点**：
- 与其他模型的测试环境不同
- 无法进行公平对比

### 方案C：调试当前配置

深入调试为什么 `test_world.world` 和 `sq_world.world` 表现不同：

1. 检查两个world文件的差异
2. 检查地图文件差异
3. 检查初始位置是否导致路径规划失败

---

## 推荐行动

**建议采用方案A**：

修改 `run_automated_test.py`，对于 MPC 模型直接使用原始 `nav_gazebo.launch`：

```python
# 在launch_ros_system()方法中
if self.model_config.get('is_mpc_ros'):
    # 使用原始mpc_ros launch文件
    launch_cmd_str = (
        f"source {self.workspace_dir}/devel/setup.bash && "
        f"roslaunch mpc_ros nav_gazebo.launch "
        f"controller:=mpc "
        f"gui:={str(self.args.visualization).lower()}"
    )
```

**原因**：
1. 原始包已经过验证，工作正常
2. 避免引入不必要的配置差异
3. 减少维护负担
4. 可以作为独立的基准测试

---

## 下一步

1. 修改 `run_automated_test.py` 使用原始 `nav_gazebo.launch`
2. 测试验证
3. 如需统一环境，再考虑方案C的深入调试
