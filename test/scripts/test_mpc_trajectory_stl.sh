#!/bin/bash
# Test STL Monitor with MPC Trajectory

echo "=========================================="
echo "STL Monitor + MPC Trajectory 测试"
echo "=========================================="

# Clean up
killall -9 roslaunch rosmaster roscore 2>/dev/null
sleep 2

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo ""
echo "🚀 启动STL监控节点..."
roslaunch stl_monitor stl_monitor.launch > /tmp/stl_mpc_test.log 2>&1 &
STL_PID=$!

sleep 3

echo ""
echo "📡 检查节点订阅的话题..."
echo "----------------------------------------"
rostopic info /stl_monitor/robustness 2>/dev/null || echo "❌ /stl_monitor/robustness 不存在"

echo ""
echo "📊 检查STL节点订阅的话题..."
echo "----------------------------------------"
ROS_NAMESPACE=/stl_monitor rostopic info | grep "Subscribers:" -A 10 || echo "无法获取订阅信息"

echo ""
echo "🤖 测试1: 发布MPC轨迹 (3个点的直线路径)"
rostopic pub -1 /mpc_trajectory nav_msgs::Path "header:
  frame_id: 'map'
poses:
- pose:
    position: {x: 0.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
- pose:
    position: {x: 5.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
- pose:
    position: {x: 10.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}"

sleep 1

echo ""
echo "🤖 测试2: 发布机器人位置 (在MPC轨迹上)"
rostopic pub -1 /amcl_pose geometry_msgs/PoseWithCovarianceStamped "header:
  frame_id: 'map'
pose:
  pose:
    position: {x: 5.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
  covariance: [0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01]"

sleep 2

echo ""
echo "📊 检查Robustness输出..."
ROBUSTNESS=$(rostopic echo -n /stl_monitor/robustness 2>/dev/null | grep "data:" | tail -1 | awk '{print $2}' || echo "N/A")
echo "   Robustness: $ROBUSTNESS (预期: ≈0.5，因为在MPC轨迹上)"

echo ""
echo "🤖 测试3: 发布机器人位置 (偏离MPC轨迹0.5m)"
rostopic pub -1 /amcl_pose geometry_msgs/PoseWithCovarianceStamped "header:
  frame_id: 'map'
pose:
  pose:
    position: {x: 5.0, y: 0.5, z: 0.0}
    orientation: {w: 1.0}
  covariance: [0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01]"

sleep 2

ROBUSTNESS2=$(rostopic echo -n /stl_monitor/robustness 2>/dev/null | grep "data:" | tail -1 | awk '{print $2}' || echo "N/A")
echo "   Robustness: $ROBUSTNESS2 (预期: ≈0.0，距离MPC轨迹0.5m)"

echo ""
echo "=========================================="
echo "STL节点日志摘要："
echo "=========================================="
grep -E "STL Monitor Node|Trajectory source|Path-Tracking|distance to path|Belief-space|mpc_trajectory" /tmp/stl_mpc_test.log | head -15

# Clean up
kill $STL_PID 2>/dev/null
killall -9 roslaunch rosmaster 2>/dev/null

echo ""
echo "=========================================="
echo "✅ 测试完成"
echo "=========================================="
