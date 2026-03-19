#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Tube MPC Navigation Test                             ║"
echo "║       Kinematic Simulator + Tube MPC                       ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Parse arguments
GOAL_X=${1:-5.0}
GOAL_Y=${2:-5.0}
GOAL_YAW=${3:-0.0}
WITH_RVIZ=${4:-false}

echo "Test Configuration:"
echo "  Start:  (0.0, 0.0, 0.0)"
echo "  Goal:   ($GOAL_X, $GOAL_Y, $GOAL_YAW)"
echo "  RViz:   $WITH_RVIZ"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "=== Cleaning up ==="
    pkill -9 -f tube_mpc_kinematic_test 2>/dev/null
    pkill -9 -f kinematic_robot_sim 2>/dev/null
    pkill -9 -f TubeMPC_Node 2>/dev/null
    pkill -9 -f move_base 2>/dev/null
    pkill -9 -f amcl 2>/dev/null
    pkill -9 -f simple_goal 2>/dev/null
    pkill -9 -f activate_test 2>/dev/null
    echo "Cleanup complete"
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

# Launch test
echo "→ Launching Tube MPC navigation test..."
roslaunch tube_mpc_benchmark tube_mpc_kinematic_test.launch \
    goal_x:=$GOAL_X \
    goal_y:=$GOAL_Y \
    goal_yaw:=$GOAL_YAW \
    with_rviz:=$WITH_RVIZ &

TEST_PID=$!
echo "  Test PID: $TEST_PID"

# Wait for system to initialize
echo ""
echo "→ Waiting for system initialization (10 seconds)..."
sleep 10

# Check if nodes are running
echo ""
echo "=== Checking ROS Nodes ==="
rosnode list 2>/dev/null | grep -E "tube|mpc|simulator|move_base|amcl" || echo "  Warning: Some nodes may not be running"

# Check topics
echo ""
echo "=== Checking ROS Topics ==="
rostopic list 2>/dev/null | grep -E "cmd_vel|odom|goal|move_base" | head -10

# Monitor goal progress
echo ""
echo "=== Monitoring Navigation Progress ==="
echo "  (Will monitor for 60 seconds or until goal is reached)"
echo ""

TIMEOUT=60
ELAPSED=0

while [ $ELAPSED -lt $TIMEOUT ]; do
    # Check if goal was reached
    if rostopic echo /move_base/status -n 1 2>/dev/null | grep -q "status: 3"; then
        echo "✓ GOAL REACHED!"
        echo ""
        echo "=== Final Position ==="
        timeout 2 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:"
        break
    fi

    # Show current position every 5 seconds
    if [ $((ELAPSED % 5)) -eq 0 ] && [ $ELAPSED -gt 0 ]; then
        echo "[$ELAPSED s] Current position:"
        timeout 2 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" | head -4 || echo "  No odom data"
    fi

    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if [ $ELAPSED -ge $TIMEOUT ]; then
    echo ""
    echo "⚠ Test timeout after $TIMEOUT seconds"
    echo ""
    echo "=== Final Position ==="
    timeout 2 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" || echo "  No odom data"
fi

echo ""
echo "=== Test Summary ==="
echo "  Duration: $ELAPSED seconds"
echo "  Goal: ($GOAL_X, $GOAL_Y, $GOAL_YAW)"
echo ""

# Wait a bit before cleanup
sleep 2

# Cleanup will be handled by trap
echo "Test completed"
