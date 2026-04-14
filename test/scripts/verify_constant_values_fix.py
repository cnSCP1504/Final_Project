#!/usr/bin/env python3
"""
验证Manuscript Metrics恒定值修复效果
检查tracking_error_history和DR margins是否正常工作
"""
import json
import sys
from pathlib import Path

def check_latest_test():
    """检查最新测试结果的修复效果"""
    # 查找最新测试目录
    results_dir = Path('/home/dixon/Final_Project/catkin/test_results')
    test_dirs = sorted(results_dir.glob('safe_regret_*'), reverse=True)

    if not test_dirs:
        print("❌ 未找到测试结果目录")
        return False

    latest_dir = test_dirs[0]
    print(f"检查测试目录: {latest_dir}")
    print()

    # 检查第一个测试
    test_dir = latest_dir / "test_01_shelf_01"
    metrics_file = test_dir / "metrics.json"

    if not metrics_file.exists():
        print(f"❌ Metrics文件未找到: {metrics_file}")
        return False

    with open(metrics_file, 'r') as f:
        data = json.load(f)

    raw = data.get('manuscript_raw_data', {})

    print("=" * 70)
    print("Manuscript Metrics修复效果检查")
    print("=" * 70)
    print()

    # 1. DR Margins检查
    print("1️⃣  DR Margins检查")
    print("-" * 70)
    dr_hist = raw.get('dr_margins_history', [])
    if len(dr_hist) == 0:
        print("  ❌ DR margins数据为空")
        dr_ok = False
    else:
        dr_unique = len(set(dr_hist))
        dr_min = min(dr_hist)
        dr_max = max(dr_hist)

        print(f"  样本数: {len(dr_hist)}")
        print(f"  唯一值数: {dr_unique}")
        print(f"  值范围: [{dr_min:.6f}, {dr_max:.6f}]")

        if dr_unique > 10:
            print(f"  ✅ 修复成功！DR margins动态变化（{dr_unique}个不同值）")
            dr_ok = True
        elif dr_unique > 1:
            print(f"  ⚠️  部分修复：DR有{dr_unique}个不同值，但仍较少")
            dr_ok = True
        else:
            print(f"  ❌ DR margins仍为恒定值: {dr_hist[0]}")
            dr_ok = False

    print()

    # 2. Tracking Error History检查
    print("2️⃣  Tracking Error History检查")
    print("-" * 70)
    te_hist = raw.get('tracking_error_history', [])

    if len(te_hist) == 0:
        print("  ❌ Tracking error history仍为空列表")
        te_ok = False
    else:
        print(f"  样本数: {len(te_hist)}")
        print(f"  ✅ 修复成功！Tracking error history有数据")
        print(f"  样本示例: {te_hist[0]}")
        te_ok = True

    print()

    # 3. STL Budget检查
    print("3️⃣  STL Budget检查")
    print("-" * 70)
    stl_hist = raw.get('stl_budget_history', [])

    if len(stl_hist) == 0:
        print("  ⚠️  STL budget数据为空")
        stl_ok = False
    else:
        stl_unique = len(set(stl_hist))
        stl_val = stl_hist[0]

        print(f"  样本数: {len(stl_hist)}")
        print(f"  唯一值数: {stl_unique}")
        print(f"  值: {stl_val}")

        if stl_unique == 1 and stl_val == 0.0:
            print(f"  ⚠️  STL budget仍为恒定值0.0（需要进一步修复）")
            stl_ok = False
        else:
            print(f"  ✅ STL budget动态变化")
            stl_ok = True

    print()

    # 4. Cost数据检查
    print("4️⃣  Cost数据检查")
    print("-" * 70)
    inst_cost = raw.get('instantaneous_cost', [])
    ref_cost = raw.get('reference_cost', [])

    print(f"  Instantaneous Cost: {len(inst_cost)} samples")
    print(f"  Reference Cost: {len(ref_cost)} samples")

    if len(inst_cost) > 0 and len(ref_cost) > 0:
        print(f"  ✅ Cost数据完整")
        cost_ok = True
    else:
        print(f"  ⚠️  Cost数据缺失（需要添加话题订阅）")
        cost_ok = False

    print()
    print("=" * 70)
    print("修复效果总结")
    print("=" * 70)

    results = {
        'DR Margins': dr_ok,
        'Tracking Error': te_ok,
        'STL Budget': stl_ok,
        'Cost Data': cost_ok
    }

    for name, ok in results.items():
        status = "✅" if ok else "❌" if ok is False else "⚠️"
        print(f"{status} {name}: {'通过' if ok else '失败' if ok is False else '待修复'}")

    print()

    all_ok = all(results.values())
    if all_ok:
        print("🎉 所有修复都成功！")
        return True
    elif dr_ok and te_ok:
        print("✅ 主要修复成功（DR Margins + Tracking Error）")
        return True
    else:
        print("⚠️  部分修复未成功，需要进一步调试")
        return False

if __name__ == '__main__':
    try:
        success = check_latest_test()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n❌ 检查过程出错: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
