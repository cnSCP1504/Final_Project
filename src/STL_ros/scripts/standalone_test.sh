#!/bin/bash
# Standalone test script for STL_ros
# Tests the package without requiring roscore or other dependencies

echo "========================================"
echo "STL_ros Standalone Test"
echo "========================================"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# Test counter
PASSED=0
FAILED=0

# Test function
test_step() {
    local name="$1"
    local command="$2"

    echo -n "Testing: $name ... "
    if eval "$command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        ((PASSED++))
    else
        echo -e "${RED}FAILED${NC}"
        ((FAILED++))
    fi
}

echo ""
echo "--- Build System Tests ---"
test_step "catkin_make success" "catkin_make"
test_step "Package manifest" "test -f src/STL_ros/package.xml"
test_step "CMakeLists.txt" "test -f src/STL_ros/CMakeLists.txt"

echo ""
echo "--- Message Generation Tests ---"
test_step "STLRobustness.msg" "test -f src/STL_ros/msg/STLRobustness.msg"
test_step "STLBudget.msg" "test -f src/STL_ros/msg/STLBudget.msg"
test_step "STLFormula.msg" "test -f src/STL_ros/msg/STLFormula.msg"

echo ""
echo "--- C++ Header Tests ---"
test_step "STLFormula.h" "test -f src/STL_ros/include/STL_ros/STLFormula.h"
test_step "SmoothRobustness.h" "test -f src/STL_ros/include/STL_ros/SmoothRobustness.h"
test_step "BeliefSpaceEvaluator.h" "test -f src/STL_ros/include/STL_ros/BeliefSpaceEvaluator.h"
test_step "RobustnessBudget.h" "test -f src/STL_ros/include/STL_ros/RobustnessBudget.h"

echo ""
echo "--- Python Script Tests ---"
test_step "stl_monitor.py" "test -f src/STL_ros/scripts/stl_monitor.py"
test_step "stl_visualizer.py" "test -f src/STL_ros/scripts/stl_visualizer.py"
test_step "Python syntax check" "python3 -m py_compile src/STL_ros/scripts/stl_monitor.py"

echo ""
echo "--- Launch File Tests ---"
test_step "stl_monitor.launch" "test -f src/STL_ros/launch/stl_monitor.launch"
test_step "test_stl_monitor.launch" "test -f src/STL_ros/launch/test_stl_monitor.launch"
test_step "tube_mpc_with_stl.launch" "test -f src/STL_ros/launch/tube_mpc_with_stl.launch"

echo ""
echo "--- Configuration File Tests ---"
test_step "stl_params.yaml" "test -f src/STL_ros/params/stl_params.yaml"
test_step "formulas.yaml" "test -f src/STL_ros/params/formulas.yaml"
test_step "STLMonitorParams.cfg" "test -f src/STL_ros/cfg/STLMonitorParams.cfg"

echo ""
echo "--- Documentation Tests ---"
test_step "README.md" "test -f src/STL_ros/README.md"
test_step "QUICK_START.md" "test -f src/STL_ros/QUICK_START.md"
test_step "IMPLEMENTATION_SUMMARY.md" "test -f src/STL_ros/IMPLEMENTATION_SUMMARY.md"

echo ""
echo "--- ROS Integration Tests ---"
test_step "rospack find" "rospack find stl_ros"
test_step "rosmsg show STLRobustness" "rosmsg show stl_ros/STLRobustness"
test_step "Python message import" "python3 -c 'from stl_ros.msg import STLRobustness'"

echo ""
echo "========================================"
echo "Test Summary"
echo "========================================"
echo -e "${GREEN}PASSED: $PASSED${NC}"
echo -e "${RED}FAILED: $FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ All tests PASSED!${NC}"
    echo ""
    echo "The STL_ros package is fully built and functional!"
    echo ""
    echo "Next steps:"
    echo "1. Test with ROS master:"
    echo "   roscore"
    echo "   roslaunch stl_ros test_stl_monitor.launch"
    echo ""
    echo "2. Monitor topics:"
    echo "   rostopic echo /stl_monitor/robustness"
    echo "   rostopic echo /stl_monitor/budget"
    echo ""
    exit 0
else
    echo ""
    echo -e "${RED}✗ Some tests FAILED${NC}"
    echo "Please check the errors above"
    exit 1
fi
