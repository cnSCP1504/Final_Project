#!/bin/bash
# Test script for DR Tightening integration with Tube MPC

echo "========================================="
echo "DR Tightening Integration Test"
echo "========================================="
echo ""

# Source ROS workspace
source /opt/ros/noetic/setup.bash
source devel/setup.bash

echo "1. Checking if dr_tightening_node exists..."
if [ -f devel/lib/dr_tightening/dr_tightening_node ]; then
    echo "   ✓ dr_tightening_node found"
else
    echo "   ✗ dr_tightening_node NOT found"
    exit 1
fi

echo ""
echo "2. Checking for launch files..."
if [ -f src/dr_tightening/launch/dr_tightening.launch ]; then
    echo "   ✓ dr_tightening.launch found"
else
    echo "   ✗ dr_tightening.launch NOT found"
    exit 1
fi

if [ -f src/dr_tightening/launch/dr_tube_mpc_integrated.launch ]; then
    echo "   ✓ dr_tube_mpc_integrated.launch found"
else
    echo "   ✗ dr_tube_mpc_integrated.launch NOT found"
    exit 1
fi

echo ""
echo "3. Checking ROS topics..."
timeout 3 rostopic list 2>/dev/null > /tmp/rostopic_list.txt
if [ $? -eq 124 ]; then
    echo "   ✗ ROS master not running - start roscore first"
    echo "   Run: roscore"
    exit 1
fi

echo "   ✓ ROS master is running"
echo ""
echo "4. Topic list:"
cat /tmp/rostopic_list.txt | grep -E "(tube_mpc|dr_)" || echo "   No DR/tube_mpc topics yet"

echo ""
echo "========================================="
echo "Integration Test Complete"
echo "========================================="
echo ""
echo "To run the integrated system:"
echo "  Terminal 1: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo "  Terminal 2: roslaunch dr_tightening dr_tightening.launch"
echo ""
echo "Or run integrated:"
echo "  roslaunch dr_tightening dr_tube_mpc_integrated.launch"
echo ""
