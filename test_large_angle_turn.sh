#!/bin/bash
# 大角度转向修复测试脚本

echo "========================================="
echo "大角度转向修复测试"
echo "========================================="
echo ""

# 清理所有ROS进程
echo "步骤1: 清理所有ROS进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2
echo "✓ 清理完成"
echo ""

# 重新编译
echo "步骤2: 重新编译..."
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
catkin_make --only-pkg-with-deps tube_mpc_ros
if [ $? -ne 0 ]; then
    echo "❌ 编译失败！"
    exit 1
fi
echo "✓ 编译成功"
echo ""

# 启动系统
echo "步骤3: 启动Safe-Regret MPC系统..."
echo "提示：系统启动后，请观察控制台输出"
echo ""
echo "关键测试场景："
echo "  1. 机器人导航到第一个目标 (0.0, -7.0)"
echo "  2. 自动发送第二个目标 (8.0, 0.0)"
echo "  3. 观察："
echo "     ✓ 控制台是否输出 'Large angle turn detected'"
echo "     ✓ 控制台是否输出 'CRITICAL HEADING ERROR: Pure rotation mode'"
echo "     ✓ etheta值保持真实（>90度）"
echo "     ✓ 机器人是否原地旋转（speed=0，angular.z≠0）"
echo "     ✓ 机器人是否不会转向反方向"
echo ""
echo "预期日志输出："
echo "  ⚠️  Large angle turn detected: 120°, enabling in-place rotation mode"
echo "  ⚠️  CRITICAL HEADING ERROR: Pure rotation mode"
echo "     etheta: 2.09 rad (120 deg)"
echo "  → 机器人应该原地旋转，速度≈0"
echo ""
echo "按Ctrl+C停止测试..."
echo ""

# 启动测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch &

# 等待系统启动
sleep 15

# 自动发送测试目标
echo ""
echo "========================================="
echo "步骤4: 自动发送测试目标..."
echo "========================================="
echo ""

# 第一个目标
echo "发送第一个目标: (0.0, -7.0)"
rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: 0.0
    y: -7.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once

echo "等待60秒让机器人到达第一个目标..."
sleep 60

# 第二个目标（大角度转向测试）
echo ""
echo "========================================="
echo "发送第二个目标: (8.0, 0.0) - 大角度转向测试"
echo "========================================="
echo ""

rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: 8.0
    y: 0.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once

echo ""
echo "观察机器人行为（60秒）..."
echo "关键指标："
echo "  ✓ 机器人是否原地旋转或大角度转向"
echo "  ✓ 机器人是否向正确方向移动（向东）"
echo "  ✓ 是否出现反向行驶（向西）"
echo ""

sleep 60

echo ""
echo "========================================="
echo "测试结束"
echo "========================================="
echo ""
echo "如果机器人正确转向，说明修复成功！"
echo "如果仍有问题，请检查控制台日志中的etheta值"
echo ""
echo "清理进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
echo "✓ 清理完成"
