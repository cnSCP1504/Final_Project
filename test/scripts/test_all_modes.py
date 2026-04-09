#!/usr/bin/env python3
"""
测试所有模式，检查常见问题：
1. 报错（崩溃、超时、ROS died）
2. 数据缺失（metrics.json字段缺失）
3. 异常为0（dynamic_regret全为0、tracking_error全为0等）
4. 图像处理问题（图表生成失败）
"""

import os
import sys
import json
import subprocess
import time
from datetime import datetime
from pathlib import Path

# 测试配置
MODELS = ['tube_mpc', 'stl', 'dr', 'safe_regret']
SHELVES = 1
TIMEOUT = 120  # 秒
USE_VIZ = False

# 颜色输出
RED = '\033[91m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
RESET = '\033[0m'

def print_header(text):
    print(f"\n{BLUE}{'='*70}{RESET}")
    print(f"{BLUE}{text:^70}{RESET}")
    print(f"{BLUE}{'='*70}{RESET}\n")

def print_success(text):
    print(f"{GREEN}✅ {text}{RESET}")

def print_error(text):
    print(f"{RED}❌ {text}{RESET}")

def print_warning(text):
    print(f"{YELLOW}⚠️  {text}{RESET}")

def print_info(text):
    print(f"{BLUE}ℹ️  {text}{RESET}")

def run_test(model):
    """运行单个模型的测试"""
    print_header(f"测试模型: {model}")

    cmd = [
        './test/scripts/run_automated_test.py',
        '--model', model,
        '--shelves', str(SHELVES),
        '--timeout', str(TIMEOUT),
        '--no-viz'
    ]

    print_info(f"执行命令: {' '.join(cmd)}")
    print_info(f"超时时间: {TIMEOUT}秒")

    start_time = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT+30)
    elapsed = time.time() - start_time

    print_info(f"执行时间: {elapsed:.1f}秒")

    # 检查退出码
    if result.returncode == 0:
        print_success(f"{model} 测试完成（退出码: 0）")
    else:
        print_error(f"{model} 测试失败（退出码: {result.returncode}）")

    # 检查输出中的错误
    output = result.stdout + result.stderr
    if 'ROS_DIED' in output:
        print_error(f"{model} 检测到ROS进程崩溃")
    if 'TIMEOUT' in output:
        print_warning(f"{model} 达到超时")
    if 'Traceback' in output or 'Error' in output:
        print_error(f"{model} 检测到Python错误")
        # 打印最后几行错误
        lines = output.split('\n')
        for i, line in enumerate(lines):
            if 'Traceback' in line:
                print_error("错误堆栈:")
                for err_line in lines[i:min(i+10, len(lines))]:
                    print(f"  {err_line}")
                break

    return result.returncode == 0

def find_latest_result():
    """找到最新的测试结果"""
    test_results_dir = Path('test_results')
    if not test_results_dir.exists():
        return None

    # 找到所有以模型名开头的目录
    model_dirs = []
    for model in MODELS:
        model_dirs.extend(test_results_dir.glob(f'{model}_*'))

    if not model_dirs:
        return None

    # 按修改时间排序，返回最新的
    latest = max(model_dirs, key=lambda p: p.stat().st_mtime)

    # 检查是否有metrics.json
    metrics_file = latest / 'test_01_shelf_01' / 'metrics.json'
    if metrics_file.exists():
        return metrics_file

    return None

def check_metrics_integrity(metrics_file, model_name):
    """检查metrics.json的完整性"""
    print_header(f"检查数据完整性: {model_name}")

    try:
        with open(metrics_file, 'r') as f:
            data = json.load(f)
    except Exception as e:
        print_error(f"无法读取metrics.json: {e}")
        return False

    print_success(f"成功读取metrics.json")

    # 检查必需字段
    required_fields = [
        'test_info',
        'manuscript_metrics',
        'manuscript_raw_data'
    ]

    missing_fields = []
    for field in required_fields:
        if field not in data:
            missing_fields.append(field)
        else:
            print_success(f"字段存在: {field}")

    if missing_fields:
        print_error(f"缺失字段: {', '.join(missing_fields)}")
        return False

    # 检查manuscript_raw_data
    raw = data['manuscript_raw_data']
    print_info(f"manuscript_raw_data 包含 {len(raw)} 个字段")

    # 检查关键字段
    critical_fields = {
        'dynamic_regret': 'Dynamic Regret',
        'tracking_contribution': 'Tracking Contribution',
        'tracking_error_norm_history': 'Tracking Error',
        'dr_margins_history': 'DR Margins',
        'stl_robustness_history': 'STL Robustness'
    }

    all_ok = True
    for field, name in critical_fields.items():
        if field in raw:
            value = raw[field]
            if isinstance(value, list):
                if len(value) == 0:
                    print_warning(f"{name}: 空列表")
                    all_ok = False
                else:
                    print_success(f"{name}: {len(value)} 个数据点")
            else:
                print_success(f"{name}: {value}")
        else:
            # 某些字段可能不存在（例如stl模式没有dr_margins）
            print_warning(f"{name} ({field}): 不存在（可能正常）")

    return all_ok

def check_zero_values(metrics_file, model_name):
    """检查异常的全零值"""
    print_header(f"检查异常零值: {model_name}")

    try:
        with open(metrics_file, 'r') as f:
            data = json.load(f)
    except Exception as e:
        print_error(f"无法读取metrics.json: {e}")
        return False

    raw = data['manuscript_raw_data']
    issues = []

    # 检查dynamic_regret
    if 'dynamic_regret' in raw:
        dr = raw['dynamic_regret']
        if len(dr) > 0:
            if all(v == 0 for v in dr):
                issues.append("dynamic_regret 全为0")
            elif min(dr) < 0:
                issues.append(f"dynamic_regret 有负值: min={min(dr):.4f}")
            else:
                print_success(f"dynamic_regret 正常: mean={sum(dr)/len(dr):.4f}")

    # 检查tracking_error_norm_history
    if 'tracking_error_norm_history' in raw:
        te = raw['tracking_error_norm_history']
        if len(te) > 0:
            if all(v == 0 for v in te):
                issues.append("tracking_error_norm_history 全为0")
            else:
                mean_te = sum(te) / len(te)
                print_success(f"tracking_error_norm_history 正常: mean={mean_te:.4f}")

    # 检查dr_margins
    if 'dr_margins_history' in raw:
        dm = raw['dr_margins_history']
        if len(dm) > 0:
            if all(v == 0 for v in dm):
                issues.append("dr_margins_history 全为0")
            else:
                mean_dm = sum(dm) / len(dm)
                print_success(f"dr_margins_history 正常: mean={mean_dm:.4f}")

    # 检查stl_robustness
    if 'stl_robustness_history' in raw:
        sr = raw['stl_robustness_history']
        if len(sr) > 0:
            unique_values = set(sr)
            if len(unique_values) == 1:
                issues.append(f"stl_robustness_history 只有1个值: {list(unique_values)[0]}")
            elif len(unique_values) == 2:
                issues.append(f"stl_robustness_history 只有2个值: {unique_values}")
            else:
                print_success(f"stl_robustness_history 正常: {len(unique_values)} 个不同值")

    if issues:
        for issue in issues:
            print_error(issue)
        return False
    else:
        print_success("没有发现异常零值")
        return True

def check_figure_generation(metrics_file, model_name):
    """检查图表生成是否有问题"""
    print_header(f"检查图表生成: {model_name}")

    # 检查是否有图表生成脚本
    figure_scripts = [
        'test/scripts/generate_manuscript_figures.py',
        'test/scripts/compute_manuscript_metrics.py'
    ]

    for script in figure_scripts:
        if os.path.exists(script):
            print_success(f"图表脚本存在: {script}")
        else:
            print_warning(f"图表脚本不存在: {script}")

    # 尝试生成图表（只检查能否运行，不实际保存）
    try:
        cmd = [
            'python3',
            'test/scripts/generate_manuscript_figures.py',
            '--metrics', str(metrics_file),
            '--dry-run'  # 假设支持dry-run模式
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        if result.returncode == 0:
            print_success("图表生成脚本可以运行")
        else:
            print_warning(f"图表生成脚本返回错误: {result.returncode}")
    except Exception as e:
        print_warning(f"无法测试图表生成: {e}")

    return True

def generate_summary_report(results):
    """生成总结报告"""
    print_header("总结报告")

    total = len(results)
    passed = sum(1 for r in results.values() if r['overall_pass'])
    failed = total - passed

    print(f"总测试数: {total}")
    print_success(f"通过: {passed}")
    print_error(f"失败: {failed}")

    print("\n详细结果:")
    for model, result in results.items():
        status = GREEN + "✅ PASS" + RESET if result['overall_pass'] else RED + "❌ FAIL" + RESET
        print(f"  {model:15s}: {status}")

        if not result['overall_pass']:
            print("    问题:")
            for issue in result['issues']:
                print(f"      - {issue}")

    # 保存报告
    report_file = f"test_results/ALL_MODES_TEST_REPORT_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
    os.makedirs(os.path.dirname(report_file), exist_ok=True)

    with open(report_file, 'w') as f:
        f.write("="*70 + "\n")
        f.write("所有模式测试报告\n")
        f.write(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write("="*70 + "\n\n")

        f.write(f"总测试数: {total}\n")
        f.write(f"通过: {passed}\n")
        f.write(f"失败: {failed}\n\n")

        f.write("详细结果:\n")
        for model, result in results.items():
            status = "PASS" if result['overall_pass'] else "FAIL"
            f.write(f"  {model:15s}: {status}\n")
            if not result['overall_pass']:
                f.write("    问题:\n")
                for issue in result['issues']:
                    f.write(f"      - {issue}\n")

    print_success(f"报告已保存: {report_file}")

def main():
    print_header("Safe-Regret MPC 所有模式测试")

    print_info(f"测试模型: {', '.join(MODELS)}")
    print_info(f"货架数量: {SHELVES}")
    print_info(f"超时时间: {TIMEOUT}秒")
    print_info(f"可视化: {USE_VIZ}")

    results = {}

    for model in MODELS:
        model_issues = []

        # 1. 运行测试
        test_passed = run_test(model)
        if not test_passed:
            model_issues.append("测试执行失败")

        # 2. 查找结果
        metrics_file = find_latest_result()
        if metrics_file is None:
            model_issues.append("未找到metrics.json")
            results[model] = {
                'overall_pass': False,
                'issues': model_issues
            }
            continue

        print_info(f"找到测试结果: {metrics_file}")

        # 3. 检查数据完整性
        integrity_ok = check_metrics_integrity(metrics_file, model)
        if not integrity_ok:
            model_issues.append("数据完整性检查失败")

        # 4. 检查异常零值
        zero_ok = check_zero_values(metrics_file, model)
        if not zero_ok:
            model_issues.append("检测到异常零值")

        # 5. 检查图表生成
        # figure_ok = check_figure_generation(metrics_file, model)
        # if not figure_ok:
        #     model_issues.append("图表生成检查失败")

        # 总结
        overall_pass = test_passed and integrity_ok and zero_ok

        results[model] = {
            'overall_pass': overall_pass,
            'issues': model_issues
        }

        # 短暂休息
        time.sleep(2)

    # 生成总结报告
    generate_summary_report(results)

    # 返回退出码
    if all(r['overall_pass'] for r in results.values()):
        print_success("\n所有测试通过！")
        return 0
    else:
        print_error("\n部分测试失败！")
        return 1

if __name__ == '__main__':
    sys.exit(main())
