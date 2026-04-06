#!/bin/bash
################################################################################
# 快速测试启动脚本
# Quick Test Launcher
#
# 快速启动各种测试场景的便捷脚本
################################################################################

show_menu() {
    clear
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║        自动化Baseline测试系统 - 快速启动菜单               ║"
    echo "║     Automated Baseline Testing System - Quick Menu         ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo ""
    echo "请选择测试场景 / Please select a test scenario:"
    echo ""
    echo "【基础测试 / Basic Tests】"
    echo "  1) Tube MPC - 所有货架（基线测试）"
    echo "  2) Tube MPC - 3个货架（快速测试）"
    echo "  3) Tube MPC - 无Gazebo（极速测试）"
    echo ""
    echo "【STL增强测试 / STL Enhanced Tests】"
    echo "  4) STL监控 - 所有货架"
    echo "  5) STL监控 - 3个货架"
    echo ""
    echo "【DR约束测试 / DR Constraint Tests】"
    echo "  6) DR约束 - 所有货架"
    echo "  7) DR约束 - 3个货架"
    echo ""
    echo "【完整系统测试 / Complete System Tests】"
    echo "  8) Safe-Regret MPC - 所有货架（完整功能）"
    echo "  9) Safe-Regret MPC - 3个货架"
    echo ""
    echo "【高级选项 / Advanced Options】"
    echo "  a) 自定义测试（Python脚本）"
    echo "  b) 自定义测试（Bash脚本）"
    echo ""
    echo "【其他 / Others】"
    echo "  h) 帮助信息"
    echo "  q) 退出"
    echo ""
    echo -n "请输入选择 (1-9, a, b, h, q): "
}

run_test() {
    local cmd=$1
    local description=$2

    echo ""
    echo "=========================================="
    echo "执行测试 / Executing Test"
    echo "=========================================="
    echo "描述 / Description: $description"
    echo "命令 / Command: $cmd"
    echo "=========================================="
    echo ""
    read -p "按Enter键开始 / Press Enter to start..."

    echo ""
    echo "测试启动中... / Starting test..."

    cd /home/dixon/Final_Project/catkin
    eval $cmd

    echo ""
    echo "=========================================="
    echo "测试完成 / Test Completed"
    echo "=========================================="
    echo ""
    read -p "按Enter键返回菜单 / Press Enter to return to menu..."
}

show_help() {
    clear
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║                    帮助信息 / Help                         ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo ""
    echo "【模型说明 / Model Descriptions】"
    echo ""
    echo "1. tube_mpc (Tube MPC)"
    echo "   - 仅使用Tube MPC进行路径跟踪"
    echo "   - 无STL监控，无DR约束"
    echo "   - 作为性能对比的基线"
    echo ""
    echo "2. stl (Tube MPC + STL)"
    echo "   - Tube MPC + STL监控"
    echo "   - 评估时序逻辑任务满足度"
    echo "   - 优化任务性能"
    echo ""
    echo "3. dr (Tube MPC + DR)"
    echo "   - Tube MPC + DR约束收紧"
    echo "   - 数据驱动的概率安全保证"
    echo "   - 提高安全性"
    echo ""
    echo "4. safe_regret (Safe-Regret MPC)"
    echo "   - 完整系统: STL + DR"
    echo "   - 时序逻辑优化 + 概率安全"
    echo "   - 最佳性能和安全性"
    echo ""
    echo "【货架位置 / Shelf Locations】"
    echo ""
    echo "货架01: (-6.0, -7.0) - 左上区域"
    echo "货架02: (-3.0, -7.0) - 左中区域"
    echo "货架03: (0.0, -7.0)  - 左下区域（默认）"
    echo "货架04: (3.0, -7.0)  - 中上区域"
    echo "货架05: (6.0, -7.0)  - 中下区域"
    echo ""
    echo "【测试结果 / Test Results】"
    echo ""
    echo "所有测试结果保存在: test_results/MODEL_YYYYMMDD_HHMMSS/"
    echo "- 每个货架的测试数据"
    echo "- metrics.json (详细指标)"
    echo "- test_summary.txt (测试摘要)"
    echo "- final_report.txt (最终报告)"
    echo ""
    echo "【故障排除 / Troubleshooting】"
    echo ""
    echo "如果测试失败或卡住:"
    echo "1. 检查ROS进程: ps aux | grep ros"
    echo "2. 清理进程: killall -9 roslaunch rosmaster roscore"
    echo "3. 查看日志: test_results/MODEL_*/test_*/logs/"
    echo ""
    echo "【更多信息 / More Information】"
    echo ""
    echo "查看完整文档: test/scripts/AUTOMATED_TESTING_README.md"
    echo ""
    read -p "按Enter键返回菜单 / Press Enter to return to menu..."
}

while true; do
    show_menu
    read choice

    case $choice in
        1)
            run_test "./test/scripts/run_automated_test.py --model tube_mpc" \
                     "Tube MPC - 所有货架（基线测试）"
            ;;
        2)
            run_test "./test/scripts/run_automated_test.py --model tube_mpc --shelves 3" \
                     "Tube MPC - 3个货架（快速测试）"
            ;;
        3)
            run_test "./test/scripts/run_automated_test.py --model tube_mpc --shelves 5 --no-gazebo" \
                     "Tube MPC - 无Gazebo（极速测试）"
            ;;
        4)
            run_test "./test/scripts/run_automated_test.py --model stl" \
                     "STL监控 - 所有货架"
            ;;
        5)
            run_test "./test/scripts/run_automated_test.py --model stl --shelves 3" \
                     "STL监控 - 3个货架"
            ;;
        6)
            run_test "./test/scripts/run_automated_test.py --model dr" \
                     "DR约束 - 所有货架"
            ;;
        7)
            run_test "./test/scripts/run_automated_test.py --model dr --shelves 3" \
                     "DR约束 - 3个货架"
            ;;
        8)
            run_test "./test/scripts/run_automated_test.py --model safe_regret" \
                     "Safe-Regret MPC - 所有货架（完整功能）"
            ;;
        9)
            run_test "./test/scripts/run_automated_test.py --model safe_regret --shelves 3" \
                     "Safe-Regret MPC - 3个货架"
            ;;
        a|A)
            echo ""
            echo "自定义Python测试 / Custom Python Test"
            echo "======================================"
            echo "请输入完整的测试命令 / Enter full test command:"
            echo "示例: ./test/scripts/run_automated_test.py --model tube_mpc --shelves 2"
            echo ""
            read -p "命令 / Command: " custom_cmd
            if [ -n "$custom_cmd" ]; then
                run_test "$custom_cmd" "自定义测试"
            fi
            ;;
        b|B)
            echo ""
            echo "自定义Bash测试 / Custom Bash Test"
            echo "=================================="
            echo "请输入完整的测试命令 / Enter full test command:"
            echo "示例: ./test/scripts/automated_baseline_test.sh --model tube_mpc --shelves 2"
            echo ""
            read -p "命令 / Command: " custom_cmd
            if [ -n "$custom_cmd" ]; then
                run_test "$custom_cmd" "自定义测试"
            fi
            ;;
        h|H)
            show_help
            ;;
        q|Q)
            echo ""
            echo "退出测试系统 / Exiting Test System"
            echo "感谢使用！/ Thank you!"
            echo ""
            exit 0
            ;;
        *)
            echo ""
            echo "无效选择，请重试 / Invalid choice, please try again"
            sleep 2
            ;;
    esac
done
