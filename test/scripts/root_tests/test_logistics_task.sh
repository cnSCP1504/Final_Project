#!/bin/bash
#
# 物流任务自动化测试脚本
# 用途：测试不同取货点的导航性能
#
# 使用方法：
#   ./test_logistics_task.sh              # 测试所有预定义取货点
#   ./test_logistics_task.sh single 0.0 -7.0  # 测试单个取货点
#

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 打印分隔线
print_separator() {
    echo "=========================================="
}

# 测试单个取货点
test_single_pickup_point() {
    local pickup_x=$1
    local pickup_y=$2
    local test_name=$3

    print_separator
    log_info "开始测试: $test_name"
    log_info "取货点坐标: ($pickup_x, $pickup_y)"
    log_info "卸货点坐标: (8.0, 0.0) [固定]"
    print_separator

    # 清理之前的ROS进程
    log_info "清理ROS进程..."
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    sleep 3

    # 创建日志目录
    local log_dir="/tmp/logistics_test_logs"
    mkdir -p "$log_dir"
    local log_file="$log_dir/test_${test_name// /_}_$(date +%Y%m%d_%H%M%S).log"

    # 启动测试
    log_info "启动物流任务测试..."
    log_info "日志文件: $log_file"

    # 后台启动launch文件
    roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
        pickup_x:=$pickup_x \
        pickup_y:=$pickup_y \
        enable_visualization:=false \
        > "$log_file" 2>&1 &

    local launch_pid=$!
    log_info "Launch进程PID: $launch_pid"

    # 等待测试完成（最多3分钟）
    local timeout=180
    local elapsed=0
    local check_interval=5

    log_info "等待测试完成（最长${timeout}秒）..."

    while [ $elapsed -lt $timeout ]; do
        sleep $check_interval
        elapsed=$((elapsed + check_interval))

        # 检查是否完成任务（查找成功标记）
        if grep -q "物流任务完成" "$log_file" 2>/dev/null; then
            log_success "测试完成: $test_name"
            print_separator
            # 显示关键日志
            grep -E "前往|到达|物流任务" "$log_file" | tail -20
            print_separator

            # 等待5秒让用户看到结果
            sleep 5
            return 0
        fi

        # 检查是否出错
        if grep -q "发生错误\|ERROR\|FATAL" "$log_file" 2>/dev/null; then
            log_error "测试过程中发现错误"
            break
        fi

        # 进度提示
        if [ $((elapsed % 30)) -eq 0 ]; then
            log_info "测试进行中... 已等待 ${elapsed} 秒"
        fi
    done

    # 超时或出错
    if [ $elapsed -ge $timeout ]; then
        log_warning "测试超时: $test_name"
    fi

    # 清理
    log_info "清理进程..."
    kill $launch_pid 2>/dev/null
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    sleep 2

    # 显示最后的日志
    print_separator
    log_info "最后的日志输出："
    tail -30 "$log_file"
    print_separator

    return 1
}

# 定义测试案例
declare -a TEST_CASES=(
    "0.0,-7.0,南侧中部"
    "3.0,-7.0,东南侧"
    "-3.0,-7.0,西南侧"
    "0.0,7.0,北侧中部"
    "0.0,0.0,地图中心"
    "5.0,0.0,东侧中部"
    "-5.0,0.0,西侧中部"
)

# 主函数
main() {
    # 检查参数
    if [ "$1" = "single" ]; then
        # 单点测试模式
        if [ $# -lt 3 ]; then
            log_error "单点测试需要提供坐标: $0 single <x> <y> [测试名称]"
            exit 1
        fi

        local pickup_x=$2
        local pickup_y=$3
        local test_name=${4:-"自定义取货点"}

        test_single_pickup_point $pickup_x $pickup_y "$test_name"
        exit $?
    fi

    # 批量测试模式
    print_separator
    log_info "物流任务批量测试"
    log_info "测试案例数量: ${#TEST_CASES[@]}"
    print_separator

    local total_tests=${#TEST_CASES[@]}
    local passed_tests=0
    local failed_tests=0

    for i in "${!TEST_CASES[@]}"; do
        IFS=',' read -r x y name <<< "${TEST_CASES[$i]}"
        test_number=$((i + 1))

        print_separator
        log_info "测试进度: [$test_number/$total_tests]"

        if test_single_pickup_point $x $y "$name"; then
            passed_tests=$((passed_tests + 1))
            log_success "✓ 测试通过: $name"
        else
            failed_tests=$((failed_tests + 1))
            log_error "✗ 测试失败: $name"
        fi

        # 测试间隔
        if [ $test_number -lt $total_tests ]; then
            log_info "等待5秒后开始下一个测试..."
            sleep 5
        fi
    done

    # 测试总结
    print_separator
    log_info "测试总结"
    print_separator
    log_info "总测试数: $total_tests"
    log_success "通过: $passed_tests"
    log_error "失败: $failed_tests"
    print_separator

    # 生成测试报告
    local report_file="/tmp/logistics_test_report_$(date +%Y%m%d_%H%M%S).txt"
    {
        echo "物流任务测试报告"
        echo "生成时间: $(date)"
        echo "=========================================="
        echo "总测试数: $total_tests"
        echo "通过: $passed_tests"
        echo "失败: $failed_tests"
        echo ""
        echo "详细结果："
        for i in "${!TEST_CASES[@]}"; do
            IFS=',' read -r x y name <<< "${TEST_CASES[$i]}"
            echo "  - $name ($x, $y)"
        done
    } > "$report_file"

    log_info "测试报告已保存: $report_file"

    if [ $failed_tests -eq 0 ]; then
        log_success "所有测试通过！"
        exit 0
    else
        log_warning "部分测试失败，请查看日志"
        exit 1
    fi
}

# 运行主函数
main "$@"
