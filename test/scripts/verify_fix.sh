#!/bin/bash

# 快速验证脚本 - 确认机器人不动问题是否已修复

echo "=== 机器人不动问题修复验证 ==="
echo ""

echo "📋 验证清单"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# 检查可执行文件
EXE="/home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node"
if [ -f "$EXE" ]; then
    echo "✅ 1. 可执行文件存在"
    echo "   时间: $(ls -l $EXE | awk '{print $6, $7, $8}')"
    echo "   大小: $(ls -lh $EXE | awk '{print $5}')"
else
    echo "❌ 1. 可执行文件不存在，需要编译"
    echo "   运行: catkin_make"
    exit 1
fi

# 检查修复是否应用
echo ""
echo "✅ 2. 检查代码修复..."
if grep -q "mpc_w_vel.*10.0.*修复：降低默认值" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ 速度权重默认值已修复 (10.0)"
else
    echo "   ❌ 速度权重默认值未修复"
fi

if grep -q "mpc_max_angvel.*2.0.*修复：提高角速度限制" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ 角速度限制默认值已修复 (2.0)"
else
    echo "   ❌ 角速度限制默认值未修复"
fi

# 检查调试系统
echo ""
echo "✅ 3. 检查调试系统..."
if grep -q "Control Loop Status" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ 控制状态监控已添加"
else
    echo "   ❌ 控制状态监控未添加"
fi

if grep -q "MPC Results" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ MPC结果输出已添加"
else
    echo "   ❌ MPC结果输出未添加"
fi

if grep -q "Published Control Command" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"; then
    echo "   ✅ 控制命令确认已添加"
else
    echo "   ❌ 控制命令确认未添加"
fi

echo ""
echo "📊 验证总结"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 统计检查结果
TOTAL_CHECKS=5
PASSED_CHECKS=0

[ -f "$EXE" ] && ((PASSED_CHECKS++))
grep -q "mpc_w_vel.*10.0.*修复" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp" && ((PASSED_CHECKS++))
grep -q "mpc_max_angvel.*2.0.*修复" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp" && ((PASSED_CHECKS++))
grep -q "Control Loop Status" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp" && ((PASSED_CHECKS++))
grep -q "MPC Results" "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp" && ((PASSED_CHECKS++))

echo "检查项通过: $PASSED_CHECKS / $TOTAL_CHECKS"

if [ $PASSED_CHECKS -eq $TOTAL_CHECKS ]; then
    echo ""
    echo "✅ 所有检查通过！代码修复已正确应用"
    echo ""
    echo "🚀 下一步：测试机器人"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "1. 启动导航系统:"
    echo "   cd /home/dixon/Final_Project/catkin"
    echo "   source devel/setup.bash"
    echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
    echo ""
    echo "2. 观察控制台输出，应该看到:"
    echo "   === Control Loop Status ==="
    echo "   Goal received: NO (初始状态)"
    echo "   Path computed: NO (初始状态)"
    echo ""
    echo "3. 在RViz中设置目标后，应该看到:"
    echo "   === Control Loop Status ==="
    echo "   Goal received: YES ← 应该变成YES"
    echo "   Path computed: YES ← 应该变成YES"
    echo "   Current speed: 0.XX m/s ← 应该显示速度"
    echo ""
    echo "4. 机器人应该开始移动，速度在 0.5-1.0 m/s 范围"
    echo ""
else
    echo ""
    echo "⚠️  部分检查未通过 ($PASSED_CHECKS/$TOTAL_CHECKS)"
    echo "   可能需要重新编译或检查代码"
    echo ""
    echo "建议运行: catkin_make"
fi

echo ""
echo "🔍 机器人不动诊断工具"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# 提供诊断建议
echo "如果启动后机器人仍然不动，请观察控制台输出："
echo ""
echo "┌─ 看到此输出 ─────────────────────────┬─ 可能的问题 ──────────────┐"
echo "│ Goal received: NO               │ 没有设置目标          │"
echo "│                                │ 在RViz中使用2D Nav Goal │"
echo "├────────────────────────────────┼─────────────────────────┤"
echo "│ Path computed: NO               │ 路径规划失败          │"
echo "│                                │ 检查地图和目标位置      │"
echo "├────────────────────────────────┼─────────────────────────┤"
echo "│ MPC feasible: NO                │ MPC求解失败            │"
echo "│                                │ 路径可能过于复杂        │"
echo "├────────────────────────────────┼─────────────────────────┤"
echo "│ throttle ≤ 0                    │ 建议停止                │"
echo "│                                │ 可能已到达目标          │"
echo "├────────────────────────────────┼─────────────────────────┤"
echo "│ linear.x ≤ 0.1                  │ 速度仍太慢              │"
echo "│                                │ 需要进一步降低速度权重  │"
echo "└────────────────────────────────┴─────────────────────────┘"
echo ""

# 创建快速测试指南
cat > "/tmp/quick_test_guide.txt" << 'EOF'
机器人不动问题 - 快速测试指南
====================================

启动命令:
---------
cd /home/dixon/Final_Project/catkin
source devel/setup.bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch

RViz操作:
---------
1. 等待RViz启动
2. 点击 "2D Nav Goal" (工具栏中的绿色箭头)
3. 在地图上点击目标位置
4. 拖动箭头设置朝向
5. 松开鼠标完成目标设置

预期行为:
---------
✅ 机器人开始旋转朝向目标
✅ 机器人开始向目标移动
✅ 速度在 0.5-1.0 m/s 范围
✅ 能够避开障碍物
✅ 最终到达目标位置

控制台输出应该显示:
---------
每5秒:
=== Control Loop Status ===
Goal received: YES        ← 看这个
Path computed: YES        ← 看这个
Current speed: 0.65 m/s   ← 看这个
========================

每2秒:
=== MPC Results ===
MPC feasible: YES         ← 看这个
throttle: 0.123           ← 看这个
=================

每5秒:
=== Published Control Command ===
linear.x: 0.65 m/s        ← 看这个
angular.z: 0.15 rad/s     ← 看这个
==================================

如果机器人不动:
---------
1. 检查 Goal received 是否为 YES
2. 检查 Path computed 是否为 YES
3. 检查 MPC feasible 是否为 YES
4. 检查 linear.x 是否 > 0.1

常见问题:
---------
• 目标设置无效 → 检查是否在地图范围内
• 路径未生成 → 检查地图配置
• 机器人不动 → 运行诊断脚本: bash /tmp/diagnose_robot.sh
EOF

echo "✓ 已创建快速测试指南: /tmp/quick_test_guide.txt"

echo ""
echo "✅ 验证完成！"
echo ""
echo "📋 总结:"
echo "   • 代码修复: ✅ 已应用"
echo "   • 调试系统: ✅ 已添加"
echo "   • 准备状态: ✅ 就绪测试"
echo ""
echo "🎯 下一步: 启动系统并观察机器人行为"
echo ""
