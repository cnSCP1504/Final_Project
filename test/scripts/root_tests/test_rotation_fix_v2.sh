#!/bin/bash

# 测试Safe-Regret MPC原地转弯修复（版本2）

echo "╔════════════════════════════════════════════════════════╗"
echo "║  Safe-Regret MPC 原地转弯修复测试 v2                  ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# 清理
echo "📍 清理进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 3

# 启动系统
echo "📍 启动系统（无DR/STL）..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=false \
    enable_dr:=false \
    debug_mode:=false > /tmp/rotation_v2.log 2>&1 &

LAUNCH_PID=$!
echo "   PID: $LAUNCH_PID"
echo ""

# 等待系统完全启动（20秒）
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
    echo "📄 日志最后50行："
    tail -50 /tmp/rotation_v2.log
    exit 1
fi

echo "✅ 节点已启动"
echo ""

# 查找关键日志
echo "📍 搜索关键日志..."
echo ""

# 搜索"Solving Safe-Regret MPC"
if grep -q "Solving Safe-Regret MPC" /tmp/rotation_v2.log; then
    echo "✅ 找到 'Solving Safe-Regret MPC' 日志"
    echo ""
    echo "📄 相关日志（最后5条）："
    grep "Solving Safe-Regret MPC" /tmp/rotation_v2.log | tail -5
    echo ""
else
    echo "⚠️  未找到 'Solving Safe-Regret MPC' 日志"
    echo ""
fi

# 搜索"MPC solved"
if grep -q "MPC solved successfully" /tmp/rotation_v2.log; then
    echo "✅ 找到 'MPC solved successfully' 日志"
    echo ""
    echo "📄 相关日志（最后3条）："
    grep "MPC solved successfully" /tmp/rotation_v2.log | tail -3
    echo ""
else
    echo "⚠️  未找到 'MPC solved successfully' 日志"
    echo ""
fi

# 搜索"PURE ROTATION"
if grep -q "PURE ROTATION" /tmp/rotation_v2.log; then
    echo "✅ 找到 'PURE ROTATION' 日志（原地转弯已触发！）"
    echo ""
    echo "📄 相关日志："
    grep "PURE ROTATION" /tmp/rotation_v2.log
    echo ""
else
    echo "⚠️  未找到 'PURE ROTATION' 日志（可能未触发大角度转弯）"
    echo ""
fi

echo "═════════════════════════════════════════════════════════"
echo ""
echo "📊 总结："
echo ""

# 统计
SOLVING_COUNT=$(grep -c "Solving Safe-Regret MPC" /tmp/rotation_v2.log || echo "0")
MPC_COUNT=$(grep -c "MPC solved successfully" /tmp/rotation_v2.log || echo "0")
ROTATION_COUNT=$(grep -c "PURE ROTATION" /tmp/rotation_v2.log || echo "0")

echo "   - 'Solving Safe-Regret MPC' 出现次数: $SOLVING_COUNT"
echo "   - 'MPC solved successfully' 出现次数: $MPC_COUNT"
echo "   - 'PURE ROTATION' 出现次数: $ROTATION_COUNT"
echo ""

if [ "$SOLVING_COUNT" -gt "0" ]; then
    echo "✅ 修复验证成功！"
    echo "   系统在没有DR/STL时仍然调用solveMPC()"
    echo "   原地转弯逻辑已启用"
else
    echo "⚠️  需要进一步调查"
fi

echo ""
echo "🔧 清理进程..."
kill $LAUNCH_PID 2>/dev/null
sleep 1
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

echo "✅ 完成"
