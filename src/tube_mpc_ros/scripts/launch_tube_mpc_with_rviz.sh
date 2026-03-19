#!/bin/bash

# Tube MPC Navigation with Complete RViz Visualization
# This script launches Tube MPC navigation with the complete RViz configuration

echo "========================================="
echo "Tube MPC Navigation with Full Visualization"
echo "========================================="
echo ""

# Check if ROS master is already running
if pgrep -x "rosmaster" > /dev/null; then
    echo "⚠️  ROS master is already running"
    echo "   Using existing ROS master..."
else
    echo "🚀 Starting ROS master..."
    roscore &
    sleep 3
fi

# Check if workspace is sourced
if [ -z "$ROS_PACKAGE_PATH" ]; then
    echo "📦 Sourcing workspace..."
    source /home/dixon/Final_Project/catkin/devel/setup.bash
fi

# Launch Tube MPC navigation with new RViz config
echo "🎯 Launching Tube MPC navigation..."
echo "   Using complete RViz configuration with Tube boundaries"
echo ""

# Kill any existing rviz instances to avoid conflicts
pkill -f rviz

# Launch with new configuration
roslaunch tube_mpc_ros tube_mpc_navigation.launch \
    rviz_config:=/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz

echo ""
echo "========================================="
echo "Visualization Guide:"
echo "========================================="
echo "🔴 Red Line    - MPC Predicted Trajectory"
echo "🔵 Blue Lines  - Tube Boundaries (4 lines)"
echo "🟡 Yellow Line - MPC Reference Path"
echo "🟢 Green Line  - Global Path"
echo "🟣 Purple      - Robot Footprint"
echo "⬜ Gray Area   - Costmap"
echo ""
echo "Use '2D Nav Goal' to set navigation target"
echo "========================================="
