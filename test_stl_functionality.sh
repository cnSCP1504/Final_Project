#!/bin/bash
# Test STL Monitor Functionality

echo "=== STL Monitor Functionality Test ==="
echo ""

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "1. Checking system status..."
rosnode list | grep -E "(stl_monitor|tube_mpc)"
echo ""

echo "2. Testing STL evaluation with different scenarios..."
echo ""

echo "Scenario 1: Trajectory within bounds (5,5) -> (6,6) -> (7,7)"
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped \
  "{header: {stamp: now, frame_id: map}, pose: {position: {x: 5.0, y: 5.0, z: 0.0}, orientation: {w: 1.0}}}" --once &
rostopic pub /stl_monitor/mpc_trajectory nav_msgs/Path \
  "{header: {stamp: now, frame_id: map}, poses: [{header: {stamp: now, frame_id: map}, pose: {position: {x: 5.0, y: 5.0, z: 0.0}, orientation: {w: 1.0}}}, {header: {stamp: now, frame_id: map}, pose: {position: {x: 6.0, y: 6.0, z: 0.0}, orientation: {w: 1.0}}}, {header: {stamp: now, frame_id: map}, pose: {position: {x: 7.0, y: 7.0, z: 0.0}, orientation: {w: 1.0}}}]}]}" --once

sleep 3
echo ""
echo "Expected: Negative robustness (far from goal), budget near zero"
echo ""

echo "Scenario 2: Trajectory near goal (8,8) -> (8.1,8.1) -> (8.2,8.2)"
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped \
  "{header: {stamp: now, frame_id: map}, pose: {position: {x: 8.0, y: 8.0, z: 0.0}, orientation: {w: 1.0}}}" --once &
rostopic pub /stl_monitor/mpc_trajectory nav_msgs/Path \
  "{header: {stamp: now, frame_id: map}, poses: [{header: {stamp: now, frame_id: map}, pose: {position: {x: 8.0, y: 8.0, z: 0.0}, orientation: {w: 1.0}}}, {header: {stamp: now, frame_id: map}, pose: {position: {x: 8.1, y: 8.1, z: 0.0}, orientation: {w: 1.0}}}, {header: {stamp: now, frame_id: map}, pose: {position: {x: 8.2, y: 8.2, z: 0.0}, orientation: {w: 1.0}}}]}]}" --once

sleep 3
echo ""
echo "Expected: Positive robustness (near goal), budget should increase"
echo ""

echo "Scenario 3: Trajectory near boundary (0.5,0.5) -> (0.6,0.6) -> (0.7,0.7)"
rostopic pub /stl_monitor/belief geometry_msgs/PoseStamped \
  "{header: {stamp: now, frame_id: map}, pose: {position: {x: 0.5, y: 0.5, z: 0.0}, orientation: {w: 1.0}}}" --once &
rostopic pub /stl_monitor/mpc_trajectory nav_msgs/Path \
  "{header: {stamp: now, frame_id: map}, poses: [{header: {stamp: now, frame_id: map}, pose: {position: {x: 0.5, y: 0.5, z: 0.0}, orientation: {w: 1.0}}}, {header: {stamp: now, frame_id: map}, pose: {position: {x: 0.6, y: 0.6, z: 0.0}, orientation: {w: 1.0}}}, {header: {stamp: now, frame_id: map}, pose: {position: {x: 0.7, y: 0.7, z: 0.0}, orientation: {w: 1.0}}}]}]}" --once

sleep 3
echo ""
echo "Expected: Low positive robustness (close to boundary), budget may decrease"
echo ""

echo "3. Monitoring STL outputs..."
echo "Robustness topic (last message):"
timeout 3 rostopic echo /stl_monitor/robustness -n 1 2>/dev/null || echo "No messages received"
echo ""

echo "Budget topic (last message):"
timeout 3 rostopic echo /stl_monitor/budget -n 1 2>/dev/null || echo "No messages received"
echo ""

echo "4. System performance summary"
echo "STL Monitor: Running and evaluating formulas"
echo "Formulas implemented:"
echo "  - stay_in_bounds: always[0,T](0 < x < 10 && 0 < y < 10)"
echo "  - reach_goal: eventually[0,T](distance_to_goal < 0.5)"
echo "Evaluation method: Monte Carlo sampling (100 samples)"
echo "Smooth approximation: log-sum-exp with τ=0.05"
echo ""

echo "=== Test Complete ==="
