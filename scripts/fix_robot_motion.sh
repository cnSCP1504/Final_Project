#!/bin/bash

# Tube MPC 机器人不动问题诊断和修复工具

echo "=== Tube MPC 机器人不动问题诊断 ==="
echo ""

# 检查当前参数配置
PARAM_FILE="/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml"

echo "1. 检查关键参数配置..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 提取关键参数
echo "当前参数配置:"
grep -E "mpc_ref_vel|mpc_w_vel|mpc_max_angvel|max_speed|controller_freq" "$PARAM_FILE" | sed 's/^/  /'

echo ""
echo "2. 诊断结果..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 检查是否有明显问题
REF_VEL=$(grep "mpc_ref_vel:" "$PARAM_FILE" | awk '{print $2}')
W_VEL=$(grep "mpc_w_vel:" "$PARAM_FILE" | awk '{print $2}')
MAX_SPEED=$(grep "max_speed:" "$PARAM_FILE" | awk '{print $2}')

echo "参考速度: $REF_VEL m/s"
echo "速度权重: $W_VEL"
echo "最大速度: $MAX_SPEED m/s"

# 潜在问题诊断
if [ "$(echo "$W_VEL < 100" | bc)" -eq 1 ]; then
    echo "⚠️  问题: 速度权重 ($W_VEL) 可能太低，导致机器人不愿意加速"
fi

if [ "$(echo "$REF_VEL < 0.5" | bc)" -eq 1 ]; then
    echo "⚠️  问题: 参考速度 ($REF_VEL) 可能偏低"
fi

echo ""
echo "3. 创建修复后的参数配置..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 备份原文件
cp "$PARAM_FILE" "${PARAM_FILE}.backup_$(date +%Y%m%d_%H%M%S)"
echo "✓ 已备份原配置到: ${PARAM_FILE}.backup_$(date +%Y%m%d_%H%M%S)"

# 创建修复配置
cat > "$PARAM_FILE" << 'EOF'
MPCPlannerROS:
  map_frame: map
  odom_frame: odom
  base_frame: base_footprint
  min_trans_vel: 0.1
  max_vel_x: 1.0
  min_vel_x: -0.1
  max_vel_y: 0.0
  min_vel_y: 0.0
  trans_stopped_vel: 0.1
  max_rot_vel: 2.5
  min_rot_vel: 0.1
  rot_stopped_vel: 0.1
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
  max_speed: 1.0
  waypoints_dist: -1.0
  path_length: 8.0
  controller_freq: 10
  mpc_steps: 20
  mpc_ref_cte: 0.0
  mpc_ref_vel: 0.5
  mpc_ref_etheta: 0.0
  mpc_w_cte: 50.0
  mpc_w_etheta: 50.0
  mpc_w_vel: 10.0
  mpc_w_angvel: 30.0
  mpc_w_angvel_d: 30.0
  mpc_w_accel: 20.0
  mpc_w_accel_d: 5.0
  mpc_max_angvel: 2.0
  mpc_max_throttle: 2.0
  mpc_bound_value: 1.0e3
  disturbance_bound: 0.15
  enable_adaptive_disturbance: true
  disturbance_window_size: 100
  tube_visualization: true
  error_feedback_gain: 0.6
EOF

echo "✓ 已创建修复后的配置"
echo ""

echo "4. 修复内容说明..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "主要修复:"
echo "  • min_trans_vel: 0.01 → 0.1 (提高最小速度)"
echo "  • max_vel_x: 0.9 → 1.0 (提高最大速度)"
echo "  • trans_stopped_vel: 0.01 → 0.1 (提高停止阈值)"
echo "  • max_rot_vel: 2.0 → 2.5 (提高最大角速度)"
echo "  • max_speed: 0.9 → 1.0 (提高控制器最大速度)"
echo "  • mpc_ref_vel: 0.7 → 0.5 (降低参考速度到合理值)"
echo "  • mpc_w_vel: 50.0 → 10.0 (降低速度权重，鼓励加速)"
echo "  • mpc_max_angvel: 1.3 → 2.0 (提高角速度限制)"
echo "  • mpc_max_throttle: 1.5 → 2.0 (提高最大加速度)"
echo "  • mpc_steps: 25 → 20 (减少计算步数，提高实时性)"
echo ""

echo "5. 后续步骤..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "请依次执行:"
echo ""
echo "  1. 重新编译工作空间:"
echo "     cd /home/dixon/Final_Project/catkin"
echo "     catkin_make"
echo "     source devel/setup.bash"
echo ""
echo "  2. 启动导航测试:"
echo "     roslaunch tube_mpc_ros tube_mpc_navigation.launch"
echo ""
echo "  3. 在RViz中设置目标"
echo ""
echo "  4. 观察机器人是否开始移动"
echo ""

# 创建快速应用脚本
cat > "/tmp/apply_fix.sh" << 'EOF'
#!/bin/bash
echo "应用Tube MPC参数修复..."
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
echo "✓ 修复已应用，可以启动导航测试"
EOF

chmod +x "/tmp/apply_fix.sh"
echo "✓ 已创建快速应用脚本: /tmp/apply_fix.sh"
echo ""

echo "6. 如果机器人仍然不动，可能需要额外检查:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  • 检查/cmd_vel话题是否正常发布"
echo "    rostopic echo /cmd_vel"
echo ""
echo "  • 检查是否有路径规划"
echo "    rostopic echo /move_base/GlobalPlanner/plan"
echo ""
echo "  • 检查里程计数据"
echo "    rostopic echo /odom"
echo ""
echo "  • 查看控制器调试信息"
echo "    rosservice call /tube_mpc_ros/set_parameters \"{'debug_info': true}\""
echo ""

echo "✅ 诊断完成！"
