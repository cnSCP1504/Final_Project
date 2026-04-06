#!/bin/bash

# 简单测试：检查safe_regret_mpc是否接收tube_mpc/tracking_error

echo "╔════════════════════════════════════════════════════════╗"
echo "║  tracking_error接收测试                                ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# 清理
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2

# 启动系统
echo "📍 启动系统（启用STL/DR，debug模式）..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    debug_mode:=true > /tmp/tracking_error_test.log 2>&1 &

LAUNCH_PID=$!
sleep 20

echo ""
echo "📍 检查话题连接..."
echo ""

# 检查话题
rostopic list 2>/dev/null | grep -E "tracking_error|tube_mpc"
echo ""

# 检查话题信息
echo "📄 /tube_mpc/tracking_error 话题信息："
rostopic info /tube_mpc/tracking_error 2>/dev/null || echo "   话题不存在或无发布者"
echo ""

# 等待数据
echo "📍 监听tracking_error数据（5秒）..."
echo ""

rostopic echo /tube_mpc/tracking_error -n 3 2>/dev/null &
ECHO_PID=$!
sleep 5
kill $ECHO_PID 2>/dev/null

echo ""
echo "📍 搜索日志中的tracking_error相关内容..."
echo ""

# 搜索所有包含tracking_error的日志行
if grep -i "tracking" /tmp/tracking_error_test.log | grep -i "error" | head -10; then
    echo ""
    echo "✅ 找到tracking_error相关日志"
else
    echo "⚠️  未找到tracking_error相关日志"
fi

echo ""
echo "📍 搜索tube_mpc_error相关日志..."
echo ""

if grep -i "tube_mpc.*error" /tmp/tracking_error_test.log | head -10; then
    echo ""
    echo "✅ 找到tube_mpc_error相关日志"
else
    echo "⚠️  未找到tube_mpc_error相关日志"
fi

echo ""
echo "🔧 清理..."
kill $LAUNCH_PID 2>/dev/null
sleep 1
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

echo ""
echo "📄 完整日志: /tmp/tracking_error_test.log"
echo "✅ 完成"
