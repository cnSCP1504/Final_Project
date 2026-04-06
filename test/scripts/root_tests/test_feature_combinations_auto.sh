#!/bin/bash
# Automated Safe-Regret MPC Feature Combinations Test
# 自动化测试所有功能组合

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Safe-Regret MPC 功能组合自动化测试                             ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

RESULT_FILE="/tmp/feature_test_results_$(date +%Y%m%d_%H%M%S).txt"
echo "测试结果将保存到: $RESULT_FILE"

# 清理旧进程
echo "清理旧进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
sleep 3

# 测试函数
test_configuration() {
    local test_num="$1"
    local test_name="$2"
    local stl_val="$3"
    local dr_val="$4"
    local terminal_val="$5"
    local duration="$6"

    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  测试 $test_num: $test_name"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo "配置: enable_stl=$stl_val, enable_dr=$dr_val, enable_terminal_set=$terminal_val"
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
        debug_mode:=true > /tmp/test_${test_num}.log 2>&1 &
    LAUNCH_PID=$!
    echo "系统已启动 (PID: $LAUNCH_PID)"
    echo ""

    # 等待初始化
    echo "等待系统初始化 ($duration 秒)..."
    sleep $duration

    # 分析日志
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  测试结果                                                     ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""

    # 检查节点
    NODE_COUNT=$(rosnode list 2>/dev/null | grep -E "safe_regret_mpc|tube_mpc" | wc -l)
    echo "✅ 节点数量: $NODE_COUNT"

    # 检查/cmd_vel
    timeout 3 rostopic echo /cmd_vel -n 1 >/dev/null 2>&1 && echo "✅ /cmd_vel 正在发布" || echo "❌ /cmd_vel 未发布"

    # 检查功能特定话题
    if [ "$stl_val" = "true" ]; then
        timeout 3 rostopic echo /stl_monitor/robustness -n 1 >/dev/null 2>&1 && echo "✅ /stl_monitor/robustness 正在发布" || echo "❌ /stl_monitor/robustness 未发布"
    fi

    if [ "$dr_val" = "true" ]; then
        timeout 3 rostopic echo /dr_margins -n 1 >/dev/null 2>&1 && echo "✅ /dr_margins 正在发布" || echo "❌ /dr_margins 未发布"
    fi

    # 统计MPC求解
    MPC_SOLVE_COUNT=$(grep -c "MPC solve" /tmp/test_${test_num}.log 2>/dev/null || echo "0")
    MPC_SUCCESS_COUNT=$(grep -c "MPC solve succeeded\|MPC solved successfully" /tmp/test_${test_num}.log 2>/dev/null || echo "0")
    MPC_FAIL_COUNT=$(grep -c "MPC solve failed" /tmp/test_${test_num}.log 2>/dev/null || echo "0")

    echo "📊 MPC求解统计:"
    echo "   - 总求解次数: $MPC_SOLVE_COUNT"
    echo "   - 成功: $MPC_SUCCESS_COUNT"
    echo "   - 失败: $MPC_FAIL_COUNT"

    # 检查错误
    ERROR_COUNT=$(grep -i "error\|exception" /tmp/test_${test_num}.log 2>/dev/null | grep -v "rostopic\|rosconsole" | wc -l)
    if [ $ERROR_COUNT -gt 0 ]; then
        echo "⚠️  发现 $ERROR_COUNT 个错误/异常"
    else
        echo "✅ 未发现严重错误"
    fi

    # 检查DR/STL特定日志
    if [ "$dr_val" = "true" ]; then
        DR_UPDATE_COUNT=$(grep -c "DR margins updated" /tmp/test_${test_num}.log 2>/dev/null || echo "0")
        echo "📊 DR margins更新: $DR_UPDATE_COUNT 次"
    fi

    if [ "$stl_val" = "true" ]; then
        STL_UPDATE_COUNT=$(grep -c "STL robustness" /tmp/test_${test_num}.log 2>/dev/null || echo "0")
        echo "📊 STL robustness更新: $STL_UPDATE_COUNT 次"
    fi

    # 判断结果
    if [ $ERROR_COUNT -eq 0 ] && [ $MPC_SUCCESS_COUNT -gt 0 ]; then
        RESULT="✅ PASS"
        echo ""
        echo "✅ 测试结果: 通过"
    else
        RESULT="❌ FAIL"
        echo ""
        echo "❌ 测试结果: 失败"
    fi

    # 保存到结果文件
    echo "测试 $test_num: $test_name - $RESULT" >> "$RESULT_FILE"
    echo "  配置: STL=$stl_val, DR=$dr_val, Terminal=$terminal_val" >> "$RESULT_FILE"
    echo "  MPC求解: $MPC_SOLVE_COUNT 次, 成功 $MPC_SUCCESS_COUNT, 失败 $MPC_FAIL_COUNT" >> "$RESULT_FILE"
    echo "  错误数: $ERROR_COUNT" >> "$RESULT_FILE"
    echo "" >> "$RESULT_FILE"

    # 停止系统
    echo "停止系统..."
    kill $LAUNCH_PID 2>/dev/null || true
    sleep 3
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
    sleep 2

    echo "等待5秒后开始下一个测试..."
    sleep 5
}

# 清空结果文件
echo "Safe-Regret MPC 功能组合测试结果" > "$RESULT_FILE"
echo "测试时间: $(date)" >> "$RESULT_FILE"
echo "========================================" >> "$RESULT_FILE"
echo "" >> "$RESULT_FILE"

# ========== 执行所有测试 ==========
test_configuration "1" "基础模式（无额外功能）" "false" "false" "false" "30"
test_configuration "2" "仅开启STL监控" "true" "false" "false" "30"
test_configuration "3" "仅开启DR约束" "false" "true" "false" "30"
test_configuration "4" "仅开启Terminal Set" "false" "false" "true" "30"
test_configuration "5" "STL + DR约束" "true" "true" "false" "30"
test_configuration "6" "DR + Terminal Set" "false" "true" "true" "30"
test_configuration "7" "全部功能（STL + DR + Terminal）" "true" "true" "true" "40"

# ========== 最终总结 ==========
echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  所有测试完成！                                                 ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "详细结果已保存到: $RESULT_FILE"
echo ""
echo "快速总结："
cat "$RESULT_FILE" | grep "测试 [0-9]"
echo ""

# 统计通过率
PASS_COUNT=$(grep -c "✅ PASS" "$RESULT_FILE")
TOTAL_COUNT=$(grep -c "测试 [0-9]" "$RESULT_FILE")
echo "通过率: $PASS_COUNT / $TOTAL_COUNT"

echo ""
echo "测试日志保存在: /tmp/test_*.log"
echo "查看详细日志: cat /tmp/test_X.log"
