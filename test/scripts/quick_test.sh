#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "=== 启动系统测试（最小速度提升到0.3）==="
echo "等待25秒..."
roslaunch tube_mpc_ros tube_mpc_simple.launch > /tmp/quick_test.log 2>&1 &
sleep 25

echo ""
echo "=== /cmd_vel输出 ==="
timeout 3 rostopic echo /cmd_vel -n 5 2>/dev/null || echo "无数据"

echo ""
echo "=== 控制状态 ==="
tail -40 /tmp/quick_test.log | grep -E "(Control Loop|Current speed)" | tail -10

echo ""
echo "=== MPC输出 ==="
grep -E "throttle:|ang_vel:" /tmp/quick_test.log | tail -10

echo ""
echo "测试完成，日志: /tmp/quick_test.log"

# 停止进程
pkill -9 -f "roslaunch|gazebo" 2>/dev/null
