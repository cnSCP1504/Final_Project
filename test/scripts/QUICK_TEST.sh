#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔═══════════════════════════════════════════════════════╗"
echo "║  关键修复：速度计算公式                               ║"
echo "║  修复前: _speed = v + throttle * dt                   ║"
echo "║         = 0 + 0.297 * 0.1 = 0.0297 m/s ❌             ║"
echo "║                                                        ║"
echo "║  修复后: _speed = v + throttle                        ║"
echo "║         = 0 + 0.297 = 0.297 m/s ✅                   ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

roslaunch tube_mpc_ros tube_mpc_simple.launch > /tmp/quick_test.log 2>&1 &
sleep 30

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 测试结果"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "📍 系统状态:"
grep -E "(Path computed|Goal received)" /tmp/quick_test.log | tail -6

echo ""
echo "🔧 MPC输出:"
grep -A4 "MPC Results" /tmp/quick_test.log | grep -E "(throttle|Current v|Computed speed)" | tail -20

echo ""
echo "📤 发布的速度命令 (/cmd_vel):"
timeout 3 rostopic echo /cmd_vel -n 3 2>/dev/null | grep -B1 -A2 "linear:" | head -20

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "💡 预期结果"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✓ Computed speed: ~0.3 m/s (throttle直接使用)"
echo "✓ /cmd_vel linear.x: 0.3 m/s"
echo "✓ 机器人在Gazebo中以0.3 m/s移动"
echo ""
echo "如果还是太慢，可以："
echo "  1. 提高mpc_max_throttle (当前10.0)"
echo "  2. 提高最小速度阈值 (当前0.3)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

wait
