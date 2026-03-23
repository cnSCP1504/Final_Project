#!/bin/bash

# Safe-Regret MPC 系统启动脚本
# 分阶段启动，便于调试和监控

echo "=========================================="
echo "Safe-Regret MPC 完整系统启动"
echo "=========================================="

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 清理之前的ROS节点
echo -e "${YELLOW}[1/6]${NC} 清理之前的ROS节点..."
pkill -9 -f roslaunch 2>/dev/null
pkill -9 -f roscore 2>/dev/null
pkill -9 -f gazebo 2>/dev/null
pkill -9 -f rviz 2>/dev/null
sleep 2

# 启动roscore
echo -e "${GREEN}[2/6]${NC} 启动roscore..."
roscore &
ROSCORE_PID=$!
sleep 3

# 检查roscore是否启动
if ! pgrep -f roscore > /dev/null; then
    echo -e "${RED}错误: roscore启动失败${NC}"
    exit 1
fi
echo -e "${GREEN}✓ roscore已启动 (PID: $ROSCORE_PID)${NC}"

# 启动Phase 1-4节点（不启动Gazebo）
echo -e "${YELLOW}[3/6]${NC} 启动Phase 1-4节点..."

# 我们创建一个简化的启动，跳过Gazebo
# 只启动ROS节点进行测试

# 检查必要的节点是否存在
NODES_OK=true

if [ ! -f "devel/lib/safe_regret/safe_regret_node" ]; then
    echo -e "${RED}✗ Phase 4节点未找到${NC}"
    NODES_OK=false
fi

if [ ! -f "devel/lib/dr_tightening/dr_tightening_node" ]; then
    echo -e "${RED}✗ Phase 3节点未找到${NC}"
    NODES_OK=false
fi

if [ ! -f "devel/lib/stl_monitor/stl_monitor_node.py" ]; then
    echo -e "${RED}✗ Phase 2节点未找到${NC}"
    NODES_OK=false
fi

if [ "$NODES_OK" = false ]; then
    echo -e "${RED}错误: 部分节点缺失，无法启动完整系统${NC}"
    kill $ROSCORE_PID
    exit 1
fi

echo -e "${GREEN}✓ 所有节点文件存在${NC}"

# 尝试启动集成launch文件（无Gazebo模式）
echo -e "${YELLOW}[4/6]${NC} 启动集成launch文件..."

# 由于Gazebo可能会很慢，我们先尝试启动基本的ROS节点
# 如果需要完整仿真，用户可以手动启动

echo -e "${YELLOW}注意: 启动可能需要30-60秒...${NC}"

# 启动系统（后台运行）
timeout 90 roslaunch safe_regret safe_regret_integrated.launch &
LAUNCH_PID=$!

# 等待启动
echo "等待系统启动..."
sleep 20

# 检查进程是否还在运行
if ! kill -0 $LAUNCH_PID 2>/dev/null; then
    echo -e "${RED}Launch进程已退出，可能启动失败${NC}"
    echo "查看日志: cat ~/.ros/log/roslaunch-*/"
else
    echo -e "${GREEN}✓ Launch进程正在运行${NC}"
fi

echo -e "${YELLOW}[5/6]${NC} 检查ROS节点状态..."
sleep 5

# 检查关键话题
echo "检查ROS话题..."
rostopic list 2>/dev/null | grep -E "safe_regret|tube_mpc|stl_|dr_" || echo "某些话题可能还未启动"

# 检查节点
echo "检查ROS节点..."
rosnode list 2>/dev/null | grep -E "safe_regret|tube_mpc|stl|dr" || echo "某些节点可能还未启动"

echo -e "${YELLOW}[6/6]${NC} 系统监控信息..."
echo ""
echo "=========================================="
echo "  Safe-Regret MPC 系统监控"
echo "=========================================="
echo ""
echo "常用监控命令:"
echo "  # 查看所有话题"
echo "  rostopic list"
echo ""
echo "  # 查看遗憾指标"
echo "  rostopic echo /safe_regret/regret_metrics"
echo ""
echo "  # 查看跟踪误差"
echo "  rostopic echo /tube_mpc/tracking_error"
echo ""
echo "  # 查看STL鲁棒性"
echo "  rostopic echo /stl_robustness"
echo ""
echo "  # 查看DR边际"
echo "  rostopic echo /dr_margins"
echo ""
echo "  # 查看节点图"
echo "  rqt_graph"
echo ""
echo "  # 停止系统"
echo "  pkill -9 roslaunch"
echo ""
echo "=========================================="
echo "  系统启动完成！"
echo "=========================================="
echo ""
echo "按Ctrl+C停止系统，或使用: pkill -9 roslaunch"
echo ""

# 保持脚本运行
wait $LAUNCH_PID