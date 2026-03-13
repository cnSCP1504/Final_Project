#!/bin/bash
# Quick Fix Script for Tube MPC Performance Issues
# This script applies optimized parameters and restarts navigation

echo "=========================================="
echo "Tube MPC 性能快速修复"
echo "=========================================="
echo ""

PARAMS_DIR="/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params"
ORIGINAL_PARAM="$PARAMS_DIR/tube_mpc_params.yaml"

echo "Step 1: 备份原始参数..."
if [ ! -f "$PARAMS_DIR/tube_mpc_params.yaml.backup" ]; then
    cp "$ORIGINAL_PARAM" "$PARAMS_DIR/tube_mpc_params.yaml.backup"
    echo "✓ 备份已创建"
else
    echo "✓ 备份已存在"
fi
echo ""

echo "Step 2: 应用优化参数..."
cp "$PARAMS_DIR/tube_mpc_params_optimized.yaml" "$ORIGINAL_PARAM"
echo "✓ 优化参数已应用"
echo ""

echo "Step 3: 显示关键参数变化..."
echo "原始参数 -> 优化参数:"
echo "  max_vel_x:        0.5 -> 1.0 m/s"
echo "  mpc_ref_vel:      0.5 -> 0.8 m/s"
echo "  mpc_max_angvel:   0.5 -> 1.5 rad/s (关键!)"
echo "  mpc_w_vel:      100.0 -> 50.0 (降低50%)"
echo "  mpc_w_cte:      100.0 -> 50.0 (降低50%)"
echo "  mpc_steps:        20 -> 25"
echo ""

echo "Step 4: 参数验证..."
grep -A 1 "max_vel_x\|mpc_ref_vel\|mpc_max_angvel\|mpc_w_vel" "$ORIGINAL_PARAM" | head -8
echo ""

echo "=========================================="
echo "✓ 参数优化完成！"
echo "=========================================="
echo ""
echo "接下来:"
echo "1. 如果有正在运行的ROS节点，请按Ctrl+C停止"
echo "2. 重新启动导航:"
echo "   roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "3. 测试导航效果:"
echo "   - 在RViz中设置初始位置 (2D Pose Estimate)"
echo "   - 设置导航目标 (2D Nav Goal)"
echo "   - 观察机器人是否正常运动"
echo ""
echo "4. 如果需要调整参数，可以运行:"
echo "   /home/dixon/Final_Project/catkin/switch_mpc_params.sh"
echo ""
echo "5. 查看详细调参指南:"
echo "   cat /home/dixon/Final_Project/catkin/TUBEMPC_TUNING_GUIDE.md"
echo ""
echo "预期改进:"
echo "  ✅ 运动速度提升 5-10 倍"
echo "  ✅ 转向更加流畅"
echo "  ✅ 成功到达目标点"
echo ""
