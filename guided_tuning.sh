#!/bin/bash
# Interactive Guided Tuning Script
# User manually tests, script auto-tunes parameters

WORKSPACE="/home/dixon/Final_Project/catkin"
PARAMS_DIR="$WORKSPACE/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "=========================================="
echo "Tube MPC 交互式参数调优"
echo "=========================================="
echo ""
echo "这个脚本将帮助你逐步调优参数"
echo "每次测试后，根据结果选择下一步操作"
echo ""
echo "目标: 在3分钟 (180秒) 内到达 (3.0, -7.0)"
echo "=========================================="
echo ""

# Backup original parameters
if [ ! -f "$PARAMS_DIR/tube_mpc_params.yaml.backup" ]; then
    cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params.yaml.backup"
    echo -e "${BLUE}✓ 原始参数已备份${NC}"
    echo ""
fi

iteration=0
success=false

while [ "$success" = false ]; do
    iteration=$((iteration + 1))

    echo -e "${YELLOW}=========================================="
    echo "第 $iteration 次迭代"
    echo "==========================================${NC}"
    echo ""

    # Generate parameters based on iteration
    case $iteration in
        1)
            PARAM_NAME="保守配置"
            MAX_VEL=0.7
            MPC_REF_VEL=0.5
            MPC_MAX_ANGVEL=1.0
            MPC_W_VEL=70.0
            ;;
        2)
            PARAM_NAME="平衡配置"
            MAX_VEL=0.9
            MPC_REF_VEL=0.7
            MPC_MAX_ANGVEL=1.3
            MPC_W_VEL=50.0
            ;;
        3)
            PARAM_NAME="优化配置"
            MAX_VEL=1.0
            MPC_REF_VEL=0.8
            MPC_MAX_ANGVEL=1.5
            MPC_W_VEL=40.0
            ;;
        4)
            PARAM_NAME="激进配置A"
            MAX_VEL=1.1
            MPC_REF_VEL=0.9
            MPC_MAX_ANGVEL=1.7
            MPC_W_VEL=35.0
            ;;
        5)
            PARAM_NAME="激进配置B"
            MAX_VEL=1.2
            MPC_REF_VEL=1.0
            MPC_MAX_ANGVEL=1.8
            MPC_W_VEL=25.0
            ;;
        *)
            PARAM_NAME="超激进配置"
            MAX_VEL=1.3
            MPC_REF_VEL=1.1
            MPC_MAX_ANGVEL=2.0
            MPC_W_VEL=20.0
            ;;
    esac

    # Apply parameters
    echo -e "${BLUE}应用参数: $PARAM_NAME${NC}"
    echo "  max_vel_x: $MAX_VEL"
    echo "  mpc_ref_vel: $MPC_REF_VEL"
    echo "  mpc_max_angvel: $MPC_MAX_ANGVEL"
    echo "  mpc_w_vel: $MPC_W_VEL"
    echo ""

    # Create parameter file
    cat > "$PARAMS_DIR/tube_mpc_params_current.yaml" << EOF
MPCPlannerROS:
  map_frame: map
  odom_frame: odom
  base_frame: base_footprint

  min_trans_vel: 0.01
  max_vel_x: $MAX_VEL
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
  max_speed: $MAX_VEL
  waypoints_dist: -1.0
  path_length: 8.0
  controller_freq: 10

  mpc_steps: 25
  mpc_ref_cte: 0.0
  mpc_ref_vel: $MPC_REF_VEL
  mpc_ref_etheta: 0.0
  mpc_w_cte: 50.0
  mpc_w_etheta: 50.0
  mpc_w_vel: $MPC_W_VEL
  mpc_w_angvel: 30.0
  mpc_w_angvel_d: 30.0
  mpc_w_accel: 20.0
  mpc_w_accel_d: 5.0
  mpc_max_angvel: $MPC_MAX_ANGVEL
  mpc_max_throttle: 1.5
  mpc_bound_value: 1.0e3

  disturbance_bound: 0.15
  enable_adaptive_disturbance: true
  disturbance_window_size: 100
  tube_visualization: true
  error_feedback_gain: 0.6
EOF

    cp "$PARAMS_DIR/tube_mpc_params_current.yaml" "$ORIGINAL_PARAM"
    echo -e "${GREEN}✓ 参数已应用${NC}"
    echo ""

    # Instructions for user
    echo -e "${YELLOW}请按照以下步骤操作:${NC}"
    echo "1. 如果有正在运行的ROS，按Ctrl+C停止"
    echo "2. 启动导航:"
    echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
    echo "3. 在RViz中设置初始位置 (2D Pose Estimate)"
    echo "4. 发送导航目标 (2D Nav Goal): (3.0, -7.0)"
    echo "5. 观察机器人运动并计时"
    echo ""

    read -p "按Enter键开始测试，或输入'skip'跳过此配置: " choice
    if [ "$choice" = "skip" ]; then
        echo -e "${YELLOW}跳过此配置${NC}"
        echo ""
        continue
    fi

    echo ""
    echo -e "${BLUE}测试进行中...${NC}"
    echo "请在测试完成后回答以下问题"
    echo ""

    # Wait for user to finish test
    read -p "测试完成了吗? (y/n): " finished
    if [ "$finished" != "y" ]; then
        echo -e "${YELLOW}请完成测试后继续${NC}"
        continue
    fi

    echo ""
    echo "请选择测试结果:"
    echo "1) 成功! 在3分钟内到达目标"
    echo "2) 接近成功，但超过3分钟"
    echo "3) 机器人移动但速度太慢"
    echo "4) 机器人卡住或不移动"
    echo "5) 机器人运动不稳定"
    echo ""

    read -p "选择 (1-5): " result

    case $result in
        1)
            echo -e "${GREEN}=========================================="
            echo "✓ 成功! 在第 $iteration 次迭代找到最优参数"
            echo "==========================================${NC}"
            success=true

            # Save successful parameters
            cp "$PARAMS_DIR/tube_mpc_params_current.yaml" "$PARAMS_DIR/tube_mpc_params_SUCCESS.yaml"
            echo -e "${BLUE}✓ 成功参数已保存: tube_mpc_params_SUCCESS.yaml${NC}"
            echo ""
            echo "参数配置:"
            echo "  max_vel_x: $MAX_VEL"
            echo "  mpc_ref_vel: $MPC_REF_VEL"
            echo "  mpc_max_angvel: $MPC_MAX_ANGVEL"
            echo "  mpc_w_vel: $MPC_W_VEL"
            ;;
        2)
            echo -e "${YELLOW}接近成功，尝试更激进的参数...${NC}"
            iteration=$((iteration - 1)) # Stay at similar level but more aggressive
            ;;
        3)
            echo -e "${YELLOW}速度太慢，尝试提高速度参数...${NC}"
            # Continue to next iteration (more aggressive)
            ;;
        4)
            echo -e "${YELLOW}机器人不移动，尝试更保守的参数...${NC}"
            # Go back to more conservative
            if [ $iteration -gt 2 ]; then
                iteration=$((iteration - 2))
            fi
            ;;
        5)
            echo -e "${YELLOW}运动不稳定，尝试更平衡的参数...${NC}"
            # Continue to next iteration
            ;;
        *)
            echo -e "${YELLOW}无效选择，继续下一个配置...${NC}"
            ;;
    esac

    echo ""

    # Safety limit
    if [ $iteration -ge 8 ]; then
        echo -e "${RED}达到最大迭代次数${NC}"
        break
    fi
done

# Restore original parameters
echo ""
echo -e "${BLUE}恢复原始参数...${NC}"
cp "$PARAMS_DIR/tube_mpc_params.yaml.backup" "$ORIGINAL_PARAM"
echo -e "${GREEN}✓ 完成${NC}"
echo ""

echo "=========================================="
echo "调优会话结束"
echo "=========================================="
echo ""

if [ "$success" = true ]; then
    echo "成功参数文件: $PARAMS_DIR/tube_mpc_params_SUCCESS.yaml"
    echo ""
    echo "应用成功参数:"
    echo "  cp $PARAMS_DIR/tube_mpc_params_SUCCESS.yaml $PARAMS_DIR/tube_mpc_params.yaml"
fi

echo ""
echo "详细日志: 查看参数目录中的配置文件"
