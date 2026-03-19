#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "=== Starting Simulator Test ==="
roslaunch tube_mpc_benchmark simple_test.launch test_duration:=5 &
PID=$!

sleep 5

echo ""
echo "=== Checking /odom data (3 samples) ==="
timeout 3 rostopic echo /odom -n 3 2>/dev/null | grep -E "position:|x:|y:" || echo "No odom data"

echo ""
echo "=== Checking /cmd_vel data ==="
timeout 2 rostopic echo /cmd_vel -n 2 2>/dev/null | grep -E "linear:|angular:|x:" || echo "No cmd_vel data"

# Cleanup
kill -9 $PID 2>/dev/null
pkill -9 -f simple_test 2>/dev/null

echo ""
echo "✓ Simulator test completed"
