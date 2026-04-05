#!/bin/bash
# Test script for in-place rotation feature in Safe-Regret MPC
# This script tests the ported in-place rotation functionality

echo "========================================="
echo "Safe-Regret MPC In-Place Rotation Test"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Clean all ROS processes
echo -e "${YELLOW}[Step 1/4] Cleaning ROS processes...${NC}"
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2
echo "✓ Clean complete"
echo ""

# Step 2: Source workspace
echo -e "${YELLOW}[Step 2/4] Sourcing workspace...${NC}"
source devel/setup.bash
echo "✓ Workspace sourced"
echo ""

# Step 3: Launch Safe-Regret MPC with DR/STL enabled
echo -e "${YELLOW}[Step 3/4] Launching Safe-Regret MPC with DR/STL...${NC}"
echo "This will enable the in-place rotation feature"
echo ""

roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=true \
    debug_mode:=true \
    use_gazebo:=true &

LAUNCH_PID=$!
echo "Launch PID: $LAUNCH_PID"
echo ""

# Step 4: Monitor for rotation mode activation
echo -e "${YELLOW}[Step 4/4] Monitoring for in-place rotation...${NC}"
echo ""
echo "Look for the following log messages:"
echo "  - '🔄 ENTERING PURE ROTATION MODE' (when angle > 90°)"
echo "  - '🔄 [PURE ROTATION] Speed LOCKED at 0.0 m/s' (during rotation)"
echo "  - '✅ EXITING PURE ROTATION MODE' (when angle < 10°)"
echo ""
echo "Press Ctrl+C to stop monitoring and clean up"
echo ""

# Wait for a few seconds for system to start
sleep 5

# Monitor log output for rotation messages
timeout 60 rostopic echo /rosout | grep --line-buffered -i "rotation\|etheta\|heading" &
MONITOR_PID=$!

# Function to cleanup on exit
cleanup() {
    echo ""
    echo -e "${YELLOW}Cleaning up...${NC}"
    kill $LAUNCH_PID 2>/dev/null
    kill $MONITOR_PID 2>/dev/null
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
    echo "✓ Cleanup complete"
    exit 0
}

# Set trap for cleanup
trap cleanup SIGINT SIGTERM

# Keep script running
echo "Monitoring... (Press Ctrl+C to stop)"
wait $LAUNCH_PID
