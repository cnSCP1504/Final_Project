#!/bin/bash

# 智能转向功能测试脚本

echo "=== 智能转向策略测试 ==="
echo ""
echo "🎯 测试目标: 验证机器人在大航向误差时优先转弯而非倒退"
echo ""

# 检查工作目录
if [ ! -f "devel/setup.bash" ]; then
    echo "❌ 错误: 请在catkin工作空间根目录运行此脚本"
    exit 1
fi

# 检查修复是否已编译
if [ ! -f "devel/lib/tube_mpc_ros/tube_TubeMPC_Node" ]; then
    echo "❌ 错误: 可执行文件不存在，请先运行 catkin_make"
    exit 1
fi

# 检查智能转向逻辑是否已编译
if grep -q "HEADING_ERROR_THRESHOLD" "src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "✅ 智能转向逻辑已实现"
    echo "   - 航向误差阈值: 60° (1.05 rad)"
    echo "   - 临界阈值: 90° (1.57 rad)"
else
    echo "❌ 错误: 智能转向逻辑未找到"
    exit 1
fi

# 检查参数优化
if grep -q "mpc_w_etheta: 200.0" "src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml"; then
    echo "✅ MPC权重已优化 (mpc_w_etheta: 200.0)"
else
    echo "⚠️  警告: MPC权重可能未优化"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📋 测试场景"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "场景1: 90度转弯测试"
echo "  操作: 在RViz中设置机器人侧面90度的目标"
echo "  预期: 机器人先原地旋转，然后前进"
echo "  观察: 控制台应显示 'CRITICAL HEADING ERROR: Pure rotation mode'"
echo ""

echo "场景2: 180度掉头测试"
echo "  操作: 设置机器人正后方的目标"
echo "  预期: 机器人原地旋转180度，然后前进"
echo "  观察: 机器人不应该倒退"
echo ""

echo "场景3: 45度小角度测试"
echo "  操作: 设置机器人前方45度的目标"
echo "  预期: 机器人边前进边转向（转向优先模式）"
echo "  观察: 控制台应显示 'High heading error: Reduced speed'"
echo ""

echo "场景4: 正常跟踪测试"
echo "  操作: 设置机器人前方小角度目标"
echo "  预期: 机器人正常MPC控制，无明显减速"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔍 观察要点"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "控制台输出（每5秒）:"
echo "  → 'etheta: X.XX rad (XX.X deg)' - 航向误差"
echo "  → 警告信息（如果触发）:"
echo "     • 'CRITICAL HEADING ERROR: Pure rotation mode' (>90°)"
echo "     • 'High heading error: Reduced speed' (60-90°)"
echo ""

echo "机器人行为:"
echo "  → 大角度误差（>90°）: 原地旋转，速度≈0"
echo "  → 中等角度（60-90°）: 减速转向，速度≈20%"
echo "  → 小角度（<60°）: 正常运动"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 准备启动"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

read -p "按Enter键启动系统，或Ctrl+C取消..."

echo ""
echo "🎯 启动中..."
echo ""

source devel/setup.bash
roslaunch tube_mpc_ros tube_mpc_simple.launch
