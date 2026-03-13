#!/bin/bash
# Quick Start - Apply Best Parameters and Test

WORKSPACE="/home/dixon/Final_Project/catkin"
PARAMS_DIR="$WORKSPACE/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "=========================================="
echo "Tube MPC 快速调优启动器"
echo "=========================================="
echo ""

# Backup
if [ ! -f "$PARAMS_DIR/tube_mpc_params.yaml.backup" ]; then
    cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params.yaml.backup"
    echo -e "${BLUE}✓ 原始参数已备份${NC}"
fi

echo "选择参数配置:"
echo "1) 优化配置 (推荐) - 平衡性能"
echo "2) 激进配置A - 追求速度"
echo "3) 激进配置B - 超高速度"
echo "4) 保守配置 - 追求稳定"
echo "5) 自动循环测试 (直到成功)"
echo ""
read -p "选择 (1-5): " choice

case $choice in
    1)
        cp "$PARAMS_DIR/tube_mpc_params_optimized.yaml" "$ORIGINAL_PARAM"
        PARAM_NAME="优化配置"
        ;;
    2)
        cp "$PARAMS_DIR/tube_mpc_params_aggressive.yaml" "$ORIGINAL_PARAM"
        PARAM_NAME="激进配置A"
        ;;
    3)
        # Create super aggressive config
        cat > "$ORIGINAL_PARAM" << 'EOF'
MPCPlannerROS:
  map_frame: map
  odom_frame: odom
  base_frame: base_footprint
  min_trans_vel: 0.01
  max_vel_x: 1.5
  min_vel_x: -0.2
  max_vel_y: 0.0
  min_vel_y: 0.0
  trans_stopped_vel: 0.01
  max_rot_vel: 3.0
  min_rot_vel: 0.1
  rot_stopped_vel: 0.01
  acc_lim_x: 4.0
  acc_lim_theta: 5.0
  acc_lim_y: 0.0
  yaw_goal_tolerance: 0.3
  xy_goal_tolerance: 0.25
  latch_xy_goal_tolerance: false
  path_distance_bias: 40.0
  goal_distance_bias: 24.0
  occdist_scale: 0.02
  forward_point_distance: 0.5
  stop_time_buffer: 0.15
  scaling_speed: 0.3
  max_scaling_factor: 0.2
  oscillation_reset_dist: 0.05
  publish_traj_pc: true
  publish_cost_grid_pc: true
  debug_info: true
  max_speed: 1.5
  waypoints_dist: -1.0
  path_length: 10.0
  controller_freq: 10
  mpc_steps: 30
  mpc_ref_cte: 0.0
  mpc_ref_vel: 1.3
  mpc_ref_etheta: 0.0
  mpc_w_cte: 30.0
  mpc_w_etheta: 30.0
  mpc_w_vel: 15.0
  mpc_w_angvel: 10.0
  mpc_w_angvel_d: 10.0
  mpc_w_accel: 8.0
  mpc_w_accel_d: 2.0
  mpc_max_angvel: 2.5
  mpc_max_throttle: 2.5
  mpc_bound_value: 1.0e3
  disturbance_bound: 0.25
  enable_adaptive_disturbance: true
  disturbance_window_size: 100
  tube_visualization: true
  error_feedback_gain: 0.8
EOF
        PARAM_NAME="激进配置B"
        ;;
    4)
        cp "$PARAMS_DIR/tube_mpc_params_conservative.yaml" "$ORIGINAL_PARAM"
        PARAM_NAME="保守配置"
        ;;
    5)
        echo -e "${YELLOW}启动交互式调优...${NC}"
        chmod +x "$WORKSPACE/guided_tuning.sh"
        exec "$WORKSPACE/guided_tuning.sh"
        ;;
    *)
        echo -e "${YELLOW}无效选择，使用优化配置${NC}"
        cp "$PARAMS_DIR/tube_mpc_params_optimized.yaml" "$ORIGINAL_PARAM"
        PARAM_NAME="优化配置"
        ;;
esac

echo -e "${GREEN}✓ 已应用: $PARAM_NAME${NC}"
echo ""

# Show key parameters
echo "关键参数:"
echo "  max_vel_x: $(grep 'max_vel_x:' $ORIGINAL_PARAM | head -1 | awk '{print $2}')"
echo "  mpc_ref_vel: $(grep 'mpc_ref_vel:' $ORIGINAL_PARAM | head -1 | awk '{print $2}')"
echo "  mpc_max_angvel: $(grep 'mpc_max_angvel:' $ORIGINAL_PARAM | head -1 | awk '{print $2}')"
echo "  mpc_w_vel: $(grep 'mpc_w_vel:' $ORIGINAL_PARAM | head -1 | awk '{print $2}')"
echo ""

echo -e "${YELLOW}=========================================="
echo "下一步操作:"
echo "==========================================${NC}"
echo "1. 启动导航:"
echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "2. 设置目标点: (3.0, -7.0)"
echo ""
echo "3. 观察运动并计时"
echo ""
echo "4. 如果不满意，重新运行此脚本尝试其他配置"
echo ""

echo -e "${BLUE}提示: 可以创建多个终端窗口同时监控${NC}"
echo "  终端1: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo "  终端2: rostopic echo /cmd_vel"
echo "  终端3: rostopic echo /amcl_pose"
echo ""
