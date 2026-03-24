#!/bin/bash

# Quick Node Startup Test
# Tests if nodes can start without errors

echo "========================================="
echo "Quick Node Startup Test"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Source ROS
source devel/setup.bash

echo "Starting ROS master..."
roscore &
ROSCORE_PID=$!
sleep 3

echo ""
echo "Test 1: Checking if safe_regret_mpc_node exists..."
if [ -f devel/lib/safe_regret_mpc/safe_regret_mpc_node ]; then
    echo -e "${GREEN}✓${NC} safe_regret_mpc_node found"
    ls -lh devel/lib/safe_regret_mpc/safe_regret_mpc_node
else
    echo -e "${RED}✗${NC} safe_regret_mpc_node not found"
    kill $ROSCORE_PID
    exit 1
fi

echo ""
echo "Test 2: Checking if node can start (10 seconds)..."
timeout 10 rosrun safe_regret_mpc safe_regret_mpc_node > /tmp/node_test.log 2>&1 &
NODE_PID=$!

sleep 5

# Check if node is still running
if ps -p $NODE_PID > /dev/null; then
    echo -e "${GREEN}✓${NC} Node started successfully"
    echo "PID: $NODE_PID"

    # Check for errors in log
    if grep -i "error\|fatal\|exception" /tmp/node_test.log; then
        echo -e "${YELLOW}⚠${NC}  Some warnings/errors found in log:"
        grep -i "error\|fatal\|exception" /tmp/node_test.log | head -10
    else
        echo -e "${GREEN}✓${NC} No errors in startup log"
    fi

    # Check if topics are being advertised
    echo ""
    echo "Test 3: Checking topics..."
    sleep 2
    TOPICS=$(rostopic list 2>/dev/null | grep safe_regret_mpc | wc -l)
    echo "Found $TOPICS safe_regret_mpc topics"

    if [ $TOPICS -gt 0 ]; then
        echo -e "${GREEN}✓${NC} Node is advertising topics:"
        rostopic list | grep safe_regret_mpc
    else
        echo -e "${YELLOW}⚠${NC}  No topics found yet"
    fi

    # Check services
    echo ""
    echo "Test 4: Checking services..."
    SERVICES=$(rosservice list 2>/dev/null | grep safe_regret_mpc | wc -l)
    echo "Found $SERVICES safe_regret_mpc services"

    if [ $SERVICES -gt 0 ]; then
        echo -e "${GREEN}✓${NC} Node is providing services:"
        rosservice list | grep safe_regret_mpc
    else
        echo -e "${YELLOW}⚠${NC}  No services found yet"
    fi

    # Cleanup
    echo ""
    echo "Cleaning up..."
    kill $NODE_PID 2>/dev/null
    kill $ROSCORE_PID 2>/dev/null

    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}Quick test PASSED${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo "Node startup log saved to: /tmp/node_test.log"

    exit 0
else
    echo -e "${RED}✗${NC} Node failed to start or crashed"
    echo ""
    echo "Log output:"
    cat /tmp/node_test.log
    kill $ROSCORE_PID 2>/dev/null
    exit 1
fi
