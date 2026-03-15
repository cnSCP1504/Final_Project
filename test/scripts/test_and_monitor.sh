#!/bin/bash

echo "=== 启动机器人并监控/cmd_vel ==="
echo ""

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 启动launch文件
roslaunch tube_mpc_ros tube_mpc_simple.launch > /tmp/launch_test.log 2>&1 &
LAUNCH_PID=$!

echo "Launch PID: $LAUNCH_PID"
echo "等待20秒让系统初始化..."
sleep 20

echo ""
echo "=== 检查系统状态 ==="
rosnode list 2>/dev/null | grep -E "(TubeMPC|move_base)" || echo "节点未找到"

echo ""
echo "=== 检查/cmd_vel话题（最近10条消息）==="
timeout 5 rostopic echo /cmd_vel -n 10 2>/dev/null || echo "无/cmd_vel数据"

echo ""
echo "=== 检查控制台关键输出 ==="
tail -50 /tmp/launch_test.log | grep -E "(Control Loop Status|Current speed|Published Control|MPC feasible|etheta)" | tail -20

echo ""
echo "=== 测试完成 ==="
echo "日志保存在: /tmp/launch_test.log"
echo ""
echo "按Ctrl+C停止所有进程..."

# 保持运行直到用户中断
wait $LAUNCH_PID
