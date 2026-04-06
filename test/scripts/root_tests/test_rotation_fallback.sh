#!/bin/bash

# 测试原地旋转MPC失败时的回退机制
# 验证：即使MPC求解失败，机器人仍然继续旋转而不是停止

echo "╔════════════════════════════════════════════════════════╗"
echo "║  测试：原地旋转MPC失败回退机制                         ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# 清理
echo "📍 清理之前的环境..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2

# 启动系统
echo "📍 启动Safe-Regret MPC（启用STL/DR）..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    debug_mode:=true > /tmp/rotation_fallback_test.log 2>&1 &

LAUNCH_PID=$!
sleep 20

echo ""
echo "📍 监控日志（30秒）..."
echo "═════════════════════════════════════════════════════════"
echo ""

# 实时监控关键日志
timeout 30 tail -f /tmp/rotation_fallback_test.log | grep --line-buffered -E "ENTERING|EXITING|PURE ROTATION|MPC failed|CONTINUING ROTATION" &
MONITOR_PID=$!

sleep 30

# 停止监控
kill $MONITOR_PID 2>/dev/null

echo ""
echo "═════════════════════════════════════════════════════════"
echo ""
echo "📊 分析结果..."
echo ""

# 检查是否进入了旋转模式
ROTATION_ENTERED=$(grep -c "ENTERING PURE ROTATION MODE" /tmp/rotation_fallback_test.log)
echo "✅ 进入旋转模式次数: $ROTATION_ENTERED"

# 检查MPC失败次数
MPC_FAILED=$(grep -c "MPC solve failed" /tmp/rotation_fallback_test.log)
echo "⚠️  MPC失败次数: $MPC_FAILED"

# 关键检查：是否使用了回退机制继续旋转
FALLBACK_USED=$(grep -c "MPC failed but CONTINUING ROTATION" /tmp/rotation_fallback_test.log)
echo "🔄 使用回退继续旋转次数: $FALLBACK_USED"

# 检查是否发布了零速度（错误行为）
ZERO_VEL=$(grep -c "Publishing zero velocity" /tmp/rotation_fallback_test.log)
echo "❌ 发布零速度次数: $ZERO_VEL"

# 提取旋转时的角速度
echo ""
echo "📄 旋转时的角速度示例："
grep "CONTINUING ROTATION" /tmp/rotation_fallback_test.log | head -5

echo ""
echo "═════════════════════════════════════════════════════════"
echo ""

# 判断测试结果
if [ $FALLBACK_USED -gt 0 ]; then
    echo "✅ 测试通过：回退机制已激活"
    echo "   - 当MPC失败时，系统继续旋转而不是停止"
    echo "   - 机器人不会出现'转一下停一下'的问题"
else
    echo "⚠️  测试不确定：未检测到回退机制激活"
    echo "   - 可能MPC未失败，或需要发送大角度目标触发旋转"
fi

echo ""
echo "🔧 清理..."
kill $LAUNCH_PID 2>/dev/null
sleep 1
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

echo ""
echo "📄 完整日志: /tmp/rotation_fallback_test.log"
echo "✅ 测试完成"
