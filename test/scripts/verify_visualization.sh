#!/bin/bash

# Visualization System Verification Script

echo "╔════════════════════════════════════════════════════════════╗"
echo "║   Tube MPC Visualization System - Verification             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

PASSED=0
FAILED=0

# Function to check file
check_file() {
    if [ -f "$1" ]; then
        echo "✓ $1"
        ((PASSED++))
        return 0
    else
        echo "✗ $1 (missing)"
        ((FAILED++))
        return 1
    fi
}

# Function to check executable
check_exec() {
    if [ -x "$1" ]; then
        echo "✓ $1 (executable)"
        ((PASSED++))
        return 0
    else
        echo "✗ $1 (not executable)"
        ((FAILED++))
        return 1
    fi
}

echo "Checking core files..."
check_file "src/tube_mpc_benchmark/launch/tube_mpc_visualization.rviz"
check_file "src/tube_mpc_benchmark/launch/visualization_test.launch"
check_file "src/tube_mpc_benchmark/scripts/realtime_visualizer.py"
check_file "src/tube_mpc_benchmark/scripts/data_recorder.py"
check_file "src/tube_mpc_benchmark/scripts/analyze_results.py"
check_file "src/tube_mpc_benchmark/docs/VISUALIZATION_GUIDE.md"
echo ""

echo "Checking executable permissions..."
check_exec "src/tube_mpc_benchmark/scripts/realtime_visualizer.py"
check_exec "src/tube_mpc_benchmark/scripts/data_recorder.py"
check_exec "src/tube_mpc_benchmark/scripts/analyze_results.py"
check_exec "src/tube_mpc_benchmark/scripts/visualize_test.sh"
check_exec "src/tube_mpc_benchmark/scripts/quick_analyze.sh"
check_exec "demo_visualization.sh"
echo ""

echo "Checking Python dependencies..."
python3 -c "import matplotlib" 2>/dev/null && echo "✓ matplotlib" || echo "✗ matplotlib (missing)"
python3 -c "import pandas" 2>/dev/null && echo "✓ pandas" || echo "✗ pandas (missing)"
python3 -c "import seaborn" 2>/dev/null && echo "✓ seaborn" || echo "✗ seaborn (missing)"
python3 -c "import numpy" 2>/dev/null && echo "✓ numpy" || echo "✗ numpy (missing)"
echo ""

echo "Checking data directories..."
mkdir -p /tmp/tube_mpc_data && echo "✓ /tmp/tube_mpc_data (ready)"
echo ""

echo "╔════════════════════════════════════════════════════════════╗"
echo "║                    Verification Summary                     ║"
echo "╠════════════════════════════════════════════════════════════╣"
echo "║  Passed: $PASSED                                              ║"
echo "║  Failed: $FAILED                                              ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ All checks passed! Visualization system is ready."
    echo ""
    echo "Quick start:"
    echo "  1. RViz test:"
    echo "     roslaunch tube_mpc_benchmark visualization_test.launch"
    echo ""
    echo "  2. Interactive demo:"
    echo "     ./demo_visualization.sh"
    echo ""
    echo "  3. Analyze existing data:"
    echo "     ./src/tube_mpc_benchmark/scripts/quick_analyze.sh"
    echo ""
    exit 0
else
    echo "❌ Some checks failed. Please fix the issues above."
    exit 1
fi
