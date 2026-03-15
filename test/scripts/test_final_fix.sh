#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔═══════════════════════════════════════════════════════╗"
echo "║  MPC速度计算修复测试                                  ║"
echo "║  最小速度: 0.5 → 1.0 m/s                             ║"
echo "║  最大throttle: 2.0 → 10.0                            ║"
echo "║  加速度权重: 20 → 10                                  ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

roslaunch tube_mpc_ros tube_mpc_simple.launch > /tmp/final_fix_test.log 2>&1 &
sleep 35

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 测试结果"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "1️⃣  路径和目标状态:"
grep -E "(Path computed|Goal received)" /tmp/final_fix_test.log | tail -6

echo ""
echo "2️⃣  MPC输出 (throttle和速度):"
grep -A3 "MPC Results" /tmp/final_fix_test.log | grep -E "(throttle|ang_vel|Current v)" | tail -15

echo ""
echo "3️⃣  发布的速度命令:"
timeout 3 rostopic echo /cmd_vel -n 5 2>/dev/null | grep -B1 "linear:" | head -20

echo ""
echo "4️⃣  当前计算速度:"
grep "Current speed:" /tmp/final_fix_test.log | tail -10

echo ""
echo "5️⃣  里程计实际速度:"
grep "Current v (odom):" /tmp/final_fix_test.log | tail -10

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "💡 预期结果"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✓ Path computed: YES"
echo "✓ Current speed: >= 1.0 m/s (受最小速度限制)"
echo "✓ /cmd_vel linear.x: >= 1.0 m/s"
echo "✓ 机器人在Gazebo中明显移动"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "完整日志: /tmp/final_fix_test.log"

# 保持运行以便观察
wait
