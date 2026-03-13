#!/bin/bash
# One-Click Automated Tuning - User manually tests, script automatically tunes

WORKSPACE="/home/dixon/Final_Project/catkin"
PARAMS_DIR="$WORKSPACE/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Clear screen
clear

echo "=========================================="
echo "一键自动化MPC调优系统"
echo "=========================================="
echo ""
echo "目标: 在3分钟 (180秒) 内到达 (3.0, -7.0)"
echo ""
echo "使用方法:"
echo "1. 脚本自动应用参数"
echo "2. 你手动启动导航并测试"
echo "3. 报告测试结果"
echo "4. 脚本自动调整参数"
echo "5. 循环直到成功"
echo "=========================================="
echo ""

# Backup
if [ ! -f "$PARAMS_DIR/tube_mpc_params.yaml.backup" ]; then
    cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params.yaml.backup"
fi

iteration=0
success=false

# Progressive parameter sets
declare -a configs=(
    "0.7:0.5:1.0:70.0:保守"
    "0.9:0.7:1.3:50.0:平衡"
    "1.0:0.8:1.5:40.0:优化"
    "1.1:0.9:1.7:35.0:激进A"
    "1.2:1.0:1.8:25.0:激进B"
    "1.3:1.1:2.0:20.0:超激进"
)

while [ "$success" = false ] && [ $iteration -lt ${#configs[@]} ]; do
    iteration=$((iteration + 1))

    # Parse configuration
    IFS=':' read -r max_vel ref_vel max_angvel w_vel desc <<< "${configs[$((iteration-1))]}"

    echo -e "${YELLOW}=========================================="
    echo "第 $iteration 次测试: $desc 配置"
    echo "==========================================${NC}"
    echo ""

    # Apply parameters
    cat > "$ORIGINAL_PARAM" << EOF
MPCPlannerROS:
  map_frame: map
  odom_frame: odom
  base_frame: base_footprint
  min_trans_vel: 0.01
  max_vel_x: $max_vel
  min_vel_x: -0.1
  max_vel_y: 0.0
  min_vel_y: 0.0
  trans_stopped_vel: 0.01
  max_rot_vel: 2.0
  min_rot_vel: 0.1
  rot_stopped_vel: 0.01
  acc_lim_x: 3.0
  acc_lim_theta: 4.0
  acc_lim_y: 0.0
  yaw_goal_tolerance: 0.5
  xy_goal_tolerance: 0.3
  latch_xy_goal_tolerance: false
  path_distance_bias: 32.0
  goal_distance_bias: 20.0
  occdist_scale: 0.02
  forward_point_distance: 0.4
  stop_time_buffer: 0.2
  scaling_speed: 0.25
  max_scaling_factor: 0.2
  oscillation_reset_dist: 0.05
  publish_traj_pc: true
  publish_cost_grid_pc: true
  debug_info: true
  max_speed: $max_vel
  waypoints_dist: -1.0
  path_length: 8.0
  controller_freq: 10
  mpc_steps: 25
  mpc_ref_cte: 0.0
  mpc_ref_vel: $ref_vel
  mpc_ref_etheta: 0.0
  mpc_w_cte: 50.0
  mpc_w_etheta: 50.0
  mpc_w_vel: $w_vel
  mpc_w_angvel: 30.0
  mpc_w_angvel_d: 30.0
  mpc_w_accel: 20.0
  mpc_w_accel_d: 5.0
  mpc_max_angvel: $max_angvel
  mpc_max_throttle: 1.5
  mpc_bound_value: 1.0e3
  disturbance_bound: 0.15
  enable_adaptive_disturbance: true
  disturbance_window_size: 100
  tube_visualization: true
  error_feedback_gain: 0.6
EOF

    echo -e "${BLUE}参数配置:${NC}"
    echo "  max_vel_x:     $max_vel m/s"
    echo "  mpc_ref_vel:   $ref_vel m/s"
    echo "  mpc_max_angvel: $max_angvel rad/s"
    echo "  mpc_w_vel:     $w_vel"
    echo -e "${GREEN}✓ 参数已应用${NC}"
    echo ""

    echo -e "${YELLOW}测试步骤:${NC}"
    echo "1. 终止现有ROS进程 (Ctrl+C)"
    echo "2. 启动导航:"
    echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
    echo "3. 在RViz中:"
    echo "   - 设置初始位置 (2D Pose Estimate)"
    echo "   - 发送目标 (3.0, -7.0) (2D Nav Goal)"
    echo "4. 观察运动并计时"
    echo ""

    read -p "准备好了吗? (按Enter开始) "

    echo ""
    echo -e "${BLUE}测试进行中...${NC}"
    echo "请在测试完成后根据实际情况选择结果"
    echo ""

    echo "选择测试结果:"
    echo "  1) ✓ 成功! 在3分钟内到达目标"
    echo "  2) ~ 接近，但用时稍长 (3-5分钟)"
    echo "  3) - 速度太慢"
    echo "  4) = 不移动或卡住"
    echo "  5) ! 运动不稳定/抖动"
    echo ""

    read -p "选择 (1-5): " result

    case $result in
        1)
            echo -e "${GREEN}=========================================="
            echo "✓ 成功! 在第 $iteration 次测试找到最优参数!"
            echo "==========================================${NC}"
            echo ""
            echo "成功参数配置:"
            echo "  配置: $desc"
            echo "  max_vel_x: $max_vel"
            echo "  mpc_ref_vel: $ref_vel"
            echo "  mpc_max_angvel: $max_angvel"
            echo "  mpc_w_vel: $w_vel"
            echo ""

            # Save success
            cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params_SUCCESS.yaml"
            echo -e "${BLUE}✓ 成功参数已保存: tube_mpc_params_SUCCESS.yaml${NC}"

            success=true
            ;;
        2)
            echo -e "${YELLOW}接近成功，继续尝试更激进参数...${NC}"
            # Continue with more aggressive
            ;;
        3)
            echo -e "${YELLOW}速度慢，跳到更激进参数...${NC}"
            # Skip to more aggressive
            iteration=$((iteration + 1))
            ;;
        4)
            echo -e "${YELLOW}不移动，尝试更保守参数...${NC}"
            # Go back to conservative
            if [ $iteration -gt 2 ]; then
                iteration=$((iteration - 2))
            fi
            ;;
        5)
            echo -e "${YELLOW}不稳定，尝试更平衡参数...${NC}"
            # Try middle ground
            ;;
        *)
            echo -e "${YELLOW}继续下一个配置...${NC}"
            ;;
    esac

    echo ""

    if [ "$success" = false ]; then
        echo -e "${BLUE}准备下一组参数...${NC}"
        sleep 1
        clear

        echo "=========================================="
        echo "进度: $iteration/${#configs[@]}"
        echo "目标: 3分钟内到达 (3.0, -7.0)"
        echo "=========================================="
        echo ""
    fi
done

# Restore and summary
echo ""
echo -e "${BLUE}恢复原始参数...${NC}"
cp "$PARAMS_DIR/tube_mpc_params.yaml.backup" "$ORIGINAL_PARAM"

echo ""
echo "=========================================="
if [ "$success" = true ]; then
    echo -e "${GREEN}调优成功!${NC}"
else
    echo -e "${RED}达到测试迭代上限${NC}"
fi
echo "=========================================="
echo ""

if [ "$success" = true ]; then
    echo "使用成功参数:"
    echo "  cp $PARAMS_DIR/tube_mpc_params_SUCCESS.yaml $PARAMS_DIR/tube_mpc_params.yaml"
    echo ""
    echo "然后重新启动导航即可使用最优参数"
fi

echo ""
echo "按Enter键退出..."
read
