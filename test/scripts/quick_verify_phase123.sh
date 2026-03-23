#!/bin/bash
# Quick Phase 1-3 Output Verification

echo "=========================================="
echo "Safe-Regret MPC - Phase 1-3 Quick Verify"
echo "=========================================="
echo ""

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

PASS=0
FAIL=0

pass() { echo -e "${GREEN}✓${NC} $1"; ((PASS++)); }
fail() { echo -e "${RED}✗${NC} $1"; ((FAIL++)); }

echo "PHASE 1: Tube MPC"
echo "------------------"
[ -f "/home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node" ] && pass "Tube MPC Node" || fail "Tube MPC Node"
[ -f "/home/dixon/Final_Project/catkin/devel/lib/libtube_mpc_lib.so" ] && pass "Tube MPC Library" || fail "Tube MPC Library"
[ -f "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/launch/tube_mpc_navigation.launch" ] && pass "Tube MPC Launch" || fail "Tube MPC Launch"
[ -f "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml" ] && pass "Tube MPC Params" || fail "Tube MPC Params"

echo ""
echo "PHASE 2: STL Monitor"
echo "--------------------"
[ -f "/home/dixon/Final_Project/catkin/devel/lib/stl_ros/stl_monitor.py" ] && pass "STL Monitor Node" || fail "STL Monitor Node"
[ -f "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/launch/stl_monitor.launch" ] && pass "STL Monitor Launch" || fail "STL Monitor Launch"
[ -f "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.py" ] && pass "STL Monitor Source" || fail "STL Monitor Source"

echo ""
echo "PHASE 3: DR Tightening"
echo "----------------------"
[ -f "/home/dixon/Final_Project/catkin/devel/lib/dr_tightening/dr_tightening_node" ] && pass "DR Tightening Node" || fail "DR Tightening Node"
[ -f "/home/dixon/Final_Project/catkin/devel/lib/libdr_tightening.so" ] && pass "DR Tightening Library" || fail "DR Tightening Library"
[ -f "/home/dixon/Final_Project/catkin/src/dr_tightening/launch/dr_tightening.launch" ] && pass "DR Tightening Launch" || fail "DR Tightening Launch"
[ -f "/home/dixon/Final_Project/catkin/src/dr_tightening/params/dr_tightening_params.yaml" ] && pass "DR Tightening Params" || fail "DR Tightening Params"

echo ""
echo "INTEGRATION: Topic Interfaces"
echo "------------------------------"

# Check if Tube MPC publishes tracking error
grep -q "advertise.*tube_mpc/tracking_error" /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/*.cpp 2>/dev/null && pass "Tube MPC → /tube_mpc/tracking_error" || fail "Tube MPC → /tube_mpc/tracking_error"

# Check if DR tightening subscribes to tracking error
grep -q "subscribe.*tube_mpc/tracking_error\|subscribe.*tracking_error" /home/dixon/Final_Project/catkin/src/dr_tightening/src/*.cpp 2>/dev/null && pass "DR Tightening ← /tube_mpc/tracking_error" || fail "DR Tightening ← /tube_mpc/tracking_error"

# Check if DR tightening publishes margins
grep -q "advertise.*dr_margins" /home/dixon/Final_Project/catkin/src/dr_tightening/src/*.cpp 2>/dev/null && pass "DR Tightening → /dr_margins" || fail "DR Tightening → /dr_margins"

echo ""
echo "SUMMARY"
echo "-------"
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ ALL CHECKS PASSED!${NC}"
    echo ""
    echo "Phase 1-3 components are properly built."
    echo ""
    echo "Quick Start Test:"
    echo "1. Terminal 1: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
    echo "2. Terminal 2: roslaunch dr_tightening dr_tightening.launch"
    echo "3. Terminal 3: roslaunch stl_monitor stl_monitor.launch"
    echo "4. Terminal 4: rostopic list | grep -E 'tube_mpc|dr_|stl_'"
    exit 0
else
    echo -e "${RED}✗ SOME CHECKS FAILED${NC}"
    echo "Please check the output above."
    exit 1
fi
