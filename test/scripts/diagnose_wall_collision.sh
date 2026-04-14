#!/bin/bash
# 诊断撞墙后系统状态

echo "========================================================================"
echo "撞墙问题诊断工具"
echo "========================================================================"

# 启动safe_regret_mpc_test
echo "1. 启动测试系统..."
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    pickup_x:=-6.5 \
    pickup_y:=-7.0 \
    use_gazebo:=true \
    enable_visualization:=false \
    debug_mode:=true &
LAUNCH_PID=$!

# 等待系统启动
echo "2. 等待系统启动（15秒）..."
sleep 15

# 监控关键话题
echo "3. 开始监控关键话题..."
echo "   按 Ctrl+C 停止监控"

# 监控循环
while true; do
    echo ""
    echo "========================================================================"
    echo "系统状态检查 ($(date '+%Y-%m-%d %H:%M:%S'))"
    echo "========================================================================"

    # 1. 检查/cmd_vel是否有输出
    echo "📊 /cmd_vel 话题状态:"
    timeout 3 rostopic echo /cmd_vel -n 1 2>/dev/null && echo "   ✓ /cmd_vel 正常发布" || echo "   ❌ /cmd_vel 无发布"

    # 2. 检查/mpc_trajectory是否有输出
    echo "📊 /mpc_trajectory 话题状态:"
    timeout 3 rostopic echo /mpc_trajectory -n 1 2>/dev/null && echo "   ✓ /mpc_trajectory 正常发布" || echo "   ❌ /mpc_trajectory 无发布"

    # 3. 检查/global_plan
    echo "📊 /move_base/GlobalPlanner/plan 状态:"
    PLAN_SIZE=$(timeout 3 rostopic echo /move_base/GlobalPlanner/plan --noarr 2>/dev/null | grep -c "position:" || echo "0")
    if [ "$PLAN_SIZE" -gt 0 ]; then
        echo "   ✓ 路径点数: $PLAN_SIZE"
    else
        echo "   ❌ 无路径或路径为空"
    fi

    # 4. 检查/tube_mpc/tracking_error
    echo "📊 /tube_mpc/tracking_error 状态:"
    timeout 3 rostopic echo /tube_mpc/tracking_error -n 1 2>/dev/null && echo "   ✓ tracking_error 正常发布" || echo "   ❌ tracking_error 无发布"

    # 5. 检查机器人位置
    echo "📊 机器人位置:"
    timeout 3 rostopic echo /odom -n 1 2>/dev/null | grep -A 1 "position:" | head -2

    # 6. 检查tube_mpc日志（最近5行）
    echo "📊 tube_mpc 最近日志:"
    rosnode list | grep -q tube_mpc && \
        timeout 3 rqt_console | grep tube_mpc | tail -5 2>/dev/null || echo "   (无法获取)"

    sleep 5
done

# 清理
kill $LAUNCH_PID 2>/dev/null
wait $LAUNCH_PID 2>/dev/null
