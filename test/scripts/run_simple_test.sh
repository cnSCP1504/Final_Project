#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "Starting simple test..."
roslaunch tube_mpc_benchmark simple_test.launch test_duration:=3 > /tmp/test.log 2>&1 &
PID=$!

sleep 8

echo ""
echo "=== Checking ROS System ==="
rosnode list
echo ""
rostopic list | head -15

echo ""
echo "=== Test Log (last 30 lines) ==="
tail -30 /tmp/test.log

# Cleanup
kill -9 $PID 2>/dev/null
pkill -9 -f simple_test 2>/dev/null

echo ""
echo "Test completed"
