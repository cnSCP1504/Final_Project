#!/bin/bash
# 实时验证Path Tracking STL

echo "=========================================="
echo "Path Tracking STL实时验证"
echo "=========================================="

killall -9 roslaunch rosmaster roscore rviz 2>/dev/null
sleep 2

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo ""
echo "🚀 启动STL监控（C++版本）..."
roslaunch stl_monitor stl_monitor.launch > /tmp/stl_verify.log 2>&1 &
sleep 4

echo ""
echo "📡 发布直线路径 (0,0) → (10,0)..."
rostopic pub -1 /move_base/GlobalPlanner/plan nav_msgs/Path "header:
  frame_id: 'map'
poses:
- pose:
    position: {x: 0.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}
- pose:
    position: {x: 10.0, y: 0.0, z: 0.0}
    orientation: {w: 1.0}"

sleep 1

echo ""
echo "📊 监控Robustness值变化："
echo "----------------------------------------"

for i in 1 2 3 4 5; do
    case $i in
        1)
            POS="5.0, 0.0"
            DESC="路径中点"
            ;;
        2)
            POS="5.0, 0.3"
            DESC="偏离路径0.3m"
            ;;
        3)
            POS="5.0, 0.6"
            DESC="偏离路径0.6m"
            ;;
        4)
            POS="5.0, 1.0"
            DESC="偏离路径1.0m"
            ;;
        5)
            POS="5.0, 0.0"
            DESC="回到路径上"
            ;;
    esac

    echo ""
    echo "[$i] 机器人位置: ($POS) - $DESC"

    # 发布机器人位置
    rostopic pub -1 /amcl_pose geometry_msgs/PoseWithCovarianceStamped "header:
  frame_id: 'map'
pose:
  pose:
    position: {x: ${POS%,*}, y: ${POS#*,}, z: 0.0}
    orientation: {w: 1.0}
  covariance: [0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0.01]"

    sleep 2

    # 读取robustness
    ROBUSTNESS=$(rostopic echo -n /stl_monitor/robustness 2>/dev/null | grep "data:" | tail -1 | awk '{print $2}' || echo "N/A")

    if [[ "$ROBUSTNESS" != "N/A" ]]; then
        # 计算距离
        DIST=$(echo "0.5 - $ROBUSTNESS" | bc)
        echo "    Robustness: $ROBUSTNESS"
        echo "    距离路径: ${DIST}m"
    else
        echo "    Robustness: N/A"
    fi
done

echo ""
echo "=========================================="
echo "STL节点日志摘要："
echo "=========================================="
grep -E "Path-Tracking|distance to path|Belief-space" /tmp/stl_verify.log | head -10

# 清理
killall roslaunch rosmaster 2>/dev/null

echo ""
echo "✅ 测试完成"
