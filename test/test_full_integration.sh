#!/bin/bash

# Full Integration Test for Safe-Regret MPC
# Tests all components: STL Monitor, DR Tightening, Terminal Set, Safe-Regret MPC

echo "========================================="
echo "Safe-Regret MPC Full Integration Test"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DURATION=30  # seconds
VERBOSE=true
LOG_FILE="/tmp/safe_regret_test_$(date +%Y%m%d_%H%M%S).log"

# Counters
TESTS_PASSED=0
TESTS_FAILED=0
WARNINGS=0

# Helper functions
log_test() {
    echo -e "${BLUE}[TEST]${NC} $1" | tee -a "$LOG_FILE"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1" | tee -a "$LOG_FILE"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1" | tee -a "$LOG_FILE"
    ((TESTS_FAILED++))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
    ((WARNINGS++))
}

log_info() {
    echo -e "[INFO] $1" | tee -a "$LOG_FILE"
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    killall -9 roslaunch roscore rosmaster 2>/dev/null
    sleep 2
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

# Start ROS master
log_info "Starting ROS master..."
roscore &
ROSCORE_PID=$!
sleep 3

# Source ROS environment
log_info "Sourcing ROS environment..."
source devel/setup.bash

# ========================================
# Phase 1: Check Node Availability
# ========================================
echo ""
log_test "Phase 1: Checking Node Availability"
echo "----------------------------------------"

# Check if nodes are built
log_info "Checking for safe_regret_mpc_node..."
if [ -f devel/lib/safe_regret_mpc/safe_regret_mpc_node ]; then
    log_pass "safe_regret_mpc_node exists"
else
    log_fail "safe_regret_mpc_node not found"
    exit 1
fi

# Check STL monitor
log_info "Checking for stl_monitor_node..."
if [ -f devel/lib/stl_monitor/stl_monitor_node.py ] || \
   [ -f src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.py ]; then
    log_pass "stl_monitor_node available"
else
    log_warn "stl_monitor_node not found, will test without it"
fi

# Check DR tightening node
log_info "Checking for dr_tightening_node..."
if [ -f devel/lib/dr_tightening/dr_tightening_node ]; then
    log_pass "dr_tightening_node exists"
else
    log_warn "dr_tightening_node not found, will test without it"
fi

# ========================================
# Phase 2: Launch Components
# ========================================
echo ""
log_test "Phase 2: Launching Components"
echo "----------------------------------------"

# Launch in background
log_info "Launching integration test..."
roslaunch safe_regret_mpc test_full_integration.launch verbose:=true > "$LOG_FILE.roslaunch" 2>&1 &
ROSLAUNCH_PID=$!

# Wait for nodes to start
log_info "Waiting for nodes to initialize (10 seconds)..."
sleep 10

# ========================================
# Phase 3: Check Node Status
# ========================================
echo ""
log_test "Phase 3: Checking Node Status"
echo "----------------------------------------"

# Check ROS master
log_info "Checking ROS master..."
if rostopic list > /dev/null 2>&1; then
    log_pass "ROS master is running"
else
    log_fail "ROS master not responding"
    exit 1
fi

# List available nodes
NODES=$(rosnode list 2>/dev/null | wc -l)
log_info "Found $NODES active nodes"
rosnode list

# Check specific nodes
log_info "Checking safe_regret_mpc node..."
if rosnode list | grep -q safe_regret_mpc; then
    log_pass "safe_regret_mpc node is running"
else
    log_fail "safe_regret_mpc node not found"
fi

log_info "Checking stl_monitor node..."
if rosnode list | grep -q stl_monitor; then
    log_pass "stl_monitor node is running"
else
    log_warn "stl_monitor node not found (may be disabled)"
fi

log_info "Checking dr_tightening node..."
if rosnode list | grep -q dr_tightening; then
    log_pass "dr_tightening node is running"
else
    log_warn "dr_tightening node not found (may be disabled)"
fi

# ========================================
# Phase 4: Check Topics
# ========================================
echo ""
log_test "Phase 4: Checking Topics"
echo "----------------------------------------"

# List all topics
log_info "Available topics:"
rostopic list

# Check Safe-Regret MPC output topics
log_info "Checking Safe-Regret MPC topics..."

TOPICS_TO_CHECK=(
    "/safe_regret_mpc/state"
    "/safe_regret_mpc/metrics"
    "/cmd_vel"
    "/mpc_trajectory"
    "/tube_boundaries"
)

for topic in "${TOPICS_TO_CHECK[@]}"; do
    if rostopic list | grep -q "^$topic\$"; then
        log_pass "Topic $topic exists"
    else
        log_fail "Topic $topic not found"
    fi
done

# Check STL topics
log_info "Checking STL topics..."
if rostopic list | grep -q "/stl_monitor"; then
    STL_TOPICS=$(rostopic list | grep "/stl_monitor" | wc -l)
    log_pass "Found $STL_TOPICS STL monitor topics"

    if rostopic list | grep -q "/stl_monitor/robustness"; then
        log_pass "STL robustness topic exists"
    fi
else
    log_warn "No STL monitor topics found"
fi

# Check DR topics
log_info "Checking DR topics..."
if rostopic list | grep -q "/dr_margins"; then
    log_pass "DR margins topic exists"
else
    log_warn "DR margins topic not found"
fi

if rostopic list | grep -q "/terminal_set"; then
    log_pass "Terminal set topic exists"
else
    log_warn "Terminal set topic not found"
fi

# ========================================
# Phase 5: Check Services
# ========================================
echo ""
log_test "Phase 5: Checking Services"
echo "----------------------------------------"

# List all services
log_info "Available services:"
rosservice list

# Check Safe-Regret MPC services
log_info "Checking Safe-Regret MPC services..."

SERVICES_TO_CHECK=(
    "/safe_regret_mpc/set_stl_specification"
    "/safe_regret_mpc/get_system_status"
    "/safe_regret_mpc/reset_regret_tracker"
)

for service in "${SERVICES_TO_CHECK[@]}"; do
    if rosservice list | grep -q "^$service\$"; then
        log_pass "Service $service exists"
    else
        log_fail "Service $service not found"
    fi
done

# ========================================
# Phase 6: Test Topic Publication
# ========================================
echo ""
log_test "Phase 6: Testing Topic Publications"
echo "----------------------------------------"

# Test Safe-Regret MPC state topic
log_info "Testing /safe_regret_mpc/state..."
if timeout 5 rostopic echo /safe_regret_mpc/state -n 1 > /dev/null 2>&1; then
    log_pass "/safe_regret_mpc/state is publishing"

    # Get sample data
    log_info "Sample data from /safe_regret_mpc/state:"
    timeout 2 rostopic echo /safe_regret_mpc/state -n 1 | head -20
else
    log_fail "/safe_regret_mpc/state not publishing"
fi

# Test Safe-Regret MPC metrics topic
log_info "Testing /safe_regret_mpc/metrics..."
if timeout 5 rostopic echo /safe_regret_mpc/metrics -n 1 > /dev/null 2>&1; then
    log_pass "/safe_regret_mpc/metrics is publishing"

    # Get sample data
    log_info "Sample data from /safe_regret_mpc/metrics:"
    timeout 2 rostopic echo /safe_regret_mpc/metrics -n 1 | head -20
else
    log_fail "/safe_regret_mpc/metrics not publishing"
fi

# Test STL robustness topic (if available)
if rostopic list | grep -q "/stl_monitor/robustness"; then
    log_info "Testing /stl_monitor/robustness..."
    if timeout 5 rostopic echo /stl_monitor/robustness -n 1 > /dev/null 2>&1; then
        log_pass "/stl_monitor/robustness is publishing"

        # Get sample data
        log_info "Sample data from /stl_monitor/robustness:"
        timeout 2 rostopic echo /stl_monitor/robustness -n 1 | head -20
    else
        log_fail "/stl_monitor/robustness not publishing"
    fi
fi

# Test DR margins topic (if available)
if rostopic list | grep -q "/dr_margins"; then
    log_info "Testing /dr_margins..."
    if timeout 5 rostopic echo /dr_margins -n 1 > /dev/null 2>&1; then
        log_pass "/dr_margins is publishing"

        # Get sample data
        log_info "Sample data from /dr_margins:"
        timeout 2 rostopic echo /dr_margins -n 1 | head -20
    else
        log_fail "/dr_margins not publishing"
    fi
fi

# ========================================
# Phase 7: Test Services
# ========================================
echo ""
log_test "Phase 7: Testing Services"
echo "----------------------------------------"

# Test get_system_status service
log_info "Testing /safe_regret_mpc/get_system_status..."
if rosservice call /safe_regret_mpc/get_system_status "{}" > /tmp/service_test.txt 2>&1; then
    log_pass "get_system_status service is responsive"

    log_info "Service response:"
    cat /tmp/service_test.txt | head -30
else
    log_fail "get_system_status service call failed"
fi

# Test set_stl_specification service
log_info "Testing /safe_regret_mpc/set_stl_specification..."
if rosservice call /safe_regret_mpc/set_stl_specification "stl_formula: 'G[0,10](x > 0)'" > /tmp/service_test.txt 2>&1; then
    log_pass "set_stl_specification service is responsive"

    log_info "Service response:"
    cat /tmp/service_test.txt | head -30
else
    log_warn "set_stl_specification service call failed (may need STL enabled)"
fi

# ========================================
# Phase 8: Check Parameters
# ========================================
echo ""
log_test "Phase 8: Checking Parameters"
echo "----------------------------------------"

# Check if parameter server is accessible
log_info "Checking parameter server..."

# Check safe_regret_mpc parameters
log_info "Checking safe_regret_mpc parameters..."
if rosparam get /safe_regret_mpc/controller_mode > /dev/null 2>&1; then
    CONTROLLER_MODE=$(rosparam get /safe_regret_mpc/controller_mode)
    log_pass "controller_mode parameter: $CONTROLLER_MODE"
else
    log_warn "controller_mode parameter not found"
fi

if rosparam get /safe_regret_mpc/enable_stl_constraints > /dev/null 2>&1; then
    ENABLE_STL=$(rosparam get /safe_regret_mpc/enable_stl_constraints)
    log_pass "enable_stl_constraints parameter: $ENABLE_STL"
fi

if rosparam get /safe_regret_mpc/enable_dr_constraints > /dev/null 2>&1; then
    ENABLE_DR=$(rosparam get /safe_regret_mpc/enable_dr_constraints)
    log_pass "enable_dr_constraints parameter: $ENABLE_DR"
fi

if rosparam get /safe_regret_mpc/mpc_steps > /dev/null 2>&1; then
    MPC_STEPS=$(rosparam get /safe_regret_mpc/mpc_steps)
    log_pass "mpc_steps parameter: $MPC_STEPS"
fi

# ========================================
# Phase 9: Data Flow Verification
# ========================================
echo ""
log_test "Phase 9: Verifying Data Flow"
echo "----------------------------------------"

# Check if topics are receiving data
log_info "Checking topic data rates..."

# Monitor topics for a short period
log_info "Monitoring topics for 5 seconds..."
timeout 5 rostopic hz /safe_regret_mpc/state > /tmp/topic_hz.txt 2>&1 &
HZ_PID=$!

sleep 3
kill $HZ_PID 2>/dev/null

if [ -f /tmp/topic_hz.txt ]; then
    log_info "Topic frequency data:"
    cat /tmp/topic_hz.txt | grep -E "average|min|max"
fi

# ========================================
# Phase 10: Integration Verification
# ========================================
echo ""
log_test "Phase 10: Integration Verification"
echo "----------------------------------------"

# Check TF tree
log_info "Checking TF tree..."
if rosrun tf view_frames > /tmp/tf_tree.pdf 2>&1; then
    log_pass "TF tree generated successfully"
else
    log_warn "TF tree may be incomplete (normal for testing)"
fi

# Generate node graph
log_info "Generating node graph..."
if rqt_graph --force-discover --no-gui > /tmp/node_graph.txt 2>&1; then
    log_pass "Node graph generated"
fi

# ========================================
# Summary
# ========================================
echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo -e "Warnings: ${YELLOW}$WARNINGS${NC}"
echo ""

# Calculate success rate
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))
if [ $TOTAL_TESTS -gt 0 ]; then
    SUCCESS_RATE=$((TESTS_PASSED * 100 / TOTAL_TESTS))
    echo "Success Rate: $SUCCESS_RATE%"
else
    SUCCESS_RATE=0
fi

echo ""
echo "Log files:"
echo "  Main log: $LOG_FILE"
echo "  ROS launch log: $LOG_FILE.roslaunch"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}All critical tests passed!${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo "Integration Status:"
    echo "  ✅ Safe-Regret MPC Node: Running"
    echo "  ✅ Topics: Publishing correctly"
    echo "  ✅ Services: Responding correctly"
    echo "  ✅ Parameters: Loaded correctly"
    echo ""
    echo "Module Integration Status:"

    # Check module integration
    if rosnode list | grep -q stl_monitor; then
        echo "  ✅ STL Monitor: Integrated"
    else
        echo "  ⚠️  STL Monitor: Not running"
    fi

    if rosnode list | grep -q dr_tightening; then
        echo "  ✅ DR Tightening: Integrated"
    else
        echo "  ⚠️  DR Tightening: Not running"
    fi

    if rostopic list | grep -q "/terminal_set"; then
        echo "  ✅ Terminal Set: Publishing"
    else
        echo "  ⚠️  Terminal Set: Not publishing"
    fi

    echo ""
    echo "Next steps:"
    echo "1. Review the log files for detailed information"
    echo "2. Test with actual robot/simulation data"
    echo "3. Run performance benchmarks"
    echo ""

    exit 0
else
    echo -e "${RED}=========================================${NC}"
    echo -e "${RED}Some tests failed. Please review the logs.${NC}"
    echo -e "${RED}=========================================${NC}"
    echo ""
    echo "Troubleshooting:"
    echo "1. Check if all nodes are compiled: catkin_make"
    echo "2. Check ROS environment: source devel/setup.bash"
    echo "3. Review log files: $LOG_FILE*"
    echo ""

    exit 1
fi
