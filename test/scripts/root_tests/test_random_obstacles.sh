#!/bin/bash
# Test script for random obstacles functionality
# This script verifies that obstacles refresh randomly and avoid the goal point

echo "╔════════════════════════════════════════════════════════╗"
echo "║  🎲 Random Obstacles Test                             ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Expected Behavior:${NC}"
echo "  1. System starts normally"
echo "  2. Random obstacles node initializes"
echo "  3. Obstacles refresh every 5 seconds"
echo "  4. Obstacles NEVER appear within 0.6m of goal point"
echo "  5. Goal point changes trigger immediate obstacle refresh"
echo ""

echo -e "${BLUE}Test Goals:${NC}"
echo "  ✓ Verify obstacles move to random positions"
echo "  ✓ Verify obstacle positions are logged"
echo "  ✓ Verify distance to goal is > 0.6m"
echo "  ✓ Verify refresh happens every 5 seconds"
echo ""

read -p "$(echo -e ${YELLOW}Press Enter to start test...${NC})"

# Step 1: Clean ROS processes
echo ""
echo -e "${BLUE}Step 1: Cleaning ROS processes...${NC}"
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null
sleep 2

if pgrep -f "roslaunch|rosmaster|roscore|gazebo" > /dev/null; then
    echo -e "${RED}✗ Failed to clean ROS processes${NC}"
    echo "  Please manually kill: killall -9 roslaunch rosmaster roscore gzserver gzclient"
    exit 1
fi
echo -e "${GREEN}✓ All ROS processes cleaned${NC}"

# Step 2: Launch system
echo ""
echo -e "${BLUE}Step 2: Launching safe_regret_mpc_test...${NC}"
echo -e "${YELLOW}Waiting for system to initialize (15 seconds)...${NC}"

roslaunch safe_regret_mpc safe_regret_mpc_test.launch > /tmp/random_obstacles_test.log 2>&1 &
LAUNCH_PID=$!

# Wait for initialization
sleep 15

# Check if launch succeeded
if ! ps -p $LAUNCH_PID > /dev/null; then
    echo -e "${RED}✗ Failed to launch system${NC}"
    echo "Check log: /tmp/random_obstacles_test.log"
    exit 1
fi

echo -e "${GREEN}✓ System launched successfully${NC}"
echo -e "  Log file: /tmp/random_obstacles_test.log"
echo ""

# Step 3: Monitor obstacle refresh
echo -e "${BLUE}Step 3: Monitoring obstacle refresh (30 seconds)...${NC}"
echo ""
echo -e "${YELLOW}Look for:${NC}"
echo "  - '随机障碍物管理器启动'"
echo "  - '开始随机刷新障碍物位置...'"
echo "  - '✓ central_obstacle_1: (x, y) | 距离目标: X.XX m'"
echo "  - '✓ central_obstacle_2: (x, y) | 距离目标: X.XX m'"
echo "  - '✓ central_obstacle_3: (x, y) | 距离目标: X.XX m'"
echo "  - Distance should ALWAYS be > 0.60 m"
echo ""

# Monitor for 30 seconds (should see ~6 refreshes at 5-second intervals)
timeout=30
elapsed=0
while [ $elapsed -lt $timeout ]; do
    # Show recent logs
    clear
    echo -e "${BLUE}===== Random Obstacles Monitor (${elapsed}s / ${timeout}s) =====${NC}"
    echo ""
    tail -30 /tmp/random_obstacles_test.log | grep -E "随机|障碍物|central_obstacle|距离目标|Random|Obstacle" || echo "Waiting for obstacle refresh..."
    echo ""
    echo -e "${YELLOW}Next refresh in: $((5 - elapsed % 5)) seconds${NC}"
    echo -e "${YELLOW}Press Ctrl+C to stop monitoring${NC}"

    sleep 2
    elapsed=$((elapsed + 2))
done

# Step 4: Test goal change
echo ""
echo -e "${BLUE}Step 4: Testing goal change response...${NC}"
echo -e "${YELLOW}Sending new goal point...${NC}"

rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: -8.0
    y: 0.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once

sleep 3
echo ""
echo -e "${GREEN}✓ New goal sent${NC}"
echo ""

# Show obstacle response to goal change
echo -e "${BLUE}Checking obstacle response to goal change...${NC}"
tail -50 /tmp/random_obstacles_test.log | grep -A 10 "目标点更新"

# Step 5: Summary
echo ""
echo -e "${BLUE}Step 5: Test Summary${NC}"
echo ""
echo "Check the logs above for:"
echo "  [ ] Obstacles refreshed to new positions"
echo "  [ ] All distances to goal > 0.60 m"
echo "  [ ] Obstacles responded to goal change"
echo "  [ ] Refresh happened approximately every 5 seconds"
echo ""

read -p "$(echo -e ${YELLOW}Press Enter when test is complete...${NC})"

# Cleanup
kill $LAUNCH_PID 2>/dev/null

echo ""
echo -e "${GREEN}Test complete!${NC}"
echo "Review the output above to verify functionality."
