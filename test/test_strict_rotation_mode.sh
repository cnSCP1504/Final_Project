#!/bin/bash
# Test Script for Strict Pure Rotation Mode (30-degree Exit Threshold)
# Version: V1.0
# Date: 2026-04-03
#
# This script tests the improved pure rotation mode where:
# - Once entered (angle > 90°), the robot MUST rotate to < 30° before exiting
# - NO forward motion is allowed during rotation mode
# - Only in-place rotation is permitted

set -e  # Exit on error

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Test: Strict Pure Rotation Mode (30° Exit Threshold)          ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Check if ROS is already running
if pgrep -f "roslaunch|rosmaster|gzserver" > /dev/null; then
    echo "⚠️  WARNING: ROS/Gazebo processes detected!"
    echo "   Killing all ROS processes..."
    killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
    sleep 2
fi

echo "✅ Clean environment confirmed"
echo ""

# Step 1: Start the system
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Starting Safe-Regret MPC system..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Start in background
roslaunch safe_regret_mpc safe_regret_mpc_test.launch > /tmp/safe_regret_test.log 2>&1 &
LAUNCH_PID=$!

echo "✅ System started (PID: $LAUNCH_PID)"
echo "   Log file: /tmp/safe_regret_test.log"
echo ""

# Wait for system to initialize
echo "⏳ Waiting for system initialization (10 seconds)..."
sleep 10

# Step 2: Monitor for the key log messages
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: Monitoring for Pure Rotation Mode activation..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "🔍 Looking for these log messages:"
echo "   1. 'ENTERING PURE ROTATION MODE' (when angle > 90°)"
echo "   2. 'Speed LOCKED at 0.0 m/s' (during rotation)"
echo "   3. 'EXITING PURE ROTATION MODE' (when angle < 30°)"
echo ""

# Monitor log file for rotation mode activation
echo "📊 Real-time log monitoring (Ctrl+C to stop):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Tail the log and filter for relevant messages
tail -f /tmp/safe_regret_test.log | grep --line-buffered -E "PURE ROTATION|Speed LOCKED|EXITING|etheta.*deg" || true

# Cleanup
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test completed. Cleaning up..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

kill $LAUNCH_PID 2>/dev/null || true
killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true

echo "✅ Cleanup complete"
echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Test Summary                                                   ║"
echo "╠════════════════════════════════════════════════════════════════╣"
echo "║  To verify the improvement:                                     ║"
echo "║  1. Send a goal that requires >90° turn                         ║"
echo "║  2. Check console for 'ENTERING PURE ROTATION MODE'             ║"
echo "║  3. Verify speed stays at 0.0 m/s during rotation              ║"
echo "║  4. Verify exit only when angle < 30°                           ║"
echo "╚════════════════════════════════════════════════════════════════╝"
