#!/bin/bash

# STL-MPC System Verification Script
# Verifies that all components are working correctly

echo "=========================================="
echo "STL-MPC System Verification"
echo "=========================================="
echo ""

source /home/dixon/Final_Project/catkin/devel/setup.bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if ROS master is running
echo -e "${YELLOW}1. Checking ROS master...${NC}"
if rostopic list > /dev/null 2>&1; then
    echo -e "${GREEN}✓${NC} ROS master is running"
else
    echo -e "${RED}✗${NC} ROS master is NOT running"
    exit 1
fi

echo ""

# Check STL nodes
echo -e "${YELLOW}2. Checking STL nodes...${NC}"
STL_NODES=("stl_monitor" "stl_visualizer" "stl_data_logger" "TubeMPC_Node")
for node in "${STL_NODES[@]}"; do
    if rosnode list | grep -q "$node"; then
        echo -e "${GREEN}✓${NC} $node is running"
    else
        echo -e "${RED}✗${NC} $node is NOT running"
    fi
done

echo ""

# Check STL topics
echo -e "${YELLOW}3. Checking STL topics...${NC}"
STL_TOPICS=("/stl/robustness" "/stl/budget" "/stl/violation" "/stl_visualization" "/tube_mpc/stl_robustness")
for topic in "${STL_TOPICS[@]}"; do
    if rostopic list | grep -q "$topic"; then
        echo -e "${GREEN}✓${NC} $topic is available"
    else
        echo -e "${RED}✗${NC} $topic is NOT available"
    fi
done

echo ""

# Check STL topic data
echo -e "${YELLOW}4. Checking STL data flow...${NC}"
echo "Testing /stl/robustness topic..."
if timeout 3 rostopic echo /stl/robustness -n 1 > /dev/null 2>&1; then
    ROBUSTNESS=$(timeout 3 rostopic echo /stl/robustness -n 1 | grep "data:" | head -1 | awk '{print $2}')
    echo -e "${GREEN}✓${NC} /stl/robustness is publishing: $ROBUSTNESS"
else
    echo -e "${RED}✗${NC} /stl/robustness is NOT publishing data"
fi

echo "Testing /stl/violation topic..."
if timeout 3 rostopic echo /stl/violation -n 1 > /dev/null 2>&1; then
    VIOLATION=$(timeout 3 rostopic echo /stl/violation -n 1 | grep "data:" | head -1 | awk '{print $2}')
    echo -e "${GREEN}✓${NC} /stl/violation is publishing: $VIOLATION"
else
    echo -e "${RED}✗${NC} /stl/violation is NOT publishing data"
fi

echo "Testing /stl/budget topic..."
if timeout 3 rostopic echo /stl/budget -n 1 > /dev/null 2>&1; then
    BUDGET=$(timeout 3 rostopic echo /stl/budget -n 1 | grep "data:" | head -1 | awk '{print $2}')
    echo -e "${GREEN}✓${NC} /stl/budget is publishing: $BUDGET"
else
    echo -e "${RED}✗${NC} /stl/budget is NOT publishing data"
fi

echo ""

# Check navigation system
echo -e "${YELLOW}5. Checking navigation system...${NC}"
NAV_TOPICS=("/cmd_vel" "/move_base/goal" "/move_base/GlobalPlanner/plan" "/amcl_pose")
for topic in "${NAV_TOPICS[@]}"; do
    if rostopic list | grep -q "$topic"; then
        echo -e "${GREEN}✓${NC} $topic is available"
    else
        echo -e "${RED}✗${NC} $topic is NOT available"
    fi
done

echo ""

# Count total nodes
TOTAL_NODES=$(rosnode list | wc -l)
echo -e "${YELLOW}6. System summary...${NC}"
echo -e "${GREEN}Total nodes running: $TOTAL_NODES${NC}"

echo ""
echo "=========================================="
echo "Verification Complete!"
echo "=========================================="
echo ""
echo "STL-MPC System Status:"
if [ "$TOTAL_NODES" -ge 10 ]; then
    echo -e "${GREEN}✓ System is running normally${NC}"
    echo ""
    echo "Available commands:"
    echo "  rostopic echo /stl/robustness     - Monitor STL robustness"
    echo "  rostopic echo /stl/budget         - Monitor STL budget"
    echo "  rostopic echo /stl/violation      - Monitor STL violations"
    echo "  rosnode list                      - List all nodes"
    echo "  rostopic list                     - List all topics"
    echo "  rqt_graph                         - Visualize node graph"
else
    echo -e "${RED}✗ System may have issues${NC}"
    echo "Please check the logs above"
fi
echo ""

# Display current STL values
echo "Current STL Values:"
echo "  Robustness: $ROBUSTNESS"
echo "  Budget: $BUDGET"
echo "  Violation: $VIOLATION"
echo ""

# Explain the values
echo "STL Value Interpretation:"
echo "  - Positive robustness: STL constraints satisfied"
echo "  - Negative robustness: STL constraints violated"
echo "  - Budget: Remaining robustness budget (should stay ≥ 0)"
echo "  - Violation=true: Current constraint violation detected"
echo ""