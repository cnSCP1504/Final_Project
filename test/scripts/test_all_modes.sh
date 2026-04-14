#!/bin/bash
# Test all MPC modes

echo "=========================================="
echo "测试Safe-Regret MPC的所有模式"
echo "=========================================="
echo ""

MODES=("tube_mpc" "stl" "dr" "safe_regret")
RESULTS=()

cd /home/dixon/Final_Project/catkin

for mode in "${MODES[@]}"; do
    echo "=========================================="
    echo "测试模式: $mode"
    echo "=========================================="
    echo ""

    # 清理环境
    echo "1. 清理环境..."
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    sleep 2
    echo "✓ 清理完成"
    echo ""

    # 启动测试（后台运行，60秒超时）
    echo "2. 启动测试（60秒超时）..."
    source devel/setup.bash
    timeout 60 python3 test/scripts/run_automated_test.py \
        --model "$mode" \
        --shelves 1 \
        --headless \
        --timeout 60 > /tmp/test_${mode}.log 2>&1 &
    TEST_PID=$!

    echo "测试PID: $TEST_PID"
    echo "等待60秒..."
    sleep 60

    # 停止测试
    kill $TEST_PID 2>/dev/null
    killall -9 python3 roslaunch rosmaster roscore 2>/dev/null
    sleep 2

    # 检查结果
    echo ""
    echo "3. 检查结果..."

    # 查找最新的测试结果目录
    LATEST_DIR=$(ls -td /home/dixon/Final_Project/catkin/test_results/test_* 2>/dev/null | head -1)

    if [ -n "$LATEST_DIR" ] && [ -d "$LATEST_DIR" ]; then
        # 检查是否有metrics.json
        if [ -f "$LATEST_DIR/metrics.json" ]; then
            echo "✓ 找到测试结果: $LATEST_DIR"

            # 提取基本信息
            python3 << EOF
import json
import sys

try:
    with open('$LATEST_DIR/metrics.json') as f:
        data = json.load(f)

    status = data.get('test_status', 'UNKNOWN')
    goals = len(data.get('goals_reached', []))

    print(f"  状态: {status}")
    print(f"  到达目标: {goals}/2")

    # 检查manuscript metrics
    metrics = data.get('manuscript_metrics', {})
    if metrics:
        print(f"  可行性率: {metrics.get('feasibility_rate', 0)*100:.1f}%")
        print(f"  平均求解时间: {metrics.get('mean_solve_time', 0):.2f} ms")

        raw = data.get('manuscript_raw_data', {})
        stl_data = raw.get('stl_robustness_history', [])
        if stl_data:
            import numpy as np
            unique_vals = len(np.unique(np.round(stl_data, 4)))
            print(f"  STL样本数: {len(stl_data)}")
            print(f"  STL唯一值: {unique_vals}")
            print(f"  STL数据质量: {'✅ 连续变化' if unique_vals > 10 else '❌ 离散值'}")

        dr_data = raw.get('dr_margins_history', [])
        if dr_data:
            print(f"  DR样本数: {len(dr_data)}")

    print("✓ 数据收集正常")

except Exception as e:
    print(f"✗ 解析错误: {e}")
    sys.exit(1)
EOF
            RESULTS+=("PASS")
        else
            echo "✗ 无metrics.json文件"
            RESULTS+=("FAIL")
    else
        echo "✗ 无metrics.json文件"
        RESULTS+=("FAIL")
    else
        echo "✗ 未找到测试结果目录"
        RESULTS+=("FAIL")

    echo ""
    sleep 2
done

# 总结
echo "=========================================="
echo "测试总结"
echo "=========================================="
for i in "${!MODES[@]}"; do
    mode="${MODES[$i]}"
    result="${RESULTS[$i]}"
    status_symbol="✓"
    if [ "$result" != "PASS" ]; then
        status_symbol="✗"
    fi
    echo "$status_symbol $mode: $result"
done
echo ""
