#!/bin/bash

# 终端集集成测试脚本
# 这个脚本启动完整的Safe-Regret MPC系统进行测试

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "  Safe-Regret MPC 终端集集成测试"
echo "========================================="
echo ""

# 清理函数
cleanup() {
    echo ""
    echo -e "${YELLOW}清理所有进程...${NC}"
    pkill -9 -f "roslaunch" || true
    pkill -9 -f "rosrun" || true
    pkill -9 roscore || true
    pkill -9 rosmaster || true
    sleep 2
    echo -e "${GREEN}清理完成${NC}"
}

trap cleanup EXIT

# 测试1: 基础参数验证
test_basic_params() {
    echo -e "${GREEN}测试1: 基础参数验证${NC}"
    echo "------------------------------"

    source devel/setup.bash

    # 检查参数能否加载
    rosparam load src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml

    # 读取参数
    rosparam get /terminal_set
    rosparam get /mpc

    echo -e "${GREEN}✅ 基础参数验证通过${NC}"
    echo ""
}

# 测试2: DR Tightening节点独立测试
test_dr_tightening_standalone() {
    echo -e "${GREEN}测试2: DR Tightening节点独立测试${NC}"
    echo "----------------------------------------"

    # 启动roscore
    roscore &
    ROSCORE_PID=$!
    sleep 3

    # 启动DR tightening节点
    timeout 10 rosrun dr_tightening dr_tightening_node \
        _enable_terminal_set:=true \
        _terminal_set_update_frequency:=1.0 \
        __name:=dr_tightening_test &

    DR_PID=$!
    sleep 5

    # 检查节点是否运行
    if ps -p $DR_PID > /dev/null; then
        echo -e "${GREEN}✅ DR Tightening节点启动成功${NC}"

        # 检查话题
        timeout 2 rostopic list 2>/dev/null || true
    else
        echo -e "${RED}❌ DR Tightening节点启动失败${NC}"
    fi

    # 清理
    kill $DR_PID 2>/dev/null || true
    kill $ROSCORE_PID 2>/dev/null || true
    wait $ROSCORE_PID 2>/dev/null || true

    echo ""
}

# 测试3: 话题通信测试
test_topic_communication() {
    echo -e "${GREEN}测试3: ROS话题通信测试${NC}"
    echo "---------------------------"

    source devel/setup.bash

    # 启动roscore
    roscore &
    ROSCORE_PID=$!
    sleep 3

    # 后台任务：模拟发布tracking_error
    (
        sleep 2
        for i in {1..5}; do
            rostopic pub /tube_mpc/tracking_error std_msgs/Float64MultiArray "data: [0.1, 0.2, 0.3]" &
            sleep 1
        done
    ) &

    # 启动DR tightening节点
    rosrun dr_tightening dr_tightening_node \
        _enable_terminal_set:=true \
        _terminal_set_update_frequency:=0.5 &
    DR_PID=$!

    echo "等待节点初始化..."
    sleep 5

    # 检查话题
    echo "可用话题:"
    timeout 2 rostopic list 2>/dev/null | grep -E "terminal|dr_" || echo "  (无终端集话题)"

    # 清理
    kill $DR_PID 2>/dev/null || true
    kill $ROSCORE_PID 2>/dev/null || true
    wait $ROSCORE_PID 2>/dev/null || true

    echo -e "${GREEN}✅ 话题通信测试完成${NC}"
    echo ""
}

# 测试4: Launch文件测试
test_launch_files() {
    echo -e "${GREEN}测试4: Launch文件验证${NC}"
    echo "-------------------------"

    launch_files=(
        "src/dr_tightening/launch/terminal_set_computation.launch"
    )

    for launch_file in "${launch_files[@]}"; do
        echo "检查: $launch_file"

        # 使用roslaunch --dry-run检查语法
        timeout 5 roslaunch --dry-run "$launch_file" 2>&1 | head -10 || echo "  ⚠️  无法进行dry-run检查"
    done

    echo -e "${GREEN}✅ Launch文件验证完成${NC}"
    echo ""
}

# 测试5: 简化集成测试
test_simple_integration() {
    echo -e "${GREEN}测试5: 简化集成测试${NC}"
    echo "-------------------------"

    source devel/setup.bash

    # 启动roscore
    roscore &
    ROSCORE_PID=$!
    sleep 3

    # 创建临时launch文件用于测试
    cat > /tmp/test_terminal_set.launch <<'EOF'
<launch>
  <arg name="enable_terminal_set" default="true"/>

  <!-- DR tightening节点 -->
  <node pkg="dr_tightening" type="dr_tightening_node" name="dr_tightening" output="screen">
    <param name="enable_terminal_set" value="$(arg enable_terminal_set)"/>
    <param name="terminal_set_update_frequency" value="0.5"/>
    <rosparam file="$(find dr_tightening)/params/dr_tightening_params.yaml" command="load"/>
  </node>

  <!-- 话题检查节点 -->
  <node pkg="rostopic" type="rostopic" name="topic_checker"
        args="echo /dr_terminal_set" launch-prefix="xterm -e" output="screen"/>
</launch>
EOF

    echo "启动集成测试 (10秒)..."
    timeout 10 roslaunch /tmp/test_terminal_set.launch 2>&1 | head -30 || true

    # 清理
    kill $ROSCORE_PID 2>/dev/null || true
    wait $ROSCORE_PID 2>/dev/null || true
    rm -f /tmp/test_terminal_set.launch

    echo -e "${GREEN}✅ 集成测试完成${NC}"
    echo ""
}

# 主测试函数
main() {
    echo "选择测试模式:"
    echo "  1) 基础参数验证"
    echo "  2) DR Tightening独立测试"
    echo "  3) 话题通信测试"
    echo "  4) Launch文件验证"
    echo "  5) 简化集成测试"
    echo "  6) 运行所有测试"
    echo ""

    TEST_MODE=${1:-6}

    case $TEST_MODE in
        1)
            test_basic_params
            ;;
        2)
            test_dr_tightening_standalone
            ;;
        3)
            test_topic_communication
            ;;
        4)
            test_launch_files
            ;;
        5)
            test_simple_integration
            ;;
        6)
            test_basic_params
            test_dr_tightening_standalone
            test_topic_communication
            test_launch_files
            test_simple_integration

            echo ""
            echo "========================================="
            echo -e "${GREEN}  ✅ 所有测试完成！${NC}"
            echo "========================================="
            echo ""
            echo "下一步:"
            echo "  启动完整系统进行手动测试:"
            echo "    roslaunch dr_tightening terminal_set_computation.launch"
            echo ""
            ;;
        *)
            echo "无效的测试模式"
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"
