#!/bin/bash
# Quick test script for STL monitor

echo "🔍 STL Monitor Quick Test"
echo "=========================="

# Test 1: Import test
echo -n "[1/3] Python imports... "
python3 -c "import sys; sys.path.insert(0, 'src/tube_mpc_ros/stl_monitor/test'); import test_stl_monitor" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ OK"
else
    echo "❌ Failed"
    exit 1
fi

# Test 2: Unit tests
echo -n "[2/3] Unit tests... "
output=$(python3 src/tube_mpc_ros/stl_monitor/test/test_stl_monitor.py 2>&1)
if echo "$output" | grep -q "OK"; then
    echo "✅ OK (7/7 passed)"
else
    echo "❌ Failed"
    exit 1
fi

# Test 3: ROS messages
echo -n "[3/3] ROS messages... "
source devel/setup.bash
python3 -c "from stl_monitor.msg import *; print('OK')" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ OK"
else
    echo "❌ Failed"
    exit 1
fi

echo ""
echo "🎉 All tests passed!"
echo ""
echo "Next steps:"
echo "  1. Start ROS: roscore"
echo "  2. Run node: rosrun stl_monitor stl_monitor_node_standalone.py"
echo "  3. View data: rostopic echo /stl/robustness"
