#!/bin/bash

# 快速验证 "all" 模式功能
# 测试所有模型，每个模型只测试1个货架

echo "======================================================================"
echo "🧪 ALL MODE 功能验证测试"
echo "======================================================================"
echo "测试配置:"
echo "  模型: all (tube_mpc, stl, dr, safe_regret)"
echo "  货架数: 1 (快速验证)"
echo "  无Gazebo: true"
echo "  无RViz: true"
echo "======================================================================"
echo ""

# 运行测试
cd /home/dixon/Final_Project/catkin
python3 test/scripts/run_automated_test.py \
    --model all \
    --shelves 1 \
    --headless \
    --timeout 60

echo ""
echo "======================================================================"
echo "✅ 验证测试完成"
echo "======================================================================"
echo ""
echo "检查结果目录:"
echo "  ls -lt test_results/ | head -5"
echo ""
echo "应该看到4个模型的结果目录:"
echo "  - tube_mpc_YYYYMMDD_HHMMSS/"
echo "  - stl_YYYYMMDD_HHMMSS/"
echo "  - dr_YYYYMMDD_HHMMSS/"
echo "  - safe_regret_YYYYMMDD_HHMMSS/"
echo ""
