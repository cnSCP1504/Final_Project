#!/bin/bash
# Automated MPC Testing and Tuning Script
# Tests navigation and auto-tunes parameters until 3-minute goal is achieved

WORKSPACE="/home/dixon/Final_Project/catkin"
PARAMS_DIR="$WORKSPACE/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"
LOG_DIR="$WORKSPACE/tuning_logs"

# Goal settings
GOAL_X=3.0
GOAL_Y=-7.0
GOAL_YAW=0.0
START_X=0.0
START_Y=0.0
START_YAW=-1.507

TIME_LIMIT=180  # 3 minutes

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p "$LOG_DIR"

# Function to log messages
log_msg() {
    echo -e "$1" | tee -a "$LOG_DIR/current_test.log"
}

# Function to backup original parameters
backup_params() {
    if [ ! -f "$PARAMS_DIR/tube_mpc_params.yaml.backup" ]; then
        cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params.yaml.backup"
        log_msg "${BLUE}✓ 原始参数已备份${NC}"
    fi
}

# Function to generate optimized parameters
generate_tuned_params() {
    local iteration=$1
    local temp_param="$PARAMS_DIR/tube_mpc_params_tuned.yaml"

    # Base values
    local max_vel_x=0.8
    local mpc_ref_vel=0.6
    local mpc_max_angvel=1.2
    local mpc_w_vel=60.0
    local mpc_w_cte=60.0
    local mpc_w_etheta=60.0

    # Progressive tuning based on iteration
    case $iteration in
        1)
            # Conservative start
            max_vel_x=0.7
            mpc_ref_vel=0.5
            mpc_max_angvel=1.0
            mpc_w_vel=70.0
            ;;
        2)
            # Balanced
            max_vel_x=0.9
            mpc_ref_vel=0.7
            mpc_max_angvel=1.3
            mpc_w_vel=50.0
            ;;
        3)
            # Optimized
            max_vel_x=1.0
            mpc_ref_vel=0.8
            mpc_max_angvel=1.5
            mpc_w_vel=40.0
            ;;
        4)
            # More aggressive
            max_vel_x=1.1
            mpc_ref_vel=0.9
            mpc_max_angvel=1.7
            mpc_w_vel=35.0
            ;;
        5)
            # Aggressive
            max_vel_x=1.2
            mpc_ref_vel=1.0
            mpc_max_angvel=1.8
            mpc_w_vel=25.0
            ;;
        *)
            # Very aggressive
            max_vel_x=1.3
            mpc_ref_vel=1.1
            mpc_max_angvel=2.0
            mpc_w_vel=20.0
            ;;
    esac

    # Create parameter file
    cat > "$temp_param" << EOF
MPCPlannerROS:
  map_frame: map
  odom_frame: odom
  base_frame: base_footprint

  min_trans_vel: 0.01
  max_vel_x: $max_vel_x
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
  max_speed: $max_vel_x
  waypoints_dist: -1.0
  path_length: 8.0
  controller_freq: 10

  mpc_steps: 25
  mpc_ref_cte: 0.0
  mpc_ref_vel: $mpc_ref_vel
  mpc_ref_etheta: 0.0
  mpc_w_cte: $mpc_w_cte
  mpc_w_etheta: $mpc_w_etheta
  mpc_w_vel: $mpc_w_vel
  mpc_w_angvel: 30.0
  mpc_w_angvel_d: 30.0
  mpc_w_accel: 20.0
  mpc_w_accel_d: 5.0
  mpc_max_angvel: $mpc_max_angvel
  mpc_max_throttle: 1.5
  mpc_bound_value: 1.0e3

  disturbance_bound: 0.15
  enable_adaptive_disturbance: true
  disturbance_window_size: 100
  tube_visualization: true
  error_feedback_gain: 0.6
EOF

    cp "$temp_param" "$ORIGINAL_PARAM"
    log_msg "${BLUE}应用参数集 #$iteration:${NC}"
    log_msg "  max_vel_x: $max_vel_x"
    log_msg "  mpc_ref_vel: $mpc_ref_vel"
    log_msg "  mpc_max_angvel: $mpc_max_angvel"
    log_msg "  mpc_w_vel: $mpc_w_vel"
}

# Function to kill existing ROS processes
kill_ros() {
    log_msg "${YELLOW}终止现有ROS进程...${NC}"
    killall -9 roslaunch roscore rviz 2>/dev/null || true
    sleep 2
}

# Function to source ROS environment
source_ros() {
    source /opt/ros/noetic/setup.bash
    source "$WORKSPACE/devel/setup.bash"
}

# Function to start ROS core and navigation
start_navigation() {
    log_msg "${BLUE}启动导航系统...${NC}"
    cd "$WORKSPACE"

    # Start ROS core in background
    roscore &
    ROSCORE_PID=$!
    sleep 3

    # Start navigation in background
    roslaunch tube_mpc_ros tube_mpc_navigation.launch > "$LOG_DIR/nav_output.log" 2>&1 &
    NAV_PID=$!
    sleep 5

    log_msg "${GREEN}✓ 导航系统已启动 (PID: $NAV_PID)${NC}"
}

# Function to set initial pose
set_initial_pose() {
    log_msg "${BLUE}设置初始位置...${NC}"

    source_ros

    # Wait for amcl to be ready
    sleep 3

    # Publish initial pose
    rosparam set /use_sim_time true
    rosrun tf static_transform_publisher 0 0 0 0 0 0 map base_link 100 &
    sleep 1

    # Set initial pose using topic
    rostopic pub /initialpose geometry_msgs/PoseWithCovarianceStamped \
        "{header: {seq: 0, stamp: {secs: 0, nsecs: 0}, frame_id: 'map'}, \
          pose: {pose: {position: {x: $START_X, y: $START_Y, z: 0.0}, \
          orientation: {x: 0.0, y: 0.0, z: $START_YAW, w: 1.0}}, \
          covariance: [0.25, 0.0, 0.0, 0.0, 0.0, 0.0, \
                       0.0, 0.25, 0.0, 0.0, 0.0, 0.0, \
                       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, \
                       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, \
                       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, \
                       0.0, 0.0, 0.0, 0.0, 0.0, 0.06853891945200942]}}" \
        --once

    sleep 2
    log_msg "${GREEN}✓ 初始位置已设置${NC}"
}

# Function to send navigation goal
send_navigation_goal() {
    log_msg "${BLUE}发送导航目标: ($GOAL_X, $GOAL_Y)${NC}"

    source_ros

    # Convert yaw to quaternion
    local yaw_half=$(echo "$GOAL_YAW / 2" | bc -l)
    local q_z=$(echo "s($yaw_half)" | bc -l)
    local q_w=$(echo "c($yaw_half)" | bc -l)

    # Send goal
    rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped \
        "{header: {seq: 0, stamp: {secs: 0, nsecs: 0}, frame_id: 'map'}, \
          pose: {position: {x: $GOAL_X, y: $GOAL_Y, z: 0.0}, \
          orientation: {x: 0.0, y: 0.0, z: $q_z, w: $q_w}}}" \
        --once

    log_msg "${GREEN}✓ 导航目标已发送${NC}"
}

# Function to monitor navigation progress
monitor_navigation() {
    local start_time=$(date +%s)
    local goal_reached=false
    local last_x=$START_X
    local last_y=$START_Y
    local stuck_count=0

    log_msg "${BLUE}开始监控导航进度 (时间限制: ${TIME_LIMIT}秒)...${NC}"

    source_ros

    while true; do
        local current_time=$(date +%s)
        local elapsed=$((current_time - start_time))

        if [ $elapsed -ge $TIME_LIMIT ]; then
            log_msg "${RED}✗ 超时 (${elapsed}秒)${NC}"
            return 1
        fi

        # Check if goal is reached
        local goal_status=$(rostopic echo /move_base/status -n 1 2>/dev/null | grep -o "status: [0-9]" | head -1 | grep -o "[0-9]" || echo "")

        # Check robot position
        local position=$(rostopic echo /amcl_pose -n 1 2>/dev/null)
        if [ -n "$position" ]; then
            local curr_x=$(echo "$position" | grep -o "x: [0-9.-]*" | head -1 | grep -o "[0-9.-]*")
            local curr_y=$(echo "$position" | grep -o "y: [0-9.-]*" | head -1 | grep -o "[0-9.-]*")

            if [ -n "$curr_x" ] && [ -n "$curr_y" ]; then
                # Calculate distance to goal
                local dx=$(echo "$GOAL_X - $curr_x" | bc)
                local dy=$(echo "$GOAL_Y - $curr_y" | bc)
                local dist=$(echo "sqrt($dx*$dx + $dy*$dy)" | bc)

                # Calculate distance moved
                local move_dx=$(echo "$curr_x - $last_x" | bc)
                local move_dy=$(echo "$curr_y - $last_y" | bc)
                local move_dist=$(echo "sqrt($move_dx*$move_dx + $move_dy*$move_dy)" | bc)

                # Check if stuck (not moving)
                if [ $(echo "$move_dist < 0.05" | bc) -eq 1 ]; then
                    stuck_count=$((stuck_count + 1))
                else
                    stuck_count=0
                    last_x=$curr_x
                    last_y=$curr_y
                fi

                # Check if reached goal
                if [ $(echo "$dist < 0.5" | bc) -eq 1 ]; then
                    log_msg "${GREEN}✓ 目标达成! 距离: $dist 米, 用时: ${elapsed}秒${NC}"
                    return 0
                fi

                # Progress update every 10 seconds
                if [ $((elapsed % 10)) -eq 0 ]; then
                    log_msg "  时间: ${elapsed}秒, 位置: ($curr_x, $curr_y), 目标距离: $dist 米"
                fi

                # Check if stuck for too long
                if [ $stuck_count -gt 30 ]; then
                    log_msg "${RED}✗ 机器人卡住 (30秒未移动)${NC}"
                    return 1
                fi
            fi
        fi

        sleep 1
    done
}

# Function to cleanup ROS processes
cleanup_ros() {
    log_msg "${YELLOW}清理ROS进程...${NC}"
    kill $NAV_PID 2>/dev/null || true
    kill $ROSCORE_PID 2>/dev/null || true
    killall -9 roslaunch roscore 2>/dev/null || true
    sleep 2
}

# Main testing loop
main() {
    log_msg "=========================================="
    log_msg "自动化MPC参数调优系统"
    log_msg "=========================================="
    log_msg "起点: ($START_X, $START_Y)"
    log_msg "终点: ($GOAL_X, $GOAL_Y)"
    log_msg "时间限制: ${TIME_LIMIT}秒"
    log_msg "=========================================="

    backup_params

    local iteration=0
    local success=false

    while [ "$success" = false ] && [ $iteration -lt 10 ]; do
        iteration=$((iteration + 1))

        log_msg "${YELLOW}=========================================="
        log_msg "第 $iteration 次测试"
        log_msg "==========================================${NC}"

        # Generate and apply tuned parameters
        generate_tuned_params $iteration

        # Start ROS
        kill_ros
        start_navigation

        # Set initial pose and send goal
        set_initial_pose
        send_navigation_goal

        # Monitor navigation
        monitor_navigation
        local result=$?

        if [ $result -eq 0 ]; then
            success=true
            log_msg "${GREEN}=========================================="
            log_msg "✓ 测试成功!"
            log_msg "总迭代次数: $iteration"
            log_msg "==========================================${NC}"

            # Save successful parameters
            cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params_SUCCESS.yaml"
            log_msg "${BLUE}✓ 成功参数已保存到: tube_mpc_params_SUCCESS.yaml${NC}"
            break
        else
            log_msg "${RED}测试失败，尝试下一组参数...${NC}"
        fi

        # Cleanup
        cleanup_ros
        sleep 3

        # Move log to iteration file
        mv "$LOG_DIR/current_test.log" "$LOG_DIR/test_iteration_$iteration.log"
    done

    # Final cleanup
    cleanup_ros

    # Restore original parameters
    cp "$PARAMS_DIR/tube_mpc_params.yaml.backup" "$ORIGINAL_PARAM"
    log_msg "${BLUE}✓ 原始参数已恢复${NC}"

    # Summary
    log_msg ""
    log_msg "=========================================="
    if [ "$success" = true ]; then
        log_msg "${GREEN}调优成功!${NC}"
        log_msg "成功参数: $PARAMS_DIR/tube_mpc_params_SUCCESS.yaml"
    else
        log_msg "${RED}调优失败，达到最大迭代次数${NC}"
    fi
    log_msg "总测试次数: $iteration"
    log_msg "详细日志: $LOG_DIR/"
    log_msg "=========================================="

    return 0
}

# Run main function
main
