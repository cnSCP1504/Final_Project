#!/bin/bash
# Quick functional test for STL_ros package

echo "========================================"
echo "STL_ros Quick Functional Test"
echo "========================================"

# Source workspace
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo ""
echo "1. Testing package availability..."
if rospack find stl_ros > /dev/null 2>&1; then
    echo "   ✓ Package found: $(rospack find stl_ros)"
else
    echo "   ✗ Package not found!"
    exit 1
fi

echo ""
echo "2. Testing message generation..."
if rosmsg show stl_ros/STLRobustness > /dev/null 2>&1; then
    echo "   ✓ STLRobustness message OK"
else
    echo "   ✗ STLRobustness message failed!"
    exit 1
fi

if rosmsg show stl_ros/STLBudget > /dev/null 2>&1; then
    echo "   ✓ STLBudget message OK"
else
    echo "   ✗ STLBudget message failed!"
    exit 1
fi

if rosmsg show stl_ros/STLFormula > /dev/null 2>&1; then
    echo "   ✓ STLFormula message OK"
else
    echo "   ✗ STLFormula message failed!"
    exit 1
fi

echo ""
echo "3. Testing Python imports..."
if python3 -c "from stl_ros.msg import STLRobustness, STLBudget, STLFormula" 2>/dev/null; then
    echo "   ✓ Python message imports OK"
else
    echo "   ✗ Python message imports failed!"
    exit 1
fi

echo ""
echo "4. Testing Python scripts..."
if python3 -m py_compile src/STL_ros/scripts/stl_monitor.py 2>/dev/null; then
    echo "   ✓ stl_monitor.py syntax OK"
else
    echo "   ✗ stl_monitor.py syntax failed!"
    exit 1
fi

if python3 -m py_compile src/STL_ros/scripts/stl_visualizer.py 2>/dev/null; then
    echo "   ✓ stl_visualizer.py syntax OK"
else
    echo "   ✗ stl_visualizer.py syntax failed!"
    exit 1
fi

echo ""
echo "5. Testing launch files..."
if xmllint --noout src/STL_ros/launch/stl_monitor.launch 2>/dev/null; then
    echo "   ✓ stl_monitor.launch XML OK"
else
    echo "   ✗ stl_monitor.launch XML failed!"
    exit 1
fi

if xmllint --noout src/STL_ros/launch/tube_mpc_with_stl.launch 2>/dev/null; then
    echo "   ✓ tube_mpc_with_stl.launch XML OK"
else
    echo "   ✗ tube_mpc_with_stl.launch XML failed!"
    exit 1
fi

echo ""
echo "6. Testing configuration files..."
if [ -f "src/STL_ros/params/stl_params.yaml" ]; then
    echo "   ✓ stl_params.yaml exists"
else
    echo "   ✗ stl_params.yaml missing!"
    exit 1
fi

if [ -f "src/STL_ros/params/formulas.yaml" ]; then
    echo "   ✓ formulas.yaml exists"
else
    echo "   ✗ formulas.yaml missing!"
    exit 1
fi

echo ""
echo "========================================"
echo "All tests PASSED! ✓"
echo "========================================"
echo ""
echo "Package is ready to use!"
echo ""
echo "To launch STL monitor:"
echo "  roslaunch stl_ros stl_monitor.launch"
echo ""
echo "To test with ROS (requires roscore):"
echo "  roslaunch stl_ros stl_monitor.launch"
echo "  rosrun stl_ros test_stl_ros.py"
echo ""
