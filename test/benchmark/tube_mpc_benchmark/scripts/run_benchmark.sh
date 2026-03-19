#!/bin/bash
#
# Tube MPC Benchmark - Batch Testing Script
#
# Usage:
#   ./run_benchmark.sh [OPTIONS]
#
# Options:
#   -n, --num-tests NUM      Number of tests (default: 100)
#   -c, --controller TYPE    Controller: tube_mpc, mpc, dwa, pure_pursuit (default: tube_mpc)
#   -m, --map-size SIZE      Map size in meters (default: 20)
#   -o, --output-dir DIR     Output directory (default: /tmp/tube_mpc_benchmark)
#   -v, --visualize          Show RViz during test
#   -h, --help               Show this help
#

set -e

# Default parameters
NUM_TESTS=100
CONTROLLER="tube_mpc"
MAP_SIZE=20.0
OUTPUT_DIR="/tmp/tube_mpc_benchmark"
VISUALIZE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-tests)
            NUM_TESTS="$2"
            shift 2
            ;;
        -c|--controller)
            CONTROLLER="$2"
            shift 2
            ;;
        -m|--map-size)
            MAP_SIZE="$2"
            shift 2
            ;;
        -o|--output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -v|--visualize)
            VISUALIZE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -n, --num-tests NUM       Number of tests (default: 100)"
            echo "  -c, --controller TYPE     Controller: tube_mpc, mpc, dwa, pure_pursuit (default: tube_mpc)"
            echo "  -m, --map-size SIZE       Map size in meters (default: 20)"
            echo "  -o, --output-dir DIR      Output directory (default: /tmp/tube_mpc_benchmark)"
            echo "  -v, --visualize           Show RViz during test"
            echo "  -h, --help                Show this help"
            echo ""
            echo "Examples:"
            echo "  $0 -n 50 -c tube_mpc"
            echo "  $0 --num-tests 200 --controller dwa --visualize"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage"
            exit 1
            ;;
    esac
done

# Validate controller
if [[ ! "$CONTROLLER" =~ ^(tube_mpc|mpc|dwa|pure_pursuit)$ ]]; then
    echo "Error: Invalid controller '$CONTROLLER'"
    echo "Valid options: tube_mpc, mpc, dwa, pure_pursuit"
    exit 1
fi

# Print configuration
echo "=========================================="
echo "Tube MPC Benchmark"
echo "=========================================="
echo "Configuration:"
echo "  Tests:        $NUM_TESTS"
echo "  Controller:   $CONTROLLER"
echo "  Map size:     ${MAP_SIZE}m"
echo "  Output:       $OUTPUT_DIR"
echo "  Visualize:    $VISUALIZE"
echo "=========================================="
echo ""

# Generate empty map
echo "Generating empty map..."
python3 $(rospack find tube_mpc_benchmark)/scripts/generate_empty_map.py

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run benchmark
echo "Starting benchmark..."
roslaunch tube_mpc_benchmark benchmark_batch.launch \
    num_tests:=$NUM_TESTS \
    controller:=$CONTROLLER \
    map_size:=$MAP_SIZE \
    output_dir:="$OUTPUT_DIR" \
    rviz:=$VISUALIZE

echo ""
echo "=========================================="
echo "Benchmark complete!"
echo "Results saved to: $OUTPUT_DIR"
echo "=========================================="

# Show quick summary
if [ -f "$OUTPUT_DIR/benchmark_summary.json" ]; then
    echo ""
    echo "Quick Summary:"
    python3 - <<EOF
import json
with open('$OUTPUT_DIR/benchmark_summary.json', 'r') as f:
    data = json.load(f)
    print(f"  Success rate: {data['success_rate']:.1f}%")
    print(f"  Avg time: {data['avg_completion_time']:.2f}s")
    print(f"  Avg error: {data['avg_position_error']:.3f}m")
EOF
fi
