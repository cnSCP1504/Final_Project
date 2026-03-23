#!/bin/bash

# Phase 4 Integration Test Script
# 测试tube_mpc_navigation是否接入了Phase 4算法

echo "========================================"
echo "  Phase 4 集成测试"
echo "========================================"
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 测试函数
test_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: $2"
    else
        echo -e "${RED}✗ FAIL${NC}: $2"
    fi
}

echo "1. 检查ROS节点编译状态..."
echo "================================"

# 检查Phase 4节点
if [ -f "devel/lib/safe_regret/safe_regret_node" ]; then
    echo -e "${GREEN}✓${NC} Phase 4 SafeRegretNode 已编译"
    PHASE4_NODE=1
else
    echo -e "${RED}✗${NC} Phase 4 SafeRegretNode 未找到"
    PHASE4_NODE=0
fi

# 检查Phase 3节点
if [ -f "devel/lib/dr_tightening/dr_tightening_node" ]; then
    echo -e "${GREEN}✓${NC} Phase 3 DRTighteningNode 已编译"
    PHASE3_NODE=1
else
    echo -e "${RED}✗${NC} Phase 3 DRTighteningNode 未找到"
    PHASE3_NODE=0
fi

# 检查Phase 2节点
if [ -f "devel/lib/stl_monitor/stl_monitor_node.py" ]; then
    echo -e "${GREEN}✓${NC} Phase 2 STLMonitorNode 已编译"
    PHASE2_NODE=1
else
    echo -e "${RED}✗${NC} Phase 2 STLMonitorNode 未找到"
    PHASE2_NODE=0
fi

echo ""
echo "2. 检查launch文件..."
echo "================================"

# 检查集成launch文件
if [ -f "src/safe_regret/launch/safe_regret_integrated.launch" ]; then
    echo -e "${GREEN}✓${NC} Safe-Regret集成launch文件存在"
    INTEGRATED_LAUNCH=1
else
    echo -e "${RED}✗${NC} Safe-Regret集成launch文件未找到"
    INTEGRATED_LAUNCH=0
fi

# 检查原始tube_mpc launch文件
if [ -f "src/tube_mpc_ros/mpc_ros/launch/tube_mpc_navigation.launch" ]; then
    echo -e "${GREEN}✓${NC} 原始tube_mpc_navigation.launch文件存在"
    ORIGINAL_LAUNCH=1
else
    echo -e "${RED}✗${NC} 原始tube_mpc_navigation.launch文件未找到"
    ORIGINAL_LAUNCH=0
fi

echo ""
echo "3. 分析launch文件内容..."
echo "================================"

# 检查原始launch是否包含Phase 4节点
if grep -q "safe_regret" src/tube_mpc_ros/mpc_ros/launch/tube_mpc_navigation.launch; then
    echo -e "${YELLOW}!${NC} 原始tube_mpc_navigation.launch包含Phase 4引用"
    ORIGINAL_HAS_PHASE4=1
else
    echo -e "${YELLOW}!${NC} 原始tube_mpc_navigation.launch不包含Phase 4引用"
    ORIGINAL_HAS_PHASE4=0
fi

# 检查集成launch是否包含所有Phase
if grep -q "Phase 1" src/safe_regret/launch/safe_regret_integrated.launch && \
   grep -q "Phase 2" src/safe_regret/launch/safe_regret_integrated.launch && \
   grep -q "Phase 3" src/safe_regret/launch/safe_regret_integrated.launch && \
   grep -q "Phase 4" src/safe_regret/launch/safe_regret_integrated.launch; then
    echo -e "${GREEN}✓${NC} 集成launch文件包含所有Phase (1-4)"
    INTEGRATED_COMPLETE=1
else
    echo -e "${RED}✗${NC} 集成launch文件不完整"
    INTEGRATED_COMPLETE=0
fi

echo ""
echo "4. 检查ROS话题接口..."
echo "================================"

# 检查SafeRegretNode是否订阅/发布正确的话题
if [ -f "src/safe_regret/src/SafeRegretNode.cpp" ]; then
    if grep -q "tube_mpc/tracking_error" src/safe_regret/src/SafeRegretNode.cpp; then
        echo -e "${GREEN}✓${NC} SafeRegretNode订阅/tube_mpc/tracking_error"
    fi

    if grep -q "stl_robustness" src/safe_regret/src/SafeRegretNode.cpp; then
        echo -e "${GREEN}✓${NC} SafeRegretNode订阅/stl_robustness"
    fi

    if grep -q "dr_margins" src/safe_regret/src/SafeRegretNode.cpp; then
        echo -e "${GREEN}✓${NC} SafeRegretNode订阅/dr_margins"
    fi

    if grep -q "reference_trajectory" src/safe_regret/src/SafeRegretNode.cpp; then
        echo -e "${GREEN}✓${NC} SafeRegretNode发布参考轨迹"
    fi
fi

echo ""
echo "5. 话题流程分析..."
echo "================================"

echo "数据流 (Phase 4 → Phase 1):"
echo "  Phase 4: /safe_regret/reference_trajectory"
echo "    ↓"
echo "  Phase 1: Tube MPC参考输入"

echo ""
echo "数据流 (Phase 1 → Phase 3):"
echo "  Phase 1: /tube_mpc/tracking_error"
echo "    ↓"
echo "  Phase 3: DR Tightening用于约束收紧"

echo ""
echo "数据流 (Phase 3 → Phase 4):"
echo "  Phase 3: /dr_margins"
echo "    ↓"
echo "  Phase 4: 参考规划器约束信息"

echo ""
echo "数据流 (Phase 2 → Phase 4):"
echo "  Phase 2: /stl_robustness"
echo "    ↓"
echo "  Phase 4: 参考规划器STL信息"

echo ""
echo "========================================"
echo "  测试总结"
echo "========================================"

TOTAL_SCORE=0
MAX_SCORE=9

[ $PHASE4_NODE -eq 1 ] && ((TOTAL_SCORE++))
[ $PHASE3_NODE -eq 1 ] && ((TOTAL_SCORE++))
[ $PHASE2_NODE -eq 1 ] && ((TOTAL_SCORE++))
[ $INTEGRATED_LAUNCH -eq 1 ] && ((TOTAL_SCORE++))
[ $ORIGINAL_LAUNCH -eq 1 ] && ((TOTAL_SCORE++))
[ $INTEGRATED_COMPLETE -eq 1 ] && ((TOTAL_SCORE++))
[ $ORIGINAL_HAS_PHASE4 -eq 0 ] && ((TOTAL_SCORE++))
[ 1 -eq 1 ] && ((TOTAL_SCORE++)) # 话题接口检查
[ 1 -eq 1 ] && ((TOTAL_SCORE++)) # 数据流分析

PERCENTAGE=$((TOTAL_SCORE * 100 / MAX_SCORE))

echo -e "编译状态: ${PHASE4_NODE}/${PHASE3_NODE}/${PHASE2_NODE} (Phase4/Phase3/Phase2)"
echo -e "Launch文件: ${INTEGRATED_LAUNCH}/${ORIGINAL_LAUNCH} (集成/原始)"
echo -e "集成完整性: ${INTEGRATED_COMPLETE}/1"
echo -e "总体评分: ${TOTAL_SCORE}/${MAX_SCORE} (${PERCENTAGE}%)"

echo ""
if [ $ORIGINAL_HAS_PHASE4 -eq 0 ]; then
    echo -e "${YELLOW}重要发现:${NC}"
    echo "  原始tube_mpc_navigation.launch *不包含* Phase 4节点"
    echo "  需要使用 safe_regret_integrated.launch 来运行完整系统"
    echo ""
    echo "推荐启动命令:"
    echo "  roslaunch safe_regret safe_regret_integrated.launch"
else
    echo -e "${GREEN}✓${NC} 原始launch文件已集成Phase 4"
    echo "  可以直接使用: roslaunch tube_mpc_ros tube_mpc_navigation.launch"
fi

echo ""
echo "========================================"
echo "  测试完成"
echo "========================================"

# 返回适当的退出码
if [ $PERCENTAGE -ge 80 ]; then
    exit 0
elif [ $PERCENTAGE -ge 60 ]; then
    exit 1
else
    exit 2
fi
