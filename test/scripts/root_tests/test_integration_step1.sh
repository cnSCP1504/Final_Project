#!/bin/bash
# Safe-Regret MPC Integration Test - Step 1: Basic Integration Mode

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Test 1: Basic Integration Mode (无额外功能)                    ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "配置："
echo "  - enable_safe_regret_integration: true"
echo "  - enable_stl: false"
echo "  - enable_dr: false"
echo "  - enable_terminal_set: false"
echo ""

# 清理旧进程
echo "清理旧进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
sleep 2

# 启动系统
echo "启动系统..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch &
LAUNCH_PID=$!

echo "系统已启动 (PID: $LAUNCH_PID)"
echo ""
echo "等待系统初始化 (15秒)..."
sleep 15

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  系统状态检查                                                 ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# 检查关键节点
echo "1. 检查关键节点："
rosnode list | grep -E "tube_mpc|safe_regret_mpc" || echo "  ⚠️  未找到关键节点"
echo ""

# 检查关键话题
echo "2. 检查关键话题："
rostopic list | grep -E "/cmd_vel|/mpc_trajectory|/tube_boundaries" || echo "  ⚠️  未找到关键话题"
echo ""

# 检查话题发布
echo "3. 检查话题发布："
echo "  - /cmd_vel_raw (tube_mpc → safe_regret_mpc):"
timeout 3 rostopic echo /cmd_vel_raw -n 1 >/dev/null 2>&1 && echo "    ✅ 正在发布" || echo "    ❌ 未发布"

echo "  - /cmd_vel (safe_regret_mpc → robot):"
timeout 3 rostopic echo /cmd_vel -n 1 >/dev/null 2>&1 && echo "    ✅ 正在发布" || echo "    ❌ 未发布"

echo "  - /mpc_trajectory:"
timeout 3 rostopic echo /mpc_trajectory -n 1 >/dev/null 2>&1 && echo "    ✅ 正在发布" || echo "    ❌ 未发布"
echo ""

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  测试完成                                                     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "按 Ctrl+C 停止测试"
echo "或者运行: kill $LAUNCH_PID"
echo ""

# 等待用户中断
wait $LAUNCH_PID
