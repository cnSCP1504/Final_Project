#!/bin/bash
# Simple STL test script

echo "=== STL数据流测试 ==="
echo "启动时间: $(date)"
echo ""

# 清理环境
echo "1. 清理旧进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2
echo "✓ 清理完成"
echo ""

# 启动测试（前台运行，实时输出）
echo "2. 启动测试系统（启用Gazebo）..."
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "启动参数："
echo "  - Model: safe_regret"
echo "  - Shelves: 1"
echo "  - Gazebo: enabled"
echo "  - RViz: disabled"
echo ""

# 启动测试
timeout 240 python3 test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 1 \
    --no-viz \
    --timeout 180

echo ""
echo "=== 测试完成 ==="
echo "结束时间: $(date)"
echo ""
echo "检查结果..."
ls -lth test_results/ | head -5
