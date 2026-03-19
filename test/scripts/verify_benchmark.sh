#!/bin/bash
#
# Quick verification script for tube_mpc_benchmark
#

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║   Tube MPC Benchmark - Quick Verification                 ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check if we're in the right directory
if [ ! -d "src/tube_mpc_benchmark" ]; then
    echo "❌ Error: tube_mpc_benchmark package not found"
    echo "Please run this script from /home/dixon/Final_Project/catkin"
    exit 1
fi

# Source ROS environment
echo "🔧 Sourcing ROS environment..."
source devel/setup.bash

# Check package
echo ""
echo "📦 Checking package..."
if rospack find tube_mpc_benchmark >/dev/null 2>&1; then
    PKG_PATH=$(rospack find tube_mpc_benchmark)
    echo "✅ Package found at: $PKG_PATH"
else
    echo "❌ Package not found by ROS"
    exit 1
fi

# Check files
echo ""
echo "📄 Checking files..."
FILES=(
    "scripts/kinematic_robot_sim.py"
    "scripts/benchmark_runner.py"
    "scripts/run_benchmark.sh"
    "launch/quick_test.launch"
    "launch/benchmark_batch.launch"
    "configs/empty_map.yaml"
    "configs/empty_map.pgm"
)

ALL_FOUND=true
for file in "${FILES[@]}"; do
    if [ -f "$PKG_PATH/$file" ]; then
        echo "✅ $file"
    else
        echo "❌ $file missing"
        ALL_FOUND=false
    fi
done

if [ "$ALL_FOUND" = false ]; then
    exit 1
fi

# Check Python syntax
echo ""
echo "🐍 Checking Python syntax..."
PYTHON_SCRIPTS=(
    "$PKG_PATH/scripts/kinematic_robot_sim.py"
    "$PKG_PATH/scripts/benchmark_runner.py"
)

for script in "${PYTHON_SCRIPTS[@]}"; do
    if python3 -m py_compile "$script" 2>/dev/null; then
        echo "✅ $(basename $script)"
    else
        echo "❌ $(basename $script) has syntax errors"
        exit 1
    fi
done

# Check ROS dependencies
echo ""
echo "🔗 Checking ROS dependencies..."
DEPS=(
    "rospy"
    "geometry_msgs"
    "nav_msgs"
    "sensor_msgs"
    "move_base"
)

for dep in "${DEPS[@]}"; do
    if rospack find "$dep" >/dev/null 2>&1; then
        echo "✅ $dep"
    else
        echo "❌ $dep not found"
        exit 1
    fi
done

# Summary
echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                    ✅ ALL CHECKS PASSED                    ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "🚀 The benchmark system is ready to use!"
echo ""
echo "Quick start:"
echo "  1. Visual test:"
echo "     roslaunch tube_mpc_benchmark quick_test.launch"
echo ""
echo "  2. Batch test (10 runs):"
echo "     roslaunch tube_mpc_benchmark benchmark_batch.launch num_tests:=10"
echo ""
echo "  3. Full benchmark (100 runs):"
echo "     ./src/tube_mpc_benchmark/scripts/run_benchmark.sh -n 100"
echo ""
echo "📖 Documentation:"
echo "  - BENCHMARK_TEST_GUIDE.md: Testing guide"
echo "  - src/tube_mpc_benchmark/README.md: Full documentation"
echo "  - src/tube_mpc_benchmark/docs/QUICK_START.md: Quick start"
echo ""

exit 0
