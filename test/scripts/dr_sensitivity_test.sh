#!/bin/bash
# DR约束敏感性分析测试脚本
# 分析DR margin是否过于保守导致经验风险远超目标

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CATKIN_DIR="/home/dixon/Final_Project/catkin"
DR_PARAMS="$CATKIN_DIR/src/dr_tightening/params/dr_tightening_params.yaml"
BACKUP_PARAMS="$CATKIN_DIR/src/dr_tightening/params/dr_tightening_params.yaml.backup"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  DR约束敏感性分析测试${NC}"
echo -e "${GREEN}========================================${NC}"

# 备份原始参数
if [ ! -f "$BACKUP_PARAMS" ]; then
    cp "$DR_PARAMS" "$BACKUP_PARAMS"
    echo -e "${YELLOW}已备份原始参数文件${NC}"
fi

# 测试参数组合
# 格式: "risk_level|tube_radius|lipschitz|window_size|test_name"
TEST_CASES=(
    "0.05|0.5|1.0|200|baseline_risk0.05"
    "0.10|0.5|1.0|200|relaxed_risk0.10"
    "0.15|0.5|1.0|200|relaxed_risk0.15"
    "0.20|0.5|1.0|200|relaxed_risk0.20"
    "0.05|0.3|1.0|200|tight_tube0.3"
    "0.05|0.7|1.0|200|loose_tube0.7"
    "0.05|0.5|0.5|200|small_lipschitz0.5"
    "0.05|0.5|1.5|200|large_lipschitz1.5"
    "0.05|0.5|1.0|100|small_window100"
    "0.05|0.5|1.0|300|large_window300"
)

# 创建结果目录
RESULTS_DIR="$CATKIN_DIR/test_results/dr_sensitivity_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"
echo "结果目录: $RESULTS_DIR"

# 清理ROS进程函数
cleanup_ros() {
    echo -e "${YELLOW}清理ROS进程...${NC}"
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
    sleep 2
}

# 修改参数函数
update_params() {
    local risk=$1
    local tube=$2
    local lipschitz=$3
    local window=$4

    # 使用sed修改参数
    sed -i "s/risk_level: .*/risk_level: $risk/" "$DR_PARAMS"
    sed -i "s/tube_radius: .*/tube_radius: $tube/" "$DR_PARAMS"
    sed -i "s/lipschitz_constant: .*/lipschitz_constant: $lipschitz/" "$DR_PARAMS"
    sed -i "s/sliding_window_size: .*/sliding_window_size: $window/" "$DR_PARAMS"

    echo -e "${GREEN}参数已更新:${NC}"
    echo "  risk_level: $risk"
    echo "  tube_radius: $tube"
    echo "  lipschitz_constant: $lipschitz"
    echo "  sliding_window_size: $window"
}

# 运行单次测试
run_test() {
    local test_case=$1
    IFS='|' read -r risk tube lipschitz window name <<< "$test_case"

    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}测试: $name${NC}"
    echo -e "${GREEN}========================================${NC}"

    # 清理
    cleanup_ros

    # 更新参数
    update_params "$risk" "$tube" "$lipschitz" "$window"

    # 创建测试目录
    TEST_DIR="$RESULTS_DIR/$name"
    mkdir -p "$TEST_DIR"

    # 保存当前参数到测试目录
    cp "$DR_PARAMS" "$TEST_DIR/dr_params_used.yaml"

    # 运行测试 (只测试1个货架，快速迭代)
    echo "运行测试..."
    cd "$CATKIN_DIR"

    # 使用timeout限制测试时间
    timeout 180s python3 test/scripts/run_automated_test.py \
        --model dr \
        --shelves 1 \
        --no-viz \
        --output-dir "$TEST_DIR" \
        2>&1 | tee "$TEST_DIR/test.log"

    local exit_code=$?

    if [ $exit_code -eq 124 ]; then
        echo -e "${RED}测试超时${NC}"
        echo "TIMEOUT" > "$TEST_DIR/status.txt"
    elif [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}测试完成${NC}"
        echo "SUCCESS" > "$TEST_DIR/status.txt"

        # 提取关键指标
        if [ -f "$TEST_DIR/dr_*/aggregated_manuscript_metrics.json" ]; then
            echo "提取指标..."
            cat "$TEST_DIR"/dr_*/aggregated_manuscript_metrics.json > "$TEST_DIR/metrics.json" 2>/dev/null || true
        fi
    else
        echo -e "${RED}测试失败: exit code $exit_code${NC}"
        echo "FAILED:$exit_code" > "$TEST_DIR/status.txt"
    fi

    # 短暂休息
    sleep 5
}

# 主循环
echo ""
echo -e "${YELLOW}将运行 ${#TEST_CASES[@]} 组测试${NC}"
echo ""

for test_case in "${TEST_CASES[@]}"; do
    run_test "$test_case"
done

# 恢复原始参数
echo ""
echo -e "${YELLOW}恢复原始参数...${NC}"
cp "$BACKUP_PARAMS" "$DR_PARAMS"

# 生成汇总报告
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  生成汇总报告${NC}"
echo -e "${GREEN}========================================${NC}"

python3 << 'PYTHON_SCRIPT'
import os
import json
from pathlib import Path

results_dir = os.environ.get('RESULTS_DIR', '/tmp')
print(f"结果目录: {results_dir}")

# 收集所有测试结果
results = []

for test_dir in sorted(Path(results_dir).iterdir()):
    if not test_dir.is_dir():
        continue

    test_name = test_dir.name
    status_file = test_dir / "status.txt"
    metrics_file = test_dir / "metrics.json"

    result = {
        'test_name': test_name,
        'status': 'unknown',
        'empirical_risk': None,
        'calibration_error': None,
        'feasibility': None
    }

    if status_file.exists():
        result['status'] = status_file.read_text().strip()

    if metrics_file.exists():
        try:
            with open(metrics_file) as f:
                metrics = json.load(f)
                result['empirical_risk'] = metrics.get('empirical_risk', {}).get('mean', 'N/A')
                result['calibration_error'] = metrics.get('calibration', {}).get('mean_error', 'N/A')
                result['feasibility'] = metrics.get('feasibility', {}).get('mean', 'N/A')
        except:
            pass

    results.append(result)

# 打印汇总表格
print("\n" + "="*80)
print("DR约束敏感性分析结果汇总")
print("="*80)
print(f"{'测试名称':<25} {'状态':<12} {'经验风险':<12} {'校准误差':<12} {'可行性':<10}")
print("-"*80)

for r in results:
    risk = f"{r['empirical_risk']:.3f}" if isinstance(r['empirical_risk'], (int, float)) else str(r['empirical_risk'])
    cal = f"{r['calibration_error']:.3f}" if isinstance(r['calibration_error'], (int, float)) else str(r['calibration_error'])
    feas = f"{r['feasibility']:.3f}" if isinstance(r['feasibility'], (int, float)) else str(r['feasibility'])

    print(f"{r['test_name']:<25} {r['status']:<12} {risk:<12} {cal:<12} {feas:<10}")

print("="*80)
print("\n结论:")
print("1. 如果所有配置的经验风险都远超目标(5%)，则问题不在DR参数")
print("2. 如果某些配置经验风险接近目标，则可通过参数调优改善")
print("3. 校准误差应接近0才表示风险校准准确")
PYTHON_SCRIPT

# 保存环境变量供python使用
export RESULTS_DIR="$RESULTS_DIR"

echo ""
echo -e "${GREEN}测试完成！${NC}"
echo -e "结果保存在: ${YELLOW}$RESULTS_DIR${NC}"
