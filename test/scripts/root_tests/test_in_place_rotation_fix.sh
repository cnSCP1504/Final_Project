#!/bin/bash

# 测试Safe-Regret MPC原地转弯功能修复
#
# 问题：在集成模式下，当DR/STL关闭时，系统直接转发tube_mpc命令，
#       绕过了safe_regret_mpc的原地转弯逻辑
#
# 修复：修改controlTimerCallback()，即使没有DR/STL也调用solveMPC()

echo "╔════════════════════════════════════════════════════════╗"
echo "║  Safe-Regret MPC 原地转弯功能修复测试                  ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# 步骤1：清理ROS进程
echo "📍 步骤1：清理所有ROS进程..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2
echo "✅ 清理完成"
echo ""

# 步骤2：启动系统（不启用DR/STL）
echo "📍 步骤2：启动Safe-Regret MPC系统（集成模式，无DR/STL）"
echo "   配置：enable_stl:=false, enable_dr:=false"
echo "   预期：原地转弯功能应该生效（修复后）"
echo ""

# 后台启动launch文件
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=false \
    enable_dr:=false \
    debug_mode:=true > /tmp/safe_regret_rotation_test.log 2>&1 &

LAUNCH_PID=$!
echo "✅ 系统启动中（PID: $LAUNCH_PID）"
echo ""

# 等待系统启动
echo "⏳ 等待系统启动（10秒）..."
sleep 10

# 检查节点是否运行
if ! rosnode list | grep -q "safe_regret_mpc"; then
    echo "❌ 错误：safe_regret_mpc节点未启动！"
    echo "📄 查看日志："
    tail -50 /tmp/safe_regret_rotation_test.log
    exit 1
fi

echo "✅ safe_regret_mpc节点已启动"
echo ""

# 步骤3：发送测试目标点（大角度转弯）
echo "📍 步骤3：发送大角度转弯目标点..."
echo "   起点：(-8.0, 0.0) 朝东（0度）"
echo "   终点：(0.0, 8.0)  朝北（90度）"
echo "   角度差：90度 → 应触发原地转弯"
echo ""

# 发送目标点
rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: 0.0
    y: 8.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.707
    w: 0.707" --once

echo "✅ 目标点已发送"
echo ""

# 步骤4：监控日志输出（30秒）
echo "📍 步骤4：监控原地转弯日志（30秒）"
echo "═════════════════════════════════════════════════════════"
echo ""

# 监控日志中的原地转弯标志
for i in {1..30}; do
    # 检查是否有原地转弯日志
    if grep -q "PURE ROTATION" /tmp/safe_regret_rotation_test.log 2>/dev/null; then
        echo "✅ 检测到原地转弯日志！"
        echo ""
        echo "📄 最近的原地转弯日志："
        grep "PURE ROTATION" /tmp/safe_regret_rotation_test.log | tail -5
        echo ""
        echo "═════════════════════════════════════════════════════════"
        echo "✅ 修复成功！原地转弯功能正常工作"
        echo ""
        echo "🔧 清理进程..."
        kill $LAUNCH_PID 2>/dev/null
        killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
        exit 0
    fi

    # 显示进度
    echo -n "⏳ $i/30秒..."
    if [ $((i % 5)) -eq 0 ]; then
        echo ""
    fi

    sleep 1
done

echo ""
echo "═════════════════════════════════════════════════════════"
echo "⚠️  30秒内未检测到原地转弯日志"
echo ""
echo "📄 查看完整日志："
echo "   cat /tmp/safe_regret_rotation_test.log"
echo ""
echo "🔧 清理进程..."
kill $LAUNCH_PID 2>/dev/null
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null

exit 1
