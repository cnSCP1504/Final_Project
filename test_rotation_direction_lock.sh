#!/bin/bash
# Test script for rotation direction lock mechanism
# This test verifies that the robot maintains consistent rotation direction
# without oscillating back and forth

echo "╔════════════════════════════════════════════════════════╗"
echo "║  🔄 Rotation Direction Lock Mechanism Test            ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Expected Behavior:${NC}"
echo "  1. When heading error > 90°, ENTER rotation mode"
echo "  2. Direction is LOCKED (Clockwise or Counter-Clockwise)"
echo "  3. Robot rotates in LOCKED direction until angle < 10°"
echo "  4. Direction should NOT flip during rotation"
echo ""

echo -e "${YELLOW}Test Steps:${NC}"
echo "  1. Clean all ROS processes"
echo "  2. Launch safe_regret_mpc_test"
echo "  3. Set initial goal to trigger large-angle turn (>90°)"
echo "  4. Monitor console output for:"
echo "     - '🔄 ENTERING PURE ROTATION MODE'"
echo "     - 'Direction LOCKED: Clockwise/Counter-Clockwise'"
echo "     - 'Direction LOCKED' in angular velocity log"
echo "  5. Verify robot does NOT oscillate direction"
echo ""

read -p "$(echo -e ${YELLOW}Press Enter to start test...${NC})"

# Step 1: Clean ROS processes
echo ""
echo -e "${BLUE}Step 1: Cleaning ROS processes...${NC}"
./test/scripts/clean_ros.sh

if pgrep -f "roslaunch|rosmaster|roscore|gazebo" > /dev/null; then
    echo -e "${RED}✗ Failed to clean ROS processes${NC}"
    echo "  Please manually kill: killall -9 roslaunch rosmaster roscore gzserver gzclient"
    exit 1
fi
echo -e "${GREEN}✓ All ROS processes cleaned${NC}"

# Step 2: Launch system
echo ""
echo -e "${BLUE}Step 2: Launching safe_regret_mpc_test...${NC}"
echo -e "${YELLOW}Waiting for system to initialize...${NC}"

roslaunch safe_regret_mpc safe_regret_mpc_test.launch > /tmp/rotation_test.log 2>&1 &
LAUNCH_PID=$!

# Wait for initialization
sleep 5

# Check if launch succeeded
if ! ps -p $LAUNCH_PID > /dev/null; then
    echo -e "${RED}✗ Failed to launch system${NC}"
    echo "Check log: /tmp/rotation_test.log"
    exit 1
fi

echo -e "${GREEN}✓ System launched successfully${NC}"
echo -e "  Log file: /tmp/rotation_test.log"
echo ""

# Step 3: Monitor for rotation mode
echo -e "${BLUE}Step 3: Monitoring for rotation direction lock...${NC}"
echo ""
echo -e "${YELLOW}Setting test goal to trigger large-angle turn...${NC}"

# Set a goal that requires >90° turn
# Assuming robot starts at (-8, 0) facing +X
# Goal at (0, 8) requires ~90° turn
rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: 0.0
    y: 8.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once

echo -e "${GREEN}✓ Goal sent${NC}"
echo ""

# Monitor console output
echo -e "${BLUE}Monitoring console output (Ctrl+C to stop):${NC}"
echo -e "${YELLOW}Look for:${NC}"
echo "  - '🔄 ENTERING PURE ROTATION MODE'"
echo "  - 'Direction LOCKED: ...'"
echo "  - 'Direction LOCKED' in angular vel messages"
echo ""

tail -f /tmp/rotation_test.log | grep --line-buffered -E "ENTERING|EXITING|Direction|LOCKED|ROTATION" &
TAIL_PID=$!

# Handle Ctrl+C
trap "echo ''; echo -e '${YELLOW}Stopping test...${NC}'; kill $TAIL_PID $LAUNCH_PID 2>/dev/null; exit 0" INT

# Wait for user to stop
read -p "$(echo -e ${YELLOW}Press Enter when test is complete...${NC})"

# Cleanup
kill $TAIL_PID 2>/dev/null
kill $LAUNCH_PID 2>/dev/null

echo ""
echo -e "${BLUE}Test Summary:${NC}"
echo "  Check the console output above for:"
echo "  1. ✓ Direction locked on entry"
echo "  2. ✓ 'Direction LOCKED' in all rotation messages"
echo "  3. ✓ No direction oscillation (no back-and-forth)"
echo "  4. ✓ Direction unlocked on exit"
echo ""
echo -e "${GREEN}Test complete!${NC}"
