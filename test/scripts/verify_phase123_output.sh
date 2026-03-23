#!/bin/bash
# Phase 1-3 Output Verification Script
# Tests Tube MPC, STL Monitor, and DR Tightening integration

set -e

echo "=========================================="
echo "Safe-Regret MPC - Phase 1-3 Verification"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASSED=0
FAILED=0
WARNINGS=0

# Helper functions
pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASSED++))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAILED++))
}

warn() {
    echo -e "${YELLOW}⚠ WARN${NC}: $1"
    ((WARNINGS++))
}

info() {
    echo -e "${NC}  $1"
}

# Test functions
test_executable_exists() {
    local exe=$1
    local name=$2
    if [ -f "$exe" ]; then
        pass "$name executable exists: $exe"
        return 0
    else
        fail "$name executable NOT found: $exe"
        return 1
    fi
}

test_library_exists() {
    local lib=$1
    local name=$2
    if [ -f "$lib" ]; then
        pass "$name library exists: $lib"
        return 0
    else
        fail "$name library NOT found: $lib"
        return 1
    fi
}

test_launch_file_exists() {
    local launch=$1
    local name=$2
    if [ -f "$launch" ]; then
        pass "$name launch file exists: $launch"
        return 0
    else
        fail "$name launch file NOT found: $launch"
        return 1
    fi
}

test_param_file_exists() {
    local param=$1
    local name=$2
    if [ -f "$param" ]; then
        pass "$name param file exists: $param"
        return 0
    else
        fail "$name param file NOT found: $param"
        return 1
    fi
}

check_topic_publisher() {
    local topic=$1
    local type=$2
    local source=$3

    info "Checking if $source publishes to $topic"
    grep -r "advertise.*$topic" /home/dixon/Final_Project/catkin/src/$source 2>/dev/null > /dev/null
    if [ $? -eq 0 ]; then
        pass "$source advertises $topic ($type)"
        return 0
    else
        fail "$source does NOT advertise $topic"
        return 1
    fi
}

check_topic_subscriber() {
    local topic=$1
    local type=$2
    local source=$3

    info "Checking if $source subscribes to $topic"
    grep -r "subscribe.*$topic" /home/dixon/Final_Project/catkin/src/$source 2>/dev/null > /dev/null
    if [ $? -eq 0 ]; then
        pass "$source subscribes to $topic ($type)"
        return 0
    else
        fail "$source does NOT subscribe to $topic"
        return 1
    fi
}

echo "=========================================="
echo "PHASE 1: Tube MPC"
echo "=========================================="
echo ""

# Check executables
test_executable_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node" \
    "Tube MPC Node"

test_executable_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_nav_mpc" \
    "Standard MPC Node"

test_library_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/libtube_mpc_lib.so" \
    "Tube MPC Library"

# Check launch files
test_launch_file_exists \
    "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/launch/tube_mpc_navigation.launch" \
    "Tube MPC Navigation"

test_launch_file_exists \
    "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/launch/tube_mpc_simple.launch" \
    "Tube MPC Simple"

# Check param files
test_param_file_exists \
    "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml" \
    "Tube MPC Params"

echo ""
echo "=========================================="
echo "PHASE 2: STL Monitor"
echo "=========================================="
echo ""

# Check executables
test_executable_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/stl_ros/stl_monitor.py" \
    "STL Monitor Node"

# Check launch files
test_launch_file_exists \
    "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/launch/stl_monitor.launch" \
    "STL Monitor"

test_launch_file_exists \
    "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/launch/stl_integration_test.launch" \
    "STL Integration Test"

# Check source files
if [ -f "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.py" ]; then
    pass "STL Monitor Node implementation exists"
else
    fail "STL Monitor Node implementation NOT found"
fi

# Check STL modules
stl_modules=(
    "STLParser.cpp"
    "SmoothRobustness.cpp"
    "BeliefSpaceEvaluator.cpp"
    "RobustnessBudget.cpp"
)

for module in "${stl_modules[@]}"; do
    if [ -f "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/src/$module" ]; then
        pass "STL Module: $module"
    else
        warn "STL Module NOT found: $module"
    fi
done

echo ""
echo "=========================================="
echo "PHASE 3: DR Tightening"
echo "=========================================="
echo ""

# Check executables
test_executable_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/dr_tightening/dr_tightening_node" \
    "DR Tightening Node"

test_executable_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/dr_tightening/dr_tightening_test" \
    "DR Tightening Test"

test_library_exists \
    "/home/dixon/Final_Project/catkin/devel/lib/libdr_tightening.so" \
    "DR Tightening Library"

# Check launch files
test_launch_file_exists \
    "/home/dixon/Final_Project/catkin/src/dr_tightening/launch/dr_tightening.launch" \
    "DR Tightening"

test_launch_file_exists \
    "/home/dixon/Final_Project/catkin/src/dr_tightening/launch/dr_tube_mpc_integrated.launch" \
    "DR + Tube MPC Integrated"

# Check param files
test_param_file_exists \
    "/home/dixon/Final_Project/catkin/src/dr_tightening/params/dr_tightening_params.yaml" \
    "DR Tightening Params"

# Check DR modules
dr_modules=(
    "ResidualCollector.cpp"
    "AmbiguityCalibrator.cpp"
    "TighteningComputer.cpp"
    "SafetyLinearization.cpp"
    "dr_tightening_node.cpp"
)

for module in "${dr_modules[@]}"; do
    if [ -f "/home/dixon/Final_Project/catkin/src/dr_tightening/src/$module" ]; then
        pass "DR Module: $module"
    else
        fail "DR Module NOT found: $module"
    fi
done

echo ""
echo "=========================================="
echo "INTEGRATION: Topic Interfaces"
echo "=========================================="
echo ""

# Tube MPC -> DR Tightening
echo "Tube MPC → DR Tightening:"
check_topic_publisher \
    "/tube_mpc/tracking_error" \
    "std_msgs/Float64MultiArray" \
    "tube_mpc_ros/mpc_ros"

check_topic_publisher \
    "/mpc_trajectory" \
    "nav_msgs/Path" \
    "tube_mpc_ros/mpc_ros"

check_topic_subscriber \
    "/tube_mpc/tracking_error" \
    "std_msgs/Float64MultiArray" \
    "dr_tightening"

check_topic_subscriber \
    "/mpc_trajectory" \
    "nav_msgs/Path" \
    "dr_tightening"

# DR Tightening -> Tube MPC (feedback)
echo ""
echo "DR Tightening → Tube MPC:"
check_topic_publisher \
    "/dr_margins" \
    "std_msgs/Float64MultiArray" \
    "dr_tightening"

# STL Monitor topics
echo ""
echo "STL Monitor Topics:"
check_topic_publisher \
    "/stl_robustness" \
    "std_msgs/Float32" \
    "stl_monitor"

check_topic_publisher \
    "/stl_budget" \
    "std_msgs/Float32" \
    "stl_monitor"

echo ""
echo "=========================================="
echo "BUILD CONFIGURATION"
echo "=========================================="
echo ""

# Check CMakeLists.txt
if [ -f "/home/dixon/Final_Project/catkin/src/dr_tightening/CMakeLists.txt" ]; then
    pass "DR Tightening CMakeLists.txt exists"

    # Check for main targets
    if grep -q "add_executable(dr_tightening_node" /home/dixon/Final_Project/catkin/src/dr_tightening/CMakeLists.txt; then
        pass "CMake: dr_tightening_node target defined"
    else
        fail "CMake: dr_tightening_node target NOT defined"
    fi

    if grep -q "add_executable(dr_tightening_test" /home/dixon/Final_Project/catkin/src/dr_tightening/CMakeLists.txt; then
        pass "CMake: dr_tightening_test target defined"
    else
        fail "CMake: dr_tightening_test target NOT defined"
    fi
else
    fail "DR Tightening CMakeLists.txt NOT found"
fi

# Check package.xml
if [ -f "/home/dixon/Final_Project/catkin/src/dr_tightening/package.xml" ]; then
    pass "DR Tightening package.xml exists"
else
    fail "DR Tightening package.xml NOT found"
fi

echo ""
echo "=========================================="
echo "DOCUMENTATION"
echo "=========================================="
echo ""

# Check README files
if [ -f "/home/dixon/Final_Project/catkin/src/dr_tightening/README.md" ]; then
    pass "DR Tightening README.md exists"
else
    warn "DR Tightening README.md NOT found"
fi

# Check PROJECT_ROADMAP
if [ -f "/home/dixon/Final_Project/catkin/PROJECT_ROADMAP.md" ]; then
    pass "PROJECT_ROADMAP.md exists"
else
    fail "PROJECT_ROADMAP.md NOT found"
fi

# Check CLAUDE.md
if [ -f "/home/dixon/Final_Project/catkin/CLAUDE.md" ]; then
    pass "CLAUDE.md exists"
else
    fail "CLAUDE.md NOT found"
fi

echo ""
echo "=========================================="
echo "SUMMARY"
echo "=========================================="
echo ""
echo -e "Total Passed: ${GREEN}$PASSED${NC}"
echo -e "Total Failed: ${RED}$FAILED${NC}"
echo -e "Total Warnings: ${YELLOW}$WARNINGS${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}=========================================="
    echo "✓ ALL CRITICAL CHECKS PASSED!"
    echo "==========================================${NC}"
    echo ""
    echo "Phase 1-3 components are properly built and configured."
    echo "Ready for integration testing and Phase 4 development."
    echo ""
    echo "Next steps:"
    echo "1. Run individual nodes: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
    echo "2. Run integration test: roslaunch dr_tightening dr_tube_mpc_integrated.launch"
    echo "3. Check ROS topics: rostopic list | grep -E 'tube_mpc|dr_|stl_'"
    echo "4. Monitor messages: rostopic echo /tube_mpc/tracking_error"
    exit 0
else
    echo -e "${RED}=========================================="
    echo "✗ SOME CRITICAL CHECKS FAILED"
    echo "==========================================${NC}"
    echo ""
    echo "Please fix the failed checks before proceeding."
    echo "Most failures are due to missing build artifacts or source files."
    echo ""
    echo "Suggested fixes:"
    echo "1. Rebuild: cd /home/dixon/Final_Project/catkin && catkin_make"
    echo "2. Check source: ls -la src/dr_tightening/src/"
    echo "3. Verify install: ls -la devel/lib/"
    exit 1
fi
