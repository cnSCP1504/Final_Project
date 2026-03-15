#!/bin/bash
# Safe-Regret MPC Launch Helper Script
# This script helps launch Safe-Regret MPC with proper environment setup

echo "========================================"
echo "Safe-Regret MPC Launch Helper"
echo "========================================"

# Go to workspace
cd /home/dixon/Final_Project/catkin

# Source workspace
echo "Sourcing workspace..."
source devel/setup.bash

echo ""
echo "========================================"
echo "Launch Options:"
echo "========================================"
echo "1. Full launch (requires all dependencies):"
echo "   roslaunch stl_ros safe_regret_mpc.launch enable_stl:=true"
echo ""
echo "2. Minimal launch (core components only):"
echo "   roslaunch stl_ros minimal_safe_regret_mpc.launch enable_stl:=true"
echo ""
echo "3. STL monitor only:"
echo "   roslaunch stl_ros stl_monitor.launch"
echo ""
echo "4. Tube MPC only:"
echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "========================================"
echo "System Status Check:"
echo "========================================"

# Check if roscore is running
if pgrep -x roscore > /dev/null; then
    echo "✓ roscore is running"
    ROSCORE_RUNNING=true
else
    echo "✗ roscore is NOT running"
    echo ""
    echo "To start the system, you need to run roscore first:"
    echo "  roscore &"
    echo ""
    echo "Then launch Safe-Regret MPC in another terminal."
    ROSCORE_RUNNING=false
fi

echo ""
echo "Checking package availability..."

if rospack find stl_ros >/dev/null 2>&1; then
    echo "✓ STL_ros package available"
else
    echo "✗ STL_ros package NOT available"
fi

if rospack find tube_mpc_ros >/dev/null 2>&1; then
    echo "✓ Tube MPC package available"
else
    echo "✗ Tube MPC package NOT available"
fi

echo ""
echo "Checking launch files..."

if [ -f "src/STL_ros/launch/safe_regret_mpc.launch" ]; then
    echo "✓ safe_regret_mpc.launch exists"
else
    echo "✗ safe_regret_mpc.launch NOT found"
fi

if [ -f "src/STL_ros/launch/minimal_safe_regret_mpc.launch" ]; then
    echo "✓ minimal_safe_regret_mpc.launch exists"
else
    echo "✗ minimal_safe_regret_mpc.launch NOT found"
fi

echo ""
echo "========================================"
echo "Recommended Launch Procedure:"
echo "========================================"
echo ""
echo "Terminal 1 - Start ROS core:"
echo "  roscore"
echo ""
echo "Terminal 2 - Launch Safe-Regret MPC:"
echo "  source devel/setup.bash"
echo "  roslaunch stl_ros minimal_safe_regret_mpc.launch enable_stl:=true"
echo ""
echo "Terminal 3 - Monitor STL status (optional):"
echo "  source devel/setup.bash"
echo "  rostopic echo /stl_monitor/robustness"
echo "  rostopic echo /stl_monitor/budget"
echo ""

# Ask user if they want to proceed with launch
if [ "$1" == "--launch" ]; then
    echo "Attempting to launch Safe-Regret MPC..."
    sleep 2

    # Launch minimal version
    roslaunch stl_ros minimal_safe_regret_mpc_launch enable_stl:=true &
    LAUNCH_PID=$!

    echo ""
    echo "Launch started! PID: $LAUNCH_PID"
    echo "Press Ctrl+C to stop"
    echo ""
    echo "To monitor STL topics, in another terminal run:"
    echo "  source devel/setup.bash"
    echo "  rostopic echo /stl_monitor/robustness"
    echo ""

    # Wait a bit then check if launch succeeded
    sleep 5

    if ps -p $LAUNCH_PID > /dev/null; then
        echo "✓ Launch appears to be running"
        echo "Check the output above for any errors"
    else
        echo "✗ Launch may have failed - check output above"
    fi

    # Keep script running
    wait $LAUNCH_PID
else
    echo ""
    echo "To actually launch, run:"
    echo "  $0 --launch"
    echo ""
    echo "Or manually:"
    echo "  roslaunch stl_ros minimal_safe_regret_mpc.launch enable_stl:=true"
fi
