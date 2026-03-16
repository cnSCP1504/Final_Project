#!/bin/bash
# Build and test STL Monitor module

set -e

echo "========================================"
echo "STL Monitor Build and Test Script"
echo "========================================"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Navigate to catkin workspace
cd /home/dixon/Final_Project/catkin

# Step 1: Check dependencies
echo -e "${YELLOW}[1/5] Checking dependencies...${NC}"

if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: python3 not found${NC}"
    exit 1
fi

if ! dpkg -l | grep -q ros-noetic; then
    echo -e "${RED}Error: ROS Noetic not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Dependencies OK${NC}"

# Step 2: Build package
echo -e "${YELLOW}[2/5] Building STL Monitor package...${NC}"

catkin_make --only-pkg-with-deps stl_monitor

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

# Step 3: Source workspace
echo -e "${YELLOW}[3/5] Sourcing workspace...${NC}"
source devel/setup.bash
echo -e "${GREEN}✓ Workspace sourced${NC}"

# Step 4: Run unit tests
echo -e "${YELLOW}[4/5] Running unit tests...${NC}"

python3 src/tube_mpc_ros/stl_monitor/test/test_stl_monitor.py

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Unit tests passed${NC}"
else
    echo -e "${RED}✗ Unit tests failed${NC}"
    exit 1
fi

# Step 5: Verify package structure
echo -e "${YELLOW}[5/5] Verifying package structure...${NC}"

required_files=(
    "src/tube_mpc_ros/stl_monitor/package.xml"
    "src/tube_mpc_ros/stl_monitor/CMakeLists.txt"
    "src/tube_mpc_ros/stl_monitor/params/stl_monitor_params.yaml"
    "src/tube_mpc_ros/stl_monitor/launch/stl_monitor.launch"
    "src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.py"
    "src/tube_mpc_ros/stl_monitor/test/test_stl_monitor.py"
)

all_files_exist=true
for file in "${required_files[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓ $file${NC}"
    else
        echo -e "${RED}✗ $file (missing)${NC}"
        all_files_exist=false
    fi
done

if [ "$all_files_exist" = true ]; then
    echo -e "${GREEN}✓ All required files present${NC}"
else
    echo -e "${RED}✗ Some files are missing${NC}"
    exit 1
fi

echo ""
echo "========================================"
echo -e "${GREEN}STL Monitor Build Complete!${NC}"
echo "========================================"
echo ""
echo "To launch the STL monitor:"
echo "  roslaunch stl_monitor stl_monitor.launch"
echo ""
echo "To run with Tube MPC:"
echo "  roslaunch tube_mpc_ros tube_mpc_with_stl.launch"
echo ""
echo "Data will be logged to: /tmp/stl_monitor_data.csv"
echo ""
