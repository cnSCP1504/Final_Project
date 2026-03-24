#!/bin/bash

# Phase 5 Compilation Verification Script
# Tests the Safe-Regret MPC unified system after successful compilation

echo "========================================="
echo "Phase 5 Compilation Verification Test"
echo "========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Test function
run_test() {
    local test_name=$1
    local test_command=$2

    echo -n "Testing: $test_name ... "

    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

echo "1. Checking compilation artifacts..."
echo "-----------------------------------"

run_test "Library exists (libsafe_regret_mpc_lib.so)" \
    "test -f devel/lib/libsafe_regret_mpc_lib.so"

run_test "Library size > 2MB" \
    "test \$(stat -f%z devel/lib/libsafe_regret_mpc_lib.so 2>/dev/null || stat -c%s devel/lib/libsafe_regret_mpc_lib.so) -gt 2000000"

run_test "Node executable exists (safe_regret_mpc_node)" \
    "test -f devel/lib/safe_regret_mpc/safe_regret_mpc_node"

run_test "Node size > 1MB" \
    "test \$(stat -f%z devel/lib/safe_regret_mpc/safe_regret_mpc_node 2>/dev/null || stat -c%s devel/lib/safe_regret_mpc/safe_regret_mpc_node) -gt 1000000"

echo ""
echo "2. Checking ROS message generation..."
echo "-----------------------------------"

run_test "SafeRegretState.msg generated" \
    "test -f devel/include/safe_regret_mpc/SafeRegretState.h"

run_test "SafeRegretMetrics.msg generated" \
    "test -f devel/include/safe_regret_mpc/SafeRegretMetrics.h"

run_test "STLRobustness.msg generated" \
    "test -f devel/include/safe_regret_mpc/STLRobustness.h"

run_test "DRMargins.msg generated" \
    "test -f devel/include/safe_regret_mpc/DRMargins.h"

run_test "TerminalSet.msg generated" \
    "test -f devel/include/safe_regret_mpc/TerminalSet.h"

echo ""
echo "3. Checking ROS service generation..."
echo "-----------------------------------"

run_test "SetSTLSpecification.srv generated" \
    "test -f devel/include/safe_regret_mpc/SetSTLSpecification.h"

run_test "GetSystemStatus.srv generated" \
    "test -f devel/include/safe_regret_mpc/GetSystemStatus.h"

run_test "ResetRegretTracker.srv generated" \
    "test -f devel/include/safe_regret_mpc/ResetRegretTracker.h"

echo ""
echo "4. Checking package structure..."
echo "-----------------------------------"

run_test "Package directory exists" \
    "test -d src/safe_regret_mpc"

run_test "Header files directory exists" \
    "test -d src/safe_regret_mpc/include/safe_regret_mpc"

run_test "Source files directory exists" \
    "test -d src/safe_regret_mpc/src"

run_test "Launch files exist" \
    "test -f src/safe_regret_mpc/launch/safe_regret_mpc.launch"

run_test "Test launch file exists" \
    "test -f src/safe_regret_mpc/launch/test_safe_regret_mpc.launch"

run_test "Parameter file exists" \
    "test -f src/safe_regret_mpc/params/safe_regret_mpc_params.yaml"

echo ""
echo "5. Checking header files..."
echo "-----------------------------------"

run_test "SafeRegretMPC.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/SafeRegretMPC.hpp"

run_test "STLConstraintIntegrator.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/STLConstraintIntegrator.hpp"

run_test "DRConstraintIntegrator.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/DRConstraintIntegrator.hpp"

run_test "TerminalSetIntegrator.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/TerminalSetIntegrator.hpp"

run_test "RegretTracker.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/RegretTracker.hpp"

run_test "PerformanceMonitor.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/PerformanceMonitor.hpp"

run_test "safe_regret_mpc_node.hpp exists" \
    "test -f src/safe_regret_mpc/include/safe_regret_mpc/safe_regret_mpc_node.hpp"

echo ""
echo "6. Checking source files..."
echo "-----------------------------------"

run_test "SafeRegretMPC.cpp exists" \
    "test -f src/safe_regret_mpc/src/SafeRegretMPC.cpp"

run_test "STLConstraintIntegrator.cpp exists" \
    "test -f src/safe_regret_mpc/src/STLConstraintIntegrator.cpp"

run_test "DRConstraintIntegrator.cpp exists" \
    "test -f src/safe_regret_mpc/src/DRConstraintIntegrator.cpp"

run_test "TerminalSetIntegrator.cpp exists" \
    "test -f src/safe_regret_mpc/src/TerminalSetIntegrator.cpp"

run_test "RegretTracker.cpp exists" \
    "test -f src/safe_regret_mpc/src/RegretTracker.cpp"

run_test "PerformanceMonitor.cpp exists" \
    "test -f src/safe_regret_mpc/src/PerformanceMonitor.cpp"

run_test "safe_regret_mpc_node.cpp exists" \
    "test -f src/safe_regret_mpc/src/safe_regret_mpc_node.cpp"

echo ""
echo "7. Checking message definitions..."
echo "-----------------------------------"

run_test "SafeRegretState.msg exists" \
    "test -f src/safe_regret_mpc/msg/SafeRegretState.msg"

run_test "SafeRegretMetrics.msg exists" \
    "test -f src/safe_regret_mpc/msg/SafeRegretMetrics.msg"

run_test "STLRobustness.msg exists" \
    "test -f src/safe_regret_mpc/msg/STLRobustness.msg"

run_test "DRMargins.msg exists" \
    "test -f src/safe_regret_mpc/msg/DRMargins.msg"

run_test "TerminalSet.msg exists" \
    "test -f src/safe_regret_mpc/msg/TerminalSet.msg"

echo ""
echo "8. Checking service definitions..."
echo "-----------------------------------"

run_test "SetSTLSpecification.srv exists" \
    "test -f src/safe_regret_mpc/srv/SetSTLSpecification.srv"

run_test "GetSystemStatus.srv exists" \
    "test -f src/safe_regret_mpc/srv/GetSystemStatus.srv"

run_test "ResetRegretTracker.srv exists" \
    "test -f src/safe_regret_mpc/srv/ResetRegretTracker.srv"

echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed! Phase 5 compilation successful.${NC}"
    echo ""
    echo "Next steps:"
    echo "1. Test node startup: roslaunch safe_regret_mpc test_safe_regret_mpc.launch"
    echo "2. Run integration tests: ./test_phase5_integration.sh (to be created)"
    echo "3. Complete core MPC solver implementation"
    echo ""
    exit 0
else
    echo -e "${RED}Some tests failed. Please review the compilation.${NC}"
    echo ""
    exit 1
fi
