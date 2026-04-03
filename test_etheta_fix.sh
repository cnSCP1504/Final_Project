#!/bin/bash

# Test script to verify etheta fix for large angle turns

echo "=== Etheta Fix Verification Test ==="
echo ""
echo "Test Scenario: Large angle turn (>90 degrees)"
echo ""

# Get robot initial position
echo "1. Getting initial robot position..."
INITIAL_POSE=$(rostopic echo /odom --noarr | grep -A 10 "pose:" | head -11)
echo "$INITIAL_POSE"
echo ""

# Wait for robot to reach first goal
echo "2. Waiting for robot to reach first goal..."
sleep 15

# Get final position after first goal
echo "3. Getting position after first goal..."
SECOND_POSE=$(rostopic echo /odom --noarr | grep -A 10 "pose:" | head -11)
echo "$SECOND_POSE"
echo ""

# Send second goal with large angle difference
echo "4. Publishing second goal with large angle difference..."
rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: 8.0
    y: 0.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once

echo ""
echo "5. Monitoring etheta values for the next 10 seconds..."
echo "   Expected: etheta should smoothly transition without large jumps"
echo ""

# Monitor rosout for etheta values
timeout 10 rostopic echo /rosout | grep --line-buffered "etheta" | head -20

echo ""
echo "=== Test Complete ==="
