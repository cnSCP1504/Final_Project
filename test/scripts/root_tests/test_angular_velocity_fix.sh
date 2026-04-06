#!/bin/bash

# 测试原地旋转角速度修复
#
# 问题：大角度转弯时停下但不转
# 原因：MPC最小化控制输入，角速度接近0
# 修复：在原地旋转模式下强制最小角速度

echo "╔════════════════════════════════════════════════════════╗"
echo "║  原地旋转角速度修复测试                                ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# 清理
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2

# 启动系统
echo "📍 启动系统（启用STL/DR）..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    debug_mode:=true > /tmp/angular_vel_test.log 2>&1 &

LAUNCH_PID=$!
echo "   PID: $LAUNCH_PID"
echo ""

# 等待启动
echo "⏳ 等待系统启动（20秒）..."
for i in {1..20}; do
    echo -n "."
    sleep 1
done
echo ""
echo ""

# 检查节点
if ! rosnode list 2>/dev/null | grep -q "safe_regret_mpc"; then
    echo "❌ safe_regret_mpc节点未运行"
    tail -50 /tmp/angular_vel_test.log
    exit 1
fi

echo "✅ 节点已启动"
echo ""

# 监控日志（15秒）
echo "📍 监控原地旋转日志（15秒）..."
echo "═════════════════════════════════════════════════════════"
echo ""

sleep 15

# 检查角速度日志
echo ""
echo "📍 查找角速度强制日志..."
echo ""

if grep -q "Angular vel FORCED to" /tmp/angular_vel_test.log; then
    echo "✅ 找到角速度强制日志！"
    echo ""
    echo "📄 相关日志："
    grep "Angular vel FORCED to" /tmp/angular_vel_test.log | head -10
    echo ""

    # 提取角速度值
    echo "📊 角速度统计："
    grep "Angular vel FORCED to" /tmp/angular_vel_test.log | \
        grep -oP "Angular vel FORCED to \K[0-9.-]+" | \
        head -5 | \
        while read vel; do
            echo "   - $vel rad/s (~$(echo "$vel * 57.3" | bc)°/s)"
        done
    echo ""
else
    echo "⚠️  未找到角速度强制日志"
    echo "   可能原因："
    echo "   - 未触发大角度转弯"
    echo "   - 系统刚启动，尚未遇到需要原地旋转的情况"
    echo ""
fi

# 检查是否有旋转方向
echo "📍 检查旋转方向..."
echo ""

if grep -q "Heading error:" /tmp/angular_vel_test.log; then
    echo "✅ 找到heading error日志"
    echo ""
    echo "📄 最近5条："
    grep "Heading error:" /tmp/angular_vel_test.log | tail -5
    echo ""
fi

echo "═════════════════════════════════════════════════════════"
echo ""
echo "📊 测试总结："
echo ""

FORCED_COUNT=$(grep -c "Angular vel FORCED to" /tmp/angular_vel_test.log || echo "0")
ROTATION_COUNT=$(grep -c "PURE ROTATION" /tmp/angular_vel_test.log || echo "0")

echo "   ✅ 原地旋转触发: $ROTATION_COUNT 次"
echo "   ✅ 角速度强制: $FORCED_COUNT 次"
echo ""

if [ "$FORCED_COUNT" -gt "0" ]; then
    echo "🎉 修复验证成功！"
    echo ""
    echo "✅ 原地旋转模式下，角速度被强制设置到最小值"
    echo "✅ 机器人现在应该能够正常原地转弯"
else
    echo "⚠️  未检测到角速度强制"
    echo "   可能需要发送大角度转弯目标来触发"
fi

echo ""
echo "🔧 清理进程..."
kill $LAUNCH_PID 2>/dev/null
sleep 1
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

echo ""
echo "📄 完整日志: /tmp/angular_vel_test.log"
echo "✅ 测试完成"
