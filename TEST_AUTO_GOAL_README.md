# 自动多目标导航测试指南

## 快速测试

### 方法1: 使用Python脚本（推荐）

```bash
# 终端1: 启动系统
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
killall -9 roslaunch rosmaster roscore gzserver gzclient python
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 终端2: 运行自动导航脚本
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
rosrun safe_regret_mpc auto_nav_goal.py
```

### 方法2: 使用Bash脚本

```bash
# 终端1: 启动系统
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
killall -9 roslaunch rosmaster roscore gzserver gzclient python
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 终端2: 运行测试脚本
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
./test_two_goals.sh
```

## 预期行为

1. **发送目标1**: (3.0, -7.0)
   - 机器人开始运动
   - 每10秒显示距离和移动距离
   - 到达<1.0米时标记为到达

2. **自动发送目标2**: (8.0, 0.0)
   - 等待2秒后自动发送
   - 测试大角度转向场景

3. **完成**: 显示"所有导航任务完成!"

## 自定义目标

编辑 `src/safe_regret_mpc/scripts/auto_nav_goal.py` 第144-146行:

```python
goals = [
    (3.0, -7.0, 0.0, "第一个目标"),
    (8.0, 0.0, 0.0, "第二个目标"),
    # 添加更多目标...
]
```

## 调整到达阈值

```bash
# 方法1: 命令行参数
rosrun safe_regret_mpc auto_nav_goal.py _goal_radius:=1.5

# 方法2: 修改默认值
# 编辑 auto_nav_goal.py 第20行
self.goal_radius = rospy.get_param('~goal_radius', 1.5)  # 改为1.5米
```

## 故障排查

### 问题: 脚本等待超时

```bash
# 检查odom话题
rostopic echo /odom --noarr

# 检查机器人位置
rostopic echo /odom --noarr | grep -A 2 "position:"

# 增加超时时间
# 编辑 auto_nav_goal.py 第118行，将timeout改为更大值
```

### 问题: 目标发送后机器人不动

```bash
# 检查TubeMPC是否收到目标
rostopic echo /rosout | grep "Goal Received"

# 检查GlobalPlanner是否生成路径
rostopic echo /move_base/GlobalPlanner/plan --noarr | grep "poses:"
```

## 相关文档

- 修复详情: `test/docs/AUTO_GOAL_PUBLISHER_FIX_REPORT.md`
- 目标到达修复: `test/docs/GOAL_ARRIVAL_FIX_REPORT.md`
