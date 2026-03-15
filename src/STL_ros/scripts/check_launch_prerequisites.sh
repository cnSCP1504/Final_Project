#!/bin/bash
# Quick script to check if we can launch safe_regret_mpc

echo "========================================"
echo "Safe-Regret MPC Launch Prerequisites Check"
echo "========================================"

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

check_package() {
    if rospack find $1 >/dev/null 2>&1; then
        echo "✓ $1 package found"
        return 0
    else
        echo "✗ $1 package NOT found"
        return 1
    fi
}

check_file() {
    if [ -f "$1" ]; then
        echo "✓ File exists: $1"
        return 0
    else
        echo "✗ File NOT found: $1"
        return 1
    fi
}

echo ""
echo "--- Package Checks ---"
check_package "stl_ros"
check_package "tube_mpc_ros"

echo ""
echo "--- File Checks ---"
check_file "src/STL_ros/launch/safe_regret_mpc.launch"
check_file "src/STL_ros/launch/stl_monitor.launch"
check_file "src/STL_ros/params/stl_params.yaml"
check_file "src/STL_ros/params/formulas.yaml"
check_file "src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml"

echo ""
echo "--- Node Checks ---"
if [ -x "devel/lib/tube_mpc_ros/tube_TubeMPC_Node" ]; then
    echo "✓ Tube MPC node executable found"
else
    echo "✗ Tube MPC node NOT executable"
fi

if [ -f "devel/lib/stl_ros/scripts/stl_monitor.py" ] || [ -f "src/STL_ros/scripts/stl_monitor.py" ]; then
    echo "✓ STL monitor script found"
else
    echo "✗ STL monitor script NOT found"
fi

echo ""
echo "========================================"
echo "Safe-Regret MPC Launch Readiness Check"
echo "========================================"
