#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Tube MPC Navigation - Simple Test                    ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Test Configuration
GOAL_X=4.0
GOAL_Y=4.0
echo "Start: (0.0, 0.0)"
echo "Goal:  ($GOAL_X, $GOAL_Y)"
echo ""

# Launch test
echo "Launching Tube MPC navigation..."
roslaunch tube_mpc_benchmark tube_mpc_kinematic_test.launch \
    goal_x:=$GOAL_X goal_y:=$GOAL_Y with_rviz:=false > /tmp/tube_mpc_test.log 2>&1 &
TEST_PID=$!

echo "Test PID: $TEST_PID"
echo ""

# Wait for startup
echo "Waiting for initialization (15 seconds)..."
sleep 15

# Monitor progress
echo ""
echo "=== Navigation Progress (every 5 seconds) ==="
for i in {1..12}; do
    echo "[$(date +%H:%M:%S)] Position check #$i:"
    timeout 2 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" | head -4 || echo "  No data"
    sleep 5
done

# Cleanup
echo ""
echo "=== Test completed ==="
kill -9 $TEST_PID 2>/dev/null
pkill -9 -f tube_mpc_kinematic_test 2>/dev/null
sleep 2
echo "Results logged to /tmp/tube_mpc_test.log"
