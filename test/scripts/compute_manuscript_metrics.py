#!/usr/bin/env python3
"""
Manuscript Metrics Calculator
计算manuscript中定义的所有性能指标

Usage:
    python3 compute_manuscript_metrics.py --input <results_dir> --output <output.json>
"""

import json
import numpy as np
import argparse
from pathlib import Path
from typing import Dict, List, Tuple
from scipy import stats

class ManuscriptMetricsCalculator:
    """Manuscript指标计算器 - 实现7类核心指标"""

    def __init__(self, tube_radius: float = 0.18, target_delta: float = 0.05):
        self.tube_radius = tube_radius
        self.target_delta = target_delta

    def load_test_data(self, results_dir: Path) -> List[Dict]:
        """加载所有测试的metrics.json文件"""
        all_tests = []

        for test_dir in sorted(results_dir.glob("test_*")):
            metrics_file = test_dir / "metrics.json"
            if metrics_file.exists():
                with open(metrics_file, 'r') as f:
                    test_data = json.load(f)
                    all_tests.append(test_data)
                    print(f"  ✓ Loaded: {metrics_file.name}")

        return all_tests

    def compute_all_metrics(self, all_tests: List[Dict]) -> Dict:
        """计算所有7类指标"""
        results = {
            'metadata': {
                'total_tests': len(all_tests),
                'tube_radius': self.tube_radius,
                'target_delta': self.target_delta
            }
        }

        print("\n=== Computing Manuscript Metrics ===")

        # 1. Satisfaction Probability
        print("[1/7] Satisfaction Probability...")
        results['satisfaction_probability'] = self._compute_satisfaction_probability(all_tests)

        # 2. Empirical Risk
        print("[2/7] Empirical Risk...")
        results['empirical_risk'] = self._compute_empirical_risk(all_tests)

        # 3. Regret Metrics
        print("[3/7] Regret Metrics...")
        results['regret'] = self._compute_regret_metrics(all_tests)

        # 4. Feasibility Rate
        print("[4/7] Feasibility Rate...")
        results['feasibility'] = self._compute_feasibility_rate(all_tests)

        # 5. Computation Metrics
        print("[5/7] Computation Metrics...")
        results['computation'] = self._compute_computation_metrics(all_tests)

        # 6. Tracking Error
        print("[6/7] Tracking Error & Tube Occupancy...")
        results['tracking'] = self._compute_tracking_metrics(all_tests)

        # 7. Calibration
        print("[7/7] Calibration Accuracy...")
        results['calibration'] = self._compute_calibration_metrics(all_tests)

        return results

    def _compute_satisfaction_probability(self, all_tests: List[Dict]) -> Dict:
        """1. Satisfaction Probability with Wilson 95% CI"""
        # 统计满足的测试数（最终STL robustness >= 0）
        satisfied = 0
        for test in all_tests:
            robustness_history = test.get('stl_robustness_history', [])
            if robustness_history:
                final_robustness = robustness_history[-1]
                if final_robustness >= 0:
                    satisfied += 1

        total = len(all_tests)
        p_sat = satisfied / total if total > 0 else 0

        # Wilson 95%置信区间
        z = 1.96
        denom = 1 + z**2 / total
        center = (p_sat + z**2 / (2 * total)) / denom
        margin = z * np.sqrt(p_sat * (1 - p_sat) / total +
                            z**2 / (4 * total**2)) / denom

        ci_lower = max(0, center - margin)
        ci_upper = min(1, center + margin)

        result = {
            'p_sat': p_sat,
            'ci_lower': ci_lower,
            'ci_upper': ci_upper,
            'satisfied_count': satisfied,
            'total_count': total
        }

        print(f"    p_sat = {p_sat:.3f} [{ci_lower:.3f}, {ci_upper:.3f}]")
        return result

    def _compute_empirical_risk(self, all_tests: List[Dict]) -> Dict:
        """2. Empirical Risk (安全约束违反率)"""
        total_violations = sum(test.get('safety_violation_count', 0)
                             for test in all_tests)
        total_steps = sum(test.get('safety_total_steps', 0)
                         for test in all_tests)

        delta_hat = total_violations / total_steps if total_steps > 0 else 0
        calibration_error = abs(delta_hat - self.target_delta)
        relative_error = calibration_error / self.target_delta if self.target_delta > 0 else 0

        result = {
            'observed_delta': delta_hat,
            'target_delta': self.target_delta,
            'calibration_error': calibration_error,
            'relative_error': relative_error,
            'total_violations': total_violations,
            'total_steps': total_steps
        }

        print(f"    δ̂ = {delta_hat:.4f} (target: {self.target_delta:.2f})")
        print(f"    Calibration error = {calibration_error:.4f}")
        return result

    def _compute_regret_metrics(self, all_tests: List[Dict]) -> Dict:
        """3. Dynamic & Safe Regret"""
        all_regrets = []
        all_costs = []

        for test in all_tests:
            regrets = test.get('dynamic_regret', [])
            costs = test.get('instantaneous_cost', [])
            all_regrets.extend(regrets)
            all_costs.extend(costs)

        if not all_costs:
            return {
                'cumulative_regret': 0,
                'normalized_regret': 0,
                'mean_regret': 0,
                'std_regret': 0,
                'total_steps': 0
            }

        T = len(all_costs)
        cumulative_regret = np.sum(all_regrets)
        normalized_regret = cumulative_regret / T if T > 0 else 0

        result = {
            'cumulative_regret': float(cumulative_regret),
            'normalized_regret': float(normalized_regret),
            'mean_regret': float(np.mean(all_regrets)) if all_regrets else 0,
            'std_regret': float(np.std(all_regrets)) if all_regrets else 0,
            'total_steps': T
        }

        print(f"    R_T/T = {normalized_regret:.4f}")
        return result

    def _compute_feasibility_rate(self, all_tests: List[Dict]) -> Dict:
        """4. Recursive Feasibility Rate"""
        total_feasible = sum(test.get('mpc_feasible_count', 0)
                            for test in all_tests)
        total_solves = sum(test.get('mpc_total_solves', 0)
                          for test in all_tests)
        total_infeasible = sum(test.get('mpc_infeasible_count', 0)
                              for test in all_tests)

        rate = total_feasible / total_solves if total_solves > 0 else 0

        result = {
            'feasibility_rate': rate,
            'feasible_count': total_feasible,
            'infeasible_count': total_infeasible,
            'total_solves': total_solves
        }

        print(f"    Feasibility rate = {rate*100:.2f}%")
        return result

    def _compute_computation_metrics(self, all_tests: List[Dict]) -> Dict:
        """5. Computation Metrics (solve time statistics)"""
        all_times = []
        total_failures = 0

        for test in all_tests:
            times = test.get('mpc_solve_times', [])
            all_times.extend(times)
            total_failures += test.get('mpc_failure_count', 0)

        if not all_times:
            return {
                'median_ms': 0,
                'mean_ms': 0,
                'std_ms': 0,
                'p90_ms': 0,
                'p95_ms': 0,
                'p99_ms': 0,
                'min_ms': 0,
                'max_ms': 0,
                'failure_count': total_failures,
                'total_solves': 0
            }

        result = {
            'median_ms': float(np.median(all_times)),
            'mean_ms': float(np.mean(all_times)),
            'std_ms': float(np.std(all_times)),
            'p90_ms': float(np.percentile(all_times, 90)),
            'p95_ms': float(np.percentile(all_times, 95)),
            'p99_ms': float(np.percentile(all_times, 99)),
            'min_ms': float(np.min(all_times)),
            'max_ms': float(np.max(all_times)),
            'failure_count': total_failures,
            'total_solves': len(all_times)
        }

        print(f"    Median = {result['median_ms']:.2f} ms, P90 = {result['p90_ms']:.2f} ms")
        return result

    def _compute_tracking_metrics(self, all_tests: List[Dict]) -> Dict:
        """6. Tracking Error & Tube Occupancy"""
        all_errors = []
        in_tube_count = 0
        total_steps = 0

        for test in all_tests:
            errors = test.get('tracking_error_norm_history', [])
            all_errors.extend(errors)

            for e in errors:
                if e <= self.tube_radius:
                    in_tube_count += 1
                total_steps += 1

        if not all_errors:
            return {
                'mean_error_m': 0,
                'std_error_m': 0,
                'rmse_m': 0,
                'max_error_m': 0,
                'min_error_m': 0,
                'occupancy_rate': 0,
                'in_tube_count': 0,
                'total_steps': 0
            }

        result = {
            'mean_error_m': float(np.mean(all_errors)),
            'std_error_m': float(np.std(all_errors)),
            'rmse_m': float(np.sqrt(np.mean(np.array(all_errors)**2))),
            'max_error_m': float(np.max(all_errors)),
            'min_error_m': float(np.min(all_errors)),
            'occupancy_rate': in_tube_count / total_steps if total_steps > 0 else 0,
            'in_tube_count': in_tube_count,
            'total_steps': total_steps
        }

        print(f"    Mean error = {result['mean_error_m']:.4f} m")
        print(f"    Tube occupancy = {result['occupancy_rate']*100:.2f}%")
        return result

    def _compute_calibration_metrics(self, all_tests: List[Dict]) -> Dict:
        """7. Calibration Accuracy"""
        total_violations = sum(test.get('safety_violation_count', 0)
                             for test in all_tests)
        total_steps = sum(test.get('safety_total_steps', 0)
                         for test in all_tests)

        observed = total_violations / total_steps if total_steps > 0 else 0
        calibration_error = abs(observed - self.target_delta)
        relative_error = calibration_error / self.target_delta if self.target_delta > 0 else 0

        result = {
            'target_delta': self.target_delta,
            'observed_delta': observed,
            'calibration_error': calibration_error,
            'relative_error': relative_error,
            'is_well_calibrated': calibration_error < 0.01  # 1% tolerance
        }

        print(f"    Calibration error = {calibration_error:.4f}")
        print(f"    Well calibrated? {result['is_well_calibrated']}")
        return result


def main():
    parser = argparse.ArgumentParser(
        description='Compute Manuscript Metrics for Safe-Regret MPC',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 compute_manuscript_metrics.py \\
      --input test_results/safe_regret_20260408_120104 \\
      --output test_results/safe_regret_20260408_120104/aggregated_metrics.json

  python3 compute_manuscript_metrics.py \\
      --input test_results/tube_mpc_20260408_120104 \\
      --output aggregated_metrics_tube_mpc.json
        """
    )

    parser.add_argument('--input', '-i', required=True,
                       help='Input results directory (containing test_XX folders)')
    parser.add_argument('--output', '-o', required=True,
                       help='Output JSON file path')
    parser.add_argument('--tube-radius', type=float, default=0.18,
                       help='Tube radius in meters (default: 0.18)')
    parser.add_argument('--target-delta', type=float, default=0.05,
                       help='Target risk level (default: 0.05)')

    args = parser.parse_args()

    # 检查输入目录
    input_dir = Path(args.input)
    if not input_dir.exists():
        print(f"❌ Error: Input directory does not exist: {input_dir}")
        return 1

    print("="*70)
    print("Manuscript Metrics Calculator")
    print("="*70)
    print(f"\nInput directory: {input_dir}")
    print(f"Output file: {args.output}")
    print(f"Tube radius: {args.tube_radius} m")
    print(f"Target delta: {args.target_delta}")
    print()

    # 初始化计算器
    calculator = ManuscriptMetricsCalculator(
        tube_radius=args.tube_radius,
        target_delta=args.target_delta
    )

    # 加载数据
    print("Loading test data...")
    all_tests = calculator.load_test_data(input_dir)

    if not all_tests:
        print(f"❌ Error: No test data found in {input_dir}")
        print("   Expected structure: {input_dir}/test_XX/metrics.json")
        return 1

    print(f"\n✓ Loaded {len(all_tests)} test results\n")

    # 计算所有指标
    metrics = calculator.compute_all_metrics(all_tests)

    # 保存结果
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w') as f:
        json.dump(metrics, f, indent=2)

    print("\n" + "="*70)
    print("✅ Metrics saved successfully!")
    print("="*70)
    print(f"\nOutput file: {output_path}\n")

    # 打印摘要
    print("=== Metrics Summary ===\n")

    print("1. Satisfaction Probability:")
    p = metrics['satisfaction_probability']
    print(f"   {p['p_sat']:.1f}% [{p['ci_lower']:.1f}%, {p['ci_upper']:.1f}%]")
    print(f"   ({p['satisfied_count']}/{p['total_count']} tests satisfied)\n")

    print("2. Empirical Risk:")
    r = metrics['empirical_risk']
    print(f"   Observed: {r['observed_delta']:.4f}")
    print(f"   Target:   {r['target_delta']:.2f}")
    print(f"   Error:    {r['calibration_error']:.4f}\n")

    print("3. Regret:")
    reg = metrics['regret']
    print(f"   Normalized: {reg['normalized_regret']:.4f}\n")

    print("4. Feasibility:")
    f = metrics['feasibility']
    print(f"   Rate: {f['feasibility_rate']*100:.2f}%")
    print(f"   ({f['feasible_count']}/{f['total_solves']} solves feasible)\n")

    print("5. Computation:")
    c = metrics['computation']
    print(f"   Median: {c['median_ms']:.2f} ms")
    print(f"   P90:    {c['p90_ms']:.2f} ms")
    print(f"   P95:    {c['p95_ms']:.2f} ms")
    print(f"   Mean:   {c['mean_ms']:.2f} ms\n")

    print("6. Tracking Error:")
    t = metrics['tracking']
    print(f"   Mean:   {t['mean_error_m']:.4f} m")
    print(f"   RMSE:   {t['rmse_m']:.4f} m")
    print(f"   Max:    {t['max_error_m']:.4f} m")
    print(f"   Tube:   {t['occupancy_rate']*100:.2f}%\n")

    print("7. Calibration:")
    cal = metrics['calibration']
    print(f"   Error: {cal['calibration_error']:.4f}")
    print(f"   Well calibrated? {'✓ Yes' if cal['is_well_calibrated'] else '✗ No'}\n")

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
