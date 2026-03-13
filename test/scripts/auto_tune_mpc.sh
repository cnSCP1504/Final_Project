#!/bin/bash
# Automated MPC Parameter Tuning Script
# Continuously tests and tunes parameters until goal is reached within 3 minutes

set -e

# Configuration
WORKSPACE="/home/dixon/Final_Project/catkin"
PARAMS_DIR="$WORKSPACE/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"
BACKUP_PARAM="$PARAMS_DIR/tube_mpc_params.yaml.backup"
LOG_DIR="$WORKSPACE/tuning_logs"
TEST_LOG="$LOG_DIR/test_$(date +%Y%m%d_%H%M%S).log"

# Goal configuration
GOAL_X=3.0
GOAL_Y=-7.0
GOAL_YAW=0.0
TIME_LIMIT=180  # 3 minutes in seconds

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create log directory
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "自动化MPC参数调优系统"
echo "=========================================="
echo "目标点: ($GOAL_X, $GOAL_Y, $GOAL_YAW)"
echo "时间限制: $TIME_LIMIT 秒 (3分钟)"
echo "日志文件: $TEST_LOG"
echo "=========================================="
echo ""

# Backup original parameters
if [ ! -f "$BACKUP_PARAM" ]; then
    cp "$ORIGINAL_PARAM" "$BACKUP_PARAM"
    echo -e "${BLUE}✓ 原始参数已备份${NC}"
fi

# Counter for iterations
iteration=0
success=false

# Parameter sets to try (ordered from conservative to aggressive)
declare -a param_sets=(
    "conservative"
    "optimized"
    "aggressive"
    "custom_1"
    "custom_2"
    "custom_3"
)

while [ "$success" = false ]; do
    iteration=$((iteration + 1))
    echo -e "${YELLOW}=========================================="
    echo "第 $iteration 次测试"
    echo "==========================================${NC}"

    # Select parameter set based on iteration
    if [ $iteration -le 3 ]; then
        param_set="${param_sets[$((iteration-1))]}"
    else
        # Generate custom parameters for iterations > 3
        param_set="custom_$((iteration-3))"
        generate_custom_params $iteration
    fi

    echo -e "${BLUE}使用参数集: $param_set${NC}"
    apply_parameters $param_set

    # Launch ROS navigation in background
    echo -e "${BLUE}启动ROS导航...${NC}"
    launch_ros

    # Wait for system to initialize
    sleep 5

    # Set initial pose
    echo -e "${BLUE}设置初始位置...${NC}"
    set_initial_pose

    # Send navigation goal
    echo -e "${BLUE}发送导航目标: ($GOAL_X, $GOAL_Y)${NC}"
    send_goal $GOAL_X $GOAL_Y $GOAL_YAW

    # Monitor progress
    echo -e "${BLUE}开始监控导航进度...${NC}"
    monitor_navigation

    # Check result
    if [ $? -eq 0 ]; then
        success=true
        echo -e "${GREEN}=========================================="
        echo "✓ 成功! 在 $elapsed_time 秒内到达目标"
        echo "==========================================${NC}"
        break
    else
        echo -e "${RED}✗ 失败! 未在时间限制内到达目标${NC}"
    fi

    # Cleanup
    cleanup_ros

    # Wait before next iteration
    if [ "$success" = false ]; then
        echo -e "${YELLOW}等待5秒后重试...${NC}"
        sleep 5
    fi

    # Safety limit
    if [ $iteration -ge 10 ]; then
        echo -e "${RED}达到最大迭代次数(10)，停止测试${NC}"
        break
    fi
done

echo ""
echo -e "${GREEN}=========================================="
echo "测试完成!"
echo "总迭代次数: $iteration"
echo "最终使用的参数: $param_set"
echo "==========================================${NC}"

# Restore original parameters
cp "$BACKUP_PARAM" "$ORIGINAL_PARAM"
echo -e "${BLUE}✓ 原始参数已恢复${NC}"

# Show summary
show_summary

exit 0
