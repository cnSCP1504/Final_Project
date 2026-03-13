#!/bin/bash

# 快速测试脚本 - 验证机器人不动问题是否已修复
# 使用简化的launch文件

set -e

echo "=== 机器人运动测试 ==="
echo ""

# 检查工作目录
if [ ! -f "devel/setup.bash" ]; then
    echo "❌ 错误: 请在catkin工作空间根目录运行此脚本"
    echo "   cd /home/dixon/Final_Project/catkin"
    exit 1
fi

echo "✅ 1. 环境检查"
echo "   工作目录: $(pwd)"
echo "   ROS版本: $(rosversion -d)"
echo ""

# Source ROS环境
source devel/setup.bash

echo "✅ 2. 准备启动系统"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 即将启动:"
echo "   • Gazebo仿真环境"
echo "   • 机器人模型"
echo "   • AMCL定位"
echo "   • move_base导航"
echo "   • Tube MPC控制器 (保证启动)"
echo "   • 自动导航目标测试"
echo "   • RViz可视化"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📋 观察要点:"
echo "   1. 控制台输出 (应该看到调试信息)"
echo "   2. RViz窗口 (机器人模型和地图)"
echo "   3. Gazebo窗口 (机器人运动)"
echo ""
echo "⏹️  停止: 按 Ctrl+C"
echo ""
read -p "按Enter键继续启动，或Ctrl+C取消..."

echo ""
echo "🎯 启动中..."
echo ""

# 启动简化的launch文件
roslaunch tube_mpc_ros tube_mpc_simple.launch
