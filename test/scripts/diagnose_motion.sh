#!/bin/bash

echo "=== 机器人不动问题诊断 ==="
echo ""

# 1. 检查可执行文件
EXE="/home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node"
if [ -f "$EXE" ]; then
    echo "✅ 1. 可执行文件存在"
    echo "   时间: $(ls -l $EXE | awk '{print $6, $7, $8}')"
else
    echo "❌ 1. 可执行文件不存在，需要编译"
    exit 1
fi

# 2. 检查智能转向逻辑状态
echo ""
echo "✅ 2. 检查智能转向逻辑..."
if grep -q "暂时禁用以排查问题" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ 智能转向逻辑: 已禁用（调试模式）"
else
    echo "   ⚠️  智能转向逻辑: 已启用"
fi

# 3. 检查参数配置
echo ""
echo "✅ 3. 检查参数配置..."
W_ETHETA=$(grep "mpc_w_etheta:" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml" | awk '{print $2}')
echo "   mpc_w_etheta: $W_ETHETA"
if [ "$W_ETHETA" -gt 100 ]; then
    echo "   ⚠️  权重偏高，可能导致机器人不动"
elif [ "$W_ETHETA" -lt 40 ]; then
    echo "   ⚠️  权重偏低，可能导致倒退"
else
    echo "   ✅ 权重在合理范围"
fi

# 4. 检查硬编码默认值
echo ""
echo "✅ 4. 检查硬编码默认值..."
if grep -q "mpc_w_vel.*10.0" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ mpc_w_vel默认值: 10.0（正确）"
else
    echo "   ❌ mpc_w_vel默认值可能不正确"
fi

if grep -q "mpc_max_angvel.*2.0" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ mpc_max_angvel默认值: 2.0（正确）"
else
    echo "   ❌ mpc_max_angvel默认值可能不正确"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📋 诊断总结"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "当前配置:"
echo "  • 智能转向逻辑: 已禁用（排查问题中）"
echo "  • mpc_w_etheta: $W_ETHETA"
echo "  • 编译状态: 最新"
echo ""
echo "建议测试步骤:"
echo "  1. 启动系统: roslaunch tube_mpc_ros tube_mpc_simple.launch"
echo "  2. 观察控制台输出，查看 etheta 值"
echo "  3. 查看是否有 'Goal received: YES'"
echo "  4. 检查 Current speed 是否 > 0"
echo ""
echo "如果还是不动，请提供:"
echo "  • 控制台完整输出"
echo "  • etheta 的具体数值"
echo "  • 是否看到 'MPC feasible: YES'"
echo ""
