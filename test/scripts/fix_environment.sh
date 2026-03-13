#!/bin/bash
# Environment Fix Script for TubeMPC_Node Crash Issue
# This script removes conflicting Ipopt library versions and restores the environment

set -e  # Exit on error

echo "=========================================="
echo "TubeMPC_Node Environment Fix Script"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "This script requires root privileges. Please run with sudo."
    exit 1
fi

echo "Step 1: Backup current library state..."
BACKUP_DIR="/tmp/ipopt_backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"
cp -a /lib/libipopt.so* "$BACKUP_DIR/" 2>/dev/null || echo "No ipopt libraries to backup"
cp -a /lib/libcoinmumps.so* "$BACKUP_DIR/" 2>/dev/null || true
cp -a /lib/libcoinhsl.so* "$BACKUP_DIR/" 2>/dev/null || true
echo "Backup created at: $BACKUP_DIR"
echo ""

echo "Step 2: Removing conflicting library symlinks..."
# Remove the symlinks created on Mar 10 00:41
if [ -L /lib/libipopt.so.3 ]; then
    echo "Removing /lib/libipopt.so.3"
    rm /lib/libipopt.so.3
fi

if [ -L /lib/libcoinmumps.so.3 ]; then
    echo "Removing /lib/libcoinmumps.so.3"
    rm /lib/libcoinmumps.so.3
fi

if [ -L /lib/libcoinhsl.so.2 ]; then
    echo "Removing /lib/libcoinhsl.so.2"
    rm /lib/libcoinhsl.so.2
fi
echo ""

echo "Step 3: Verifying system Ipopt library..."
if [ -e /lib/libipopt.so.1 ]; then
    echo "System Ipopt library found:"
    ls -la /lib/libipopt.so.1
    echo "Version info:"
    readlink -f /lib/libipopt.so.1
else
    echo "WARNING: System Ipopt library not found!"
    echo "You may need to reinstall: sudo apt install coinor-libipopt1v5"
fi
echo ""

echo "Step 4: Updating dynamic library cache..."
ldconfig
echo "Library cache updated."
echo ""

echo "Step 5: Verifying library dependencies..."
echo "Checking TubeMPC_Node dependencies:"
if [ -f /home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node ]; then
    ldd /home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node | grep -i ipopt || echo "No Ipopt dependency found"
else
    echo "TubeMPC_Node not found. You may need to rebuild the project."
fi
echo ""

echo "=========================================="
echo "Environment fix completed!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Rebuild the project:"
echo "   cd /home/dixon/Final_Project/catkin"
echo "   catkin_make clean"
echo "   catkin_make"
echo ""
echo "2. Test the fix:"
echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "If problems persist, check the backup at: $BACKUP_DIR"
