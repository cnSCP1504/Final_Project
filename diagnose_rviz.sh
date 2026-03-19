#!/bin/bash
# RViz Diagnostic Script for Tube MPC + DR Tightening Integration

echo "========================================="
echo "RViz Diagnostic Tool"
echo "========================================="
echo ""

# Source ROS
source /opt/ros/noetic/setup.bash
source devel/setup.bash

echo "1. Checking ROS Master..."
if rostopic list > /dev/null 2>&1; then
    echo "   ✓ ROS Master is running"
else
    echo "   ✗ ROS Master NOT running - start roscore first"
    exit 1
fi

echo ""
echo "2. Checking TF Frames..."
echo "   Current TF tree:"
timeout 3 rosrun tf view_frames 2>/dev/null || echo "   Generating TF frames..."
if [ -f frames.pdf ]; then
    echo "   ✓ TF tree saved to frames.pdf"
    echo "   Opening PDF..."
    xdg-open frames.pdf 2>/dev/null || evince frames.pdf 2>/dev/null || echo "   (Could not open PDF automatically)"
fi

echo ""
echo "3. Checking if map frame exists..."
if timeout 2 rosrun tf tf_echo map odom 2>&1 | grep -q "Translation"; then
    echo "   ✓ map→odom transform exists (AMCL is working)"
else
    echo "   ✗ map→odom transform MISSING (AMCL not publishing yet)"
    echo "   → This is why RViz shows blank!"
    echo "   → Solution: Wait a few seconds for AMCL to initialize"
fi

echo ""
echo "4. Checking Visualization Topics..."
echo "   Available topics:"

# Check Tube MPC topics
if rostopic list | grep -q "/mpc_trajectory"; then
    echo "   ✓ /mpc_trajectory (Tube MPC planned path)"
else
    echo "   ✗ /mpc_trajectory NOT found"
fi

if rostopic list | grep -q "/tube_boundaries"; then
    echo "   ✓ /tube_boundaries (Tube MPC safety boundaries)"
else
    echo "   ✗ /tube_boundaries NOT found"
fi

# Check DR Tightening topics
if rostopic list | grep -q "/dr_margins_debug"; then
    echo "   ✓ /dr_margins_debug (DR tightening visualization)"
else
    echo "   ✗ /dr_margins_debug NOT found"
fi

# Check Navigation topics
if rostopic list | grep -q "/move_base/GlobalPlanner/plan"; then
    echo "   ✓ /move_base/GlobalPlanner/plan (Global path)"
else
    echo "   ✗ /move_base/GlobalPlanner/plan NOT found"
fi

if rostopic list | grep -q "/map"; then
    echo "   ✓ /map (Static map)"
else
    echo "   ✗ /map NOT found"
fi

echo ""
echo "5. Checking Topic Activity..."
echo "   Topic publishing rates:"

for topic in "/mpc_trajectory" "/tube_boundaries" "/dr_margins_debug" "/move_base/GlobalPlanner/plan"; do
    if rostopic list | grep -q "$topic"; then
        rate=$(timeout 3 rostopic hz $topic 2>&1 | grep "average rate" | head -1 || echo "0.0")
        if [[ $rate == *"0.0"* ]] || [[ $rate == "" ]]; then
            echo "   ⚠ $topic: Not publishing yet"
        else
            echo "   ✓ $topic: $rate"
        fi
    fi
done

echo ""
echo "6. RViz Configuration Check..."
RVIZ_CONFIG="src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz"
if [ -f "$RVIZ_CONFIG" ]; then
    echo "   ✓ RViz config exists: $RVIZ_CONFIG"

    # Check if DR markers are included
    if grep -q "dr_margins_debug" "$RVIZ_CONFIG"; then
        echo "   ✓ DR Tightening markers configured"
    else
        echo "   ✗ DR Tightening markers NOT configured"
    fi

    # Check fixed frame
    FIXED_FRAME=$(grep "Fixed Frame" "$RVIZ_CONFIG" | head -1)
    echo "   Fixed Frame: $FIXED_FRAME"
else
    echo "   ✗ RViz config NOT found"
fi

echo ""
echo "========================================="
echo "RViz Display Components"
echo "========================================="
echo ""
echo "What you SHOULD see in RViz:"
echo ""
echo "1. Robot Model:"
echo "   → Shows robot at current position"
echo "   → Requires: TF transforms (odom→base_footprint→base_link)"
echo "   → Color: Blue/gray robot model"
echo ""
echo "2. Map:"
echo "   → Shows static map of environment"
echo "   → Topic: /map"
echo "   → Color: Gray/white map with walls"
echo ""
echo "3. Global Path (Green):"
echo "   → Shows planned path from start to goal"
echo "   → Topic: /move_base/GlobalPlanner/plan"
echo "   → Color: Green line"
echo ""
echo "4. MPC Trajectory (Red):"
echo "   → Shows short-term MPC predictions"
echo "   → Topic: /mpc_trajectory"
echo "   → Color: Red line"
echo ""
echo "5. Tube Boundaries (Blue):"
echo "   → Shows safety tube around trajectory"
echo "   → Topic: /tube_boundaries"
echo "   → Color: Blue lines"
echo ""
echo "6. DR Margins Debug (Markers):"
echo "   → Shows distributionally robust margins"
echo "   → Topic: /dr_margins_debug"
echo "   → Color: Various colored spheres/lines"
echo ""
echo "7. Costmaps:"
echo "   → Shows local and global collision costmaps"
echo "   → Topics: /move_base/local_costmap/costmap, /move_base/global_costmap/costmap"
echo "   → Color: Semi-transparent overlays"
echo ""

echo "========================================="
echo "Troubleshooting Steps"
echo "========================================="
echo ""
echo "If RViz shows BLANK:"
echo ""
echo "Step 1: Check Fixed Frame"
echo "   → In RViz: Sets → Fixed Frame → Change from 'map' to 'odom'"
echo "   → If odom works, the issue is with map→odom transform"
echo ""
echo "Step 2: Check TF Tree"
echo "   → Run: rosrun tf view_frames"
echo "   → Open frames.pdf to see the TF tree"
echo "   → Look for missing connections"
echo ""
echo "Step 3: Enable Displays"
echo "   → In RViz left panel: Click checkboxes next to display names"
echo "   → Make sure Robot Model, Map, and Path displays are enabled"
echo ""
echo "Step 4: Check Camera Position"
echo "   → In RViz: Use mouse to zoom (scroll wheel) and pan (middle-click drag)"
echo "   → Try: 'F' key to focus on robot"
echo ""
echo "Step 5: Wait for Initialization"
echo "   → AMCL may take 5-10 seconds to start publishing map→odom"
echo "   → Global planner may take a few seconds after goal is set"
echo ""

echo "========================================="
echo "Quick Fixes"
echo "========================================="
echo ""
echo "Fix 1: Restart RViz with correct config"
echo "   rosrun rviz rviz -d \$(find tube_mpc_ros)/params/tube_mpc_navigation.rviz"
echo ""
echo "Fix 2: Force TF initialization"
echo "   rosrun tf static_transform_publisher 0 0 0 0 0 0 map odom 100"
echo ""
echo "Fix 3: Check if nodes are running"
echo "   rosnode list | grep -E '(amcl|map_server|move_base|tube_mpc|dr_tightening)'"
echo ""
echo "Fix 4: Manually set initial pose in RViz"
echo "   → Use '2D Pose Estimate' tool in RViz"
echo "   → Click on map near robot start position"
echo ""

echo "Diagnostic complete!"
