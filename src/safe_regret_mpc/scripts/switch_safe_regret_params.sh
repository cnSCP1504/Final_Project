#!/bin/bash
# Safe-Regret MPC参数切换工具
# 用于快速切换不同的参数配置

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 参数文件路径
PARAM_DIR="/home/dixon/Final_Project/catkin/src/safe_regret_mpc/params"
ORIGINAL_PARAM="$PARAM_DIR/safe_regret_mpc_params.yaml"
BACKUP_PARAM="$PARAM_DIR/safe_regret_mpc_params.yaml.backup"

# 函数：显示菜单
show_menu() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}   Safe-Regret MPC参数切换工具${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo ""
    echo -e "${GREEN}当前配置:${NC}"
    if [ -f "$BACKUP_PARAM" ]; then
        echo -e "  有备份: ${GREEN}是${NC}"
    else
        echo -e "  有备份: ${RED}否${NC}"
    fi
    echo ""
    echo -e "${GREEN}可用配置:${NC}"
    echo "  1) ${YELLOW}默认配置${NC} (safe_regret_mpc_params.yaml) - 基础性能"
    echo "  2) ${YELLOW}平滑模式${NC} (safe_regret_mpc_params_smooth.yaml) - 运动平顺"
    echo "  3) ${YELLOW}快速模式${NC} (safe_regret_mpc_params_fast.yaml) - 高速运输"
    echo "  4) 查看当前参数"
    echo "  5) 创建参数备份"
    echo "  6) 恢复参数备份"
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
    grep -E "ref_vel:|max_angvel:|controller_frequency:|w_cte:|w_etheta:|w_vel:|w_accel:" "$ORIGINAL_PARAM" | grep -v "^#" | sed 's/^/  /'
}

# 函数：显示当前参数
show_current_params() {
    echo -e "${BLUE}=== 当前Safe-Regret MPC参数 ===${NC}"
    echo ""
    echo -e "${GREEN}系统配置:${NC}"
    grep -E "controller_frequency|mode:" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
    echo -e "${GREEN}MPC核心:${NC}"
    grep -E "horizon:|ref_vel:|max_angvel:" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
    echo -e "${GREEN}代价权重:${NC}"
    grep -E "w_cte:|w_etheta:|w_vel:|w_angvel:|w_accel:" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
    echo -e "${GREEN}约束:${NC}"
    grep -E "max_cte:|max_etheta:" "$ORIGINAL_PARAM" | sed 's/^/  /'
    echo ""
}

# 主循环
while true; do
    show_menu
    read -p "请选择 [0-6]: " choice

    case $choice in
        1)
            # 恢复默认配置
            if [ -f "$BACKUP_PARAM" ]; then
                restore_backup
            else
                echo -e "${YELLOW}⚠ 无备份，使用当前配置作为默认配置${NC}"
            fi
            ;;
        2)
            # 平滑模式
            switch_param "safe_regret_mpc_params_smooth.yaml"
            ;;
        3)
            # 快速模式
            switch_param "safe_regret_mpc_params_fast.yaml"
            ;;
        4)
            # 显示当前参数
            show_current_params
            ;;
        5)
            # 创建备份
            backup_current
            ;;
        6)
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
