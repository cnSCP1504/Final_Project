#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Tube MPC Visualization Test                           ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Parse arguments
GOAL_X=${1:-4.0}
GOAL_Y=${2:-4.0}
VISUALIZER=${3:-false}

echo "Test Configuration:"
echo "  Goal: ($GOAL_X, $GOAL_Y)"
echo "  Real-time Plotter: $VISUALIZER"
echo ""

# Launch test with visualization
roslaunch tube_mpc_benchmark visualization_test.launch \
    goal_x:=$GOAL_X \
    goal_y:=$GOAL_Y \
    enable_visualizer:=$VISUALIZER \
    enable_recorder:=true

echo ""
echo "Test completed"
echo "Data saved to: /tmp/tube_mpc_data/"
echo ""
echo "To analyze results:"
echo "  python3 src/tube_mpc_benchmark/scripts/analyze_results.py \\"
echo "    --csv /tmp/tube_mpc_data/navigation_data_*.csv \\"
echo "    --output my_analysis.png"
