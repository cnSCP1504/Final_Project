# MPC纯基准测试问题诊断报告

**日期**: 2026-04-15
**状态**: 🔍 诊断中

---

## 问题描述

MPC纯基准测试启动后，机器人无法移动，RViz和Gazebo正常打开，但：
- 日志显示 `Failed to path generation`（路径生成失败）
- 机器人位置保持不变（`已移动: 0.00m`）

---

## 根本原因分析

### 问题1：YAML参数覆盖问题

**发现**：
- launch文件中设置了 `<param name="global_path_topic" value="/move_base/GlobalPlanner/plan"/>`
- 但YAML文件 `mpc_last_params.yaml` 中有 `MPCPlannerROS: global_path_topic: /move_base/MPCPlannerROS/global_plan`
- **YAML加载顺序在param之后**，导致参数被覆盖

**日志证据**：
```
* /nav_mpc/global_path_topic: /move_base/Global...    ← launch设置（正确）
* /nav_mpc/MPCPlannerROS/global_path_topic: /move_base/MPCPla...  ← YAML覆盖（错误！）
```

### 问题2：GlobalPlanner话题未被正确发布

**发现**：
- move_base使用 `GlobalPlanner` 作为全局规划器
- 但GlobalPlanner可能没有发布路径到 `/move_base/GlobalPlanner/plan`

**需要验证**：
1. GlobalPlanner是否真的发布了路径？
2. 如果发布了，话题名称是什么？

---

## 解决方案

### 方案1：修改YAML参数（推荐）

修改 `src/tube_mpc_ros/mpc_ros/params/mpc_last_params.yaml`：
```yaml
MPCPlannerROS:
  global_path_topic: /move_base/GlobalPlanner/plan  # 修改为正确的话题
```

### 方案2：不加载YAML文件

在launch文件中移除YAML加载，只保留关键参数：
```xml
<node name="nav_mpc" pkg="mpc_ros" type="nav_mpc" output="screen">
  <param name="global_path_topic" value="/move_base/GlobalPlanner/plan"/>
  <param name="path_length" value="25.0"/>
  <param name="waypoints_dist" value="-1.0"/>
  <!-- 不加载YAML文件 -->
</node>
```

### 方案3：检查GlobalPlanner是否发布路径

运行测试并检查话题：
```bash
roslaunch mpc_ros mpc_baseline_test.launch &
sleep 10
rostopic list | grep plan
rostopic echo /move_base/GlobalPlanner/plan --noarr
```

---

## 下一步行动

1. **检查GlobalPlanner话题**：
   ```bash
   rostopic list | grep -i plan
   ```

2. **修改YAML文件**（如果确认话题名称）

3. **重新测试**

---

## 技术细节

### navMPCNode参数加载顺序

1. 节点启动时读取默认参数（代码中的默认值）
2. 加载YAML文件（覆盖命名空间参数）
3. 加载launch文件中的param（覆盖全局参数）

**问题**：
- YAML中的 `MPCPlannerROS/global_path_topic` 和 launch中的 `global_path_topic` 是两个不同的参数
- 节点可能读取的是 `MPCPlannerROS/global_path_topic`（错误值）

### 代码位置

**navMPCNode.cpp (line 135)**:
```cpp
pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");
```

这里使用的是私有命名空间 `~global_path_topic`，对应 launch 中的 `<param name="global_path_topic">`。

但YAML加载的是 `MPCPlannerROS/global_path_topic`，可能导致混淆。

---

## 结论

**问题根源**：YAML参数覆盖了launch文件中设置的参数

**解决方案**：修改YAML文件中的 `MPCPlannerROS/global_path_topic` 为正确的话题名称
