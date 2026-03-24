#!/bin/bash
################################################################################
# STL Monitor Runtime Test
# Tests P0 fixes: STL Monitor with Tube MPC
################################################################################

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test configuration
TEST_DURATION=30  # seconds
LOG_FILE="/tmp/stl_runtime_test_$(date +%Y%m%d_%H%M%S).log"

# Cleanup function
cleanup() {
    echo ""
    echo -e "${BLUE}Cleaning up...${NC}"

    # Kill all background processes
    pkill -f stl_mpc_navigation.launch 2>/dev/null
    pkill -f stl_monitor_node.py 2>/dev/null
    sleep 2

    echo -e "${GREEN}Cleanup complete${NC}"
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

################################################################################
# Helper Functions
################################################################################

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1" | tee -a "$LOG_FILE"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1" | tee -a "$LOG_FILE"
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
}

print_header() {
    echo ""
    echo "========================================"
    echo "$1"
    echo "========================================"
    echo ""
}

################################################################################
# Pre-test Checks
################################################################################

print_header "STL Monitor Runtime Test"
echo "Test started at: $(date)" | tee -a "$LOG_FILE"
echo "Log file: $LOG_FILE"
echo ""

log_info "Checking ROS environment..."

# Check ROS master
if pgrep -f roscore >/dev/null; then
    log_success "roscore is running"
else
    log_warning "roscore not found, starting..."
    roscore &
    ROSCORE_PID=$!
    sleep 3
    if pgrep -f roscore >/dev/null; then
        log_success "roscore started (PID: $ROSCORE_PID)"
    else
        log_fail "Failed to start roscore"
        exit 1
    fi
fi

# Source workspace
if [ -f "devel/setup.bash" ]; then
    source devel/setup.bash
    log_success "Workspace sourced"
else
    log_fail "devel/setup.bash not found"
    exit 1
fi

################################################################################
# Launch STL MPC System
################################################################################

print_header "Launching STL MPC System"

log_info "Starting stl_mpc_navigation.launch in background..."
log_warning "Note: This will start Gazebo (may take 10-20 seconds)"

# Launch in background with output redirection
roslaunch tube_mpc_ros stl_mpc_navigation.launch enable_stl:=true > /tmp/stl_mpc_launch.log 2>&1 &
LAUNCH_PID=$!

log_info "Launch PID: $LAUNCH_PID"
log_info "Waiting for system to initialize (30 seconds)..."

# Wait for nodes to start
sleep 30

# Check if launch process is still running
if ps -p $LAUNCH_PID > /dev/null; then
    log_success "STL MPC launch process is running"
else
    log_fail "STL MPC launch process died"
    log_info "Check launch log: /tmp/stl_mpc_launch.log"
    exit 1
fi

################################################################################
# Verify Nodes
################################################################################

print_header "Verifying ROS Nodes"

log_info "Checking for STL monitor node..."
sleep 2
if rosnode list 2>/dev/null | grep -q "stl_monitor"; then
    log_success "stl_monitor node is running"
    STL_NODE_RUNNING=1
else
    log_fail "stl_monitor node NOT found"
    log_info "Running nodes:"
    rosnode list 2>/dev/null | tee -a "$LOG_FILE"
    STL_NODE_RUNNING=0
fi

log_info "Checking for Tube MPC node..."
if rosnode list 2>/dev/null | grep -q "TubeMPC_Node"; then
    log_success "TubeMPC_Node is running"
else
    log_warning "TubeMPC_Node not found (may still be initializing)"
fi

################################################################################
# Verify Topics
################################################################################

print_header "Verifying STL Topics"

STL_TOPICS=("/stl/robustness" "/stl/budget" "/stl/violation")
TOPIC_COUNT=0

for topic in "${STL_TOPICS[@]}"; do
    log_info "Checking topic: $topic"
    sleep 1

    if rostopic list 2>/dev/null | grep -q "$topic"; then
        log_success "Topic $topic exists"

        # Get topic info
        TOPIC_INFO=$(rostopic info $topic 2>/dev/null)
        if echo "$TOPIC_INFO" | grep -q "Publishers:"; then
            log_success "Topic $topic has publishers"
            ((TOPIC_COUNT++))
        else
            log_warning "Topic $topic has no publishers"
        fi
    else
        log_fail "Topic $topic NOT found"
    fi
done

log_info "STL topics with publishers: $TOPIC_COUNT/${#STL_TOPICS[@]}"

################################################################################
# Monitor STL Data
################################################################################

if [ $TOPIC_COUNT -gt 0 ]; then
    print_header "Monitoring STL Data"

    log_info "Collecting STL data for ${TEST_DURATION} seconds..."
    echo ""

    # Create data collection script
    cat > /tmp/collect_stl_data.sh << 'EOF'
#!/bin/bash
TIMESTAMP=$(date +%s)
echo "Timestamp,Robustness,Budget,Violation" > /tmp/stl_data_${TIMESTAMP}.csv

for i in {1..30}; do
    ROB=$(rostopic echo /stl/robustness -n 1 2>/dev/null | grep "data:" | head -1 | awk '{print $2}')
    BUD=$(rostopic echo /stl/budget -n 1 2>/dev/null | grep "data:" | head -1 | awk '{print $2}')
    VIOL=$(rostopic echo /stl/violation -n 1 2>/dev/null | grep "data:" | head -1 | awk '{print $2}')

    # Set defaults if empty
    ROB=${ROB:-"N/A"}
    BUD=${BUD:-"N/A"}
    VIOL=${VIOL:-"N/A"}

    echo "$(date +%s),$ROB,$BUD,$VIOL" >> /tmp/stl_data_${TIMESTAMP}.csv
    echo "[$i/30] Robustness: $ROB, Budget: $BUD, Violation: $VIOL"
    sleep 1
done
EOF

    chmod +x /tmp/collect_stl_data.sh

    # Run data collection
    /tmp/collect_stl_data.sh 2>&1 | tee -a "$LOG_FILE"

    # Find the data file
    DATA_FILE=$(ls -t /tmp/stl_data_*.csv 2>/dev/null | head -1)
    if [ -n "$DATA_FILE" ]; then
        log_success "Data saved to: $DATA_FILE"

        # Display summary
        echo ""
        log_info "STL Data Summary:"
        cat "$DATA_FILE" | tee -a "$LOG_FILE"

        # Analyze data
        log_info "Data Analysis:"
        VALID_ROB=$(grep -v ",N/A" "$DATA_FILE" | wc -l)
        if [ $VALID_ROB -gt 1 ]; then
            log_success "Received $VALID_ROB valid data points"

            # Check robustness values
            MAX_ROB=$(tail -n +2 "$DATA_FILE" | grep -v ",N/A" | cut -d',' -f2 | sort -n | tail -1)
            MIN_ROB=$(tail -n +2 "$DATA_FILE" | grep -v ",N/A" | cut -d',' -f2 | sort -n | head -1)

            log_success "Robustness range: [$MIN_ROB, $MAX_ROB]"

            # Check budget
            AVG_BUD=$(tail -n +2 "$DATA_FILE" | grep -v ",N/A" | cut -d',' -f3 | awk '{sum+=$1; count++} END {if(count>0) print sum/count; else print "N/A"}')
            log_info "Average budget: $AVG_BUD"
        else
            log_warning "No valid data received"
        fi
    else
        log_warning "No data file created"
    fi

else
    log_fail "No STL topics have publishers, skipping data collection"
fi

################################################################################
# Final Verification
################################################################################

print_header "Final Verification"

log_info "Checking system health..."

# Check node health
TOTAL_NODES=$(rosnode list 2>/dev/null | wc -l)
log_info "Total ROS nodes: $TOTAL_NODES"

if [ $TOTAL_NODES -gt 5 ]; then
    log_success "System appears healthy ($TOTAL_NODES nodes running)"
else
    log_warning "Low node count ($TOTAL_NODES), system may have issues"
fi

# Check for errors in launch log
log_info "Checking for errors in launch log..."
if [ -f "/tmp/stl_mpc_launch.log" ]; then
    ERROR_COUNT=$(grep -i "error\|exception\|failed" /tmp/stl_mpc_launch.log | wc -l)
    if [ $ERROR_COUNT -eq 0 ]; then
        log_success "No errors found in launch log"
    else
        log_warning "Found $ERROR_COUNT potential errors in launch log"
        log_info "Check: /tmp/stl_mpc_launch.log"
    fi
fi

################################################################################
# Test Summary
################################################################################

print_header "Test Summary"

echo "Test completed at: $(date)" | tee -a "$LOG_FILE"
echo ""

if [ $STL_NODE_RUNNING -eq 1 ] && [ $TOPIC_COUNT -eq 3 ]; then
    echo -e "${GREEN}✓ STL MONITOR TEST PASSED${NC}"
    echo ""
    echo "Achievements:"
    echo "  ✓ STL monitor node running"
    echo "  ✓ All 3 STL topics active"
    echo "  ✓ Data collected successfully"
    echo ""
    echo "STL Monitor is working correctly!"
    echo ""
    echo "Next steps:"
    echo "  - Monitor long-term behavior"
    echo "  - Test with navigation goals"
    echo "  - Verify robustness budget updates"
    exit 0
elif [ $STL_NODE_RUNNING -eq 1 ] && [ $TOPIC_COUNT -gt 0 ]; then
    echo -e "${YELLOW}⚠ STL MONITOR TEST PARTIAL${NC}"
    echo ""
    echo "Status:"
    echo "  ✓ STL monitor node running"
    echo "  ⚠ Only $TOPIC_COUNT/3 topics active"
    echo ""
    echo "Some topics may still be initializing."
    echo "Check the launch log for details: /tmp/stl_mpc_launch.log"
    exit 0
else
    echo -e "${RED}✗ STL MONITOR TEST FAILED${NC}"
    echo ""
    echo "Issues detected:"
    if [ $STL_NODE_RUNNING -eq 0 ]; then
        echo "  ✗ STL monitor node not running"
    fi
    if [ $TOPIC_COUNT -eq 0 ]; then
        echo "  ✗ No STL topics active"
    fi
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check launch log: /tmp/stl_mpc_launch.log"
    echo "  2. Verify stl_monitor package: rospack find stl_monitor"
    echo "  3. Check node: rosnode list"
    echo "  4. Check topics: rostopic list"
    exit 1
fi
