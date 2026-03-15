#!/bin/bash
# Test script for Safe-Regret MPC launch
# This validates the launch file without requiring a full ROS environment

echo "========================================"
echo "Safe-Regret MPC Launch Test"
echo "========================================"

cd /home/dixon/Final_Project/catkin

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

test_item() {
    local test_name="$1"
    local test_command="$2"

    echo -n "Testing: $test_name ... "
    if eval "$test_command" > /dev/null 2>&1; then
        echo "✓ PASSED"
        ((TESTS_PASSED++))
        return 0
    else
        echo "✗ FAILED"
        ((TESTS_FAILED++))
        return 1
    fi
}

echo ""
echo "--- Phase 1: Launch File Validation ---"

test_item "Launch file exists" "test -f src/STL_ros/launch/safe_regret_mpc.launch"
test_item "Minimal launch exists" "test -f src/STL_ros/launch/minimal_safe_regret_mpc.launch"
test_item "STL monitor launch exists" "test -f src/STL_ros/launch/stl_monitor.launch"

echo ""
echo "--- Phase 2: Launch File Syntax ---"

test_item "Safe-Regret MPC XML syntax" "xmllint --noout src/STL_ros/launch/safe_regret_mpc.launch"
test_item "Minimal launch XML syntax" "xmllint --noout src/STL_ros/launch/minimal_safe_regret_mpc.launch"
test_item "STL monitor XML syntax" "xmllint --noout src/STL_ros/launch/stl_monitor.launch"

echo ""
echo "--- Phase 3: Launch File References ---"

test_item "STL params file" "test -f src/STL_ros/params/stl_params.yaml"
test_item "Formulas config file" "test -f src/STL_ros/params/formulas.yaml"
test_item "Tube MPC params file" "test -f src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml"

echo ""
echo "--- Phase 4: Node Executables ---"

test_item "Tube MPC node exists" "test -x devel/lib/tube_mpc_ros/tube_TubeMPC_Node"
test_item "STL monitor script exists" "test -f src/STL_ros/scripts/stl_monitor.py"
test_item "STL monitor script executable" "test -x src/STL_ros/scripts/stl_monitor.py"

echo ""
echo "--- Phase 5: Package Dependencies ---"

source devel/setup.bash
test_item "stl_ros package" "rospack find stl_ros"
test_item "tube_mpc_ros package" "rospack find tube_mpc_ros"
test_item "rostopic available" "which rostopic"
test_item "rviz available" "which rviz"

echo ""
echo "--- Phase 6: Message System ---"

test_item "STL messages generated" "rosmsg show stl_ros/STLRobustness > /dev/null"
test_item "STL budget message" "rosmsg show stl_ros/STLBudget > /dev/null"
test_item "STL formula message" "rosmsg show stl_ros/STLFormula > /dev/null"

echo ""
echo "========================================"
echo "Launch Test Results"
echo "========================================"
echo "PASSED: $TESTS_PASSED"
echo "FAILED: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo ""
    echo "✓ ALL TESTS PASSED - Launch file is ready!"
    echo ""
    echo "To launch Safe-Regret MPC:"
    echo "  roslaunch stl_ros minimal_safe_regret_mpc.launch enable_stl:=true"
    echo ""
    echo "Note: This requires a running ROS core and other dependencies."
    echo "For testing, consider launching components individually."
    exit 0
else
    echo ""
    echo "✗ SOME TESTS FAILED - Please check the errors above"
    exit 1
fi
