#!/bin/bash
# Upgrade to New Ipopt Library Script
# This script removes old Ipopt versions and configures the system to use only the new version

set -e  # Exit on error

echo "=========================================="
echo "Ipopt Library Upgrade Script"
echo "Removing old version, keeping new version"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "This script requires root privileges. Please run with sudo."
    exit 1
fi

echo "Step 1: Backup current library state..."
BACKUP_DIR="/tmp/ipopt_upgrade_backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"
cp -a /lib/libipopt.so* "$BACKUP_DIR/" 2>/dev/null || true
echo "Backup created at: $BACKUP_DIR"
echo ""

echo "Step 2: Remove old system Ipopt packages..."
echo "Uninstalling coinor-libipopt packages..."
apt-get remove --purge -y coinor-libipopt1v5 coinor-libipopt-dev || echo "Packages not found or already removed"
echo ""

echo "Step 3: Remove old library symlinks..."
rm -f /lib/libipopt.so.1
rm -f /lib/libipopt.so.1.9.9
rm -f /lib/libipopt.so.3
echo "Old symlinks removed."
echo ""

echo "Step 4: Create new symlinks for Ipopt 3.14.20..."
# Create symlink for libipopt.so.1 (needed by compiled binaries)
ln -s /usr/local/lib/libipopt.so.3.14.20 /lib/libipopt.so.1
ln -s /usr/local/lib/libipopt.so.3.14.20 /lib/libipopt.so
ln -s /usr/local/lib/libipopt.so.3.14.20 /lib/libipopt.so.3

# Also create in /usr/local/lib for consistency
cd /usr/local/lib
ln -sf libipopt.so.3.14.20 libipopt.so.1
ln -sf libipopt.so.3.14.20 libipopt.so

echo "New symlinks created:"
ls -la /lib/libipopt.so* | head -5
echo ""

echo "Step 5: Handle MUMPS and HSL libraries..."
# Create symlinks for MUMPS if they don't exist
if [ -f /usr/local/lib/libcoinmumps.so.3 ] && [ ! -e /lib/libcoinmumps.so.3 ]; then
    ln -s /usr/local/lib/libcoinmumps.so.3 /lib/libcoinmumps.so.3
    echo "Created MUMPS symlink"
fi

if [ -f /usr/local/lib/libcoinhsl.so.2 ] && [ ! -e /lib/libcoinhsl.so.2 ]; then
    ln -s /usr/local/lib/libcoinhsl.so.2 /lib/libcoinhsl.so.2
    echo "Created HSL symlink"
fi
echo ""

echo "Step 6: Update dynamic library cache..."
ldconfig
echo "Library cache updated."
echo ""

echo "Step 7: Configure library search paths..."
# Ensure /usr/local/lib is in the library search path
if ! grep -q "/usr/local/lib" /etc/ld.so.conf.d/usr-local.conf 2>/dev/null; then
    echo "/usr/local/lib" > /etc/ld.so.conf.d/usr-local.conf
    echo "Added /usr/local/lib to library search path"
fi
ldconfig
echo ""

echo "Step 8: Verify new Ipopt library..."
if [ -e /lib/libipopt.so.1 ]; then
    echo "New Ipopt library configured:"
    ls -la /lib/libipopt.so.1
    echo "Target:"
    readlink -f /lib/libipopt.so.1
    echo "Version info:"
    strings /lib/libipopt.so.1 | grep -i "Ipopt\|COIN-OR\|3\.14" | head -3
fi
echo ""

echo "Step 9: Update CMakeLists.txt for new Ipopt..."
cd /home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros
if [ -f CMakeLists.txt ]; then
    # Backup original
    cp CMakeLists.txt CMakeLists.txt.backup
    echo "CMakeLists.txt backed up."

    # Update include and link directories
    sed -i 's|include_directories(/usr/include)|include_directories(/usr/include /usr/local/include)|' CMakeLists.txt
    sed -i 's|link_directories(/usr/lib)|link_directories(/usr/lib /usr/local/lib)|' CMakeLists.txt
    echo "CMakeLists.txt updated to use /usr/local/lib"
fi
echo ""

echo "=========================================="
echo "Ipopt Upgrade Completed!"
echo "=========================================="
echo ""
echo "Summary of changes:"
echo "  - Removed: coinor-libipopt1v5 (old version 3.11.9)"
echo "  - Active: Ipopt 3.14.20 (new version)"
echo "  - Symlinks: libipopt.so.1 → libipopt.so.3.14.20"
echo "  - CMakeLists.txt: Updated to use /usr/local/lib"
echo ""
echo "Next steps:"
echo "1. Rebuild the project:"
echo "   cd /home/dixon/Final_Project/catkin"
echo "   catkin_make clean"
echo "   catkin_make"
echo ""
echo "2. Verify the library linkage:"
echo "   ldd devel/lib/tube_mpc_ros/tube_TubeMPC_Node | grep ipopt"
echo ""
echo "3. Test the fix:"
echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "If problems persist, check the backup at: $BACKUP_DIR"
