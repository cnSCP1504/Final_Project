# MPC纯基准测试问题分析报告

**日期**: 2026-04-15
**状态**: 🔍 问题诊断中

---

## 当前问题

MPC纯基准测试启动后，机器人无法移动。日志显示：
- `Goal Received :goalCB!` - 目标已接收
- `Failed to path generation` - 路径生成失败（持续出现）

---

## 已尝试的修复

### 修复1：修改YAML参数
- 修改 `tube_mpc_ros/params/mpc_last_params.yaml` 中的 `global_path_topic`
- 结果：❌ 无效

### 修复2：使用mpc_ros原始参数文件
- 使用 `mpc_ros/params/mpc_last_params.yaml`
- 添加话题remap：`/move_base/TrajectoryPlannerROS/global_plan` → `/move_base/GlobalPlanner/plan`
- 结果：❌ 无效

### 修复3：增加path_length
- 设置 `path_length=25.0`
- 结果：❌ 无效

---

## 问题根源分析

### 核心问题：GlobalPlanner话题未被正确订阅

**navMPCNode默认订阅**：`/move_base/TrajectoryPlannerROS/global_plan`

**但move_base配置**：
- `base_global_planner: global_planner/GlobalPlanner`
- 发布话题：`/move_base/GlobalPlanner/plan`

**话题不匹配！**

### 为什么remap无效？

查看代码发现，navMPCNode订阅的是参数 `_globalPath_topic`，而不是话题名称：

```cpp
// navMPCNode.cpp line 135
pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");

// line 156
_sub_path = _nh.subscribe(_globalPath_topic, 1, &MPCNode::pathCB, this);
```

**问题**：
1. 参数从YAML加载：`MPCPlannerROS/global_path_topic`（YAML中不存在这个参数！）
2. 如果参数不存在，使用默认值：`/move_base/TrajectoryPlannerROS/global_plan`
3. remap只能改变话题名称，不能改变参数值

---

## 解决方案

### 方案1：添加global_path_topic参数到YAML

修改 `mpc_ros/params/mpc_last_params.yaml`：

```yaml
MPCPlannerROS:
  global_path_topic: /move_base/GlobalPlanner/plan  # 添加这一行
  map_frame: "map"
  ...
```

### 方案2：在launch文件中设置参数

```xml
<node name="nav_mpc" pkg="mpc_ros" type="nav_mpc" output="screen">
  <rosparam file="$(find mpc_ros)/params/mpc_last_params.yaml" command="load"/>
  <!-- ✅ 直接设置参数（在YAML之后）-->
  <param name="MPCPlannerROS/global_path_topic" value="/move_base/GlobalPlanner/plan"/>
  <param name="global_path_topic" value="/move_base/GlobalPlanner/plan"/>
</node>
```

### 方案3：使用mpc_ros原始配置

直接使用 `mpc_ros` 包的 `nav_gazebo.launch`，它已经配置好了。

---

## 推荐方案

**方案1**最简单，只需在YAML中添加一行参数。

---

## 关键发现

### navMPCNode参数加载机制

1. **默认参数**（代码中定义）：
   ```cpp
   pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");
   ```

2. **YAML参数**（如果有）：
   ```yaml
   MPCPlannerROS:
     global_path_topic: /move_base/xxx
   ```

3. **launch参数**（如果有）：
   ```xml
   <param name="global_path_topic" value="/move_base/yyy"/>
   ```

**问题**：YAML中不存在 `global_path_topic` 参数，所以使用默认值！

### 查看当前参数

```bash
cat /home/dixon/Final_Project/catkin/test_results/mpc_20260415_064741/test_01_shelf_01/logs/launch.log | grep "global_path_topic"
```

结果：
```
（无输出 - 说明参数不存在！）
```

---

## 下一步行动

1. **添加参数到YAML文件**
2. **重新测试**
