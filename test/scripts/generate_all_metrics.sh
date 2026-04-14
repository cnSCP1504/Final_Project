#!/bin/bash

################################################################################
# Safe-Regret MPC - 完整指标生成流程
# 自动计算manuscript指标并生成所有图表
#
# Usage:
#   ./generate_all_metrics.sh <results_directory>
#
# Example:
#   ./generate_all_metrics.sh test_results/safe_regret_20260408_120104
################################################################################

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_header() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo ""
}

print_step() {
    echo -e "${GREEN}[$1] $2${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# 检查参数
if [ $# -eq 0 ]; then
    print_error "Usage: $0 <results_directory>"
    echo ""
    echo "Example:"
    echo "  $0 test_results/safe_regret_20260408_120104"
    exit 1
fi

RESULTS_DIR="$1"

# 检查目录是否存在
if [ ! -d "$RESULTS_DIR" ]; then
    print_error "Results directory does not exist: $RESULTS_DIR"
    exit 1
fi

# 检查是否有测试数据
TEST_COUNT=$(find "$RESULTS_DIR" -name "metrics.json" | wc -l)
if [ "$TEST_COUNT" -eq 0 ]; then
    print_error "No test data found in $RESULTS_DIR"
    echo "Expected structure: $RESULTS_DIR/test_XX/metrics.json"
    exit 1
fi

print_header "Safe-Regret MPC Metrics Generation Pipeline"
echo "Results directory: $RESULTS_DIR"
echo "Test data found: $TEST_COUNT metrics.json files"
echo ""

# Step 1: 计算聚合指标
print_step "1/3" "Computing aggregated metrics..."

AGGREGATED_METRICS="$RESULTS_DIR/aggregated_metrics.json"

python3 test/scripts/compute_manuscript_metrics.py \
    --input "$RESULTS_DIR" \
    --output "$AGGREGATED_METRICS" \
    --tube-radius 0.18 \
    --target-delta 0.05

if [ $? -eq 0 ]; then
    print_success "Aggregated metrics saved to: $AGGREGATED_METRICS"
else
    print_error "Failed to compute aggregated metrics"
    exit 1
fi

# Step 2: 生成所有图表
print_step "2/3" "Generating manuscript figures..."

OUTPUT_DIR="$RESULTS_DIR/metrics"

python3 test/scripts/generate_manuscript_figures.py \
    --metrics "$AGGREGATED_METRICS" \
    --output "$OUTPUT_DIR"

if [ $? -eq 0 ]; then
    print_success "Figures saved to: $OUTPUT_DIR"
else
    print_error "Failed to generate figures"
    exit 1
fi

# Step 3: 生成摘要报告
print_step "3/3" "Generating summary report..."

REPORT_FILE="$RESULTS_DIR/performance_summary.md"

cat > "$REPORT_FILE" << 'EOF'
# Safe-Regret MPC Performance Summary

**Generated**: $(date)

## Overview

This report summarizes the performance metrics for Safe-Regret MPC evaluated according to the manuscript specification.

## Metrics Summary

EOF

# 提取关键指标并添加到报告
python3 - << 'PYTHON_EOF'
import json
import sys

# 读取聚合指标
with open('$AGGREGATED_METRICS', 'r') as f:
    metrics = json.load(f)

# 打印Markdown表格
print("### 1. Satisfaction Probability\n")
sat = metrics['satisfaction_probability']
print(f"- **Satisfaction Rate**: {sat['p_sat']:.1%}")
print(f"- **95% CI**: [{sat['ci_lower']:.1%}, {sat['ci_upper']:.1%}]")
print(f"- **Tests**: {sat['satisfied_count']}/{sat['total_count']} satisfied\n")

print("### 2. Empirical Risk\n")
risk = metrics['empirical_risk']
print(f"- **Observed δ̂**: {risk['observed_delta']:.4f}")
print(f"- **Target δ**: {risk['target_delta']:.2f}")
print(f"- **Calibration Error**: {risk['calibration_error']:.4f}")
print(f"- **Relative Error**: {risk['relative_error']:.2%}\n")

print("### 3. Regret Metrics\n")
regret = metrics['regret']
print(f"- **Normalized Regret (R_T/T)**: {regret['normalized_regret']:.4f}")
print(f"- **Cumulative Regret**: {regret['cumulative_regret']:.2f}")
print(f"- **Total Steps**: {regret['total_steps']}\n")

print("### 4. Feasibility Rate\n")
feas = metrics['feasibility']
print(f"- **Feasibility Rate**: {feas['feasibility_rate']:.2%}")
print(f"- **Feasible Solves**: {feas['feasible_count']}/{feas['total_solves']}")
print(f"- **Infeasible Solves**: {feas['infeasible_count']}\n")

print("### 5. Computation Metrics\n")
comp = metrics['computation']
print(f"- **Median Solve Time**: {comp['median_ms']:.2f} ms")
print(f"- **Mean Solve Time**: {comp['mean_ms']:.2f} ms")
print(f"- **P90 Solve Time**: {comp['p90_ms']:.2f} ms")
print(f"- **P95 Solve Time**: {comp['p95_ms']:.2f} ms")
print(f"- **P99 Solve Time**: {comp['p99_ms']:.2f} ms")
print(f"- **Failure Count**: {comp['failure_count']}\n")

print("### 6. Tracking Error\n")
track = metrics['tracking']
print(f"- **Mean Error**: {track['mean_error_m']*100:.2f} cm")
print(f"- **Std Error**: {track['std_error_m']*100:.2f} cm")
print(f"- **RMSE**: {track['rmse_m']*100:.2f} cm")
print(f"- **Max Error**: {track['max_error_m']*100:.2f} cm")
print(f"- **Tube Occupancy**: {track['occupancy_rate']:.2%}")
print(f"- **Steps in Tube**: {track['in_tube_count']}/{track['total_steps']}\n")

print("### 7. Calibration Accuracy\n")
calib = metrics['calibration']
print(f"- **Target δ**: {calib['target_delta']:.2f}")
print(f"- **Observed δ**: {calib['observed_delta']:.4f}")
print(f"- **Calibration Error**: {calib['calibration_error']:.4f}")
print(f"- **Well Calibrated**: {'✓ Yes' if calib['is_well_calibrated'] else '✗ No'}\n")

print("## Figures\n")
print("The following figures have been generated:\n")
print("1. **satisfaction_probability.png** - STL satisfaction rate with 95% CI")
print("2. **empirical_risk.png** - Safety constraint violation rate")
print("3. **computation_metrics.png** - MPC solve time distribution")
print("4. **feasibility_rate.png** - Recursive feasibility rate")
print("5. **tracking_error_distribution.png** - Tracking error statistics")
print("6. **calibration_accuracy.png** - Target vs. observed risk")
print("7. **performance_dashboard.png** - Comprehensive summary dashboard")
print()

print("## Usage in Manuscript\n")
print("To include these figures in your manuscript:\n")
print("```latex")
print("\\\\begin{figure}[htbp]")
print("  \\\\centering")
print("  \\\\includegraphics[width=0.8\\\\columnwidth]{metrics/satisfaction_probability.pdf}")
print("  \\\\caption{STL specification satisfaction rate.}")
print("  \\\\label{fig:satisfaction}")
print("\\\\end{figure}")
print("```")
PYTHON_EOF

if [ $? -eq 0 ]; then
    print_success "Summary report saved to: $REPORT_FILE"
else
    print_warning "Failed to generate summary report (non-critical)"
fi

# 完成
print_header "✅ Metrics Generation Complete!"
echo ""
echo "Output files:"
echo "  📊 Aggregated metrics: $AGGREGATED_METRICS"
echo "  📈 Figures:            $OUTPUT_DIR/"
echo "  📝 Summary report:     $REPORT_FILE"
echo ""
echo "Quick preview:"
echo "  cat $REPORT_FILE"
echo ""
echo "Next steps:"
echo "  1. Review the figures in $OUTPUT_DIR"
echo "  2. Check the summary report: cat $REPORT_FILE"
echo "  3. Integrate figures into latex/manuscript.tex"
echo ""
