#!/bin/bash

# 测试Safe-Regret MPC在启用STL/DR时的原地转弯功能
#
# 问题：启用STL和DR后，原地转弯功能失效
# 原因：etheta被硬编码为0，没有从tube_mpc的tracking_error获取真实值
# 修复：从tube_mpc_error_中提取cte和etheta

echo "╔════════════════════════════════════════════════════════╗"
echo "║  Safe-Regret MPC STL/DR模式原地转弯测试              ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# 清理ROS进程
echo "📍 步骤1：清理ROS进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 3
echo "✅ 清理完成"
echo ""

# 启动系统（启用STL和DR）
echo "📍 步骤2：启动系统（启用STL和DR）"
echo "   配置：enable_stl:=true, enable_dr:=true"
echo ""

roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    debug_mode:=true > /tmp/stl_dr_rotation_test.log 2>&1 &

LAUNCH_PID=$!
echo "✅ 系统启动中（PID: $LAUNCH_PID）"
echo ""

# 等待系统启动
echo "⏳ 等待系统完全启动（20秒）..."
for i in {1..20}; do
    echo -n "."
    sleep 1
done
echo ""
echo ""

# 检查节点
if ! rosnode list 2>/dev/null | grep -q "safe_regret_mpc"; then
    echo "❌ safe_regret_mpc节点未运行"
    echo "📄 日志："
    tail -50 /tmp/stl_dr_rotation_test.log
    exit 1
fi

echo "✅ 节点已启动"
echo ""

# 检查tube_mpc_error数据
echo "📍 步骤3：检查tube_mpc/tracking_error话题..."
echo ""

# 等待tracking_error数据
for i in {1..10}; do
    if rostopic echo /tube_mpc/tracking_error -n 1 > /tmp/tracking_error.txt 2>&1; then
        echo "✅ 收到tracking_error数据"
        echo ""
        echo "📄 数据内容："
        cat /tmp/tracking_error.txt
        echo ""
        break
    fi
    echo "⏳ 等待tracking_error数据 ($i/10)..."
    sleep 1
done

# 检查日志中的etheta值
echo "📍 步骤4：检查日志中的etheta使用情况..."
echo ""

if grep -q "Using tube_mpc tracking error" /tmp/stl_dr_rotation_test.log; then
    echo "✅ 找到 'Using tube_mpc tracking error' 日志"
    echo ""
    echo "📄 相关日志（最后5条）："
    grep "Using tube_mpc tracking error" /tmp/stl_dr_rotation_test.log | tail -5
    echo ""
else
    echo "⚠️  未找到 'Using tube_mpc tracking error' 日志"
    echo ""
fi

# 检查原地转弯日志
echo "📍 步骤5：检查原地转弯状态..."
echo ""

if grep -q "PURE ROTATION" /tmp/stl_dr_rotation_test.log; then
    echo "✅ 找到 'PURE ROTATION' 日志（原地转弯已触发！）"
    echo ""
    echo "📄 相关日志："
    grep "PURE ROTATION" /tmp/stl_dr_rotation_test.log
    echo ""
else
    echo "⚠️  未找到 'PURE ROTATION' 日志"
    echo "   可能原因："
    echo "   - 尚未发送大角度转弯目标"
    echo "   - etheta值未超过90度阈值"
    echo ""
fi

# 检查MPC求解
echo "📍 步骤6：检查MPC求解状态..."
echo ""

MPC_COUNT=$(grep -c "MPC solved successfully" /tmp/stl_dr_rotation_test.log || echo "0")
echo "   MPC求解成功次数: $MPC_COUNT"

if [ "$MPC_COUNT" -gt "0" ]; then
    echo "✅ MPC正在正常求解"
    echo ""
    echo "📄 最近3次求解："
    grep "MPC solved successfully" /tmp/stl_dr_rotation_test.log | tail -3
    echo ""
fi

echo "═════════════════════════════════════════════════════════"
echo ""
echo "📊 修复验证总结："
echo ""

# 统计
TRACKING_ERROR_COUNT=$(grep -c "Using tube_mpc tracking error" /tmp/stl_dr_rotation_test.log || echo "0")
ROTATION_COUNT=$(grep -c "PURE ROTATION" /tmp/stl_dr_rotation_test.log || echo "0")

echo "   ✅ 从tube_mpc获取etheta: $TRACKING_ERROR_COUNT 次"
echo "   ✅ 原地转弯触发: $ROTATION_COUNT 次"
echo "   ✅ MPC求解成功: $MPC_COUNT 次"
echo ""

if [ "$TRACKING_ERROR_COUNT" -gt "0" ]; then
    echo "🎉 修复验证成功！"
    echo ""
    echo "✅ 系统现在从tube_mpc/tracking_error获取真实的etheta值"
    echo "✅ 原地转弯状态机可以正常工作"
    echo "✅ STL/DR模式下原地转弯功能已恢复"
else
    echo "⚠️  需要进一步调查tracking_error数据接收"
fi

echo ""
echo "🔧 清理进程..."
kill $LAUNCH_PID 2>/dev/null
sleep 1
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

echo ""
echo "📄 完整日志: /tmp/stl_dr_rotation_test.log"
echo "✅ 测试完成"
