#!/bin/bash
# MPC参数切换工具
# 用于快速切换不同的MPC参数配置

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 参数文件路径
PARAM_DIR="/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAM_DIR/tube_mpc_params.yaml"
BACKUP_PARAM="$PARAM_DIR/tube_mpc_params.yaml.backup"

# 函数：显示菜单
show_menu() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}   MPC参数配置切换工具${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo ""
    echo -e "${GREEN}当前配置:${NC}"
    if [ -L "$ORIGINAL_PARAM" ]; then
        TARGET=$(readlink -f "$ORIGINAL_PARAM")
        echo -e "  活动配置: ${YELLOW}$(basename $TARGET)${NC}"
    else
        echo -e "  活动配置: ${YELLOW}tube_mpc_params.yaml (原始)${NC}"
    fi
    echo ""
    echo -e "${GREEN}可用配置:${NC}"
    echo "  1) ${YELLOW}原始配置${NC} (tube_mpc_params.yaml) - 基础性能"
    echo "  2) ${YELLOW}平滑模式${NC} (tube_mpc_params_smooth.yaml) - 运动平顺，减少抖动"
    echo "  3) ${YELLOW}快速模式${NC} (tube_mpc_params_fast.yaml) - 高速运输，快速到达"
    echo "  4) ${YELLOW}精确模式${NC} (tube_mpc_params_precision.yaml) - 精确跟踪，低速稳定"
    echo "  5) 查看当前参数"
    echo "  6) 创建参数备份"
    echo "  7) 恢复参数备份"
    echo "  0) 退出"
    echo ""
}

# 函数：备份当前参数
backup_current() {
    if [ ! -f "$BACKUP_PARAM" ]; then
        cp "$ORIGINAL_PARAM" "$BACKUP_PARAM"
        echo -e "${GREEN}✓ 备份已创建: $BACKUP_PARAM${NC}"
    else
        echo -e "${YELLOW}⚠ 备份已存在${NC}"
    fi
}

# 函数：恢复备份
restore_backup() {
    if [ -f "$BACKUP_PARAM" ]; then
        cp "$BACKUP_PARAM" "$ORIGINAL_PARAM"
        echo -e "${GREEN}✓ 备份已恢复${NC}"
    else
        echo -e "${RED}✗ 备份文件不存在${NC}"
    fi
}

# 函数：切换参数配置
switch_param() {
    local config_name=$1
    local source_file="$PARAM_DIR/$config_name"

    # 检查源文件是否存在
    if [ ! -f "$source_file" ]; then
        echo -e "${RED}✗ 配置文件不存在: $source_file${NC}"
        return 1
    fi

    # 创建备份
    if [ ! -f "$BACKUP_PARAM" ] && [ -f "$ORIGINAL_PARAM" ]; then
        cp "$ORIGINAL_PARAM" "$BACKUP_PARAM"
        echo -e "${YELLOW}已自动创建备份${NC}"
    fi

    # 切换配置
    cp "$source_file" "$ORIGINAL_PARAM"
    echo -e "${GREEN}✓ 已切换到: $config_name${NC}"

    # 显示关键参数变化
    echo -e "${BLUE}关键参数:${NC}"
    grep -E "max_vel_x|max_rot_vel|controller_freq|mpc_ref_vel|mpc_w_cte|mpc_w_etheta|mpc_w_vel|mpc_w_accel" "$ORIGINAL_PARAM" | grep -v "^#" | sed 's/^/  /'
}

# 函数：显示当前参数
show_current_params() {
    echo -e "${BLUE}=== 当前MPC参数 ===${NC}"
    echo ""
    echo -e "${GREEN}速度限制:${NC}"
    grep -E "max_vel_x|max_rot_vel" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
    echo -e "${GREEN}加速度限制:${NC}"
    grep -E "acc_lim_x|acc_lim_theta" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
    echo -e "${GREEN}MPC核心:${NC}"
    grep -E "controller_freq|mpc_steps|mpc_ref_vel" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
    echo -e "${GREEN}代价权重:${NC}"
    grep -E "mpc_w_cte|mpc_w_etheta|mpc_w_vel|mpc_w_accel" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
}

# 主循环
while true; do
    show_menu
    read -p "请选择 [0-7]: " choice

    case $choice in
        1)
            # 恢复原始配置
            if [ -f "$BACKUP_PARAM" ]; then
                restore_backup
            else
                echo -e "${YELLOW}⚠ 无备份，使用当前配置作为原始配置${NC}"
            fi
            ;;
        2)
            # 平滑模式
            switch_param "tube_mpc_params_smooth.yaml"
            ;;
        3)
            # 快速模式
            switch_param "tube_mpc_params_fast.yaml"
            ;;
        4)
            # 精确模式
            switch_param "tube_mpc_params_precision.yaml"
            ;;
        5)
            # 显示当前参数
            show_current_params
            ;;
        6)
            # 创建备份
            backup_current
            ;;
        7)
            # 恢复备份
            restore_backup
            ;;
        0)
            # 退出
            echo -e "${GREEN}再见！${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}✗ 无效选择${NC}"
            ;;
    esac

    echo ""
    read -p "按Enter键继续..."
    clear
done
