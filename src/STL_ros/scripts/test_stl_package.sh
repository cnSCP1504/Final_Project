#!/bin/bash
# Test script for STL_ros package

set -e  # Exit on error

echo "==================================="
echo "STL_ros Package Test Suite"
echo "==================================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0

# Test function
run_test() {
    local test_name=$1
    local test_command=$2

    echo -n "Testing: $test_name ... "
    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Change to workspace directory
cd /home/dixon/Final_Project/catkin

echo -e "\n--- 1. Package Discovery Tests ---"
run_test "Package exists" "rospack find stl_ros"
run_test "Package can be listed" "rospack list | grep stl_ros"

echo -e "\n--- 2. Message Generation Tests ---"
run_test "STLRobustness message" "rosmsg show stl_ros/STLRobustness"
run_test "STLBudget message" "rosmsg show stl_ros/STLBudget"
run_test "STLFormula message" "rosmsg show stl_ros/STLFormula"

echo -e "\n--- 3. Python Import Tests ---"
run_test "Import stl_ros messages" "python3 -c 'import stl_ros.msg'"
run_test "Import STLRobustness" "python3 -c 'from stl_ros.msg import STLRobustness'"
run_test "Import STLBudget" "python3 -c 'from stl_ros.msg import STLBudget'"
run_test "Import STLFormula" "python3 -c 'from stl_ros.msg import STLFormula'"

echo -e "\n--- 4. Python Script Syntax Tests ---"
run_test "stl_monitor.py syntax" "python3 -m py_compile src/STL_ros/scripts/stl_monitor.py"
run_test "stl_visualizer.py syntax" "python3 -m py_compile src/STL_ros/scripts/stl_visualizer.py"

echo -e "\n--- 5. Launch File Tests ---"
run_test "stl_monitor.launch XML syntax" "xmllint --noout src/STL_ros/launch/stl_monitor.launch"
run_test "tube_mpc_with_stl.launch XML syntax" "xmllint --noout src/STL_ros/launch/tube_mpc_with_stl.launch"

echo -e "\n--- 6. Configuration File Tests ---"
run_test "stl_params.yaml exists" "test -f src/STL_ros/params/stl_params.yaml"
run_test "formulas.yaml exists" "test -f src/STL_ros/params/formulas.yaml"

echo -e "\n--- 7. Documentation Tests ---"
run_test "README.md exists" "test -f src/STL_ros/README.md"
run_test "QUICK_START.md exists" "test -f src/STL_ros/QUICK_START.md"
run_test "IMPLEMENTATION_SUMMARY.md exists" "test -f src/STL_ros/IMPLEMENTATION_SUMMARY.md"

echo -e "\n--- 8. Header File Tests ---"
run_test "STLFormula.h exists" "test -f src/STL_ros/include/STL_ros/STLFormula.h"
run_test "SmoothRobustness.h exists" "test -f src/STL_ros/include/STL_ros/SmoothRobustness.h"
run_test "BeliefSpaceEvaluator.h exists" "test -f src/STL_ros/include/STL_ros/BeliefSpaceEvaluator.h"
run_test "RobustnessBudget.h exists" "test -f src/STL_ros/include/STL_ros/RobustnessBudget.h"

echo -e "\n==================================="
echo "Test Results Summary"
echo "==================================="
echo -e "${GREEN}PASSED: $TESTS_PASSED${NC}"
echo -e "${RED}FAILED: $TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi
