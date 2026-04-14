#!/usr/bin/env python3
"""
测试ManuscriptMetricsCollector修复

验证每次测试都能正确收集数据
"""

import sys
import os
import tempfile
from pathlib import Path

# 添加scripts目录到path
sys.path.insert(0, str(Path(__file__).parent))

from manuscript_metrics_collector import ManuscriptMetricsCollector

def test_collector_reset():
    """测试收集器重置功能"""
    print("=" * 70)
    print("测试：ManuscriptMetricsCollector重置功能")
    print("=" * 70)

    # 创建临时目录
    with tempfile.TemporaryDirectory() as tmpdir:
        print(f"临时目录: {tmpdir}")

        # 创建收集器
        collector = ManuscriptMetricsCollector(tmpdir)
        print(f"✓ 收集器已创建")
        print(f"  Output dir: {collector.output_dir}")

        # 模拟第一次测试数据
        print("\n=== 测试1：添加数据 ===")
        collector.metrics_data.append({
            'timestamp': 1.0,
            'seq': 0,
            'stl_robustness': 0.5,
            'stl_budget': 1.0,
            'stl_baseline': 0.1,
            'safety_margin': 0.2,
            'safety_violation': 0,
            'dr_margin': 0.15,
            'dynamic_regret': 0.0,
            'safe_regret': 0.0,
            'reference_cost': 10.0,
            'actual_cost': 10.5,
            'mpc_feasible': 1,
            'mpc_solve_time': 10.0,
            'mpc_objective': 5.0,
            'tracking_error_norm': 0.1,
            'cte': 0.05,
            'etheta': 0.05,
            'tube_occupied': 0,
            'linear_vel': 0.5,
            'angular_vel': 0.0
        })
        collector.metrics_data.append({
            'timestamp': 2.0,
            'seq': 1,
            'stl_robustness': 0.6,
            'stl_budget': 0.9,
            'stl_baseline': 0.1,
            'safety_margin': 0.25,
            'safety_violation': 0,
            'dr_margin': 0.15,
            'dynamic_regret': 0.0,
            'safe_regret': 0.0,
            'reference_cost': 10.0,
            'actual_cost': 10.3,
            'mpc_feasible': 1,
            'mpc_solve_time': 12.0,
            'mpc_objective': 4.8,
            'tracking_error_norm': 0.15,
            'cte': 0.08,
            'etheta': 0.07,
            'tube_occupied': 0,
            'linear_vel': 0.5,
            'angular_vel': 0.1
        })
        print(f"  测试1数据量: {len(collector.metrics_data)}")

        # 计算指标
        metrics1 = collector.compute_final_metrics()
        print(f"  测试1指标: {metrics1['task_info']['total_snapshots']} snapshots")

        # 重置收集器
        print("\n=== 重置收集器 ===")
        collector.reset()
        print(f"  重置后数据量: {len(collector.metrics_data)}")

        # 模拟第二次测试数据
        print("\n=== 测试2：添加数据 ===")
        collector.metrics_data.append({
            'timestamp': 10.0,
            'seq': 0,
            'stl_robustness': 0.7,
            'stl_budget': 1.0,
            'stl_baseline': 0.1,
            'safety_margin': 0.22,
            'safety_violation': 0,
            'dr_margin': 0.15,
            'dynamic_regret': 0.0,
            'safe_regret': 0.0,
            'reference_cost': 10.0,
            'actual_cost': 10.4,
            'mpc_feasible': 1,
            'mpc_solve_time': 11.0,
            'mpc_objective': 4.9,
            'tracking_error_norm': 0.12,
            'cte': 0.06,
            'etheta': 0.06,
            'tube_occupied': 0,
            'linear_vel': 0.6,
            'angular_vel': 0.0
        })
        collector.metrics_data.append({
            'timestamp': 11.0,
            'seq': 1,
            'stl_robustness': 0.8,
            'stl_budget': 0.95,
            'stl_baseline': 0.1,
            'safety_margin': 0.28,
            'safety_violation': 0,
            'dr_margin': 0.15,
            'dynamic_regret': 0.0,
            'safe_regret': 0.0,
            'reference_cost': 10.0,
            'actual_cost': 10.2,
            'mpc_feasible': 1,
            'mpc_solve_time': 13.0,
            'mpc_objective': 4.7,
            'tracking_error_norm': 0.18,
            'cte': 0.09,
            'etheta': 0.09,
            'tube_occupied': 0,
            'linear_vel': 0.6,
            'angular_vel': 0.1
        })
        collector.metrics_data.append({
            'timestamp': 12.0,
            'seq': 2,
            'stl_robustness': 0.9,
            'stl_budget': 0.9,
            'stl_baseline': 0.1,
            'safety_margin': 0.30,
            'safety_violation': 0,
            'dr_margin': 0.15,
            'dynamic_regret': 0.0,
            'safe_regret': 0.0,
            'reference_cost': 10.0,
            'actual_cost': 10.1,
            'mpc_feasible': 1,
            'mpc_solve_time': 14.0,
            'mpc_objective': 4.6,
            'tracking_error_norm': 0.20,
            'cte': 0.10,
            'etheta': 0.10,
            'tube_occupied': 0,
            'linear_vel': 0.6,
            'angular_vel': 0.2
        })
        print(f"  测试2数据量: {len(collector.metrics_data)}")

        # 计算指标
        metrics2 = collector.compute_final_metrics()
        print(f"  测试2指标: {metrics2['task_info']['total_snapshots']} snapshots")

        # 验证结果
        print("\n=== 验证结果 ===")
        assert len(collector.metrics_data) == 3, f"测试2应该有3个数据点，实际: {len(collector.metrics_data)}"
        assert metrics2['task_info']['total_snapshots'] == 3, "指标应该显示3个快照"
        print("✓ 测试2数据独立于测试1（没有混在一起）")

        # 测试get_raw_data
        raw_data = collector.get_raw_data()
        assert raw_data['total_snapshots'] == 3, "raw_data应该有3个快照"
        assert len(raw_data['data']) == 3, "raw_data data列表应该有3个元素"
        print("✓ get_raw_data()工作正常")

        print("\n" + "=" * 70)
        print("✅ 所有测试通过！")
        print("=" * 70)

if __name__ == '__main__':
    test_collector_reset()
