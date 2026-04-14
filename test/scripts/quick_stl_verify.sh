#!/bin/bash
# Quick STL verification - Start system and check topics

echo "=== 快速STL验证 ==="
echo ""

# 清理
echo "1. 清理环境..."
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2
echo "✓ 完成"
echo ""

# 启动系统
echo "2. 启动Safe-Regret MPC系统（STL+DR+Gazebo）..."
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 后台启动
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    use_gazebo:=true \
    enable_visualization:=false \
    debug_mode:=true > /tmp/roslaunch.log 2>&1 &
LAUNCH_PID=$!

echo "✓ Launch启动，PID: $LAUNCH_PID"
echo "日志: /tmp/roslaunch.log"
echo ""

# 等待系统初始化
echo "3. 等待系统初始化（40秒）..."
for i in {40..1}; do
    echo -ne "剩余 $i 秒...\r"
    sleep 1
done
echo -e "\n✓ 初始化时间结束"
echo ""

# 检查节点
echo "4. 检查ROS节点..."
source devel/setup.bash
echo "运行中的节点:"
rosnode list 2>/dev/null | grep -E "stl_monitor|safe_regret|move_base|amcl" | head -10
echo ""

# 检查话题
echo "5. 检查STL话题..."
rostopic list 2>/dev/null | grep stl_monitor
echo ""

# 检查是否有数据
echo "6. 检查STL数据（5秒采样）..."
timeout 5 rostopic echo /stl_monitor/robustness -n 5 2>/dev/null || echo "  ⚠️  暂无数据（可能机器人还未开始移动）"
echo ""

# 检查amcl和plan
echo "7. 检查导航状态..."
if timeout 2 rostopic echo /amcl_pose -n 1 2>/dev/null | grep -q "position:"; then
    echo "  ✓ AMCL定位正常"
else
    echo "  ⚠️  AMCL定位未启动"
fi

if timeout 2 rostopic echo /move_base/GlobalPlanner/plan -n 1 2>/dev/null | grep -q "poses:"; then
    echo "  ✓ Global Planner正常"
else
    echo "  ⚠️  Global Planner未生成路径"
fi
echo ""

# 检查launch日志
echo "8. 检查STL节点日志..."
tail -30 /tmp/roslaunch.log 2>/dev/null | grep -E "stl_monitor|Belief-space|robustness" | tail -10
echo ""

echo "=== 验证完成 ==="
echo ""
echo "如需查看实时数据，在新终端运行:"
echo "  source devel/setup.bash"
echo "  rostopic echo /stl_monitor/robustness"
echo ""
echo "停止测试:"
echo "  kill $LAUNCH_PID"
