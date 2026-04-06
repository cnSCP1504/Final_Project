#!/bin/bash

# 诊断etheta振荡问题

echo "╔════════════════════════════════════════════════════════╗"
echo "║  etheta振荡诊断                                        ║"
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
    debug_mode:=true > /tmp/etheta_diagnosis.log 2>&1 &

LAUNCH_PID=$!
sleep 20

echo ""
echo "📍 监控etheta值变化（15秒）..."
echo "═════════════════════════════════════════════════════════"
echo ""

sleep 15

echo ""
echo "📍 分析etheta数据..."
echo ""

# 提取etheta值
echo "📄 最近30条etheta日志："
grep -E "Using tube_mpc tracking error.*etheta" /tmp/etheta_diagnosis.log | tail -30
echo ""

# 统计正负值
echo "📊 etheta符号统计："
POS_COUNT=$(grep -E "etheta=[0-9]+" /tmp/etheta_diagnosis.log | wc -l)
NEG_COUNT=$(grep -E "etheta=-" /tmp/etheta_diagnosis.log | wc -l)
echo "   正值次数: $POS_COUNT"
echo "   负值次数: $NEG_COUNT"
echo ""

# 检查是否有符号跳变
echo "🔍 检查符号跳变："
grep -E "Using tube_mpc tracking error.*etheta" /tmp/etheta_diagnosis.log | \
    grep -oP "etheta=\K[0-9.-]+" | \
    awk 'NR>1 {
        if ($1 * prev < 0) {
            print "⚠️  符号跳变: " prev " → " $1
        }
    }
    {prev = $1}'
echo ""

# 检查原地旋转状态
echo "📍 原地旋转状态："
grep -E "ENTERING|EXITING|PURE ROTATION" /tmp/etheta_diagnosis.log | tail -20
echo ""

# 检查角速度
echo "📍 角速度值："
grep -E "Angular vel.*OVERRIDDEN" /tmp/etheta_diagnosis.log | tail -10
echo ""

echo "═════════════════════════════════════════════════════════"
echo ""
echo "🔧 清理..."
kill $LAUNCH_PID 2>/dev/null
sleep 1
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

echo ""
echo "📄 完整日志: /tmp/etheta_diagnosis.log"
echo "✅ 诊断完成"
