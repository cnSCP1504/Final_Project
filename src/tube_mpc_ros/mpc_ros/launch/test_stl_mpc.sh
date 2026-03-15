#!/bin/bash

# STL-MPC Navigation Test Script
# This script tests the new STL-integrated Tube MPC navigation system

echo "=========================================="
echo "STL-MPC Navigation System Test"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if we're in the catkin workspace
if [ ! -f "/home/dixon/Final_Project/catkin/devel/setup.bash" ]; then
    echo -e "${RED}Error: Catkin workspace not properly built${NC}"
    echo "Please run: catkin_make"
    exit 1
fi

# Source the workspace
source /home/dixon/Final_Project/catkin/devel/setup.bash

# Check for required files
echo -e "${YELLOW}Checking required files...${NC}"

FILES=(
    "src/tube_mpc_ros/mpc_ros/launch/stl_mpc_navigation.launch"
    "src/tube_mpc_ros/mpc_ros/params/stl_params.yaml"
    "src/tube_mpc_ros/mpc_ros/params/stl_rviz_navigation.rviz"
)

ALL_FILES=true
for file in "${FILES[@]}"; do
    if [ -f "/home/dixon/Final_Project/catkin/$file" ]; then
        echo -e "${GREEN}✓${NC} $file"
    else
        echo -e "${RED}✗${NC} $file (missing)"
        ALL_FILES=false
    fi
done

echo ""

# Check STL_ros package
echo -e "${YELLOW}Checking STL_ros package...${NC}"
if [ -d "/home/dixon/Final_Project/catkin/src/STL_ros" ]; then
    echo -e "${GREEN}✓${NC} STL_ros package exists"

    # Check for source files
    if [ -f "/home/dixon/Final_Project/catkin/src/STL_ros/scripts/stl_monitor.py" ]; then
        echo -e "${GREEN}✓${NC} STL monitor source found"
    else
        echo -e "${YELLOW}⚠${NC} STL monitor source NOT found (only .pyc compiled)"
    fi

    if [ -f "/home/dixon/Final_Project/catkin/src/STL_ros/scripts/stl_visualizer.py" ]; then
        echo -e "${GREEN}✓${NC} STL visualizer source found"
    else
        echo -e "${YELLOW}⚠${NC} STL visualizer source NOT found (only .pyc compiled)"
    fi

    # Check for executables
    if [ -f "/home/dixon/Final_Project/catkin/devel/lib/stl_ros/stl_monitor.py" ]; then
        echo -e "${GREEN}✓${NC} STL monitor executable built"
    else
        echo -e "${RED}✗${NC} STL monitor executable NOT built"
    fi
else
    echo -e "${RED}✗${NC} STL_ros package NOT found"
fi

echo ""

# Summary
if [ "$ALL_FILES" = true ]; then
    echo -e "${GREEN}All STL-MPC files are in place!${NC}"
else
    echo -e "${RED}Some STL-MPC files are missing${NC}"
fi

echo ""
echo "=========================================="
echo "Launch Options:"
echo "=========================================="
echo ""
echo "1. Launch STL-MPC navigation (with STL):"
echo "   roslaunch tube_mpc_ros stl_mpc_navigation.launch"
echo ""
echo "2. Launch without STL (standard Tube MPC):"
echo "   roslaunch tube_mpc_ros stl_mpc_navigation.launch enable_stl:=false"
echo ""
echo "3. Launch with custom STL config:"
echo "   roslaunch tube_mpc_ros stl_mpc_navigation.launch stl_config:=/path/to/config.yaml"
echo ""
echo "4. Compare with original Tube MPC:"
echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "=========================================="
echo "Notes:"
echo "=========================================="
echo "- STL nodes may fail if source code is not implemented"
echo "- Use enable_stl:=false to test base navigation system"
echo "- Check /stl/* topics for STL monitoring data"
echo "- Data logged to ~/stl_mpc_data_*.bag"
echo ""
echo "For more details, see:"
echo "  src/tube_mpc_ros/mpc_ros/launch/STL_MPC_README.md"
echo ""

# Ask if user wants to launch
read -p "Do you want to launch STL-MPC navigation now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Launching STL-MPC navigation...${NC}"
    roslaunch tube_mpc_ros stl_mpc_navigation.launch
else
    echo "Launch cancelled. You can launch manually later."
fi