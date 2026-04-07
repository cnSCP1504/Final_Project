#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
验证Manuscript Metrics订阅者修复
验证每个测试都能正确收集数据
"""

import json
from pathlib import Path

def check_test_metrics(test_dir):
    """检查单个测试的metrics是否有效"""
    metrics_file = test_dir / "metrics.json"

    if not metrics_file.exists():
        return False, "❌ metrics.json不存在"

    with open(metrics_file, 'r') as f:
        metrics = json.load(f)

    # 🔥 关键修复：检查manuscript_raw_data而不是manuscript_metrics
    raw_data = metrics.get('manuscript_raw_data', {})

    # 1. 检查STL数据
    stl_steps = raw_data.get('stl_total_steps', 0)
    stl_samples = len(raw_data.get('stl_robustness_history', []))

    # 2. 检查DR数据
    dr_samples = len(raw_data.get('dr_margins_history', []))

    # 3. 检查MPC数据
    mpc_solves = raw_data.get('mpc_total_solves', 0)
    mpc_times = len(raw_data.get('mpc_solve_times', []))

    # 4. 检查Tracking数据
    tracking_samples = len(raw_data.get('tracking_error_history', []))

    # 判断是否有效：至少有一个数据源有数据
    has_data = (
        stl_samples > 0 or
        dr_samples > 0 or
        mpc_solves > 0 or
        tracking_samples > 0
    )

    status = "✅ 有效" if has_data else "❌ 无数据"

    info = {
        'status': status,
        'stl_steps': stl_steps,
        'stl_samples': stl_samples,
        'dr_samples': dr_samples,
        'mpc_solves': mpc_solves,
        'mpc_times': mpc_times,
        'tracking_samples': tracking_samples,
        'has_data': has_data
    }

    return has_data, info

def main():
    print("=" * 60)
    print("验证Manuscript Metrics订阅者修复")
    print("=" * 60)

    # 检查最新的测试结果
    test_results_base = Path("/home/dixon/Final_Project/catkin/test_results")

    # 找到最新的测试目录
    test_dirs = sorted(test_results_base.glob("safe_regret_*"), reverse=True)

    if not test_dirs:
        print("❌ 没有找到测试结果")
        return

    latest_test = test_dirs[0]
    print(f"\n检查测试目录: {latest_test.name}")

    # 检查每个子测试
    sub_tests = sorted(latest_test.glob("test_*"))

    if not sub_tests:
        print("❌ 没有找到子测试")
        return

    print(f"\n找到 {len(sub_tests)} 个子测试\n")

    valid_count = 0
    invalid_count = 0

    for test_path in sub_tests:
        test_name = test_path.name
        has_data, info = check_test_metrics(test_path)

        if has_data:
            valid_count += 1
            print(f"✅ {test_name}")
        else:
            invalid_count += 1
            print(f"❌ {test_name}")

        # 打印详细信息
        print(f"   STL: {info['stl_samples']} samples (steps: {info['stl_steps']})")
        print(f"   DR:  {info['dr_samples']} samples")
        print(f"   MPC: {info['mpc_solves']} solves, {info['mpc_times']} times")
        print(f"   Tracking: {info['tracking_samples']} samples")
        print()

    print("=" * 60)
    print(f"总结: {valid_count}/{len(sub_tests)} 个测试有有效数据")
    print("=" * 60)

    if invalid_count > 0:
        print("\n⚠️  仍有测试无数据，可能需要进一步调试")
        return 1
    else:
        print("\n✅ 所有测试都有有效数据，修复成功！")
        return 0

if __name__ == '__main__':
    exit(main())
