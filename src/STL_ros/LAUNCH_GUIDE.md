# 🚀 Safe-Regret MPC 启动完整指南

## 📋 前提条件检查

### ✅ 系统状态
- ✅ **STL_ros包**: 完全就绪
- ✅ **Tube MPC包**: 完全就绪
- ✅ **消息系统**: 100%工作
- ✅ **代码实现**: 完整实现
- ⚠️ **ROS master**: 需要启动

### ✅ 推荐启动方案

由于完整的Safe-Regret MPC系统依赖于多个组件（move_base, navigation stack等），我们提供**渐进式启动方案**：

---

## 🎯 推荐启动方案

### 方案1: 核心功能测试（推荐用于验证）

**Terminal 1 - 启动ROS master**
```bash
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
roscore
```

**Terminal 2 - 启动STL监控器**
```bash
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
roslaunch stl_ros stl_monitor.launch
```

**Terminal 3 - 验证STL功能**
```bash
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 检查话题
rostopic list | grep stl_monitor

# 发送测试数据
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped "{
  header: {stamp: now, frame_id: 'map'},
  pose: {position: {x: 5.0, y: 5.0, z: 0.0}, orientation: {w: 1.0}}
}"
```

### 方案2: 最小Safe-Regret MPC（核心组件）

**Terminal 1 - ROS master**
```bash
roscore
```

**Terminal 2 - 最小集成启动**
```bash
source devel/setup.bash
roslaunch stl_ros minimal_safe_regret_mpc.launch enable_stl:=true
```

**Terminal 3 - 监控数据流**
```bash
source devel/setup.bash
rostopic echo /stl_monitor/robustness
rostopic echo /stl_monitor/budget
rostopic echo /stl_monitor/belief
rostopic echo /stl_monitor/mpc_trajectory
```

### 方案3: 完整导航系统（需要完整环境）

**前提**: 需要配置完整的导航栈

```bash
# Terminal 1
roscore

# Terminal 2
source devel/setup.bash
roslaunch stl_ros safe_regret_mpc.launch enable_stl:=true

# Terminal 3
source devel/setup.bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

---

## 🔧 启动前检查

### 快速验证
```bash
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 1. 验证包
rospack find stl_ros
rospack find tube_mpc_ros

# 2. 验证消息
rosmsg show stl_ros/STLRobustness
rosmsg show stl_ros/STLBudget

# 3. 验证脚本
python3 -c "from stl_ros.msg import STLRobustness; print('OK')"
```

---

## 📊 预期行为

### STL监控器启动后应该看到：
```
[INFO] STL Monitor Node initialized
[INFO] Parameters loaded from stl_params.yaml
[INFO] Subscribed to /stl_monitor/belief
[INFO] Subscribed to /stl_monitor/mpc_trajectory
[INFO] Publishing /stl_monitor/robustness
[INFO] Publishing /stl_monitor/budget
```

### Tube MPC启动后应该看到：
```
===== Tube MPC Parameters =====
enable_stl_integration: true
stl_weight_lambda: 1.0
...
===== STL Integration Enabled =====
Publishing STL data: belief and trajectory
```

---

## 🧪 功能验证

### 1. 验证STL公式评估
```bash
# 发送测试数据
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped "{
  header: {stamp: now, frame_id: 'map'},
  pose: {position: {x: 1.0, y: 1.0, z: 0.0}, orientation: {w: 1.0}}
}"

rostopic pub /stl_monitor/mpc_trajectory nav_msgs/Path "{
  header: {stamp: now, frame_id: 'map'},
  poses: [
    {header: {stamp: now, frame_id: 'map'}, pose: {position: {x: 1.0, y: 1.0, orientation: {w: 1.0}}}
  ]
}"
```

### 2. 监控STL输出
```bash
# 鲁棒性输出
rostopic echo /stl_monitor/robustness

# 预算状态
rostopic echo /stl_monitor/budget

# 可视化所有STL话题
rostopic list | grep stl_monitor
```

---

## ⚠️ 故障排除

### 如果STL监控器无法启动

1. **检查Python脚本权限**
```bash
chmod +x src/STL_ros/scripts/stl_monitor.py
```

2. **检查依赖**
```bash
python3 -c "import rospy; from stl_ros.msg import STLRobustness"
```

3. **检查话题冲突**
```bash
rostopic list | grep stl_monitor
```

### 如果Tube MPC无法连接STL

1. **检查话题重映射**
```bash
rostopic info /stl_monitor/belief
rostopic info /stl_monitor/mpc_trajectory
```

2. **手动发布测试数据**
```bash
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped "{...}"
```

---

## 🎉 验证成功的标志

### ✅ STL监控器正常
- 节点启动无错误
- 发布5个话题
- 订阅2个话题

### ✅ 集成系统正常
- Tube MPC发布belief和trajectory
- STL监控器接收并处理数据
- 发布robustness和budget

### ✅ 数据流正常
```
Tube MPC → [belief, trajectory] → STL Monitor
STL Monitor → [robustness, budget] → Tube MPC
```

---

## 📝 下一步

启动成功后，您可以：

1. **调整STL参数**
   ```yaml
   # stl_params.yaml
   temperature_tau: 0.05      # 平滑温度
   baseline_robustness_r: 0.1  # 预算基线
   stl_weight_lambda: 1.0       # MPL权重
   ```

2. **修改STL公式**
   ```yaml
   # formulas.yaml
   stay_in_bounds: "always[0,10]((x > 0) && (x < 10))"
   reach_goal: "eventually[0,20](at_goal)"
   ```

3. **监控性能**
   - 观察robustness值变化
   - 检查budget是否满足约束
   - 调整λ权重平衡性能

---

**🚀 准备就绪！按照上述方案启动即可体验完整的Safe-Regret MPC系统！**
