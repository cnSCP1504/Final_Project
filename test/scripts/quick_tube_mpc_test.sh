#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "Quick Tube MPC Test - Starting system..."

# Launch the test
roslaunch tube_mpc_benchmark tube_mpc_kinematic_test.launch \
    goal_x:=3.0 \
    goal_y:=3.0 \
    goal_yaw:=0.0 \
    with_rviz:=false &

TEST_PID=$!
echo "Test launched with PID: $TEST_PID"

# Wait for startup
echo "Waiting 15 seconds for system initialization..."
sleep 15

# Check nodes
echo ""
echo "=== ROS Nodes ==="
rosnode list 2>/dev/null | head -15

# Check topics
echo ""
echo "=== Key Topics ==="
rostopic list 2>/dev/null | grep -E "cmd_vel|odom|move_base"

# Check if tube MPC is running
echo ""
echo "=== Tube MPC Status ==="
if rosnode list 2>/dev/null | grep -q "TubeMPC"; then
    echo "✓ Tube MPC Node is running"
    rosnode info /TubeMPC_Node 2>/dev/null | head -10
else
    echo "✗ Tube MPC Node not found"
fi

# Check current position
echo ""
echo "=== Current Position ==="
timeout 3 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" || echo "No odom data"

echo ""
echo "Test completed. Cleaning up..."
kill -9 $TEST_PID 2>/dev/null
pkill -9 -f tube_mpc_kinematic_test 2>/dev/null
sleep 2
echo "Done"
