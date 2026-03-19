#!/bin/bash

# Quick analysis script for Tube MPC data
# Analyzes the most recent navigation test and generates plots

cd /home/dixon/Final_Project/catkin

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Tube MPC Quick Analysis                               ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Find most recent CSV file
CSV_FILE=$(ls -t /tmp/tube_mpc_data/navigation_data_*.csv 2>/dev/null | head -1)

if [ -z "$CSV_FILE" ]; then
    echo "❌ No data files found in /tmp/tube_mpc_data/"
    echo "   Please run a test first:"
    echo "   ./src/tube_mpc_benchmark/scripts/visualize_test.sh"
    exit 1
fi

echo "Found data file: $CSV_FILE"
echo ""

# Generate analysis plots
echo "Generating trajectory analysis..."
python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
    --csv "$CSV_FILE" \
    --output "tube_mpc_trajectory.png"

# Check for MPC metrics
if [ -f "/tmp/tube_mpc_metrics.csv" ]; then
    echo "Generating MPC metrics analysis..."
    python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
        --mpc-csv "/tmp/tube_mpc_metrics.csv" \
        --output "tube_mpc_metrics.png"
fi

echo ""
echo "✓ Analysis complete!"
echo ""
echo "Generated files:"
echo "  - tube_mpc_trajectory.png (Navigation trajectory analysis)"
echo "  - tube_mpc_metrics.png (MPC performance metrics)"
echo ""
echo "To view the plots:"
echo "  eog tube_mpc_trajectory.png tube_mpc_metrics.png"
echo ""
echo "Or open them in your file manager"
