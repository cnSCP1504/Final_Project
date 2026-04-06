#!/bin/bash
# ============================================================================
# V4 Large Angle Turn Fix - Comprehensive Testing Script
# ============================================================================
# This script tests the V4 fix for large angle turning problems
#
# V4 Fix Components:
# 1. Angular velocity direction correction
# 2. Continuous rotation state machine (30° exit threshold)
# 3. Conditional minimum speed threshold
#
# Expected Behavior:
# - Robot should rotate in place when etheta > 90°
# - Robot should rotate in the CORRECT direction (not shortest path)
# - Robot should continue rotating until etheta < 30°
# - Robot should NOT stop in the wrong direction
# ============================================================================

set -e  # Exit on error

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     V4 Large Angle Turn Fix - Testing Script                   ║"
echo "║     Date: 2026-04-02                                           ║"
echo "║     Testing etheta normalization and rotation behavior         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Step 1: Clean ROS processes
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Step 1: Cleaning ROS processes...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

killall -9 roslaunch rosmaster roscore gzserver gzclient python 2>/dev/null || true
sleep 2

echo -e "${GREEN}✓ ROS processes cleaned${NC}"
echo ""

# Step 2: Verify code implementation
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Step 2: Verifying V4 fix implementation...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

TUBEMPC_FILE="src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"
HEADER_FILE="src/tube_mpc_ros/mpc_ros/include/TubeMPCNode.h"

# Check 1: Angular velocity direction correction
echo -n "Checking angular velocity direction correction... "
if grep -q "CORRECTING ANGULAR VELOCITY DIRECTION" "$TUBEMPC_FILE"; then
    echo -e "${GREEN}✓ FOUND${NC}"
else
    echo -e "${RED}✗ NOT FOUND${NC}"
    exit 1
fi

# Check 2: State machine for continuous rotation
echo -n "Checking continuous rotation state machine... "
if grep -q "ENTERING pure rotation mode" "$TUBEMPC_FILE"; then
    echo -e "${GREEN}✓ FOUND${NC}"
else
    echo -e "${RED}✗ NOT FOUND${NC}"
    exit 1
fi

# Check 3: Conditional minimum speed threshold
echo -n "Checking conditional minimum speed threshold... "
if grep -q "pure_rotation_mode = (std::abs(etheta) > 1.57)" "$TUBEMPC_FILE"; then
    echo -e "${GREEN}✓ FOUND${NC}"
else
    echo -e "${RED}✗ NOT FOUND${NC}"
    exit 1
fi

# Check 4: State variable in header
echo -n "Checking state variable in header file... "
if grep -q "_in_place_rotation" "$HEADER_FILE"; then
    echo -e "${GREEN}✓ FOUND${NC}"
else
    echo -e "${RED}✗ NOT FOUND${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All V4 fix components verified${NC}"
echo ""

# Step 3: Build system
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Step 3: Building system...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

catkin_make
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo ""

# Step 4: Create test scenario documentation
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Step 4: Test scenario setup...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cat > /tmp/etheta_test_scenarios.txt << 'EOF'
═══════════════════════════════════════════════════════════════════════════
                    V4 FIX TEST SCENARIOS
═══════════════════════════════════════════════════════════════════════════

Test Scenario 1: Moderate Large Angle Turn (120°)
─────────────────────────────────────────────────────────────────────────
Start Position: (-8.0, 0.0)
Start Heading: East (0°)
Goal 1: (0.0, -7.0) [normal navigation]
Goal 2: (8.0, 0.0) [requires ~120° left turn]

Expected Behavior:
  1. Reach Goal 1 successfully
  2. When Goal 2 is sent:
     - etheta ≈ 2.09 rad (120°)
     - Console: "ENTERING pure rotation mode"
     - Console: "CORRECTING ANGULAR VELOCITY DIRECTION"
     - Speed: 0.0 m/s
     - Angular velocity: Positive (left turn)
  3. Continuous rotation:
     - etheta: 120° → 90° → 60° → 30°
     - Console: "PURE ROTATION MODE ACTIVE" (repeated)
  4. Exit rotation:
     - Console: "EXITING pure rotation mode"
     - Speed: > 0.3 m/s (starts moving forward)
  5. Navigate to Goal 2

SUCCESS CRITERIA:
  ✓ Robot rotates LEFT (not right)
  ✓ Robot continues rotating until etheta < 30°
  ✓ Robot does NOT stop at -60° (wrong direction)
  ✓ Robot reaches Goal 2

═══════════════════════════════════════════════════════════════════════════

Test Scenario 2: Extreme Large Angle Turn (180°)
─────────────────────────────────────────────────────────────────────────
Start Position: (-8.0, 0.0)
Start Heading: East (0°)
Goal 1: (0.0, -7.0) [normal navigation]
Goal 2: (-8.0, 0.0) [requires 180° turn]

Expected Behavior:
  1. Reach Goal 1 successfully
  2. When Goal 2 is sent:
     - etheta ≈ 3.14 rad (180°)
     - Console: "ENTERING pure rotation mode"
     - Robot rotates in place
  3. Continuous rotation:
     - Should take longer than 120° turn
     - Console shows decreasing etheta values
  4. Exit rotation at etheta < 30°
  5. Navigate to Goal 2

SUCCESS CRITERIA:
  ✓ Robot rotates in correct direction
  ✓ Rotation is sustained (not stopping early)
  ✓ Eventually reaches Goal 2

═══════════════════════════════════════════════════════════════════════════

Test Scenario 3: Right Angle Turn (>90°)
─────────────────────────────────────────────────────────────────────────
Start Position: (0.0, -8.0)
Start Heading: South (-90°)
Goal 1: (0.0, 0.0) [normal navigation]
Goal 2: (8.0, 0.0) [requires ~90° right turn]

Expected Behavior:
  1. Reach Goal 1 successfully
  2. When Goal 2 is sent:
     - etheta ≈ -1.57 rad (-90°)
     - Console: "ENTERING pure rotation mode"
     - Angular velocity: Negative (right turn)
  3. Rotate in place until etheta > -30°
  4. Exit and navigate to Goal 2

SUCCESS CRITERIA:
  ✓ Robot rotates RIGHT (not left)
  ✓ Sustained rotation until threshold
  ✓ Reaches Goal 2

═══════════════════════════════════════════════════════════════════════════

KEY LOG OUTPUTS TO MONITOR:
─────────────────────────────────────────────────────────────────────────
✓ "ENTERING pure rotation mode"     → Robot entered rotation mode
✓ "CORRECTING ANGULAR VELOCITY DIRECTION" → Direction was corrected
✓ "PURE ROTATION MODE ACTIVE"       → Robot is rotating
✓ "EXITING pure rotation mode"      → Robot finished rotating
✓ "Pure rotation mode: Min speed threshold disabled" → Speed logic working

═══════════════════════════════════════════════════════════════════════════
EOF

cat /tmp/etheta_test_scenarios.txt
echo ""

# Step 5: Instructions for manual testing
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Step 5: Manual Testing Instructions${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cat << 'EOF'
To test the V4 fix manually:

1. Start the system:
   $ roslaunch safe_regret_mpc safe_regret_mpc_test.launch

2. In RViz, set "2D Nav Goal" to test scenarios above

3. Monitor console output for:
   - "ENTERING pure rotation mode"
   - "CORRECTING ANGULAR VELOCITY DIRECTION"
   - "EXITING pure rotation mode"

4. Verify robot behavior:
   - Rotates in correct direction (matching etheta sign)
   - Continues rotating until etheta < 30°
   - Does not stop in wrong direction
   - Successfully reaches goal

5. Check /cmd_vel topic:
   $ rostopic echo /cmd_vel

   During rotation:
   - linear.x: 0.0
   - angular.z: should match etheta sign

6. Record test results:
   - Take screenshots of console output
   - Note any unexpected behavior
   - Check if all success criteria are met

EOF

echo ""

# Step 6: Automated test (optional)
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Step 6: Automated System Launch${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

read -p "Do you want to launch the system now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${GREEN}Launching system...${NC}"
    roslaunch safe_regret_mpc safe_regret_mpc_test.launch
else
    echo -e "${YELLOW}System launch skipped. Run manually when ready.${NC}"
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     Testing script completed!                                    ║${NC}"
echo -e "${GREEN}║     Test scenarios saved to: /tmp/etheta_test_scenarios.txt     ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
