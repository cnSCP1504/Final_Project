#!/bin/bash
################################################################################
# 自动化Baseline测试脚本
# Automated Baseline Testing Script
#
# 功能：
# 1. 循环测试不同货架位置作为取货点
# 2. 支持选择不同的baseline模型
# 3. 自动检测两次目标到达后重启并切换货架
# 4. 收集并保存测试metrics到结构化文件夹
#
# 使用方法：
#   ./automated_baseline_test.sh [OPTIONS]
#
# 选项：
#   --model MODEL          模型类型: tube_mpc, stl, dr, safe_regret (默认: tube_mpc)
#   --shelves NUM          测试货架数量 (默认: 5)
#   --timeout SEC          单次测试超时时间 (默认: 240秒)
#   --no-gazebo            禁用Gazebo可视化
#   --help                 显示帮助信息
#
# 示例：
#   # 测试所有货架，使用tube_mpc模型
#   ./automated_baseline_test.sh --model tube_mpc
#
#   # 测试前3个货架，使用STL增强模型
#   ./automated_baseline_test.sh --model stl --shelves 3
################################################################################

set -e  # 遇到错误立即退出

# ==================== 默认配置 ====================
MODEL="tube_mpc"
NUM_SHELVES=5
TEST_TIMEOUT=240  # 4分钟
USE_GAZEBO=true
ENABLE_VISUALIZATION=false
DEBUG_MODE=true

# 目录配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SHELF_CONFIG="$SCRIPT_DIR/shelf_locations.yaml"
RESULTS_BASE_DIR="$WORKSPACE_DIR/test_results"

# ROS配置
CATKIN_WS="$WORKSPACE_DIR"
LAUNCH_FILE="safe_regret_mpc/safe_regret_mpc_test.launch"

# 进程ID存储
ROSMASTER_PID=""
LAUNCH_PID=""
MONITOR_PID=""

# 测试状态
CURRENT_SHELF=0
TOTAL_GOALS_REACHED=0
TEST_START_TIME=""
CURRENT_MODEL=""

# ==================== 颜色输出 ====================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ==================== 日志函数 ====================
log_info() {
    echo -e "${BLUE}[INFO]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_test() {
    echo -e "${PURPLE}[TEST]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

# ==================== 帮助信息 ====================
show_help() {
    cat << EOF
${CYAN}自动化Baseline测试脚本${NC}

${GREEN}功能：${NC}
  - 循环测试不同货架位置作为取货点
  - 支持选择不同的baseline模型
  - 自动检测两次目标到达后重启并切换货架
  - 收集并保存测试metrics到结构化文件夹

${GREEN}使用方法：${NC}
  $0 [OPTIONS]

${GREEN}选项：${NC}
  --model MODEL          模型类型:
                         - tube_mpc: 仅使用Tube MPC (默认)
                         - stl: Tube MPC + STL监控
                         - dr: Tube MPC + DR约束收紧
                         - safe_regret: 完整Safe-Regret MPC (STL+DR)
  --shelves NUM          测试货架数量 (默认: 5, 范围: 1-5)
  --timeout SEC          单次测试超时时间，秒 (默认: 240)
  --no-gazebo            禁用Gazebo可视化（加速测试）
  --viz                  启用RViz可视化
  --help                 显示此帮助信息

${GREEN}示例：${NC}
  # 测试所有货架，使用tube_mpc模型
  $0 --model tube_mpc

  # 测试前3个货架，使用STL增强模型
  $0 --model stl --shelves 3

  # 快速测试（无Gazebo）
  $0 --model tube_mpc --no-gazebo --shelves 2

${GREEN}输出目录结构：${NC}
  test_results/
  └── MODEL_YYYYMMDD_HHMMSS/
      ├── test_01_shelf_01/
      │   ├── rosbag/
      │   ├── logs/
      │   ├── metrics.csv
      │   └── test_summary.txt
      ├── test_02_shelf_02/
      │   └── ...
      └── final_report.txt

EOF
}

# ==================== 参数解析 ====================
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --model)
                MODEL="$2"
                shift 2
                ;;
            --shelves)
                NUM_SHELVES="$2"
                shift 2
                ;;
            --timeout)
                TEST_TIMEOUT="$2"
                shift 2
                ;;
            --no-gazebo)
                USE_GAZEBO=false
                shift
                ;;
            --viz)
                ENABLE_VISUALIZATION=true
                shift
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 验证模型类型
    if [[ ! "$MODEL" =~ ^(tube_mpc|stl|dr|safe_regret)$ ]]; then
        log_error "无效的模型类型: $MODEL"
        log_info "支持的模型: tube_mpc, stl, dr, safe_regret"
        exit 1
    fi

    # 验证货架数量
    if [[ $NUM_SHELVES -lt 1 || $NUM_SHELVES -gt 5 ]]; then
        log_error "货架数量必须在1-5之间: $NUM_SHELVES"
        exit 1
    fi
}

# ==================== 模型配置 ====================
get_model_params() {
    local model=$1

    case $model in
        tube_mpc)
            echo "--enable_safe_regret_integration=false"
            ;;
        stl)
            echo "--enable_safe_regret_integration=true --enable_stl=true --enable_dr=false"
            ;;
        dr)
            echo "--enable_safe_regret_integration=true --enable_stl=false --enable_dr=true"
            ;;
        safe_regret)
            echo "--enable_safe_regret_integration=true --enable_stl=true --enable_dr=true"
            ;;
        *)
            log_error "未知模型: $model"
            return 1
            ;;
    esac
}

# ==================== 读取货架配置 ====================
read_shelf_config() {
    if [[ ! -f "$SHELF_CONFIG" ]]; then
        log_error "货架配置文件不存在: $SHELF_CONFIG"
        exit 1
    fi

    # 读取货架位置（简单解析YAML）
    # 实际应用中可以使用Python或yq工具解析
    log_info "读取货架配置: $SHELF_CONFIG"
}

# ==================== 清理ROS进程 ====================
cleanup_ros_processes() {
    log_info "清理ROS进程..."

    # 停止监控进程
    if [[ -n "$MONITOR_PID" ]]; then
        kill $MONITOR_PID 2>/dev/null || true
        wait $MONITOR_PID 2>/dev/null || true
        MONITOR_PID=""
    fi

    # 杀死所有ROS相关进程
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
    killall -9 play 2>/dev/null || true  # rosbag play

    # 等待进程完全退出
    sleep 3

    # 清理ROS节点
    pkill -9 -f ros || true
    pkill -9 -f gazebo || true

    log_success "ROS进程清理完成"
}

# ==================== 创建结果目录 ====================
create_results_directory() {
    local model=$1
    local timestamp=$(date '+%Y%m%d_%H%M%S')
    local result_dir="$RESULTS_BASE_DIR/${model}_${timestamp}"

    mkdir -p "$result_dir"
    mkdir -p "$result_dir/logs"

    echo "$result_dir"
}

# ==================== 启动ROS系统 ====================
launch_ros_system() {
    local shelf_x=$1
    local shelf_y=$2
    local shelf_id=$3
    local result_dir=$4

    log_info "启动ROS系统..."
    log_info "  取货点: ($shelf_x, $shelf_y)"
    log_info "  货架ID: $shelf_id"
    log_info "  模型: $MODEL"

    cd "$CATKIN_WS"
    source devel/setup.bash

    # 获取模型参数
    local model_params=$(get_model_params "$MODEL")

    # 构建launch命令
    local launch_cmd="roslaunch $LAUNCH_FILE"
    launch_cmd="$launch_cmd pickup_x:=$shelf_x pickup_y:=$shelf_y"
    launch_cmd="$launch_cmd use_gazebo:=$USE_GAZEBO"
    launch_cmd="$launch_cmd enable_visualization:=$ENABLE_VISUALIZATION"
    launch_cmd="$launch_cmd debug_mode:=$DEBUG_MODE"
    launch_cmd="$launch_cmd $model_params"

    log_info "Launch命令: $launch_cmd"

    # 启动launch文件（后台运行）
    $launch_cmd > "$result_dir/logs/launch.log" 2>&1 &
    LAUNCH_PID=$!

    log_info "ROS系统启动中 (PID: $LAUNCH_PID)..."

    # 等待系统初始化
    sleep 10

    # 检查是否启动成功
    if ! ps -p $LAUNCH_PID > /dev/null; then
        log_error "ROS系统启动失败，查看日志: $result_dir/logs/launch.log"
        return 1
    fi

    log_success "ROS系统启动成功"
    return 0
}

# ==================== 监控目标到达 ====================
monitor_goal_reaching() {
    local result_dir=$1
    local timeout=$2
    local log_file="$result_dir/logs/goal_monitor.log"

    log_info "开始监控目标到达事件..."

    local start_time=$(date +%s)
    local goals_reached=0

    while true; do
        # 检查超时
        local current_time=$(date +%s)
        local elapsed=$((current_time - start_time))

        if [[ $elapsed -gt $timeout ]]; then
            log_warning "监控超时 (${timeout}秒)"
            echo "TIMEOUT" > "$result_dir/test_status.txt"
            return 1
        fi

        # 监控rosout消息，检测目标到达
        # 查找"目标已到达"或"✓ 目标已到达"的消息
        if rostopic echo /rosout -n 1 2>/dev/null | grep -q "目标已到达"; then
            goals_reached=$((goals_reached + 1))
            local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
            echo "[$timestamp] 目标 $goals_reached 到达" >> "$log_file"
            log_success "检测到目标到达 ($goals_reached/2)"

            # 如果到达两次，返回成功
            if [[ $goals_reached -ge 2 ]]; then
                log_success "两次目标均已到达！测试完成"
                echo "SUCCESS" > "$result_dir/test_status.txt"
                return 0
            fi

            # 等待一段时间再检测下一个目标
            sleep 5
        fi

        sleep 1
    done
}

# ==================== 收集测试数据 ====================
collect_test_data() {
    local shelf_id=$1
    local result_dir=$2

    log_info "收集测试数据..."

    # 保存rosbag（可选，如果需要记录）
    local rosbag_dir="$result_dir/rosbag"
    mkdir -p "$rosbag_dir"

    # 收集topics数据
    local topics_file="$result_dir/metrics.csv"

    # CSV header
    echo "timestamp,topic,value" > "$topics_file"

    # 收集关键metrics
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')

    # 从ROS参数服务器获取数据
    if rosparam get /auto_nav_goal/pickup_x >/dev/null 2>&1; then
        local pickup_x=$(rosparam get /auto_nav_goal/pickup_x)
        local pickup_y=$(rosparam get /auto_nav_goal/pickup_y)
        echo "$timestamp,pickup_x,$pickup_x" >> "$topics_file"
        echo "$timestamp,pickup_y,$pickup_y" >> "$topics_file"
    fi

    # 保存当前机器人位置
    if timeout 5 rostopic echo /odom -n 1 >/dev/null 2>&1; then
        local odom_data=$(timeout 5 rostopic echo /odom -n 1)
        local pos_x=$(echo "$odom_data" | grep -A 1 "position:" | tail -1 | awk '{print $2}' | cut -d',' -f1)
        local pos_y=$(echo "$odom_data" | grep -A 1 "position:" | tail -1 | awk '{print $3}' | cut -d',' -f1)
        echo "$timestamp,final_position_x,$pos_x" >> "$topics_file"
        echo "$timestamp,final_position_y,$pos_y" >> "$topics_file"
    fi

    log_success "测试数据已保存到: $topics_file"
}

# ==================== 生成测试摘要 ====================
generate_test_summary() {
    local shelf_id=$1
    local shelf_name=$2
    local result_dir=$3
    local test_result=$4

    local summary_file="$result_dir/test_summary.txt"

    cat > "$summary_file" << EOF
=================================================================
                 测试摘要 / Test Summary
=================================================================

测试时间 / Test Time: $(date '+%Y-%m-%d %H:%M:%S')

模型配置 / Model Configuration:
  - 模型类型 / Model Type: $MODEL
  - 货架ID / Shelf ID: $shelf_id
  - 货架名称 / Shelf Name: $shelf_name

测试结果 / Test Result: $test_result

测试参数 / Test Parameters:
  - 超时时间 / Timeout: ${TEST_TIMEOUT}秒
  - Gazebo启用 / Gazebo Enabled: $USE_GAZEBO
  - 可视化 / Visualization: $ENABLE_VISUALIZATION

文件位置 / File Locations:
  - Launch日志 / Launch Log: logs/launch.log
  - 监控日志 / Monitor Log: logs/goal_monitor.log
  - 测试指标 / Metrics: metrics.csv

=================================================================
EOF

    log_success "测试摘要已生成: $summary_file"
}

# ==================== 执行单次测试 ====================
run_single_test() {
    local shelf_index=$1
    local result_base_dir=$2

    # 货架位置（从配置读取）
    # 这里简化为固定位置，实际应从YAML读取
    local shelf_x shelf_y shelf_name

    case $shelf_index in
        1)
            shelf_x=-6.0; shelf_y=-7.0; shelf_name="货架01 - 左上"
            ;;
        2)
            shelf_x=-3.0; shelf_y=-7.0; shelf_name="货架02 - 左中"
            ;;
        3)
            shelf_x=0.0; shelf_y=-7.0; shelf_name="货架03 - 左下"
            ;;
        4)
            shelf_x=3.0; shelf_y=-7.0; shelf_name="货架04 - 中上"
            ;;
        5)
            shelf_x=6.0; shelf_y=-7.0; shelf_name="货架05 - 中下"
            ;;
        *)
            log_error "无效的货架索引: $shelf_index"
            return 1
            ;;
    esac

    local shelf_id="shelf_$(printf '%02d' $shelf_index)"
    local test_dir="$result_base_dir/test_$(printf '%02d' $shelf_index)_${shelf_id}"

    log_test "========================================"
    log_test "开始测试 $shelf_index/$NUM_SHELVES: $shelf_name"
    log_test "========================================"

    # 创建测试目录
    mkdir -p "$test_dir/logs"

    # 启动ROS系统
    if ! launch_ros_system "$shelf_x" "$shelf_y" "$shelf_id" "$test_dir"; then
        log_error "ROS系统启动失败"
        generate_test_summary "$shelf_id" "$shelf_name" "$test_dir" "FAILED (启动失败)"
        return 1
    fi

    # 监控目标到达
    log_info "等待机器人完成取货和卸货任务..."
    local monitor_result=0

    if ! monitor_goal_reaching "$test_dir" "$TEST_TIMEOUT"; then
        log_warning "测试未在超时时间内完成"
        monitor_result=1
    fi

    # 收集数据
    collect_test_data "$shelf_id" "$test_dir"

    # 清理ROS进程
    cleanup_ros_processes

    # 生成测试摘要
    local test_result="SUCCESS"
    if [[ $monitor_result -ne 0 ]]; then
        test_result="TIMEOUT"
    fi

    generate_test_summary "$shelf_id" "$shelf_name" "$test_dir" "$test_result"

    log_test "测试 $shelf_index/$NUM_SHELVES 完成: $test_result"

    # 等待系统完全关闭
    sleep 5

    return $monitor_result
}

# ==================== 生成最终报告 ====================
generate_final_report() {
    local result_dir=$1
    local total_tests=$2
    local successful_tests=$3

    local report_file="$result_dir/final_report.txt"

    cat > "$report_file" << EOF
=================================================================
              自动化Baseline测试最终报告
           Automated Baseline Testing Final Report
=================================================================

测试时间 / Test Time: $(date '+%Y-%m-%d %H:%M:%S')

模型配置 / Model Configuration:
  - 模型类型 / Model Type: $MODEL

测试统计 / Test Statistics:
  - 总测试数 / Total Tests: $total_tests
  - 成功测试 / Successful Tests: $successful_tests
  - 失败测试 / Failed Tests: $((total_tests - successful_tests))
  - 成功率 / Success Rate: $(awk "BEGIN {printf \"%.1f\", $successful_tests*100.0/$total_tests}")%

测试参数 / Test Parameters:
  - 超时时间 / Timeout: ${TEST_TIMEOUT}秒/测试
  - Gazebo启用 / Gazebo Enabled: $USE_GAZEBO
  - 可视化 / Visualization: $ENABLE_VISUALIZATION

结果目录 / Results Directory: $result_dir

详细结果 / Detailed Results:
EOF

    # 添加每个测试的摘要
    for i in $(seq 1 $total_tests); do
        local test_dir="$result_dir/test_$(printf '%02d' $i)_shelf_$(printf '%02d' $i)"
        local summary_file="$test_dir/test_summary.txt"

        if [[ -f "$summary_file" ]]; then
            echo "" >> "$report_file"
            echo "--- 测试 $i ---" >> "$report_file"
            grep "测试结果" "$summary_file" >> "$report_file" || true
        fi
    done

    echo "" >> "$report_file"
    echo "=================================================================" >> "$report_file"

    log_success "最终报告已生成: $report_file"
    cat "$report_file"
}

# ==================== 主函数 ====================
main() {
    log_test "========================================"
    log_test "  自动化Baseline测试系统启动"
    log_test "  Automated Baseline Testing System"
    log_test "========================================"

    # 解析参数
    parse_arguments "$@"

    # 显示配置
    log_info "测试配置:"
    log_info "  模型: $MODEL"
    log_info "  货架数量: $NUM_SHELVES"
    log_info "  超时时间: ${TEST_TIMEOUT}秒"
    log_info "  Gazebo: $USE_GAZEBO"
    log_info "  可视化: $ENABLE_VISUALIZATION"

    # 创建结果目录
    local result_dir=$(create_results_directory "$MODEL")
    log_success "结果目录: $result_dir"

    # 清理之前的ROS进程
    cleanup_ros_processes

    # 执行测试循环
    local successful_tests=0

    for i in $(seq 1 $NUM_SHELVES); do
        if run_single_test $i "$result_dir"; then
            successful_tests=$((successful_tests + 1))
        fi

        # 等待系统完全关闭
        sleep 3
    done

    # 生成最终报告
    generate_final_report "$result_dir" $NUM_SHELVES $successful_tests

    log_test "========================================"
    log_test "  所有测试完成"
    log_test "  成功率: $successful_tests/$NUM_SHELVES"
    log_test "========================================"

    # 返回成功/失败
    if [[ $successful_tests -eq $NUM_SHELVES ]]; then
        exit 0
    else
        exit 1
    fi
}

# ==================== 脚本入口 ====================
# 捕获Ctrl+C
trap cleanup_ros_processes EXIT INT TERM

# 运行主函数
main "$@"
