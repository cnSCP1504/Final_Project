#!/bin/bash
# Quick start script for Safe-Regret MPC
# This provides the easiest way to get STL monitoring running

echo "========================================"
echo "Safe-Regret MPC - Quick Start"
echo "========================================"

cd /home/dixon/Final_Project/catkin

echo ""
echo "Step 1: Source workspace..."
source devel/setup.bash

echo ""
echo "Step 2: Check ROS master..."
if pgrep -x roscore > /dev/null; then
    echo "✓ roscore is running"
else
    echo "✗ roscore is NOT running"
    echo ""
    echo "Please start roscore in another terminal:"
    echo "  roscore &"
    echo ""
    echo "Then run this script again."
    exit 1
fi

echo ""
echo "Step 3: Launch STL monitor..."
echo "========================================"

# Check if stl_monitor.launch exists
if [ ! -f "src/STL_ros/launch/stl_monitor.launch" ]; then
    echo "✗ stl_monitor.launch not found!"
    exit 1
fi

# Launch in background for testing
echo "Starting STL monitor..."
roslaunch stl_ros stl_monitor.launch &
STL_PID=$!

echo ""
echo "Step 4: Wait for initialization..."
sleep 5

echo ""
echo "Step 5: Verify STL monitor is running..."
if ps -p $STL_PID > /dev/null 2>&1; then
    echo "✓ STL monitor is running (PID: $STL_PID)"
else
    echo "✗ STL monitor failed to start"
    exit 1
fi

echo ""
echo "========================================"
echo "✓ STL Monitor Started Successfully!"
echo "========================================"
echo ""
echo "Running components:"
echo "  - STL monitor node"
echo "  - Robustness publisher"
echo "  - Budget publisher"
echo ""
echo "Available topics:"
echo "  /stl_monitor/belief (input)"
echo "  /stl_monitor/mpc_trajectory (input)"
echo "  /stl_monitor/robustness (output)"
echo "  /stl_monitor/budget (output)"
echo ""
echo "To test the system:"
echo "1. Send test data:"
echo "   rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped '{...}'"
echo ""
echo "2. Monitor STL outputs:"
echo "   rostopic echo /stl_monitor/robustness"
echo "   rostopic echo /stl_monitor/budget"
echo ""
echo "3. Stop the monitor:"
echo "  Press Ctrl+C or kill $STL_PID"
echo ""

# Keep the script running
echo "Press Ctrl+C to stop the STL monitor..."
wait $STL_PID
