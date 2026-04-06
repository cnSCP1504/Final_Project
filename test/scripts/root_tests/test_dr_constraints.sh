#!/bin/bash
# Test DR Constraints Implementation

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Test: DR Constraints Implementation                         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "配置："
echo "  - enable_safe_regret_integration: true"
echo "  - enable_stl: true"
echo "  - enable_dr: true (CRITICAL: Testing DR constraints)"
echo "  - enable_terminal_set: false"
echo ""

# 启动系统
echo "启动系统..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    enable_terminal_set:=false \
    debug_mode:=true &

LAUNCH_PID=$!
echo "系统已启动 (PID: $LAUNCH_PID)"
echo ""

echo "等待系统初始化 (20秒)..."
sleep 20

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  检查DR约束是否工作                                           ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# 检查关键日志
echo "1. 检查MPC日志中的DR约束信息："
timeout 5 rostopic echo /rosout | grep -i "dr\|constraint\|margin" | head -20 || echo "  ⚠️  未找到DR相关日志"
echo ""

echo "2. 检查DR margins是否发布："
timeout 3 rostopic echo /dr_margins -n 1 >/dev/null 2>&1 && echo "  ✅ /dr_margins 正在发布" || echo "  ❌ /dr_margins 未发布"
echo ""

echo "3. 检查STL robustness是否发布："
timeout 3 rostopic echo /stl_monitor/robustness -n 1 >/dev/null 2>&1 && echo "  ✅ /stl_monitor/robustness 正在发布" || echo "  ❌ /stl_monitor/robustness 未发布"
echo ""

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  监控MPC求解过程                                               ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "监控以下关键输出："
echo "  - 'DR margins updated' - DR margins已传递给solver"
echo "  - 'STL robustness' - STL鲁棒性已传递给solver"
echo "  - 'MPC solve succeeded' - MPC求解成功"
echo "  - 'DR constraint' - DR约束被评估"
echo ""

# 监控日志30秒
echo "监控日志 (30秒)..."
timeout 30 rostopic echo /rosout --noarr --filter "m.msg =~ 'DR|STL|MPC|constraint|margin|robustness|budget'" | head -50

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
