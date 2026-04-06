#!/bin/bash

# 非交互式双目标测试脚本
# 修复：使用距离检查而不是 /move_base/status

echo "=== 启动双目标导航测试 ==="
echo ""
echo "目标1: (3.0, -7.0)"
echo "目标2: (8.0, 0.0) - 大角度转向测试"
echo "到达阈值: 0.5 米（严格模式）"
echo ""
echo "开始测试..."
echo ""

# 发送第一个目标
echo "[目标 1/2] 发送第一个目标 (3.0, -7.0)..."
rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: 3.0
    y: -7.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once

echo "等待到达第一个目标（最多120秒）..."
for i in {1..120}; do
    sleep 1

    # 检查是否到达目标（使用距离检查）
    ODOM_OUTPUT=$(rostopic echo /odom --noarr 2>/dev/null | grep -A 2 "position:" | head -3)
    if [ -n "$ODOM_OUTPUT" ]; then
        # 提取x和y坐标
        X=$(echo "$ODOM_OUTPUT" | grep "x:" | awk '{print $2}')
        Y=$(echo "$ODOM_OUTPUT" | grep "y:" | awk '{print $2}')

        # 计算距离目标的距离
        if [ -n "$X" ] && [ -n "$Y" ]; then
            # 目标是 (3.0, -7.0)
            DX=$(echo "3.0 - $X" | bc)
            DY=$(echo "-7.0 - $Y" | bc)
            DIST=$(echo "sqrt($DX*$DX + $DY*$DY)" | bc)

            # 检查是否到达（距离 < 0.5米，严格模式）
            IS_ARRIVED=$(echo "$DIST < 0.5" | bc)
            if [ "$IS_ARRIVED" -eq 1 ]; then
                echo "✓ 第一个目标已到达！距离: $DIST 米"
                echo ""
                echo "等待2秒后发送第二个目标..."
                sleep 2

                # 发送第二个目标
                echo "[目标 2/2] 发送第二个目标 (8.0, 0.0) - 测试大角度转向..."
                rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
                  frame_id: 'map'
                pose:
                  position:
                    x: 8.0
                    y: 0.0
                    z: 0.0
                  orientation:
                    x: 0.0
                    y: 0.0
                    z: 0.0
                    w: 1.0" --once

                echo ""
                echo "=== 关键测试：观察大角度转向 ==="
                echo "监控etheta值和机器人行为..."
                echo ""

                # 监控60秒
                for j in {1..60}; do
                    echo "[$j/60] 机器人状态:"
                    rostopic echo /odom --noarr 2>/dev/null | grep -A 4 "position:" | head -5
                    echo "etheta值:"
                    tail -1 /tmp/tube_mpc_data.csv 2>/dev/null | awk -F',' '{printf "  Etheta: %8.4f\n", $3}'
                    echo "速度命令:"
                    rostopic echo /cmd_vel --noarr 2>/dev/null | head -6
                    echo ""
                    sleep 2
                done

                echo "=== 测试完成 ==="
                exit 0
            fi
        fi
    fi

    # 每10秒打印状态
    if [ $((i % 10)) -eq 0 ]; then
        echo "  已等待 $i 秒..."
        echo "  当前位置:"
        rostopic echo /odom --noarr 2>/dev/null | grep -A 2 "position:" | head -3
        echo "  路径点数:"
        rostopic echo /move_base/GlobalPlanner/plan --noarr 2>/dev/null | grep "poses:" | head -1
        echo ""
    fi
done

echo "⚠️ 超时：第一个目标未在120秒内到达"
exit 1
