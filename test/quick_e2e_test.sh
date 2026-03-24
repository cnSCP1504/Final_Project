#!/bin/bash

# Quick End-to-End Test
# Fast verification of complete system

echo "========================================="
echo "Quick E2E Test - Safe-Regret MPC"
echo "========================================="
echo ""

source devel/setup.bash

echo "1. Starting ROS master..."
roscore > /dev/null 2>&1 &
ROSCORE_PID=$!
sleep 2

echo "2. Starting MPC node..."
rosrun safe_regret_mpc safe_regret_mpc_node > /tmp/mpc.log 2>&1 &
MPC_PID=$!
sleep 3

echo "3. Checking topics..."
TOPICS=$(rostopic list | grep -E "(cmd_vel|safe_regret_mpc)" | wc -l)
echo "   Found $TOPICS Safe-Regret MPC topics"

echo "4. Publishing test data..."
rostopic pub -1 /odom nav_msgs/Odometry '{header: {frame_id: "map"}, pose: {pose: {position: {x: 1.0, y: 2.0, z: 0.0}, orientation: {w: 1.0}}}, twist: {twist: {linear: {x: 0.5}}}}' > /dev/null &
rostopic pub -1 /global_plan nav_msgs/Path '{header: {frame_id: "map"}, poses: [{pose: {position: {x: 1.0, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}]}' > /dev/null &
sleep 2

echo "5. Checking output..."
if timeout 3 rostopic echo /cmd_vel -n 1 > /dev/null 2>&1; then
    echo "   ✓ Control command published"
else
    echo "   ✗ No control command"
fi

if timeout 3 rostopic echo /safe_regret_mpc/state -n 1 > /dev/null 2>&1; then
    echo "   ✓ System state published"
else
    echo "   ✗ No system state"
fi

echo "6. Testing services..."
if rosservice call /safe_regret_mpc/get_system_status "{}" > /dev/null 2>&1; then
    echo "   ✓ get_system_status working"
else
    echo "   ✗ get_system_status failed"
fi

echo "7. Checking MPC solver..."
if grep -q "Solving MPC" /tmp/mpc.log 2>/dev/null; then
    echo "   ✓ MPC solver running"
    SOLVES=$(grep -c "succeeded" /tmp/mpc.log 2>/dev/null || echo "0")
    echo "   ✓ Solved $SOLVES optimization problems"
else
    echo "   ⚠ MPC solver not yet called"
fi

echo ""
echo "Cleaning up..."
kill $MPC_PID $ROSCORE_PID 2>/dev/null
sleep 1

echo ""
echo "========================================="
echo "Quick Test Complete"
echo "========================================="
echo ""
echo "For detailed testing, run:"
echo "  ./test_end_to_end.sh"
echo ""
