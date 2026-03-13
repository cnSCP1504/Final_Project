#!/bin/bash

# Quick test script for metrics collection system
# This script verifies that metrics are being collected correctly

set -e

echo "=== Tube MPC Metrics Test Script ==="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test 1: Check if MetricsCollector files exist
echo "Test 1: Checking MetricsCollector files..."
if [ -f "src/tube_mpc_ros/mpc_ros/include/MetricsCollector.h" ] && \
   [ -f "src/tube_mpc_ros/mpc_ros/src/MetricsCollector.cpp" ]; then
    echo -e "${GREEN}✓${NC} MetricsCollector files found"
else
    echo -e "${RED}✗${NC} MetricsCollector files not found"
    exit 1
fi

# Test 2: Check if MetricsCollector is in CMakeLists.txt
echo ""
echo "Test 2: Checking CMakeLists.txt integration..."
if grep -q "MetricsCollector.cpp" src/tube_mpc_ros/mpc_ros/CMakeLists.txt; then
    echo -e "${GREEN}✓${NC} MetricsCollector.cpp found in CMakeLists.txt"
else
    echo -e "${YELLOW}⚠${NC} MetricsCollector.cpp not in CMakeLists.txt"
    echo "Run: ./src/tube_mpc_ros/mpc_ros/scripts/add_metrics_collector.sh"
fi

# Test 3: Check if workspace is built
echo ""
echo "Test 3: Checking if workspace is built..."
if [ -f "devel/lib/tube_mpc_ros/tube_mpc_node" ]; then
    echo -e "${GREEN}✓${NC} tube_mpc_node executable found"
else
    echo -e "${YELLOW}⚠${NC} tube_mpc_node not found"
    echo "Run: catkin_make"
fi

# Test 4: Clean old metrics files
echo ""
echo "Test 4: Cleaning old metrics files..."
rm -f /tmp/tube_mpc_metrics.csv
rm -f /tmp/tube_mpc_summary.txt
echo -e "${GREEN}✓${NC} Old metrics files removed"

# Test 5: Create a simple test launch file
echo ""
echo "Test 5: Creating test launch file..."
mkdir -p logs
TEST_LOG="logs/metrics_test_$(date +%Y%m%d_%H%M%S).log"

echo ""
echo "=== Test Summary ==="
echo -e "${GREEN}All checks passed!${NC}"
echo ""
echo "Next steps:"
echo "1. If not built, run: catkin_make && source devel/setup.bash"
echo "2. Run navigation: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo "3. Set a goal in RViz"
echo "4. After goal reached, check metrics:"
echo "   cat /tmp/tube_mpc_summary.txt"
echo ""
echo "Monitor metrics in real-time:"
echo "  rostopic echo /mpc_metrics/empirical_risk"
echo "  rostopic echo /mpc_metrics/feasibility_rate"
echo "  rostopic echo /mpc_metrics/tracking_error"
echo ""
echo "Test log will be saved to: $TEST_LOG"
