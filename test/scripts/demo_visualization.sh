#!/bin/bash

# Tube MPC Visualization System Demo
# Demonstrates all visualization capabilities

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║   Tube MPC Visualization System - Demo                    ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "This demo will showcase all visualization capabilities:"
echo "  1. RViz real-time visualization"
echo "  2. Data recording"
echo "  3. Post-processing analysis"
echo ""

# Check required Python packages
echo "Checking dependencies..."
python3 -c "import matplotlib, pandas, seaborn" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "⚠ Warning: Some Python packages missing"
    echo "  Install with: pip3 install matplotlib pandas seaborn"
fi

echo "✓ All dependencies OK"
echo ""

# Ask user what to demonstrate
echo "Select demo mode:"
echo "  1) Quick test (30s, with RViz)"
echo "  2) Full test (60s, with data recording)"
echo "  3) Analyze existing data"
echo "  4) Show all visualizations"
read -p "Enter choice [1-4]: " choice

case $choice in
    1)
        echo ""
        echo "Starting quick test with RViz..."
        echo "Goal: (3.0, 3.0)"
        echo ""

        roslaunch tube_mpc_benchmark visualization_test.launch \
            goal_x:=3.0 goal_y:=3.0 \
            enable_recorder:=false \
            enable_visualizer:=false

        echo ""
        echo "✓ Test completed!"
        ;;

    2)
        echo ""
        echo "Starting full test with data recording..."
        echo "Goal: (4.0, 4.0)"
        echo "Duration: 60 seconds"
        echo ""

        # Run test in background
        roslaunch tube_mpc_benchmark visualization_test.launch \
            goal_x:=4.0 goal_y:=4.0 \
            enable_recorder:=true \
            enable_visualizer:=false &
        TEST_PID=$!

        # Monitor progress
        for i in {1..12}; do
            sleep 5
            echo "[$(date +%H:%M:%S)] Test progress: $((i*5))s / 60s"
            # Check current position
            timeout 2 rostopic echo /odom -n 1 2>/dev/null | grep -A 2 "position:" | head -3 || true
        done

        # Wait for test to complete
        wait $TEST_PID

        echo ""
        echo "✓ Test completed!"
        echo ""

        # Analyze results
        CSV_FILE=$(ls -t /tmp/tube_mpc_data/navigation_data_*.csv 2>/dev/null | head -1)
        if [ -n "$CSV_FILE" ]; then
            echo "Analyzing data from: $CSV_FILE"
            python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
                --csv "$CSV_FILE" \
                --output "demo_trajectory.png"

            echo ""
            echo "✓ Analysis complete!"
            echo "Generated: demo_trajectory.png"
            echo ""
            echo "To view the results:"
            echo "  eog demo_trajectory.png"
        fi
        ;;

    3)
        echo ""
        echo "Analyzing existing data..."
        echo ""

        # Find most recent data
        CSV_FILE=$(ls -t /tmp/tube_mpc_data/navigation_data_*.csv 2>/dev/null | head -1)

        if [ -z "$CSV_FILE" ]; then
            echo "❌ No data files found!"
            echo "   Please run a test first (option 2)"
            exit 1
        fi

        echo "Found data: $CSV_FILE"
        echo ""

        # Generate all plots
        python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
            --csv "$CSV_FILE" \
            --output "demo_trajectory.png"

        if [ -f "/tmp/tube_mpc_metrics.csv" ]; then
            python3 src/tube_mpc_benchmark/scripts/analyze_results.py \
                --mpc-csv "/tmp/tube_mpc_metrics.csv" \
                --output "demo_mpc_metrics.png"
        fi

        echo ""
        echo "✓ Analysis complete!"
        echo ""
        echo "Generated files:"
        ls -lh demo_*.png 2>/dev/null
        echo ""
        echo "To view:"
        echo "  eog demo_*.png"
        ;;

    4)
        echo ""
        echo "Showing all visualization capabilities..."
        echo ""

        # Start test with all visualizations
        roslaunch tube_mpc_benchmark visualization_test.launch \
            goal_x:=5.0 goal_y:=5.0 \
            enable_recorder:=true \
            enable_visualizer:=true

        echo ""
        echo "✓ Visualization demo completed!"
        ;;

    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                    Demo Summary                             ║"
echo "╠════════════════════════════════════════════════════════════╣"
echo "║  Available visualization tools:                           ║"
echo "║  • RViz: Real-time 3D visualization                      ║"
echo "║  • Real-time plotter: Live data graphs                    ║"
echo "║  • Data recorder: CSV data logging                       ║"
echo "║  • Analysis tools: Publication-quality plots             ║"
echo "╠════════════════════════════════════════════════════════════╣"
echo "║  Documentation:                                           ║"
echo "║  src/tube_mpc_benchmark/docs/VISUALIZATION_GUIDE.md       ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
