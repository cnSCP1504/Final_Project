#!/bin/bash
# 手动测试Path Tracking STL

echo "=========================================="
echo "Path Tracking STL手动验证测试"
echo "=========================================="

# 清理
killall -9 roslaunch rosmaster roscore 2>/dev/null
sleep 2

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo ""
echo "📦 启动STL监控节点（C++版本）..."
roslaunch stl_monitor stl_monitor.launch > /tmp/stl_manual_test.log 2>&1 &
STL_PID=$!

sleep 3

echo ""
echo "🔍 检查节点类型..."
grep "STL Monitor Node\|C++\|Python" /tmp/stl_manual_test.log | head -5

echo ""
echo "📡 发布测试路径..."
rostopic pub -1 /move_base/GlobalPlanner/plan nav_msgs/Path "header:
  frame_id: 'map'
poses:
- pose:
    position: {x: 0.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
- pose:
    position: {x: 3.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
- pose:
    position: {x: 3.0, y: 3.0, z: 0.0}
    orientation: {w: 1.0}
- pose:
    position: {x: 0.0, y: 3.0, z: 0.0}
    orientation: {w: 1.0}"

sleep 1

echo ""
echo "🤖 测试1: 机器人在路径上 (1.5, 0.0)"
rostopic pub -1 /amcl_pose geometry_msgs/PoseWithCovarianceStamped "header:
  frame_id: 'map'
pose:
  pose:
    position: {x: 1.5, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
  covariance: [0.01, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.01, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.01, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.01, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.01, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.01]"

sleep 2
ROBUSTNESS1=$(rostopic echo -n /stl_monitor/robustness 2>/dev/null | grep data | tail -1 | awk '{print $2}' || echo "N/A")
echo "   Robustness: $ROBUSTNESS1 (预期: ≈0.5，因为在路径上)"

echo ""
echo "🤖 测试2: 机器人偏离路径 (1.5, 0.5)"
rostopic pub -1 /amcl_pose geometry_msgs/PoseWithCovarianceStamped "header:
  frame_id: 'map'
pose:
  pose:
    position: {x: 1.5, y: 0.5, z: 0.0}
    orientation: {w: 1.0}
  covariance: [0.01, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.01, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.01, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.01, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.01, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.01]"

sleep 2
ROBUSTNESS2=$(rostopic echo -n /stl_monitor/robustness 2>/dev/null | grep data | tail -1 | awk '{print $2}' || echo "N/A")
echo "   Robustness: $ROBUSTNESS2 (预期: ≈0.0，距离路径0.5m)"

echo ""
echo "🤖 测试3: 机器人远离路径 (1.5, 1.5)"
rostopic pub -1 /amcl_pose geometry_msgs/PoseWithCovarianceStamped "header:
  frame_id: 'map'
pose:
  pose:
    position: {x: 1.5, y: 1.5, z: 0.0}
    orientation: {w: 1.0}
  covariance: [0.01, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.01, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.01, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.01, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.01, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.01]"

sleep 2
ROBUSTNESS3=$(rostopic echo -n /stl_monitor/robustness 2>/dev/null | grep data | tail -1 | awk '{print $2}' || echo "N/A")
echo "   Robustness: $ROBUSTNESS3 (预期: ≈-1.0，距离路径1.5m)"

echo ""
echo "=========================================="
echo "测试结果汇总"
echo "=========================================="
echo "测试1 (在路径上):    Robustness = $ROBUSTNESS1"
echo "测试2 (偏离0.5m):    Robustness = $ROBUSTNESS2"
echo "测试3 (偏离1.5m):    Robustness = $ROBUSTNESS3"
echo ""

# 简单验证
if [[ ! "$ROBUSTNESS1" =~ "N/A" ]] && [[ ! "$ROBUSTNESS2" =~ "N/A" ]] && [[ ! "$ROBUSTNESS3" =~ "N/A" ]]; then
    echo "✅ 所有测试都收到了Robustness值"

    # 检查是否递减（路径上 > 偏离0.5m > 偏离1.5m）
    if (( $(echo "$ROBUSTNESS1 > $ROBUSTNESS2" | bc -l) )) && (( $(echo "$ROBUSTNESS2 > $ROBUSTNESS3" | bc -l) )); then
        echo "✅ Robustness值正确递减（符合Path Tracking逻辑）"
        echo "   → Path Tracking STL实现正确！"
    else
        echo "❌ Robustness值顺序不对"
        echo "   预期: $ROBUSTNESS1 > $ROBUSTNESS2 > $ROBUSTNESS3"
    fi
else
    echo "❌ 部分测试未收到Robustness值"
fi

echo ""
echo "📝 完整日志："
tail -30 /tmp/stl_manual_test.log | grep -E "STL|Belief|Path-Tracking|distance"

# 清理
kill $STL_PID 2>/dev/null
killall -9 roslaunch rosmaster 2>/dev/null

echo ""
echo "=========================================="
echo "测试完成"
echo "=========================================="
