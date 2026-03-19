#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Tube MPC Visualization System Test                   ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Test configuration
GOAL_X=3.0
GOAL_Y=3.0
TEST_DURATION=45

echo "Test Configuration:"
echo "  Goal: ($GOAL_X, $GOAL_Y)"
echo "  Duration: ${TEST_DURATION}s"
echo "  With RViz: No (command line mode)"
echo "  Data Recording: Yes"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "=== Cleaning up ==="
    pkill -9 -f visualization_test 2>/dev/null
    pkill -9 -f kinematic_robot_sim 2>/dev/null
    pkill -9 -f TubeMPC_Node 2>/dev/null
    pkill -9 -f move_base 2>/dev/null
    pkill -9 -f data_recorder 2>/dev/null
    pkill -9 -f amcl 2>/dev/null
}

trap cleanup EXIT INT TERM

# Launch visualization test (no RViz, with data recording)
echo "→ Launching visualization test..."
roslaunch tube_mpc_benchmark visualization_test.launch \
    goal_x:=$GOAL_X \
    goal_y:=$GOAL_Y \
    enable_visualizer:=false \
    enable_recorder:=true > /tmp/viz_test.log 2>&1 &

TEST_PID=$!
echo "  Test PID: $TEST_PID"

# Wait for system initialization
echo ""
echo "→ Waiting for system initialization (15 seconds)..."
sleep 15

# Check system status
echo ""
echo "=== System Status Check ==="
echo "Running nodes:"
rosnode list 2>/dev/null | grep -E "tube|mpc|simulator|recorder|move_base" || echo "  Some nodes may not be running"

echo ""
echo "Data topics:"
rostopic list 2>/dev/null | grep -E "odom|cmd_vel|global_path|mpc_metrics" | head -10

# Monitor navigation progress
echo ""
echo "=== Navigation Progress (every 5 seconds) ==="
for i in $(seq 1 $((TEST_DURATION/5))); do
    echo "[$(date +%H:%M:%S)] Check #$i:"
    timeout 3 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" | head -4 || echo "  No odom data"
    sleep 5
done

# Final check
echo ""
echo "=== Final Status ==="
timeout 3 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" || echo "No final position data"

# Check if data was recorded
echo ""
echo "=== Data Recording Check ==="
CSV_FILES=$(ls -1 /tmp/tube_mpc_data/navigation_data_*.csv 2>/dev/null | tail -1)
if [ -n "$CSV_FILES" ]; then
    echo "✓ Data recorded: $CSV_FILES"
    LINES=$(wc -l < "$CSV_FILES")
    echo "  Records: $LINES"

    # Show sample data
    echo ""
    echo "Sample data (last 3 rows):"
    tail -3 "$CSV_FILES" | column -t -s,
else
    echo "✗ No data files found"
fi

echo ""
echo "=== Test Log (last 20 lines) ==="
tail -20 /tmp/viz_test.log | grep -E "Tube|MPC|Goal|position|ERROR|WARN" || tail -20 /tmp/viz_test.log

echo ""
echo "✓ Test completed"
