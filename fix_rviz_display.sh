#!/bin/bash
# Quick fix script for RViz blank display issue

echo "========================================="
echo "RViz Display Fix Script"
echo "========================================="
echo ""

source /opt/ros/noetic/setup.bash
source devel/setup.bash

echo "Step 1: Checking TF tree status..."
echo "Current TF frames:"
timeout 2 rosrun tf tf_echo map odom 2>&1 | head -5

echo ""
echo "Step 2: Manually set initial pose to force AMCL initialization..."
echo "Publishing initial pose at (0, 0) with heading -1.507 rad..."

timeout 3 rostopic pub /initialpose geometry_msgs/PoseWithCovarianceStamped "
header:
  stamp: now
  frame_id: 'map'
pose:
  pose:
    position:
      x: 0.0
      y: 0.0
      z: 0.0
    orientation:
      x: 0.0
      y: 0.0
      z: -0.697849326905
      w: 0.716248519467
  covariance: [0.25, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.25, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.06853891945200942]"
echo ""

echo "Step 3: Waiting for TF to stabilize..."
sleep 3

echo ""
echo "Step 4: Checking TF again..."
if timeout 2 rosrun tf tf_echo map odom 2>&1 | grep -q "Translation"; then
    echo "✓ map→odom transform is now working!"
else
    echo "⚠ Still waiting for map→odom transform..."
    echo "This may take another 5-10 seconds for AMCL to fully initialize"
fi

echo ""
echo "Step 5: Checking topics..."
echo "odom_path publishing:"
if timeout 2 rostopic echo /odom_path -n1 2>&1 | grep -q "poses"; then
    echo "✓ /odom_path is now publishing"
    echo "This means Tube MPC should start working!"
else
    echo "⚠ /odom_path still not publishing"
    echo "The path transformation is still failing"
fi

echo ""
echo "Step 6: Tube MPC topics..."
echo "mpc_trajectory:"
if timeout 2 rostopic echo /mpc_trajectory -n1 2>&1 | grep -q "pose"; then
    echo "✓ /mpc_trajectory is publishing"
else
    echo "⚠ /mpc_trajectory not publishing yet"
fi

echo ""
echo "tracking_error (for DR Tightening):"
if timeout 2 rostopic echo /tube_mpc/tracking_error -n1 2>&1 | grep -q "data"; then
    echo "✓ /tube_mpc/tracking_error is publishing"
    echo "DR Tightening should now be working!"
else
    echo "⚠ /tube_mpc/tracking_error not publishing yet"
    echo "Tube MPC hasn't started solving yet"
fi

echo ""
echo "========================================="
echo "RViz Display Instructions"
echo "========================================="
echo ""
echo "If RViz is still blank:"
echo ""
echo "1. Change Fixed Frame:"
echo "   → In RViz: Sets → Fixed Frame → Change to 'odom'"
echo "   → If odom works but map doesn't, the issue is map→odom transform"
echo ""
echo "2. Check displays are enabled:"
echo "   → Left panel in RViz"
echo "   → Make sure checkboxes are ticked for:"
echo "     - Robot Model"
echo "     - Map"
echo "     - Global_Path"
echo "     - Tube MPC Visualization"
echo ""
echo "3. Focus camera:"
echo "   → Press 'F' key in RViz to focus on robot"
echo "   → Or use mouse to zoom (scroll) and pan (middle-click drag)"
echo ""
echo "4. Manually set initial pose:"
echo "   → Use '2D Pose Estimate' tool in RViz toolbar"
echo "   → Click on map at robot start position (near origin)"
echo ""
echo "5. Wait for AMCL:"
echo "   → AMCL may take 10-15 seconds to fully initialize"
echo "   → The robot needs to receive laser scan data to localize"
echo ""
echo "========================================="
