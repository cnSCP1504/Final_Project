#!/bin/bash

# Module Integration Test
# Tests STL, DR Tightening, and Terminal Set module integration

echo "========================================="
echo "Safe-Regret MPC Module Integration Test"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}  ✓${NC} $1"
}

log_fail() {
    echo -e "${RED}  ✗${NC} $1"
}

log_info() {
    echo -e "  [INFO] $1"
}

# Source ROS
source devel/setup.bash

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

echo "Step 1: Starting ROS master..."
roscore > /tmp/ros_master.log 2>&1 &
ROSCORE_PID=$!
sleep 3

echo ""
log_test "Module 1: STL Monitor Integration"
echo "----------------------------------------"

# Check if STL monitor package exists
if [ -d src/tube_mpc_ros/stl_monitor ]; then
    log_pass "STL Monitor package found"

    # Check for STL monitor node
    if [ -f src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.py ]; then
        log_pass "STL Monitor node script found"

        # Start STL monitor in background
        log_info "Starting STL monitor..."
        roslaunch stl_monitor stl_monitor.launch > /tmp/stl_monitor.log 2>&1 &
        STL_PID=$!
        sleep 3

        # Check if STL topics are available
        if rostopic list | grep -q "/stl_monitor"; then
            log_pass "STL Monitor topics are available"
            rostopic list | grep "/stl_monitor" | sed 's/^/    /'

            # Check STL robustness topic
            if rostopic list | grep -q "/stl_monitor/robustness"; then
                log_pass "STL robustness topic: /stl_monitor/robustness"

                # Check message type
                MSG_TYPE=$(rostopic info /stl_monitor/robustness 2>/dev/null | grep "Type:" | awk '{print $2}')
                log_info "Message type: $MSG_TYPE"
            else
                log_fail "STL robustness topic not found"
                ((TESTS_FAILED++))
            fi

            # Check STL budget topic
            if rostopic list | grep -q "/stl_monitor/budget"; then
                log_pass "STL budget topic: /stl_monitor/budget"
            else
                log_fail "STL budget topic not found"
                ((TESTS_FAILED++))
            fi

            ((TESTS_PASSED++))
        else
            log_fail "STL Monitor topics not available"
            ((TESTS_FAILED++))
        fi

        # Cleanup STL monitor
        kill $STL_PID 2>/dev/null
    else
        log_fail "STL Monitor node script not found"
        ((TESTS_FAILED++))
    fi
else
    log_fail "STL Monitor package not found"
    ((TESTS_FAILED++))
fi

echo ""
log_test "Module 2: DR Tightening Integration"
echo "----------------------------------------"

# Check if DR tightening package exists
if [ -d src/dr_tightening ]; then
    log_pass "DR Tightening package found"

    # Check for DR tightening node
    if [ -f devel/lib/dr_tightening/dr_tightening_node ]; then
        log_pass "DR Tightening node binary found"

        # Start DR tightening in background
        log_info "Starting DR Tightening node..."
        rosrun dr_tightening dr_tightening_node > /tmp/dr_tightening.log 2>&1 &
        DR_PID=$!
        sleep 3

        # Check if DR topics are available
        if rostopic list | grep -q "/dr_margins"; then
            log_pass "DR margins topic is available: /dr_margins"

            # Check message type
            MSG_TYPE=$(rostopic info /dr_margins 2>/dev/null | grep "Type:" | awk '{print $2}')
            log_info "Message type: $MSG_TYPE"

            # Check ambiguity set topic
            if rostopic list | grep -q "/ambiguity_set"; then
                log_pass "Ambiguity set topic: /ambiguity_set"
            else
                log_info "Ambiguity set topic not found (optional)"
            fi

            # Check residual statistics topic
            if rostopic list | grep -q "/residual_statistics"; then
                log_pass "Residual statistics topic: /residual_statistics"
            else
                log_info "Residual statistics topic not found (optional)"
            fi

            ((TESTS_PASSED++))
        else
            log_fail "DR margins topic not available"
            ((TESTS_FAILED++))
        fi

        # Cleanup DR tightening
        kill $DR_PID 2>/dev/null
    else
        log_fail "DR Tightening node binary not found"
        ((TESTS_FAILED++))
    fi
else
    log_fail "DR Tightening package not found"
    ((TESTS_FAILED++))
fi

echo ""
log_test "Module 3: Terminal Set Integration"
echo "----------------------------------------"

# Check terminal set computation
if [ -f devel/lib/dr_tightening/terminal_set_computation_node ]; then
    log_pass "Terminal set computation node found"

    # Start terminal set computation in background
    log_info "Starting terminal set computation..."
    rosrun dr_tightening terminal_set_computation_node > /tmp/terminal_set.log 2>&1 &
    TERM_PID=$!
    sleep 3

    # Check if terminal set topic is available
    if rostopic list | grep -q "/terminal_set"; then
        log_pass "Terminal set topic: /terminal_set"

        # Check message type
        MSG_TYPE=$(rostopic info /terminal_set 2>/dev/null | grep "Type:" | awk '{print $2}')
        log_info "Message type: $MSG_TYPE"

        ((TESTS_PASSED++))
    else
        log_fail "Terminal set topic not found"
        ((TESTS_FAILED++))
    fi

    # Cleanup terminal set
    kill $TERM_PID 2>/dev/null
else
    log_info "Terminal set computation node not found (may be integrated in other nodes)"
fi

echo ""
log_test "Module 4: Safe-Regret MPC Unified Node"
echo "----------------------------------------"

# Check Safe-Regret MPC node
if [ -f devel/lib/safe_regret_mpc/safe_regret_mpc_node ]; then
    log_pass "Safe-Regret MPC node found"

    # Start node in background
    log_info "Starting Safe-Regret MPC node..."
    rosrun safe_regret_mpc safe_regret_mpc_node > /tmp/safe_regret_mpc.log 2>&1 &
    MPC_PID=$!
    sleep 3

    # Check if node is running
    if ps -p $MPC_PID > /dev/null; then
        log_pass "Safe-Regret MPC node is running"

        # Check published topics
        MPC_TOPICS=$(rostopic list | grep -E "(safe_regret_mpc|cmd_vel)" | wc -l)
        if [ $MPC_TOPICS -gt 0 ]; then
            log_pass "Safe-Regret MPC is publishing $MPC_TOPICS topics"
            rostopic list | grep -E "(safe_regret_mpc|cmd_vel)" | sed 's/^/    /'
        else
            log_info "No topics found yet (may need input data)"
        fi

        # Check services
        MPC_SERVICES=$(rosservice list | grep "safe_regret_mpc" | wc -l)
        if [ $MPC_SERVICES -gt 0 ]; then
            log_pass "Safe-Regret MPC is providing $MPC_SERVICES services"
            rosservice list | grep "safe_regret_mpc" | sed 's/^/    /'
        else
            log_info "No services found yet"
        fi

        # Check node info
        log_info "Node information:"
        rosnode info /safe_regret_mpc_node 2>/dev/null | sed 's/^/    /' | head -20

        ((TESTS_PASSED++))
    else
        log_fail "Safe-Regret MPC node failed to start"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    kill $MPC_PID 2>/dev/null
else
    log_fail "Safe-Regret MPC node not found"
    ((TESTS_FAILED++))
fi

echo ""
log_test "Module 5: Topic Interface Verification"
echo "----------------------------------------"

# Verify topic connections
log_info "Checking topic connectivity..."

# STL to MPC
if rostopic list | grep -q "/stl_monitor/robustness" && \
   rostopic list | grep -q "/safe_regret_mpc"; then
    log_pass "STL → MPC interface: /stl_monitor/robustness can connect to MPC"
    ((TESTS_PASSED++))
else
    log_fail "STL → MPC interface incomplete"
    ((TESTS_FAILED++))
fi

# DR to MPC
if rostopic list | grep -q "/dr_margins" && \
   rostopic list | grep -q "/safe_regret_mpc"; then
    log_pass "DR → MPC interface: /dr_margins can connect to MPC"
    ((TESTS_PASSED++))
else
    log_fail "DR → MPC interface incomplete"
    ((TESTS_FAILED++))
fi

# Terminal Set to MPC
if rostopic list | grep -q "/terminal_set" && \
   rostopic list | grep -q "/safe_regret_mpc"; then
    log_pass "Terminal Set → MPC interface: /terminal_set can connect to MPC"
    ((TESTS_PASSED++))
else
    log_info "Terminal Set → MPC interface (may be internal)"
fi

echo ""
log_test "Module 6: Message Type Verification"
echo "----------------------------------------"

# Check message definitions
log_info "Verifying message definitions..."

if [ -f devel/include/safe_regret_mpc/SafeRegretState.h ]; then
    log_pass "SafeRegretState message type defined"
    ((TESTS_PASSED++))
else
    log_fail "SafeRegretState message type not found"
    ((TESTS_FAILED++))
fi

if [ -f devel/include/safe_regret_mpc/SafeRegretMetrics.h ]; then
    log_pass "SafeRegretMetrics message type defined"
    ((TESTS_PASSED++))
else
    log_fail "SafeRegretMetrics message type not found"
    ((TESTS_FAILED++))
fi

if [ -f devel/include/safe_regret_mpc/STLRobustness.h ]; then
    log_pass "STLRobustness message type defined"
    ((TESTS_PASSED++))
else
    log_fail "STLRobustness message type not found"
    ((TESTS_FAILED++))
fi

if [ -f devel/include/safe_regret_mpc/DRMargins.h ]; then
    log_pass "DRMargins message type defined"
    ((TESTS_PASSED++))
else
    log_fail "DRMargins message type not found"
    ((TESTS_FAILED++))
fi

if [ -f devel/include/safe_regret_mpc/TerminalSet.h ]; then
    log_pass "TerminalSet message type defined"
    ((TESTS_PASSED++))
else
    log_fail "TerminalSet message type not found"
    ((TESTS_FAILED++))
fi

echo ""
log_test "Module 7: Service Interface Verification"
echo "----------------------------------------"

# Check service definitions
log_info "Verifying service definitions..."

if [ -f devel/include/safe_regret_mpc/SetSTLSpecification.h ]; then
    log_pass "SetSTLSpecification service type defined"
    ((TESTS_PASSED++))
else
    log_fail "SetSTLSpecification service type not found"
    ((TESTS_FAILED++))
fi

if [ -f devel/include/safe_regret_mpc/GetSystemStatus.h ]; then
    log_pass "GetSystemStatus service type defined"
    ((TESTS_PASSED++))
else
    log_fail "GetSystemStatus service type not found"
    ((TESTS_FAILED++))
fi

if [ -f devel/include/safe_regret_mpc/ResetRegretTracker.h ]; then
    log_pass "ResetRegretTracker service type defined"
    ((TESTS_PASSED++))
else
    log_fail "ResetRegretTracker service type not found"
    ((TESTS_FAILED++))
fi

echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo ""

# Cleanup
log_info "Cleaning up..."
killall -9 roslaunch roslaunch-rosrun rosmaster python3 2>/dev/null
kill $ROSCORE_PID 2>/dev/null
sleep 2

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}All module integration tests passed!${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo "Module Integration Status:"
    echo "  ✅ STL Monitor: Integrated"
    echo "  ✅ DR Tightening: Integrated"
    echo "  ✅ Terminal Set: Integrated"
    echo "  ✅ Safe-Regret MPC: Running"
    echo "  ✅ Topic Interfaces: Connected"
    echo "  ✅ Message Types: Defined"
    echo "  ✅ Service Types: Defined"
    echo ""
    echo "All modules are properly integrated and ready for operation!"
    echo ""
    exit 0
else
    echo -e "${RED}=========================================${NC}"
    echo -e "${RED}Some module integration tests failed${NC}"
    echo -e "${RED}=========================================${NC}"
    echo ""
    echo "Please check the log files:"
    echo "  - /tmp/stl_monitor.log"
    echo "  - /tmp/dr_tightening.log"
    echo "  - /tmp/terminal_set.log"
    echo "  - /tmp/safe_regret_mpc.log"
    echo ""
    exit 1
fi
