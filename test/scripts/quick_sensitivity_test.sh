#!/bin/bash
# 快速参数敏感性测试脚本
# Quick Parameter Sensitivity Test Script
#
# 用于快速验证参数敏感性分析脚本是否正常工作
# 只测试每个参数的2个极端值，完整测试请运行 parameter_sensitivity_analysis.py

echo "========================================"
echo "🔬 快速参数敏感性测试"
echo "========================================"
echo ""
echo "本脚本测试每个参数的2个极端值:"
echo "  - mpc_steps: 10, 30"
echo "  - temperature: 0.01, 1.0"
echo "  - window_size: 50, 500"
echo ""
echo "完整测试请运行:"
echo "  python3 test/scripts/parameter_sensitivity_analysis.py"
echo ""

# 设置颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 工作目录
WORK_DIR="/home/dixon/Final_Project/catkin"
cd $WORK_DIR

# 输出目录
OUTPUT_DIR="test_results/sensitivity_analysis_$(date +%Y%m%d_%H%M%S)"
mkdir -p $OUTPUT_DIR

echo "输出目录: $OUTPUT_DIR"
echo ""

# 备份原始参数
echo "📦 备份原始参数文件..."
mkdir -p $OUTPUT_DIR/backups
cp src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml $OUTPUT_DIR/backups/
cp src/tube_mpc_ros/stl_monitor/params/stl_monitor_params.yaml $OUTPUT_DIR/backups/
cp src/dr_tightening/params/dr_tightening_params.yaml $OUTPUT_DIR/backups/
echo ""

# 测试函数
run_test() {
    local param_name=$1
    local param_value=$2
    local test_name="${param_name}_${param_value}"

    echo ""
    echo "========================================"
    echo "🧪 测试: $param_name = $param_value"
    echo "========================================"

    # 修改参数
    echo "📝 修改参数..."
    case $param_name in
        "mpc_steps")
            sed -i "s/mpc_steps:.*/mpc_steps: $param_value/" src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml
            ;;
        "temperature")
            sed -i "s/temperature:.*/temperature: $param_value/" src/tube_mpc_ros/stl_monitor/params/stl_monitor_params.yaml
            ;;
        "window_size")
            sed -i "s/sliding_window_size:.*/sliding_window_size: $param_value/" src/dr_tightening/params/dr_tightening_params.yaml
            ;;
    esac

    # 清理ROS
    echo "🧹 清理ROS进程..."
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    sleep 2

    # 运行测试
    echo "🚀 运行测试..."
    source devel/setup.bash
    python3 test/scripts/run_automated_test.py --model dr --shelves 1 --timeout 120 --no-viz 2>&1 | tee $OUTPUT_DIR/${test_name}.log

    # 保存结果
    latest_dr=$(ls -td test_results/dr_* 2>/dev/null | head -1)
    if [ -d "$latest_dr" ]; then
        echo "💾 保存结果到 $OUTPUT_DIR/${test_name}/"
        cp -r $latest_dr $OUTPUT_DIR/${test_name}/
    fi

    echo "✅ 测试完成: $test_name"
}

# 主测试循环
echo ""
echo "========================================"
echo "开始参数敏感性测试"
echo "========================================"

# 测试 mpc_steps
run_test "mpc_steps" 10
run_test "mpc_steps" 30

# 测试 temperature
run_test "temperature" 0.01
run_test "temperature" 1.0

# 测试 window_size
run_test "window_size" 50
run_test "window_size" 500

# 恢复原始参数
echo ""
echo "📦 恢复原始参数文件..."
cp $OUTPUT_DIR/backups/tube_mpc_params.yaml src/tube_mpc_ros/mpc_ros/params/
cp $OUTPUT_DIR/backups/stl_monitor_params.yaml src/tube_mpc_ros/stl_monitor/params/
cp $OUTPUT_DIR/backups/dr_tightening_params.yaml src/dr_tightening/params/

echo ""
echo "========================================"
echo "✅ 参数敏感性测试完成"
echo "========================================"
echo ""
echo "结果保存在: $OUTPUT_DIR"
echo ""
echo "下一步: 分析结果"
echo "  python3 test/scripts/analyze_sensitivity_results.py $OUTPUT_DIR"
