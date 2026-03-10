#!/bin/bash
# Tube MPC Parameter Switching Script
# This script allows you to switch between different parameter configurations

PARAMS_DIR="/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"
BACKUP_PARAM="$PARAMS_DIR/tube_mpc_params.yaml.backup"

echo "=========================================="
echo "Tube MPC Parameter Switcher"
echo "=========================================="
echo ""

# Create backup of original if not exists
if [ ! -f "$BACKUP_PARAM" ]; then
    echo "Creating backup of original parameters..."
    cp "$ORIGINAL_PARAM" "$BACKUP_PARAM"
    echo "Backup created at: $BACKUP_PARAM"
    echo ""
fi

echo "Available parameter configurations:"
echo "1) Original (current)"
echo "2) Optimized (balanced performance)"
echo "3) Aggressive (maximum speed)"
echo "4) Conservative (maximum stability)"
echo "5) Restore from backup"
echo ""
read -p "Select configuration (1-5): " choice

case $choice in
    1)
        echo "Keeping original configuration..."
        ;;
    2)
        echo "Switching to OPTIMIZED configuration..."
        cp "$PARAMS_DIR/tube_mpc_params_optimized.yaml" "$ORIGINAL_PARAM"
        echo "✓ Optimized parameters activated"
        ;;
    3)
        echo "Switching to AGGRESSIVE configuration..."
        cp "$PARAMS_DIR/tube_mpc_params_aggressive.yaml" "$ORIGINAL_PARAM"
        echo "✓ Aggressive parameters activated"
        ;;
    4)
        echo "Switching to CONSERVATIVE configuration..."
        cp "$PARAMS_DIR/tube_mpc_params_conservative.yaml" "$ORIGINAL_PARAM"
        echo "✓ Conservative parameters activated"
        ;;
    5)
        echo "Restoring from backup..."
        cp "$BACKUP_PARAM" "$ORIGINAL_PARAM"
        echo "✓ Original parameters restored"
        ;;
    *)
        echo "Invalid selection. No changes made."
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "Current key parameters:"
echo "=========================================="
grep -A 2 "max_vel_x\|mpc_ref_vel\|mpc_max_angvel\|mpc_w_vel\|mpc_w_cte" "$ORIGINAL_PARAM" | head -20
echo ""

echo "Next steps:"
echo "1. If ROS is running, restart it to load new parameters:"
echo "   - Press Ctrl+C to stop current launch"
echo "   - Run: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "2. Monitor performance with debug info enabled"
echo ""
echo "3. Fine-tune parameters if needed by editing:"
echo "   $ORIGINAL_PARAM"
