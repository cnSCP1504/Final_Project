#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔══════════════════════════════════════════════╗"
echo "║  测试最小速度提升到 0.5 m/s                   ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

roslaunch tube_mpc_ros tube_mpc_simple.launch > /tmp/test_0.5.log 2>&1 &
sleep 30

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 测试结果"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "1️⃣  路径和目标状态:"
grep -E "(Path computed|Goal received)" /tmp/test_0.5.log | tail -6

echo ""
echo "2️⃣  实际里程计速度 (v):"
grep "Current v (odom)" /tmp/test_0.5.log | tail -5

echo ""
echo "3️⃣  MPC输出的throttle:"
grep "throttle:" /tmp/test_0.5.log | tail -5

echo ""
echo "4️⃣  计算后的速度 (_speed):"
grep "Current speed:" /tmp/test_0.5.log | tail -5

echo ""
echo "5️⃣  发布到/cmd_vel的命令:"
timeout 3 rostopic echo /cmd_vel -n 5 2>/dev/null | grep -B1 -A1 "linear:" | head -15

echo ""
echo "6️⃣  路径点数量:"
grep "path generation" /tmp/test_0.5.log | tail -3

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "💡 如果机器人还是不动，运行直接速度测试："
echo "   python3 /home/dixon/Final_Project/catkin/test_direct_velocity.py"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "完整日志: /tmp/test_0.5.log"

# 保持运行
wait
