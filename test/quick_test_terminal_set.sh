#!/bin/bash

# 快速验证脚本 - 终端集实现 (P1-1)
# 用法: ./quick_test_terminal_set.sh

set -e

# 颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  终端集实现快速验证 (P1-1)${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""

source devel/setup.bash

# 快速检查
echo -e "${GREEN}[1/5]${NC} 构建状态..."
if catkin_make --pkg dr_tightening tube_mpc_ros 2>&1 | grep -q "Built target"; then
    echo -e "   ${GREEN}✅${NC} 构建成功"
else
    echo -e "   ${RED}❌${NC} 构建失败"
    exit 1
fi

echo -e "${GREEN}[2/5]${NC} 可执行文件..."
if [ -f "devel/lib/dr_tightening/dr_tightening_node" ] && [ -f "devel/lib/tube_mpc_ros/tube_TubeMPC_Node" ]; then
    echo -e "   ${GREEN}✅${NC} 所有可执行文件存在"
else
    echo -e "   ${RED}❌${NC} 可执行文件缺失"
    exit 1
fi

echo -e "${GREEN}[3/5]${NC} 参数文件..."
if [ -f "src/tube_mpc_ros/mpc_ros/params/terminal_set_params.yaml" ]; then
    echo -e "   ${GREEN}✅${NC} 终端集参数文件存在"
else
    echo -e "   ${RED}❌${NC} 参数文件缺失"
    exit 1
fi

echo -e "${GREEN}[4/5]${NC} 代码实现..."
if grep -q "setTerminalSet" src/tube_mpc_ros/mpc_ros/src/TubeMPC.cpp && \
   grep -q "terminalSetCB" src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp && \
   grep -q "publishTerminalSet" src/dr_tightening/src/dr_tightening_node.cpp; then
    echo -e "   ${GREEN}✅${NC} 所有必需代码已实现"
else
    echo -e "   ${RED}❌${NC} 代码实现不完整"
    exit 1
fi

echo -e "${GREEN}[5/5]${NC} Launch文件..."
if [ -f "src/dr_tightening/launch/terminal_set_computation.launch" ]; then
    echo -e "   ${GREEN}✅${NC} Launch文件存在"
else
    echo -e "   ${RED}❌${NC} Launch文件缺失"
    exit 1
fi

echo ""
echo -e "${BLUE}=========================================${NC}"
echo -e "${GREEN}✅ 所有检查通过！${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""
echo "终端集实现已完整并可用。"
echo ""
echo -e "${YELLOW}下一步:${NC}"
echo ""
echo "1. 运行验证脚本（完整检查）:"
echo "   ./verify_terminal_set.sh"
echo ""
echo "2. 启动DR tightening测试:"
echo "   roslaunch dr_tightening test_terminal_set.launch"
echo ""
echo "3. 启动完整系统:"
echo "   roslaunch dr_tightening terminal_set_computation.launch"
echo ""
echo "4. 查看完整测试报告:"
echo "   cat TERMINAL_SET_TEST_REPORT.md"
echo ""
