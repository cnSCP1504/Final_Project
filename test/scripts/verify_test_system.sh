#!/bin/bash
################################################################################
# 测试系统验证脚本
# Verify Test System Installation
################################################################################

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        自动化Baseline测试系统 - 安装验证                     ║"
echo "║     Automated Baseline Testing System - Verification        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

ERRORS=0

# 检查核心脚本
echo "【检查核心脚本】"
echo "----------------------------------------"

files=(
    "automated_baseline_test.sh"
    "run_automated_test.py"
    "goal_monitor.py"
    "quick_test.sh"
)

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        if [ -x "$file" ]; then
            echo "✅ $file - 存在且可执行"
        else
            echo "⚠️  $file - 存在但不可执行，修复中..."
            chmod +x "$file"
            echo "✅ $file - 已修复为可执行"
        fi
    else
        echo "❌ $file - 文件不存在"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""

# 检查配置文件
echo "【检查配置文件】"
echo "----------------------------------------"

config_files=(
    "shelf_locations.yaml"
    "AUTOMATED_TESTING_README.md"
    "QUICK_START.txt"
)

for file in "${config_files[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file - 存在"
    else
        echo "❌ $file - 文件不存在"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""

# 检查Python依赖
echo "【检查Python依赖】"
echo "----------------------------------------"

python3 -c "import yaml" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ PyYAML - 已安装"
else
    echo "⚠️  PyYAML - 未安装（可选）"
    echo "   安装命令: pip3 install pyyaml"
fi

python3 -c "import rospkg" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ rospkg - 已安装"
else
    echo "⚠️  rospkg - 未安装（可选）"
    echo "   安装命令: pip3 install rospkg"
fi

echo ""

# 检查结果目录
echo "【检查结果目录】"
echo "----------------------------------------"

RESULTS_DIR="$HOME/Final_Project/catkin/test_results"
if [ -d "$RESULTS_DIR" ]; then
    echo "✅ 结果目录存在: $RESULTS_DIR"
else
    echo "⚠️  结果目录不存在，将在首次运行时创建"
    echo "   目录: $RESULTS_DIR"
fi

echo ""

# 检查ROS环境
echo "【检查ROS环境】"
echo "----------------------------------------"

if [ -n "$ROS_DISTRO" ]; then
    echo "✅ ROS已安装: $ROS_DISTRO"
else
    echo "⚠️  ROS环境未加载"
    echo "   请运行: source /opt/ros/noetic/setup.bash"
fi

echo ""

# 总结
echo "【验证总结】"
echo "----------------------------------------"

if [ $ERRORS -eq 0 ]; then
    echo "✅ 所有核心文件就绪！"
    echo ""
    echo "📖 快速开始:"
    echo "   1. 交互式菜单: ./quick_test.sh"
    echo "   2. 查看快速指南: cat QUICK_START.txt"
    echo "   3. 运行测试: ./run_automated_test.py --model tube_mpc"
    echo ""
    echo "📚 详细文档:"
    echo "   - 使用指南: cat AUTOMATED_TESTING_README.md"
    echo "   - 文件清单: cat ../../test/TESTING_SYSTEM_SUMMARY.md"
    exit 0
else
    echo "❌ 发现 $ERRORS 个错误，请检查缺失的文件"
    exit 1
fi
