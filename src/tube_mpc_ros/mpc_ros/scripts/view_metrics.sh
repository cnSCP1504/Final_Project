#!/bin/bash
# Quick script to view latest Tube MPC metrics

echo "=================================================="
echo "Tube MPC Metrics Summary"
echo "=================================================="
echo ""

# Check if metrics file exists
if [ ! -f /tmp/tube_mpc_metrics.csv ]; then
    echo "❌ Error: Metrics file not found: /tmp/tube_mpc_metrics.csv"
    echo "   Please run a test first to generate metrics data."
    exit 1
fi

# Get line count
LINES=$(wc -l < /tmp/tube_mpc_metrics.csv)
STEPS=$((LINES - 1))

echo "📊 Data file: /tmp/tube_mpc_metrics.csv"
echo "   Total records: $STEPS"
echo "   File size: $(du -h /tmp/tube_mpc_metrics.csv | cut -f1)"
echo ""

# Show recent data
echo "=================================================="
echo "Recent Data (Last 10 steps)"
echo "=================================================="
tail -11 /tmp/tube_mpc_metrics.csv | column -t -s,
echo ""

# Calculate basic stats
echo "=================================================="
echo "Quick Statistics"
echo "=================================================="
awk -F',' 'NR>1 {
    sum_cte += $2; sum_etheta += $3; sum_vel += $4; sum_solve += $12
    sum_cost += $14; count++
} END {
    printf "Average CTE:          %.6f m\n", sum_cte/count
    printf "Average Heading Error: %.6f rad (%.2f°)\n", sum_etheta/count, sum_etheta/count*180/3.14159
    printf "Average Velocity:     %.3f m/s\n", sum_vel/count
    printf "Average Solve Time:   %.2f ms\n", sum_solve/count
    printf "Average Cost:         %.3f\n", sum_cost/count
    printf "Total Steps:          %d\n", count
}' /tmp/tube_mpc_metrics.csv
echo ""

# Check for report
if [ -f /tmp/tube_mpc_metrics_report.txt ]; then
    echo "✅ Full analysis report available:"
    echo "   📄 /tmp/tube_mpc_metrics_report.txt"
    echo ""
    echo "View full report with: cat /tmp/tube_mpc_metrics_report.txt"
else
    echo "💡 Generate full analysis report with:"
    echo "   ./analyze_metrics_simple.py /tmp/tube_mpc_metrics.csv"
fi

echo ""
