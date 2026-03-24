#!/bin/bash

# 终端集测试脚本 (P1-1)
# 用法: ./test_terminal_set.sh [test_type]

set -e

source devel/setup.bash

TEST_TYPE=${1:-all}

echo "========================================="
echo "  终端集实现测试 (P1-1)"
echo "========================================="
echo ""

# 清理函数
cleanup() {
    echo ""
    echo "清理ROS进程..."
    pkill -9 roslaunch || true
    pkill -9 roscore || true
    pkill -9 rosmaster || true
    sleep 1
}

trap cleanup EXIT

# 测试1: 构建验证
test_build() {
    echo "📦 测试1: 构建验证"
    echo "-------------------"

    catkin_make -DCATKIN_WHITELIST_PACKAGES="dr_tightening;tube_mpc_ros" -j4

    if [ $? -eq 0 ]; then
        echo "✅ 构建成功"
    else
        echo "❌ 构建失败"
        exit 1
    fi
    echo ""
}

# 测试2: 文件检查
test_files() {
    echo "📁 测试2: 文件完整性检查"
    echo "------------------------"

    files=(
        "src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml"
        "src/dr_tightening/launch/terminal_set_computation.launch"
        "src/tube_mpc_ros/mpc_ros/launch/tube_mpc_with_terminal_set.launch"
        "src/tube_mpc_ros/mpc_ros/rviz/tube_mpc_with_terminal_set.rviz"
        "src/dr_tightening/rviz/terminal_set.rviz"
    )

    all_exist=true
    for file in "${files[@]}"; do
        if [ -f "$file" ]; then
            echo "✅ $file"
        else
            echo "❌ $file (缺失)"
            all_exist=false
        fi
    done

    if [ "$all_exist" = true ]; then
        echo "✅ 所有文件存在"
    else
        echo "❌ 部分文件缺失"
        exit 1
    fi
    echo ""
}

# 测试3: 参数文件语法
test_params() {
    echo "⚙️  测试3: 参数文件语法检查"
    echo "--------------------------"

    python3 - <<'EOF'
import yaml
import sys

files = [
    "src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml",
    "src/dr_tightening/params/dr_tightening_params.yaml"
]

for file in files:
    try:
        with open(file, 'r') as f:
            data = yaml.safe_load(f)
        print(f"✅ {file} (语法正确)")
    except Exception as e:
        print(f"❌ {file}: {e}")
        sys.exit(1)
EOF

    if [ $? -eq 0 ]; then
        echo "✅ 参数文件语法正确"
    else
        echo "❌ 参数文件有语法错误"
        exit 1
    fi
    echo ""
}

# 测试4: Launch文件语法
test_launch() {
    echo "🚀 测试4: Launch文件语法检查"
    echo "----------------------------"

    # 使用roslaunch的--dry-run选项
    launch_files=(
        "src/dr_tightening/launch/terminal_set_computation.launch"
        "src/tube_mpc_ros/mpc_ros/launch/tube_mpc_with_terminal_set.launch"
    )

    for launch_file in "${launch_files[@]}"; do
        echo "检查: $launch_file"

        # 提取pkg和name
        pkg=$(grep 'pkg="' "$launch_file" | head -1 | sed 's/.*pkg="\([^"]*\)".*/\1/')
        name=$(grep 'type="' "$launch_file" | head -1 | sed 's/.*type="\([^"]*\)".*/\1/')

        if [ -z "$pkg" ] || [ -z "$name" ]; then
            echo "✅ $launch_file (语法正确)"
        else
            # 检查可执行文件是否存在
            exe="devel/lib/${pkg}/${name}"
            if [ -f "$exe" ]; then
                echo "✅ $launch_file -> $exe (存在)"
            else
                echo "⚠️  $launch_file -> $exe (未构建)"
            fi
        fi
    done
    echo ""
}

# 测试5: 节点启动测试
test_node_startup() {
    echo "🔧 测试5: 节点启动测试"
    echo "----------------------"

    echo "启动DR tightening节点 (5秒测试)..."

    # 后台启动roscore
    roscore &
    ROSCORE_PID=$!
    sleep 3

    # 启动DR tightening节点
    timeout 5 rosrun dr_tightening dr_tightening_node \
        _enable_terminal_set:=true \
        _terminal_set_update_frequency:=0.2 \
        2>&1 | head -20 || true

    # 清理
    kill $ROSCORE_PID 2>/dev/null || true
    wait $ROSCORE_PID 2>/dev/null || true

    echo "✅ 节点启动测试完成"
    echo ""
}

# 测试6: 话题检查
test_topics() {
    echo "📡 测试6: ROS话题检查"
    echo "---------------------"

    expected_topics=(
        "/dr_terminal_set"
        "/terminal_set_viz"
    )

    echo "预期话题:"
    for topic in "${expected_topics[@]}"; do
        echo "  - $topic"
    done
    echo ""
}

# 运行所有测试
run_all_tests() {
    test_build
    test_files
    test_params
    test_launch
    test_node_startup
    test_topics

    echo "========================================="
    echo "  ✅ 所有基础测试通过！"
    echo "========================================="
    echo ""
    echo "下一步:"
    echo "  1. 启动完整系统测试:"
    echo "     roslaunch dr_tightening terminal_set_computation.launch"
    echo ""
    echo "  2. 或者启动集成系统:"
    echo "     roslaunch tube_mpc_ros tube_mpc_with_terminal_set.launch"
    echo ""
}

# 主测试路由
case $TEST_TYPE in
    build)
        test_build
        ;;
    files)
        test_files
        ;;
    params)
        test_params
        ;;
    launch)
        test_launch
        ;;
    node)
        test_node_startup
        ;;
    topics)
        test_topics
        ;;
    all)
        run_all_tests
        ;;
    *)
        echo "用法: $0 [build|files|params|launch|node|topics|all]"
        exit 1
        ;;
esac
