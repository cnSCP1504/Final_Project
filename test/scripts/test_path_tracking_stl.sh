#!/bin/bash
# Path Tracking STL快速测试脚本

echo "========================================"
echo "Path Tracking STL验证测试"
echo "========================================"

# 清理之前的ROS进程
echo "🧹 清理ROS进程..."
killall -9 roslaunch rosmaster roscore python3 2>/dev/null
sleep 2

# Source环境
echo "📦 Sourcing ROS环境..."
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 启动STL监控节点（独立测试）
echo "🚀 启动STL监控节点..."
roslaunch stl_monitor stl_monitor.launch &
STL_PID=$!

# 等待节点启动
echo "⏳ 等待节点启动（3秒）..."
sleep 3

# 运行验证脚本
echo "🧪 运行验证测试..."
python3 /home/dixon/Final_Project/catkin/test/scripts/verify_path_tracking_stl.py
TEST_RESULT=$?

# 清理
echo "🧹 清理进程..."
kill $STL_PID 2>/dev/null
killall -9 roslaunch rosmaster roscore python3 2>/dev/null

# 返回测试结果
if [ $TEST_RESULT -eq 0 ]; then
    echo "✅ 测试通过！"
    exit 0
else
    echo "❌ 测试失败！"
    exit 1
fi
