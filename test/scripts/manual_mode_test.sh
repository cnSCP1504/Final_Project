#!/bin/bash
# Manual test for each MPC mode

echo "=========================================="
echo "手动测试Safe-Regret MPC的所有模式"
echo "=========================================="
echo ""

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 测试每个模式
MODES=(
    "tube_mpc:enable_stl:=false enable_dr:=false"
    "stl:enable_stl:=true enable_dr:=false"
    "dr:enable_stl:=false enable_dr:=true"
    "safe_regret:enable_stl:=true enable_dr:=true enable_reference_planner:=true"
)

for mode_config in "${MODES[@]}"; do
    mode_name=$(echo $mode_config | cut -d: -f1)
    params=$(echo $mode_config | cut -d: -f2-)

    echo "=========================================="
    echo "测试模式: $mode_name"
    echo "参数: $params"
    echo "=========================================="
    echo ""

    # 清理
    echo "1. 清理环境..."
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    sleep 2
    echo "✓ 完成"
    echo ""

    # 启动系统（后台）
    echo "2. 启动系统（后台）..."
    roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
        $params \
        use_gazebo:=true \
        enable_visualization:=false > /tmp/${mode_name}_test.log 2>&1 &
    LAUNCH_PID=$!

    echo "✓ 启动PID: $LAUNCH_PID"
    echo "日志: /tmp/${mode_name}_test.log"
    echo ""

    # 等待系统初始化
    echo "3. 等待Gazebo初始化（45秒）..."
    for i in {45..1}; do
        echo -ne "剩余 $i 秒...\r"
        sleep 1
    done
    echo -e "\n✓ 初始化完成"
    echo ""

    # 检查节点
    echo "4. 检查ROS节点..."
    source devel/setup.bash
    echo "运行中的关键节点:"
    rosnode list 2>/dev/null | grep -E "tube_mpc|stl_monitor|dr_tightening|safe_regret" | head -10
    echo ""

    # 检查话题
    echo "5. 检查话题..."
    rosnode list 2>/dev/null | grep -E "stl_monitor|dr_margins|metrics" | wc -l
    echo "个STL/DR相关节点"
    echo ""

    # 根据模式检查特定话题
    case $mode_name in
        "tube_mpc")
            echo "  Tube MPC模式: 无STL/DR话题（预期）"
            ;;
        "stl")
            echo "  STL模式: 检查STL话题..."
            timeout 3 rostopic echo /stl_monitor/robustness -n 3 2>/dev/null | head -10 || echo "    暂无数据"
            ;;
        "dr")
            echo "  DR模式: 检查DR话题..."
            timeout 3 rostopic echo /dr_margins -n 1 2>/dev/null | head -10 || echo "    暂无数据"
            ;;
        "safe_regret")
            echo "  Safe-Regret模式: 检查STL和DR话题..."
            timeout 3 rostopic echo /stl_monitor/robustness -n 3 2>/dev/null | head -10 || echo "    暂无数据"
            timeout 3 rostopic echo /dr_margins -n 1 2>/dev/null | head -10 || echo "    暂无数据"
            ;;
    esac
    echo ""

    # 停止测试
    echo "6. 停止测试..."
    kill $LAUNCH_PID 2>/dev/null
    killall -9 roslaunch rosmaster roscore gzserver 2>/dev/null
    sleep 2
    echo "✓ 完成"
    echo ""
    echo ""
    sleep 2
done

echo "=========================================="
echo "测试完成"
echo "=========================================="
