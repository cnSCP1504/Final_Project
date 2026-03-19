#!/bin/bash
#
# Benchmark System Validation Script
# Tests the tube_mpc_benchmark package components
#

set -e

echo "=========================================="
echo "Tube MPC Benchmark - System Validation"
echo "=========================================="
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
PASSED=0
FAILED=0

test_pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((PASSED++))
}

test_fail() {
    echo -e "${RED}✗${NC} $1"
    ((FAILED++))
}

test_info() {
    echo -e "${YELLOW}→${NC} $1"
}

# Test 1: Package structure
echo "Test 1: Package Structure"
test_info "Checking package files..."

FILES=(
    "src/tube_mpc_benchmark/package.xml"
    "src/tube_mpc_benchmark/CMakeLists.txt"
    "src/tube_mpc_benchmark/scripts/kinematic_robot_sim.py"
    "src/tube_mpc_benchmark/scripts/benchmark_runner.py"
    "src/tube_mpc_benchmark/scripts/run_benchmark.sh"
    "src/tube_mpc_benchmark/launch/quick_test.launch"
    "src/tube_mpc_benchmark/launch/benchmark_batch.launch"
    "src/tube_mpc_benchmark/configs/empty_map.yaml"
    "src/tube_mpc_benchmark/configs/empty_map.pgm"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        test_pass "Found: $file"
    else
        test_fail "Missing: $file"
    fi
done
echo ""

# Test 2: ROS package
echo "Test 2: ROS Package"
test_info "Checking ROS package..."

if source devel/setup.bash && rospack find tube_mpc_benchmark >/dev/null 2>&1; then
    PACKAGE_PATH=$(rospack find tube_mpc_benchmark)
    test_pass "Package found at: $PACKAGE_PATH"
else
    test_fail "Package not found by ROS"
fi
echo ""

# Test 3: Python scripts
echo "Test 3: Python Scripts"
test_info "Checking Python syntax..."

PYTHON_SCRIPTS=(
    "src/tube_mpc_benchmark/scripts/kinematic_robot_sim.py"
    "src/tube_mpc_benchmark/scripts/benchmark_runner.py"
    "src/tube_mpc_benchmark/scripts/simple_goal_publisher.py"
    "src/tube_mpc_benchmark/scripts/generate_empty_map.py"
)

for script in "${PYTHON_SCRIPTS[@]}"; do
    if python3 -m py_compile "$script" 2>/dev/null; then
        test_pass "Valid Python: $script"
    else
        test_fail "Syntax error: $script"
    fi
done
echo ""

# Test 4: Shell scripts
echo "Test 4: Shell Scripts"
test_info "Checking shell script permissions..."

SHELL_SCRIPTS=(
    "src/tube_mpc_benchmark/scripts/run_benchmark.sh"
)

for script in "${SHELL_SCRIPTS[@]}"; do
    if [ -x "$script" ]; then
        test_pass "Executable: $script"
    else
        test_fail "Not executable: $script"
    fi
done
echo ""

# Test 5: Map files
echo "Test 5: Map Files"
test_info "Checking map configuration..."

MAP_YAML="src/tube_mpc_benchmark/configs/empty_map.yaml"
MAP_PGM="src/tube_mpc_benchmark/configs/empty_map.pgm"

if [ -f "$MAP_PGM" ]; then
    SIZE=$(stat -f%z "$MAP_PGM" 2>/dev/null || stat -c%s "$MAP_PGM" 2>/dev/null)
    if [ "$SIZE" -gt 100000 ]; then
        test_pass "Map image exists (${SIZE} bytes)"
    else
        test_fail "Map image too small: $SIZE bytes"
    fi
else
    test_fail "Map image not found"
fi

if grep -q "image:" "$MAP_YAML" 2>/dev/null; then
    test_pass "Map configuration valid"
else
    test_fail "Map configuration invalid"
fi
echo ""

# Test 6: Launch files
echo "Test 6: Launch Files"
test_info "Checking launch file syntax..."

LAUNCH_FILES=(
    "src/tube_mpc_benchmark/launch/quick_test.launch"
    "src/tube_mpc_benchmark/launch/benchmark_batch.launch"
)

for launch in "${LAUNCH_FILES[@]}"; do
    if xmllint --noout "$launch" 2>/dev/null; then
        test_pass "Valid XML: $launch"
    else
        test_fail "Invalid XML: $launch"
    fi
done
echo ""

# Test 7: Dependencies
echo "Test 7: ROS Dependencies"
test_info "Checking required ROS packages..."

REQUIRED_PACKAGES=(
    "rospy"
    "geometry_msgs"
    "nav_msgs"
    "sensor_msgs"
    "tf2_ros"
    "move_base_msgs"
)

for pkg in "${REQUIRED_PACKAGES[@]}"; do
    if rospack find "$pkg" >/dev/null 2>&1; then
        test_pass "Package available: $pkg"
    else
        test_fail "Package missing: $pkg"
    fi
done
echo ""

# Summary
echo "=========================================="
echo "Validation Summary"
echo "=========================================="
echo -e "${GREEN}Passed:${NC} $PASSED"
echo -e "${RED}Failed:${NC} $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    echo ""
    echo "The benchmark system is ready to use."
    echo ""
    echo "Next steps:"
    echo "  1. Run quick test:"
    echo "     roslaunch tube_mpc_benchmark quick_test.launch"
    echo ""
    echo "  2. Run batch benchmark:"
    echo "     ./src/tube_mpc_benchmark/scripts/run_benchmark.sh -n 10"
    echo ""
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo "Please check the errors above."
    exit 1
fi
