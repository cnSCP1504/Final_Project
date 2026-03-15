#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════╗"
echo "║  最终测试 - 路径点要求已降低           ║"
echo "║  从 >=6 改为 >=2                        ║"
echo "╚════════════════════════════════════════╝"
echo ""

roslaunch tube_mpc_ros tube_mpc_simple.launch > /tmp/final_test.log 2>&1 &
sleep 30

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 测试结果"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "1️⃣  路径状态:"
grep "Path computed" /tmp/final_test.log | tail -5

echo ""
echo "2️⃣  目标状态:"
grep "Goal received" /tmp/final_test.log | tail -5

echo ""
echo "3️⃣  /cmd_vel输出:"
timeout 3 rostopic echo /cmd_vel -n 3 2>/dev/null | grep -A2 "linear:" || echo "无数据"

echo ""
echo "4️⃣  路径生成消息:"
grep "path generation" /tmp/final_test.log | tail -5

echo ""
echo "5️⃣  当前速度:"
grep "Current speed:" /tmp/final_test.log | tail -5

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "完整日志: /tmp/final_test.log"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 停止进程
pkill -9 -f "roslaunch|gazebo|rviz" 2>/dev/null
