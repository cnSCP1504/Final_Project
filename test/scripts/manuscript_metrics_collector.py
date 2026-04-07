#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Manuscript Metrics Collector for Safe-Regret MPC

收集论文manuscript Section VI.B中定义的性能指标：
1. Satisfaction Metrics (Section VI.B.1)
2. Safety Metrics (Section VI.B.2)
3. Regret Metrics (Section VI.B.3)
4. Feasibility & Computation (Section VI.B.4)
5. Tracking Performance (Section VI.B.5)
"""

import rospy
import json
import csv
from datetime import datetime
from pathlib import Path
import numpy as np

try:
    from safe_regret_mpc.msg import SafeRegretMetrics
except ImportError:
    print("Warning: safe_regret_mpc/SafeRegretMetrics message not found")
    SafeRegretMetrics = None

class ManuscriptMetricsCollector:
    """
    论文性能指标收集器

    收集manuscript中定义的所有关键metrics，用于论文实验评估
    """

    def __init__(self, output_dir):
        self.output_dir = Path(output_dir)
        self.metrics_data = []
        self.start_time = None
        self.subscriber = None

        if SafeRegretMetrics is None:
            print("⚠️  SafeRegretMetrics message type not available")
            return

        # 订阅metrics话题
        self.subscriber = rospy.Subscriber(
            '/safe_regret_mpc/metrics',
            SafeRegretMetrics,
            self.metrics_callback
        )

        print("✓ ManuscriptMetricsCollector initialized")
        print(f"  Output directory: {self.output_dir}")
        print(f"  Subscribed to: /safe_regret_mpc/metrics")

    def metrics_callback(self, msg):
        """接收metrics消息并存储"""
        snapshot = {
            # Timestamp
            'timestamp': msg.header.stamp.to_sec() if msg.header.stamp else 0.0,
            'seq': msg.header.seq if msg.header.stamp else len(self.metrics_data),

            # ========== Section VI.B.1: Satisfaction Metrics ==========
            'stl_robustness': msg.stl_robustness,           # ρ(φ; x_k)
            'stl_budget': msg.stl_robustness_budget,        # R_k
            'stl_baseline': msg.stl_robustness_baseline,    # r̲

            # ========== Section VI.B.2: Safety Metrics ==========
            'safety_margin': msg.safety_margin,             # h(x) - σ
            'safety_violation': msg.safety_violation,       # 1 if h(x) < 0
            'dr_margin': msg.dr_margin if hasattr(msg, 'dr_margin') else 0.0,  # σ_{k,t}

            # ========== Section VI.B.3: Regret Metrics ==========
            'dynamic_regret': msg.dynamic_regret if hasattr(msg, 'dynamic_regret') else 0.0,
            'safe_regret': msg.safe_regret if hasattr(msg, 'safe_regret') else 0.0,
            'reference_cost': msg.reference_cost if hasattr(msg, 'reference_cost') else 0.0,
            'actual_cost': msg.actual_cost if hasattr(msg, 'actual_cost') else 0.0,

            # ========== Section VI.B.4: Computation Metrics ==========
            'mpc_feasible': msg.mpc_feasible,
            'mpc_solve_time': msg.mpc_solve_time,           # ms
            'mpc_objective': msg.mpc_objective_value,

            # ========== Section VI.B.5: Tracking Metrics ==========
            'tracking_error_norm': msg.tracking_error_norm,  # ‖e_t‖
            'cte': msg.tracking_error_cte,                   # Cross-track error
            'etheta': msg.tracking_error_etheta,             # Heading error
            'tube_occupied': msg.tube_occupied,

            # Control inputs
            'linear_vel': msg.linear_velocity,
            'angular_vel': msg.angular_velocity,
        }

        self.metrics_data.append(snapshot)

        # 每100个快照打印一次
        if len(self.metrics_data) % 100 == 0:
            print(f"  📊 Collected {len(self.metrics_data)} snapshots")

    def compute_manuscript_metrics(self):
        """计算论文metrics摘要"""

        if not self.metrics_data:
            print("⚠️  No metrics data collected!")
            return None

        n = len(self.metrics_data)

        # 提取各项指标
        stl_robustness = [d['stl_robustness'] for d in self.metrics_data]
        stl_budget = [d['stl_budget'] for d in self.metrics_data]
        safety_violations = sum(1 for d in self.metrics_data if d['safety_violation'])
        mpc_feasible = sum(1 for d in self.metrics_data if d['mpc_feasible'])
        tracking_errors = [d['tracking_error_norm'] for d in self.metrics_data]
        solve_times = [d['mpc_solve_time'] for d in self.metrics_data if d['mpc_solve_time'] > 0]

        # ========== 论文Metrics摘要 ==========
        summary = {
            # Task info
            'task_info': {
                'collection_date': datetime.now().isoformat(),
                'total_snapshots': n,
                'duration_seconds': self.metrics_data[-1]['timestamp'] - self.metrics_data[0]['timestamp'] if n > 0 else 0
            },

            # ========== Section VI.B.1: Satisfaction Metrics ==========
            'satisfaction_metrics': {
                'stl_robustness_mean': np.mean(stl_robustness),
                'stl_robustness_std': np.std(stl_robustness),
                'stl_robustness_min': np.min(stl_robustness),
                'stl_robustness_max': np.max(stl_robustness),
                'stl_budget_final': stl_budget[-1] if stl_budget else 0.0,
                'satisfaction_probability': sum(1 for r in stl_robustness if r >= 0) / n,
            },

            # ========== Section VI.B.2: Safety Metrics ==========
            'safety_metrics': {
                'empirical_risk': safety_violations / n,      # ĉ_n
                'target_risk_delta': 0.1,                     # δ = 10%
                'safety_violations': safety_violations,
                'safety_violation_rate': safety_violations / n,
            },

            # ========== Section VI.B.3: Regret Metrics ==========
            'regret_metrics': {
                'dynamic_regret_final': self.metrics_data[-1]['dynamic_regret'],
                'safe_regret_final': self.metrics_data[-1]['safe_regret'],
                'regret_normalized': self.metrics_data[-1]['safe_regret'] / n if n > 0 else 0.0,
            },

            # ========== Section VI.B.4: Computation Metrics ==========
            'computation_metrics': {
                'mpc_feasibility_rate': mpc_feasible / n,
                'total_solves': n,
                'feasible_solves': mpc_feasible,
                'solve_time_mean': np.mean(solve_times) if solve_times else 0,
                'solve_time_median': np.median(solve_times) if solve_times else 0,
                'solve_time_p90': np.percentile(solve_times, 90) if solve_times else 0,
                'solve_time_max': np.max(solve_times) if solve_times else 0,
            },

            # ========== Section VI.B.5: Tracking Metrics ==========
            'tracking_metrics': {
                'tracking_error_mean': np.mean(tracking_errors),
                'tracking_error_std': np.std(tracking_errors),
                'tracking_error_max': np.max(tracking_errors),
                'tube_occupancy_rate': sum(1 for d in self.metrics_data if d['tube_occupied']) / n,
            }
        }

        return summary

    def save_manuscript_data(self):
        """保存论文数据到文件"""

        if not self.metrics_data:
            print("⚠️  No metrics data to save!")
            return False

        # 1. 保存时间序列数据（CSV）
        csv_file = self.output_dir / "manuscript_time_series.csv"
        with open(csv_file, 'w', newline='') as f:
            fieldnames = self.metrics_data[0].keys()
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(self.metrics_data)
        print(f"✓ Saved time series: {csv_file}")

        # 2. 保存摘要统计（JSON）
        summary = self.compute_manuscript_metrics()
        json_file = self.output_dir / "manuscript_summary.json"
        with open(json_file, 'w') as f:
            json.dump(summary, f, indent=2)
        print(f"✓ Saved summary: {json_file}")

        # 3. 生成论文表格（LaTeX格式）
        self._generate_latex_tables(summary)

        return True

    def _generate_latex_tables(self, summary):
        """生成论文用的LaTeX表格"""

        # Table 1: Overall Performance
        latex_table = f"""
% ========== Table 1: Overall Performance ==========
\\begin{{table}}[h]
\\centering
\\caption{{Safe-Regret MPC Performance Summary}}
\\label{{tab:performance}}
\\begin{{tabular}}{{lr}}
\\toprule
Metric & Value \\\\
\\midrule
Satisfaction Probability & {summary['satisfaction_metrics']['satisfaction_probability']:.3f} \\\\
Empirical Risk $\\hat{{\\delta}}_n$ & {summary['safety_metrics']['empirical_risk']:.3f} \\\\
Target Risk $\\delta$ & {summary['safety_metrics']['target_risk_delta']:.1f} \\\\
MPC Feasibility Rate & {summary['computation_metrics']['mpc_feasibility_rate']:.3f} \\\\
Mean Tracking Error & {summary['tracking_metrics']['tracking_error_mean']:.3f} m \\\\
\\bottomrule
\\end{{tabular}}
\\end{{table}}
"""

        latex_file = self.output_dir / "manuscript_tables.tex"
        with open(latex_file, 'w') as f:
            f.write(latex_table)
        print(f"✓ Saved LaTeX tables: {latex_file}")

    def print_summary(self):
        """打印摘要到控制台"""
        summary = self.compute_manuscript_metrics()

        if not summary:
            return

        print("\n" + "=" * 80)
        print("📊 Manuscript Metrics Summary")
        print("=" * 80)

        print(f"\n📈 Satisfaction Metrics (Section VI.B.1):")
        print(f"  STL Robustness: μ={summary['satisfaction_metrics']['stl_robustness_mean']:.3f}, "
              f"σ={summary['satisfaction_metrics']['stl_robustness_std']:.3f}")
        print(f"  Satisfaction Prob: {summary['satisfaction_metrics']['satisfaction_probability']:.3f}")

        print(f"\n🛡️  Safety Metrics (Section VI.B.2):")
        print(f"  Empirical Risk: {summary['safety_metrics']['empirical_risk']:.3f}")
        print(f"  Safety Violations: {summary['safety_metrics']['safety_violations']}")

        print(f"\n📉 Regret Metrics (Section VI.B.3):")
        print(f"  Safe Regret: {summary['regret_metrics']['safe_regret_final']:.3f}")
        print(f"  Normalized: {summary['regret_metrics']['regret_normalized']:.6f}")

        print(f"\n⚙️  Computation Metrics (Section VI.B.4):")
        print(f"  MPC Feasibility: {summary['computation_metrics']['mpc_feasibility_rate']:.1%}")
        print(f"  Solve Time: μ={summary['computation_metrics']['solve_time_mean']:.1f}ms, "
              f"P90={summary['computation_metrics']['solve_time_p90']:.1f}ms")

        print(f"\n🎯 Tracking Metrics (Section VI.B.5):")
        print(f"  Tracking Error: μ={summary['tracking_metrics']['tracking_error_mean']:.3f}m, "
              f"σ={summary['tracking_metrics']['tracking_error_std']:.3f}m")

        print("\n" + "=" * 80)


def main():
    """测试函数"""
    rospy.init_node('test_manuscript_metrics')

    collector = ManuscriptMetricsCollector("/tmp/test_metrics")

    # 等待数据
    rospy.sleep(10)

    # 保存
    collector.save_manuscript_data()
    collector.print_summary()


if __name__ == '__main__':
    main()
