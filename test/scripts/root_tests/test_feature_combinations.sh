#!/bin/bash
# Test Safe-Regret MPC Feature Combinations
# 逐个开启功能，验证系统稳定性

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Safe-Regret MPC 功能组合测试                                  ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# 清理旧进程
echo "清理旧进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
sleep 3

# 测试结果数组
declare -a TEST_NAMES
declare -a TEST_RESULTS
TEST_COUNT=0

# 测试函数
test_configuration() {
    local test_name="$1"
    local stl_val="$2"
    local dr_val="$3"
    local terminal_val="$4"
    local duration="$5"

    TEST_COUNT=$((TEST_COUNT + 1))
    TEST_NAMES[$TEST_COUNT]="$test_name"

    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  测试 $TEST_COUNT: $test_name"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "配置："
    echo "  - enable_stl: $stl_val"
    echo "  - enable_dr: $dr_val"
    echo "  - enable_terminal_set: $terminal_val"
    echo ""

    # 清理
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
    sleep 2

    # 启动系统
    echo "启动系统..."
    roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
        enable_stl:=$stl_val \
        enable_dr:=$dr_val \
        enable_terminal_set:=$terminal_val \
        debug_mode:=true > /tmp/test_${TEST_COUNT}.log 2>&1 &
    LAUNCH_PID=$!

    echo "系统已启动 (PID: $LAUNCH_PID)"
    echo ""

    # 等待初始化
    echo "等待系统初始化 (25秒)..."
    sleep 25

    # 检查关键节点
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  节点状态检查                                                 ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""

    # 检查节点
    echo "1. 关键节点："
    rosnode list 2>/dev/null | grep -E "safe_regret_mpc|tube_mpc|dr_tightening|stl_monitor" || echo "  ⚠️  未找到关键节点"
    echo ""

    # 检查话题
    echo "2. 关键话题："
    rostopic list 2>/dev/null | grep -E "/cmd_vel|/dr_margins|/stl_monitor" | head -10 || echo "  ⚠️  未找到关键话题"
    echo ""

    # 检查话题发布
    echo "3. 话题发布状态："
    timeout 3 rostopic echo /cmd_vel -n 1 >/dev/null 2>&1 && echo "  ✅ /cmd_vel 正在发布" || echo "  ❌ /cmd_vel 未发布"

    if [ "$dr_val" = "true" ]; then
        timeout 3 rostopic echo /dr_margins -n 1 >/dev/null 2>&1 && echo "  ✅ /dr_margins 正在发布" || echo "  ❌ /dr_margins 未发布"
    fi

    if [ "$stl_val" = "true" ]; then
        timeout 3 rostopic echo /stl_monitor/robustness -n 1 >/dev/null 2>&1 && echo "  ✅ /stl_monitor/robustness 正在发布" || echo "  ❌ /stl_monitor/robustness 未发布"
    fi
    echo ""

    # 检查MPC日志
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  MPC求解状态                                                 ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""

    echo "检查MPC求解日志..."
    grep -E "MPC solve|Solving|DR margins|STL robustness|succeeded|failed" /tmp/test_${TEST_COUNT}.log | tail -20
    echo ""

    # 检查错误
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  错误检查                                                     ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""

    ERROR_COUNT=$(grep -i "error\|exception\|failed" /tmp/test_${TEST_COUNT}.log | wc -l)
    if [ $ERROR_COUNT -gt 0 ]; then
        echo "⚠️  发现 $ERROR_COUNT 个错误/异常"
        echo "最近的错误："
        grep -i "error\|exception\|failed" /tmp/test_${TEST_COUNT}.log | tail -5
    else
        echo "✅ 未发现错误"
    fi
    echo ""

    # 判断测试结果
    if [ $ERROR_COUNT -eq 0 ]; then
        TEST_RESULTS[$TEST_COUNT]="✅ PASS"
        echo "✅ 测试结果: 通过"
    else
        TEST_RESULTS[$TEST_COUNT]="❌ FAIL"
        echo "❌ 测试结果: 失败"
    fi
    echo ""

    # 停止系统
    echo "停止系统..."
    kill $LAUNCH_PID 2>/dev/null || true
    sleep 3
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
    sleep 2

    echo "按Enter继续下一个测试..."
    read
}

# ========== 测试序列 ==========

# 测试1: 基础模式（无额外功能）
test_configuration \
    "基础模式（无额外功能）" \
    "false" \
    "false" \
    "false" \
    "30"

# 测试2: 仅开启STL
test_configuration \
    "仅开启STL监控" \
    "true" \
    "false" \
    "false" \
    "30"

# 测试3: 仅开启DR
test_configuration \
    "仅开启DR约束" \
    "false" \
    "true" \
    "false" \
    "30"

# 测试4: 仅开启Terminal Set
test_configuration \
    "仅开启Terminal Set" \
    "false" \
    "false" \
    "true" \
    "30"

# 测试5: STL + DR
test_configuration \
    "STL + DR约束" \
    "true" \
    "true" \
    "false" \
    "30"

# 测试6: DR + Terminal Set
test_configuration \
    "DR + Terminal Set" \
    "false" \
    "true" \
    "true" \
    "30"

# 测试7: 全部开启
test_configuration \
    "全部功能开启（STL + DR + Terminal Set）" \
    "true" \
    "true" \
    "true" \
    "40"

# ========== 测试总结 ==========
echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  测试总结                                                     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

for i in $(seq 1 $TEST_COUNT); do
    echo "$i. ${TEST_NAMES[$i]}: ${TEST_RESULTS[$i]}"
done

echo ""
echo "详细日志保存在: /tmp/test_*.log"
echo ""
echo "测试完成！"
