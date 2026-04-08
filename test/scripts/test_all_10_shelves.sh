#!/bin/bash
# 测试所有10个取货点（5个南侧 + 5个北侧）

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     Safe-Regret MPC 10个取货点完整测试                         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "测试配置:"
echo "  - 取货点数量: 10个（5个南侧Y=-7 + 5个北侧Y=+7）"
echo "  - 测试模式: $1"
echo "  - 超时时间: 每个取货点240秒"
echo ""
echo "取货点列表:"
echo "  1. shelf_01: (-6.5, -7.0) - 南侧西"
echo "  2. shelf_02: (-3.5, -7.0) - 南侧中"
echo "  3. shelf_03: ( 0.0, -7.0) - 南侧中"
echo "  4. shelf_04: ( 3.5, -7.0) - 南侧东"
echo "  5. shelf_05: ( 6.5, -7.0) - 南侧最东"
echo "  6. shelf_06: (-6.5,  7.0) - 北侧西"
echo "  7. shelf_07: (-3.5,  7.0) - 北侧中"
echo "  8. shelf_08: ( 0.0,  7.0) - 北侧中"
echo "  9. shelf_09: ( 3.5,  7.0) - 北侧东"
echo " 10. shelf_10: ( 6.5,  7.0) - 北侧最东"
echo ""
echo "预计总用时: 约40-60分钟（10个取货点 × 4-6分钟/个）"
echo ""
echo "开始测试..."
echo ""

# 清理环境
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 运行测试
python3 test/scripts/run_automated_test.py \
    --model "$1" \
    --shelves 10 \
    --timeout 240 \
    --visualization

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     测试完成                                                  ║"
echo "╚════════════════════════════════════════════════════════════════╝"
