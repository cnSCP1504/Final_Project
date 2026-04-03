#!/bin/bash
# 测试目标到达修复的脚本
# 问题：路径点过少时机器人停止运动，无法到达目标
# 修复：放宽路径点数要求，允许使用2-3个点继续控制

echo "========================================="
echo "测试目标到达修复"
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
echo "提示：在RViz中设置两个目标点进行测试"
echo "  - 第一个目标: (3.0, -7.0)"
echo "  - 第二个目标: (8.0, 0.0)"
echo ""
echo "预期行为："
echo "  ✓ 机器人应该能够到达两个目标点"
echo "  ✓ 即使路径点数减少到2-3个，也应继续控制"
echo "  ✓ 当距离目标 < 1.0m 时，标记为到达"
echo ""
echo "按Ctrl+C停止测试..."
echo ""

# 启动测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

echo ""
echo "========================================="
echo "测试结束"
echo "========================================="
