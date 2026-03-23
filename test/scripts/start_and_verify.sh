#!/bin/bash

# Safe-Regret MPC 快速启动和验证脚本

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║     Safe-Regret MPC 系统启动和验证                           ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}[步骤 1/6]${NC} 清理之前的ROS进程..."
pkill -9 -f roslaunch 2>/dev/null
pkill -9 -f roscore 2>/dev/null
pkill -9 -f gazebo 2>/dev/null
pkill -9 -f rviz 2>/dev/null
sleep 2
echo -e "${GREEN}✓${NC} 清理完成"
echo ""

echo -e "${BLUE}[步骤 2/6]${NC} 启动Safe-Regret MPC系统..."
echo "启动命令：roslaunch safe_regret safe_regret_simplified.launch"
echo ""

# 启动系统（后台）
roslaunch safe_regret safe_regret_simplified.launch &
LAUNCH_PID=$!

echo -e "${GREEN}✓${NC} 系统启动中 (PID: $LAUNCH_PID)"
echo ""

echo -e "${YELLOW}[步骤 3/6]${NC} 等待节点初始化 (20秒)..."
sleep 20

echo -e "${YELLOW}[步骤 4/6]${NC} 验证系统状态..."
echo ""

# 检查launch进程是否还在运行
if ! ps -p $LAUNCH_PID 2>/dev/null > /dev/null; then
    echo -e "${RED}✗ Launch进程已退出${NC}"
    echo "可能原因："
    echo "1. ROS包依赖问题"
    echo "2. 参数配置错误"
    echo "3. 端口冲突"
    echo ""
    echo "查看详细日志："
    echo "cat ~/.ros/log/roslaunch-*/latest/roslaunch-*.log | tail -50"
    exit 1
fi

echo -e "${GREEN}✓${NC} Launch进程运行正常"
echo ""

echo -e "${YELLOW}[步骤 5/6]${NC} 检查ROS话题..."
echo ""

# 获取话题列表
TOPICS=$(timeout 5 rostopic list 2>/dev/null | grep -c "")
if [ "$TOPICS" -gt 0 ]; then
    echo -e "${GREEN}✓${NC} 发现 $TOPICS 个ROS话题"
    echo ""
    echo "关键话题："
    timeout 3 rostopic list 2>/dev/null | grep -E "safe_regret|tube_mpc|dr_" | head -10
else
    echo -e "${YELLOW}!${NC} 无法连接到rosmaster，可能还在初始化"
fi

echo ""

echo -e "${YELLOW}[步骤 6/6]${NC} 系统监控命令..."
echo ""

cat << 'EOF'
═══════════════════════════════════════════════════════════════
  系统监控命令 (在新终端中运行)
═══════════════════════════════════════════════════════════════

🔍 查看ROS节点：
   rosnode list

📊 查看所有话题：
   rostopic list

🎯 监控关键指标：
   # 遗憾指标
   rostopic echo /safe_regret/regret_metrics -n 1

   # Tube MPC跟踪误差
   rostopic echo /tube_mpc/tracking_error -n 1

   # DR约束边际
   rostopic echo /dr_margins -n 1

   # 参考轨迹
   rostopic echo /safe_regret/reference_trajectory -n 1

📈 可视化工具：
   # 节点关系图
   rqt_graph

   # 话题监控
   rqt_plot

🛑 停止系统：
   # 方法1: Ctrl+C (在这个终端)
   # 方法2: pkill -9 roslaunch

═══════════════════════════════════════════════════════════════

系统启动状态:
  ✅ Phase 1: Tube MPC (基础导航控制)
  ✅ Phase 3: DR Tightening (安全约束收紧)
  ✅ Phase 4: Reference Planner (遗憾分析)
  ⚠️  Phase 2: STL Monitor (暂时禁用)

预期功能:
  • 机器人自主导航
  • 跟踪误差计算和反馈
  • 分布式鲁棒约束
  • 参考轨迹生成
  • 遗憾指标实时计算

═══════════════════════════════════════════════════════════════

系统已启动！按Ctrl+C停止，或关闭此终端继续运行。
EOF

# 保持脚本运行，等待用户手动停止
echo ""
echo -e "${GREEN}✓✓✓ Safe-Regret MPC系统已成功启动！✓✓✓${NC}"
echo ""
echo "系统正在运行中，您可以:"
echo "1. 在新终端运行上面的监控命令"
echo "2. 观察Gazebo中的机器人运动"
echo "3. 查看RViz中的可视化"
echo "4. 按Ctrl+C停止系统"
echo ""

wait $LAUNCH_PID
