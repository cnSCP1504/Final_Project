#!/bin/bash
# 拐弯优化自动化调参系统
# 循环运行roslaunch，监控拐弯性能，自动调整参数

WORKSPACE="/home/dixon/Final_Project/catkin"
PARAMS_DIR="$WORKSPACE/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"
LOG_DIR="$WORKSPACE/turning_logs"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p "$LOG_DIR"

echo "=========================================="
echo "拐弯优化自动化调参系统"
echo "=========================================="
echo "目标: 改善机器人拐弯跟踪性能"
echo "方法: 循环测试→数据分析→参数调整"
echo "=========================================="
echo ""

# 备份原始参数
if [ ! -f "$PARAMS_DIR/tube_mpc_params.yaml.backup" ]; then
    cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params.yaml.backup"
fi

iteration=0
turning_score=0
best_score=0

while [ $iteration -lt 15 ]; do
    iteration=$((iteration + 1))

    echo -e "${YELLOW}=========================================="
    echo "第 $iteration 次拐弯优化迭代"
    echo "==========================================${NC}"
    echo ""

    # 根据迭代次数生成拐弯优化的参数
    case $iteration in
        1)
            # 基础配置 - 提高角速度权重
            mpc_max_angvel=1.2
            mpc_w_etheta=80.0
            mpc_w_angvel=40.0
            mpc_ref_etheta=0.0
            forward_point=0.4
            path_bias=32.0
            desc="基础拐弯优化"
            ;;
        2)
            # 增加前瞻距离
            mpc_max_angvel=1.5
            mpc_w_etheta=100.0
            mpc_w_angvel=30.0
            mpc_ref_etheta=0.0
            forward_point=0.5
            path_bias=32.0
            desc="增加前瞻距离"
            ;;
        3)
            # 提高航向误差权重
            mpc_max_angvel=1.8
            mpc_w_etheta=150.0
            mpc_w_angvel=25.0
            mpc_ref_etheta=0.0
            forward_point=0.5
            path_bias=35.0
            desc="提高航向权重"
            ;;
        4)
            # 降低角速度限制，增加稳定性
            mpc_max_angvel=1.3
            mpc_w_etheta=120.0
            mpc_w_angvel=50.0
            mpc_ref_etheta=0.0
            forward_point=0.45
            path_bias=30.0
            desc="平衡转向稳定性"
            ;;
        5)
            # 增加路径跟踪权重
            mpc_max_angvel=1.6
            mpc_w_etheta=130.0
            mpc_w_angvel=35.0
            mpc_ref_etheta=0.0
            forward_point=0.5
            path_bias=40.0
            desc="增强路径跟踪"
            ;;
        6)
            # 优化角速度变化率
            mpc_max_angvel=1.7
            mpc_w_etheta=140.0
            mpc_w_angvel=45.0
            mpc_w_angvel_d=40.0
            mpc_ref_etheta=0.0
            forward_point=0.5
            path_bias=38.0
            desc="优化角速度变化"
            ;;
        7)
            # 增加预测步数
            mpc_max_angvel=1.8
            mpc_w_etheta=135.0
            mpc_w_angvel=38.0
            mpc_ref_etheta=0.0
            forward_point=0.55
            path_bias=36.0
            mpc_steps=30
            desc="增加预测能力"
            ;;
        8)
            # 综合优化 - 高配置
            mpc_max_angvel=2.0
            mpc_w_etheta=145.0
            mpc_w_angvel=32.0
            mpc_w_angvel_d=35.0
            mpc_ref_etheta=0.0
            forward_point=0.6
            path_bias=42.0
            mpc_steps=30
            desc="综合高性能"
            ;;
        *)
            # 动态调整 - 基于前面结果
            if [ $turning_score -lt 3 ]; then
                # 分数低，需要更激进
                mpc_max_angvel=2.2
                mpc_w_etheta=160.0
                mpc_w_angvel=25.0
                forward_point=0.6
                path_bias=45.0
                desc="激进拐弯优化"
            else
                # 分数中等，微调
                mpc_max_angvel=1.9
                mpc_w_etheta=140.0
                mpc_w_angvel=35.0
                forward_point=0.55
                path_bias=40.0
                desc="精细调优"
            fi
            ;;
    esac

    # 应用参数
    apply_turning_parameters

    echo -e "${BLUE}参数配置 #$iteration: $desc${NC}"
    echo "  mpc_max_angvel:   $mpc_max_angvel rad/s"
    echo "  mpc_w_etheta:     $mpc_w_etheta"
    echo "  mpc_w_angvel:     $mpc_w_angvel"
    echo "  forward_point:    $forward_point m"
    echo "  path_distance_bias: $path_bias"
    echo -e "${GREEN}✓ 参数已应用${NC}"
    echo ""

    # 启动导航系统
    echo -e "${YELLOW}启动导航系统...${NC}"
    launch_and_monitor &

    ROS_PID=$!
    sleep 8  # 等待系统启动

    # 发送测试路径
    echo -e "${BLUE}发送测试路径...${NC}"
    send_test_path

    # 监控一段时间
    echo -e "${BLUE}监控拐弯性能 (30秒)...${NC}"
    monitor_turning_performance 30

    # 分析结果
    echo ""
    echo "请观察机器人拐弯表现并评分:"
    echo "  1) 很差 - 完全不拐弯或拐弯过度"
    echo "  2) 差 - 拐弯延迟或严重偏离路径"
    echo "  3) 中 - 能拐弯但不够平滑"
    echo "  4) 良 - 拐弯较好，偶尔偏离"
    echo "  5) 优 - 拐弯平滑，紧跟路径"
    echo ""

    read -p "拐弯性能评分 (1-5): " score

    # 记录结果
    echo "Iteration $iteration: $desc | Score: $score" >> "$LOG_DIR/turning_scores.log"

    if [ $score -gt $best_score ]; then
        best_score=$score
        cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params_best_turning.yaml"
        echo -e "${GREEN}✓ 新的最佳拐弯配置已保存!${NC}"
    fi

    turning_score=$score

    # 清理
    kill $ROS_PID 2>/dev/null || true
    killall -9 roslaunch roscore 2>/dev/null || true
    sleep 2

    # 如果达到满意分数，退出
    if [ $score -ge 4 ]; then
        echo -e "${GREEN}=========================================="
        echo "✓ 拐弯优化完成!"
        echo "最佳配置: tube_mpc_params_best_turning.yaml"
        echo "迭代次数: $iteration"
        echo "==========================================${NC}"
        break
    fi

    echo -e "${BLUE}继续优化...${NC}"
    sleep 1
    clear
done

# 恢复原始参数
cp "$PARAMS_DIR/tube_mpc_params.yaml.backup" "$ORIGINAL_PARAM"

echo ""
echo "=========================================="
echo "拐弯优化总结"
echo "=========================================="
echo "总迭代次数: $iteration"
echo "最佳评分: $best_score/5"
echo "最佳配置: $PARAMS_DIR/tube_mpc_params_best_turning.yaml"
echo "详细日志: $LOG_DIR/turning_scores.log"
echo "=========================================="
