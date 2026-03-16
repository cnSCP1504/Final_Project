#!/bin/bash
# Test STL Monitor standalone node

set -e

echo "========================================"
echo "STL Monitor Standalone Test"
echo "========================================"

# Source workspace
source devel/setup.bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test 1: Python syntax check
echo -e "${YELLOW}[1/4] Checking Python syntax...${NC}"
python3 -m py_compile src/tube_mpc_ros/stl_monitor/src/stl_monitor_node_standalone.py
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Python syntax OK${NC}"
else
    echo -e "${RED}✗ Python syntax error${NC}"
    exit 1
fi

# Test 2: Unit tests
echo -e "${YELLOW}[2/4] Running unit tests...${NC}"
python3 src/tube_mpc_ros/stl_monitor/test/test_stl_monitor.py
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Unit tests passed${NC}"
else
    echo -e "${RED}✗ Unit tests failed${NC}"
    exit 1
fi

# Test 3: Check ROS messages
echo -e "${YELLOW}[3/4] Checking ROS messages...${NC}"
if [ -f "devel/lib/python3/dist-packages/stl_monitor/msg/__init__.py" ]; then
    echo -e "${GREEN}✓ ROS messages generated${NC}"

    # List messages
    python3 -c "from stl_monitor.msg import *; print('Available messages: STLFormula, STLRobustness, STLBudget')"
else
    echo -e "${RED}✗ ROS messages not found${NC}"
    exit 1
fi

# Test 4: Verify node structure
echo -e "${YELLOW}[4/4] Verifying node structure...${NC}"
required_files=(
    "src/tube_mpc_ros/stl_monitor/src/stl_monitor_node_standalone.py"
    "src/tube_mpc_ros/stl_monitor/params/stl_monitor_params.yaml"
    "src/tube_mpc_ros/stl_monitor/launch/stl_monitor.launch"
)

all_ok=true
for file in "${required_files[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓ $file${NC}"
    else
        echo -e "${RED}✗ $file (missing)${NC}"
        all_ok=false
    fi
done

if [ "$all_ok" = false ]; then
    exit 1
fi

echo ""
echo "========================================"
echo -e "${GREEN}All Tests Passed!${NC}"
echo "========================================"
echo ""
echo "To test the node with ROS:"
echo "  1. Start ROS core: roscore"
echo "  2. In another terminal, run the node:"
echo "     rosrun stl_monitor stl_monitor_node_standalone.py"
echo ""
echo "To test with simulated data:"
echo "  roslaunch stl_monitor stl_integration_test.launch"
echo ""
