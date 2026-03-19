#!/bin/bash

# Tube MPC Visualization Verification Script
# This script checks if all visualization topics are being published correctly

echo "========================================="
echo "Tube MPC Visualization Verification"
echo "========================================="
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if ROS is running
if ! pgrep -x "rosmaster" > /dev/null; then
    echo -e "${RED}❌ ROS master is not running${NC}"
    echo "   Please start ROS first with: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
    exit 1
fi

echo -e "${GREEN}✅ ROS master is running${NC}"
echo ""

# Function to check topic
check_topic() {
    local topic=$1
    local description=$2

    if rostopic info $topic > /dev/null 2>&1; then
        echo -e "${GREEN}✅${NC} $description"
        echo "   Topic: $topic"

        # Get topic info
        local type=$(rostopic info $topic | grep "Type:" | awk '{print $2}')
        local publishers=$(rostopic info $topic | grep "Publishers:" | awk '{print $2}')

        echo "   Type: $type"
        echo "   Publishers: $publishers"

        # Try to get one message
        echo "   Testing data reception..."
        if timeout 3 rostopic echo $topic -n 1 > /dev/null 2>&1; then
            echo -e "   ${GREEN}✅ Data receiving correctly${NC}"
        else
            echo -e "   ${YELLOW}⚠️  No data received (topic might be waiting for navigation to start)${NC}"
        fi
    else
        echo -e "${RED}❌${NC} $description"
        echo "   Topic: $topic - NOT FOUND"
    fi
    echo ""
}

# Check all visualization topics
echo "Checking Tube MPC Visualization Topics:"
echo "---------------------------------------"
check_topic "/mpc_trajectory" "MPC Predicted Trajectory (Red Line)"
check_topic "/tube_boundaries" "Tube Boundaries (Blue Lines)"
check_topic "/mpc_reference" "MPC Reference Path (Yellow Line)"
check_topic "/move_base/GlobalPlanner/plan" "Global Path (Green Line)"
check_topic "/move_base/local_costmap/footprint" "Robot Footprint (Purple)"
check_topic "/map" "Map Display"
check_topic "/move_base/local_costmap/costmap" "Local Costmap"
check_topic "/cmd_vel" "Velocity Commands"

echo "========================================="
echo "Summary:"
echo "========================================="
echo ""
echo "If all topics show ✅, your visualization is working correctly!"
echo ""
echo "Expected RViz Display:"
echo "  🔴 Red Line    - MPC Predicted Trajectory"
echo "  🔵 Blue Lines  - Tube Boundaries (4 lines forming a rectangle)"
echo "  🟡 Yellow Line - MPC Reference Path"
echo "  🟢 Green Line  - Global Path"
echo "  🟣 Purple      - Robot Footprint"
echo "  ⬜ Gray Area   - Costmap"
echo ""
echo "If you see issues:"
echo "  1. Make sure Tube MPC navigation is running"
echo "  2. Set a navigation goal using '2D Nav Goal' in RViz"
echo "  3. The Tube boundaries will appear when the robot is navigating"
echo ""
echo "For complete visualization, use:"
echo "  ./src/tube_mpc_ros/scripts/launch_tube_mpc_with_rviz.sh"
echo "========================================="
