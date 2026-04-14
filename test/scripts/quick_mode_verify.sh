#!/bin/bash
# Quick verification of all modes - check node startup and topic publishing

echo "=========================================="
echo "快速验证所有MPC模式"
echo "=========================================="
echo ""

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 测试配置
declare -A MODE_CONFIG
MODE_CONFIG[tube_mpc]="enable_stl:=false enable_dr:=false"
MODE_CONFIG[stl]="enable_stl:=true enable_dr:=false"
MODE_CONFIG[dr]="enable_stl:=false enable_dr:=true"
MODE_CONFIG[safe_regret]="enable_stl:=true enable_dr:=true"

MODES=("tube_mpc" "stl" "dr" "safe_regret")

for mode in "${MODES[@]}"; do
    echo "=========================================="
    echo "模式: $mode"
    echo "=========================================="

    # 清理
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    sleep 1

    # 启动
    echo "启动系统..."
    roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
        ${MODE_CONFIG[$mode]} \
        use_gazebo:=true \
        enable_visualization:=false > /tmp/${mode}_verify.log 2>&1 &
    LAUNCH_PID=$!

    echo "PID: $LAUNCH_PID"
    echo "等待35秒..."
    sleep 35

    # 检查节点
    echo "检查节点..."
    source devel/setup.bash
    NODE_COUNT=$(rosnode list 2>/dev/null | wc -l)
    echo "  ROS节点数: $NODE_COUNT"

    # 检查关键节点
    if rosnode list 2>/dev/null | grep -q "/tube_mpc"; then
        echo "  ✓ tube_mpc节点运行中"
    else
        echo "  ✗ tube_mpc节点未运行"
    fi

    case $mode in
        "tube_mpc")
            if rosnode list 2>/dev/null | grep -q "/stl_monitor"; then
                echo "  ⚠️  stl_monitor节点运行（不应该在tube_mpc模式）"
            else
                echo "  ✓ stl_monitor未运行（正确）"
            fi
            ;;
        "stl")
            if rosnode list 2>/dev/null | grep -q "/stl_monitor"; then
                echo "  ✓ stl_monitor节点运行中"
                # 检查话题
                if timeout 2 rostopic echo /stl_monitor/robustness -n 1 2>/dev/null | grep -q "data:"; then
                    echo "  ✓ /stl_monitor/robustness 发布数据"
                else
                    echo "  ⚠️  /stl_monitor/robustness 暂无数据（可能机器人未移动）"
                fi
            else
                echo "  ✗ stl_monitor节点未运行"
            fi
            ;;
        "dr")
            if rosnode list 2>/dev/null | grep -q "/dr_tightening"; then
                echo "  ✓ dr_tightening节点运行中"
                # 检查话题
                if timeout 2 rostopic echo /dr_margins -n 1 2>/dev/null | grep -q "data:"; then
                    echo "  ✓ /dr_margins 发布数据"
                else
                    echo "  ⚠️  /dr_margins 暂无数据"
                fi
            else
                echo "  ✗ dr_tightening节点未运行"
            fi
            ;;
        "safe_regret")
            if rosnode list 2>/dev/null | grep -q "/stl_monitor"; then
                echo "  ✓ stl_monitor节点运行中"
            else
                echo "  ✗ stl_monitor节点未运行"
            fi
            if rosnode list 2>/dev/null | grep -q "/dr_tightening"; then
                echo "  ✓ dr_tightening节点运行中"
            else
                echo "  ✗ dr_tightening节点未运行"
            fi
            ;;
    esac

    # 停止
    kill $LAUNCH_PID 2>/dev/null
    killall -9 roslaunch rosmaster roscore gzserver python 2>/dev/null
    sleep 2
    echo ""
    echo ""
done

echo "=========================================="
echo "验证完成"
echo "=========================================="
