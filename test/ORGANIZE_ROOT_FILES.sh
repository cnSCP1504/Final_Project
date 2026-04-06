#!/bin/bash
#
# 整理根目录下的测试文件到test文件夹
#
# 分类规则:
# - *.sh 测试脚本 -> test/scripts/
# - *FIX*.md 修复报告 -> test/reports/fixes/
# - *REPORT*.md 测试报告 -> test/reports/
# - *GUIDE*.md 指南文档 -> test/docs/guides/
# - *SUMMARY*.md 总结文档 -> test/docs/
# - *README*.md 说明文档 -> test/docs/
# - 其他.md 相关文档 -> test/docs/
#

set -e

echo "=========================================="
echo "开始整理根目录测试文件"
echo "=========================================="

# 创建必要的子目录
mkdir -p test/reports/fixes
mkdir -p test/reports/tests
mkdir -p test/docs/guides
mkdir -p test/docs/summaries
mkdir -p test/scripts/root_tests
mkdir -p test/diagnostics/root_scripts

# 1. 移动测试脚本到 test/scripts/root_tests/
echo ""
echo "[1/6] 移动测试脚本..."
TEST_SCRIPTS=(
    "test_angular_velocity_fix.sh"
    "test_dr_constraints.sh"
    "test_etheta_fix.sh"
    "test_feature_combinations_auto.sh"
    "test_feature_combinations.sh"
    "test_goal_arrival_fix.sh"
    "test_goal_reached_fix.sh"
    "test_in_place_rotation_fix.sh"
    "test_in_place_rotation_simple.sh"
    "test_integration_step1.sh"
    "test_large_angle_turn.sh"
    "test_logistics_task.sh"
    "test_random_obstacles.sh"
    "test_rotation_direction_lock.sh"
    "test_rotation_fallback.sh"
    "test_rotation_fix_v2.sh"
    "test_stl_dr_rotation_fix.sh"
    "test_tracking_error_receive.sh"
    "test_two_goals.sh"
)

for script in "${TEST_SCRIPTS[@]}"; do
    if [ -f "$script" ]; then
        mv "$script" test/scripts/root_tests/
        echo "  ✓ 移动: $script"
    fi
done

# 2. 移动诊断脚本到 test/diagnostics/root_scripts/
echo ""
echo "[2/6] 移动诊断脚本..."
DIAGNOSTIC_SCRIPTS=(
    "diagnose_etheta_oscillation.sh"
    "get_gazebo_camera_pose.sh"
)

for script in "${DIAGNOSTIC_SCRIPTS[@]}"; do
    if [ -f "$script" ]; then
        mv "$script" test/diagnostics/root_scripts/
        echo "  ✓ 移动: $script"
    fi
done

# 3. 移动修复报告到 test/reports/fixes/
echo ""
echo "[3/6] 移动修复报告..."
FIX_REPORTS=(
    "ANGULAR_VELOCITY_FIX_FINAL.md"
    "ANGULAR_VELOCITY_FIX_REPORT.md"
    "ANGULAR_VELOCITY_OVERRIDE_FIX_REPORT.md"
    "APPROACH_ZONE_FIX_REPORT.md"
    "BOOST_MUTEX_FIX_REPORT.md"
    "DR_CONSTRAINT_FIX_REPORT.md"
    "GOAL_REACHED_FIX_REPORT.md"
    "IN_PLACE_ROTATION_FIX_STL_DR_REPORT.md"
    "IN_PLACE_ROTATION_PORT_REPORT.md"
    "ROTATION_DIRECTION_LOCK_FIX_REPORT.md"
    "ROTATION_FALLBACK_FIX_REPORT.md"
    "ROTATION_LOCK_IMPLEMENTATION_SUMMARY.md"
)

for report in "${FIX_REPORTS[@]}"; do
    if [ -f "$report" ]; then
        mv "$report" test/reports/fixes/
        echo "  ✓ 移动: $report"
    fi
done

# 4. 移动测试报告到 test/reports/tests/
echo ""
echo "[4/6] 移动测试报告..."
TEST_REPORTS=(
    "FEATURE_COMBINATION_TEST_REPORT.md"
    "INTEGRATION_STATUS_REPORT.md"
    "INTEGRATION_TEST_REPORT_FINAL.md"
    "METRICS_COLLECTION_TEST_DETAILED_REPORT.md"
    "METRICS_COLLECTION_TEST_REPORT.md"
)

for report in "${TEST_REPORTS[@]}"; do
    if [ -f "$report" ]; then
        mv "$report" test/reports/tests/
        echo "  ✓ 移动: $report"
    fi
done

# 5. 移动指南文档到 test/docs/guides/
echo ""
echo "[5/6] 移动指南文档..."
GUIDES=(
    "INTEGRATION_TESTING_GUIDE.md"
    "LOGISTICS_TASK_GUIDE.md"
    "METRICS_COLLECTION_GUIDE.md"
    "TUBE_MPC_METRICS_GUIDE.md"
)

for guide in "${GUIDES[@]}"; do
    if [ -f "$guide" ]; then
        mv "$guide" test/docs/guides/
        echo "  ✓ 移动: $guide"
    fi
done

# 6. 移动总结和说明文档到 test/docs/
echo ""
echo "[6/6] 移动总结和说明文档..."
DOCS=(
    "GAZEBO_OBSTACLE_FIX.md"
    "LARGE_ANGLE_TURN_FIX_README.md"
    "LOGISTICS_TASK_UPDATE_SUMMARY.md"
    "METRICS_SYSTEM_IMPLEMENTATION_SUMMARY.md"
    "OBSTACLE_RANGE_LIMIT_SUMMARY.md"
    "STRICT_ROTATION_MODE_README.md"
    "TEST_AUTO_GOAL_README.md"
    "TEST_STRICT_ROTATION_FIX.md"
    "TUBE_MPC_METRICS_IMPLEMENTATION_SUMMARY.md"
)

for doc in "${DOCS[@]}"; do
    if [ -f "$doc" ]; then
        mv "$doc" test/docs/
        echo "  ✓ 移动: $doc"
    fi
done

# 创建索引文件
echo ""
echo "创建文件索引..."
cat > test/ROOT_FILES_ORGANIZATION.md << 'EOF'
# 根目录测试文件整理索引

## 整理时间
2026-04-05

## 文件分类

### 1. 测试脚本 -> test/scripts/root_tests/
所有从根目录移动来的测试脚本。

### 2. 诊断脚本 -> test/diagnostics/root_scripts/
用于诊断和调试的脚本。

### 3. 修复报告 -> test/reports/fixes/
各类bug修复和改进的详细报告。

### 4. 测试报告 -> test/reports/tests/
功能测试和集成测试的报告。

### 5. 指南文档 -> test/docs/guides/
系统使用和配置的指南。

### 6. 总结文档 -> test/docs/
实现总结、技术文档等。

## 查找文件

### 查找所有测试脚本
```bash
find test/scripts/root_tests/ -name "*.sh"
```

### 查找所有修复报告
```bash
find test/reports/fixes/ -name "*.md"
```

### 查找所有指南
```bash
find test/docs/guides/ -name "*.md"
```

## 运行测试

### 运行根目录测试脚本
```bash
# 物流任务测试
test/scripts/root_tests/test_logistics_task.sh

# 原地旋转测试
test/scripts/root_tests/test_strict_rotation_mode.sh

# 大角度转向测试
test/scripts/root_tests/test_large_angle_turn.sh
```

### 查看文档
```bash
# 查看物流任务指南
cat test/docs/guides/LOGISTICS_TASK_GUIDE.md

# 查看最新修复报告
ls -lt test/reports/fixes/ | head -5
```
EOF

echo "  ✓ 创建: test/ROOT_FILES_ORGANIZATION.md"

# 统计信息
echo ""
echo "=========================================="
echo "整理完成！"
echo "=========================================="
echo ""
echo "📊 统计信息："
echo "  测试脚本: $(ls test/scripts/root_tests/ | wc -l) 个"
echo "  诊断脚本: $(ls test/diagnostics/root_scripts/ | wc -l) 个"
echo "  修复报告: $(ls test/reports/fixes/ | wc -l) 个"
echo "  测试报告: $(ls test/reports/tests/ | wc -l) 个"
echo "  指南文档: $(ls test/docs/guides/ | wc -l) 个"
echo "  其他文档: $(ls test/docs/*.md 2>/dev/null | wc -l) 个"
echo ""
echo "📁 文件索引: test/ROOT_FILES_ORGANIZATION.md"
echo ""
