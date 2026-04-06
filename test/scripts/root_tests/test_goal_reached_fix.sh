#!/bin/bash
# Test Goal Reached Fix for Safe-Regret MPC
# 测试到达终点后停止的修复

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Safe-Regret MPC Goal Reached 修复测试                          ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# 清理旧进程
echo "清理旧进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
sleep 3

# Source workspace
source devel/setup.bash

echo "启动全功能Safe-Regret MPC..."
echo "配置: enable_stl=true, enable_dr=true, enable_terminal_set=true"
echo ""

# 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    enable_terminal_set:=true \
    enable_visualization:=true \
    debug_mode:=true > /tmp/goal_reached_test.log 2>&1 &

LAUNCH_PID=$!
echo "系统已启动 (PID: $LAUNCH_PID)"
echo ""
echo "等待系统初始化 (30秒)..."
sleep 30

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  测试说明                                                      ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "请按以下步骤测试："
echo ""
echo "1. 在RViz中设置一个导航目标点"
echo "2. 观察机器人导航到目标点"
echo "3. 关键观察：机器人到达目标点后是否停止？"
echo ""
echo "预期行为："
echo "  ✅ 机器人到达目标点（距离 < 0.5m）后完全停止"
echo "  ✅ 控制台显示：'Goal reached! Distance: X.XX m < 0.50 m'"
echo "  ✅ /cmd_vel发布零速度命令"
echo ""
echo "错误行为（已修复）："
echo "  ❌ 机器人到达目标点后继续缓慢前进"
echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  实时监控                                                      ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "监控命令（在另一个终端运行）："
echo ""
echo "  # 查看goal reached日志"
echo "  tail -f /tmp/goal_reached_test.log | grep -i 'goal'"
echo ""
echo "  # 查看cmd_vel输出"
echo "  rostopic echo /cmd_vel"
echo ""
echo "  # 查看机器人位置"
echo "  rostopic echo /odom"
echo ""
echo "按Ctrl+C停止测试..."
echo ""

# 等待用户中断
wait $LAUNCH_PID
