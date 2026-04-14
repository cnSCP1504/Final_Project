#!/usr/bin/env python3
"""
Manuscript Figure Generator
生成manuscript级别的性能图表

Usage:
    python3 generate_manuscript_figures.py --metrics <aggregated_metrics.json> --output <output_dir>
"""

import json
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import argparse
from pathlib import Path
from typing import Dict, List

# 设置matplotlib参数
plt.rcParams['font.size'] = 10
plt.rcParams['font.family'] = 'serif'
plt.rcParams['axes.labelsize'] = 12
plt.rcParams['axes.labelweight'] = 'bold'
plt.rcParams['axes.titlesize'] = 14
plt.rcParams['axes.titleweight'] = 'bold'
plt.rcParams['xtick.labelsize'] = 9
plt.rcParams['ytick.labelsize'] = 9
plt.rcParams['legend.fontsize'] = 9
plt.rcParams['figure.dpi'] = 100
plt.rcParams['savefig.dpi'] = 300
plt.rcParams['savefig.bbox'] = 'tight'

# 色盲友好配色方案
COLORS = {
    'blue': '#2E86AB',
    'red': '#C73E1D',
    'orange': '#F18F01',
    'purple': '#A23B72',
    'green': '#6A994E',
    'gray': '#808080',
    'black': '#000000'
}


class ManuscriptFigureGenerator:
    """Manuscript图表生成器"""

    def __init__(self, metrics_data: Dict):
        self.metrics = metrics_data
        self.figures = []

    def generate_all_figures(self, output_dir: Path):
        """生成所有manuscript图表"""
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        print("\n=== Generating Manuscript Figures ===")

        # 1. Satisfaction Probability
        print("[1/7] Generating satisfaction probability figure...")
        self._plot_satisfaction_probability(output_dir)

        # 2. Empirical Risk
        print("[2/7] Generating empirical risk figure...")
        self._plot_empirical_risk(output_dir)

        # 3. Computation Metrics
        print("[3/7] Generating computation metrics figure...")
        self._plot_computation_metrics(output_dir)

        # 4. Feasibility Rate
        print("[4/7] Generating feasibility rate figure...")
        self._plot_feasibility_rate(output_dir)

        # 5. Tracking Error Distribution
        print("[5/7] Generating tracking error distribution figure...")
        self._plot_tracking_error_distribution(output_dir)

        # 6. Calibration Accuracy
        print("[6/7] Generating calibration accuracy figure...")
        self._plot_calibration_accuracy(output_dir)

        # 7. Summary Dashboard
        print("[7/7] Generating summary dashboard...")
        self._plot_summary_dashboard(output_dir)

        print("\n✅ All figures generated successfully!")
        print(f"   Output directory: {output_dir}\n")

    def _plot_satisfaction_probability(self, output_dir: Path):
        """1. Satisfaction Probability（单方法版本）"""
        sat = self.metrics['satisfaction_probability']
        p_sat = sat['p_sat'] * 100  # 转为百分比
        ci_lower = sat['ci_lower'] * 100
        ci_upper = sat['ci_upper'] * 100

        fig, ax = plt.subplots(figsize=(6, 5))

        # 绘制柱状图
        bars = ax.bar(['Safe-Regret MPC'], [p_sat],
                      yerr=[[p_sat - ci_lower], [ci_upper - p_sat]],
                      capsize=5, alpha=0.7, color=COLORS['blue'],
                      error_kw={'linewidth': 2})

        # 标注数值
        ax.text(0, p_sat, f'{p_sat:.1f}%',
               ha='center', va='bottom', fontsize=12, fontweight='bold')

        # 设置标签和标题
        ax.set_ylabel('Satisfaction Probability (%)', fontweight='bold')
        ax.set_xlabel('Method', fontweight='bold')
        ax.set_title('STL Specification Satisfaction Rate', fontweight='bold')
        ax.set_ylim([0, 105])
        ax.grid(axis='y', alpha=0.3)

        # 添加置信区间说明
        ax.text(0.02, 0.98, 'Wilson 95% CI',
               transform=ax.transAxes,
               verticalalignment='top',
               fontsize=9,
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

        plt.tight_layout()
        plt.savefig(output_dir / 'satisfaction_probability.png')
        plt.savefig(output_dir / 'satisfaction_probability.pdf')
        plt.close()

    def _plot_empirical_risk(self, output_dir: Path):
        """2. Empirical Risk"""
        risk = self.metrics['empirical_risk']
        observed = risk['observed_delta']
        target = risk['target_delta']

        fig, ax = plt.subplots(figsize=(6, 5))

        # 判断是否接近目标值
        color = COLORS['blue'] if abs(observed - target) < 0.02 else COLORS['red']

        # 绘制柱状图
        bars = ax.bar(['Safe-Regret MPC'], [observed],
                      color=color, alpha=0.7)

        # 添加目标线
        ax.axhline(y=target, color=COLORS['green'], linestyle='--', linewidth=2,
                  label=f'Target $\\delta = {target:.2f}$')

        # 标注数值
        ax.text(0, observed, f'{observed:.3f}',
               ha='center', va='bottom' if observed < target else 'top',
               fontsize=12, fontweight='bold')

        # 设置标签和标题
        ax.set_ylabel('Empirical Risk ($\\widehat{\\delta}$)', fontweight='bold')
        ax.set_xlabel('Method', fontweight='bold')
        ax.set_title('Safety Constraint Violation Rate', fontweight='bold')
        ax.set_ylim([0, max(observed * 1.5, target * 2)])
        ax.legend(loc='upper right')
        ax.grid(axis='y', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_dir / 'empirical_risk.png')
        plt.savefig(output_dir / 'empirical_risk.pdf')
        plt.close()

    def _plot_computation_metrics(self, output_dir: Path):
        """3. Computation Metrics（箱线图）"""
        comp = self.metrics['computation']

        fig, ax = plt.subplots(figsize=(6, 5))

        # 由于我们只有统计量，没有原始数据，我们创建一个简化的箱线图
        # 使用median, mean, p90, p95, p99
        stats_to_plot = {
            'median': comp['median_ms'],
            'mean': comp['mean_ms'],
            'p90': comp['p90_ms'],
            'p95': comp['p95_ms']
        }

        x_pos = np.arange(len(stats_to_plot))
        values = list(stats_to_plot.values())
        labels = list(stats_to_plot.keys())

        # 绘制柱状图
        bars = ax.bar(x_pos, values, color=COLORS['blue'], alpha=0.7)

        # 添加实时预算线（8ms）
        ax.axhline(y=8, color=COLORS['green'], linestyle='--', linewidth=2,
                  label='Real-time budget (8 ms)')

        # 标注数值
        for i, (bar, val) in enumerate(zip(bars, values)):
            ax.text(bar.get_x() + bar.get_width()/2., val,
                   f'{val:.1f}',
                   ha='center', va='bottom', fontsize=9, fontweight='bold')

        # 设置标签和标题
        ax.set_xticks(x_pos)
        ax.set_xticklabels(labels)
        ax.set_ylabel('Solve Time (ms)', fontweight='bold')
        ax.set_xlabel('Statistic', fontweight='bold')
        ax.set_title('MPC Computation Time Distribution', fontweight='bold')
        ax.legend(loc='upper left')
        ax.grid(axis='y', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_dir / 'computation_metrics.png')
        plt.savefig(output_dir / 'computation_metrics.pdf')
        plt.close()

    def _plot_feasibility_rate(self, output_dir: Path):
        """4. Feasibility Rate"""
        feas = self.metrics['feasibility']
        rate = feas['feasibility_rate'] * 100  # 转为百分比
        target_rate = 99.5  # 仿真目标

        fig, ax = plt.subplots(figsize=(6, 5))

        # 判断是否达到目标
        color = COLORS['blue'] if rate >= target_rate else COLORS['red']

        # 绘制柱状图
        bars = ax.bar(['Safe-Regret MPC'], [rate],
                      color=color, alpha=0.7)

        # 添加目标线
        ax.axhline(y=target_rate, color=COLORS['green'], linestyle='--', linewidth=2,
                  label=f'Target ({target_rate:.1f}%)')

        # 标注数值
        ax.text(0, rate, f'{rate:.2f}%',
               ha='center', va='bottom', fontsize=12, fontweight='bold')

        # 设置标签和标题
        ax.set_ylabel('Feasibility Rate (%)', fontweight='bold')
        ax.set_xlabel('Method', fontweight='bold')
        ax.set_title('MPC Recursive Feasibility Rate', fontweight='bold')
        ax.set_ylim([rate * 0.95, 100.5])
        ax.legend(loc='lower right')
        ax.grid(axis='y', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_dir / 'feasibility_rate.png')
        plt.savefig(output_dir / 'feasibility_rate.pdf')
        plt.close()

    def _plot_tracking_error_distribution(self, output_dir: Path):
        """5. Tracking Error Distribution（需要原始数据）"""
        track = self.metrics['tracking']

        # 由于我们只有统计量，我们创建一个简化版本的可视化
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

        # 左图：统计量柱状图
        stats_names = ['Mean', 'Std', 'RMSE', 'Max']
        stats_values = [
            track['mean_error_m'] * 100,  # 转为cm
            track['std_error_m'] * 100,
            track['rmse_m'] * 100,
            track['max_error_m'] * 100
        ]

        x_pos = np.arange(len(stats_names))
        bars = ax1.bar(x_pos, stats_values, color=COLORS['blue'], alpha=0.7)

        # 添加管半径参考线
        tube_radius_cm = self.metrics['metadata']['tube_radius'] * 100
        ax1.axhline(y=tube_radius_cm, color=COLORS['red'], linestyle='--',
                   linewidth=2, label=f'Tube radius ({tube_radius_cm:.1f} cm)')

        # 标注数值
        for bar, val in zip(bars, stats_values):
            ax1.text(bar.get_x() + bar.get_width()/2., val,
                    f'{val:.2f}',
                    ha='center', va='bottom', fontsize=9, fontweight='bold')

        ax1.set_xticks(x_pos)
        ax1.set_xticklabels(stats_names)
        ax1.set_ylabel('Error (cm)', fontweight='bold')
        ax1.set_title('Tracking Error Statistics', fontweight='bold')
        ax1.legend()
        ax1.grid(axis='y', alpha=0.3)

        # 右图：管占用量
        occupancy = track['occupancy_rate'] * 100
        colors_pie = [COLORS['green'], COLORS['red']]
        sizes = [occupancy, 100 - occupancy]
        labels = [f'In tube\n({occupancy:.1f}%)', f'Out of tube\n({100-occupancy:.1f}%)']

        ax2.pie(sizes, labels=labels, colors=colors_pie, autopct='',
               startangle=90, textprops={'fontsize': 11, 'fontweight': 'bold'})
        ax2.set_title(f'Tube Occupancy Rate\n({track["in_tube_count"]}/{track["total_steps"]} steps)',
                     fontweight='bold')

        plt.tight_layout()
        plt.savefig(output_dir / 'tracking_error_distribution.png')
        plt.savefig(output_dir / 'tracking_error_distribution.pdf')
        plt.close()

    def _plot_calibration_accuracy(self, output_dir: Path):
        """6. Calibration Accuracy"""
        calib = self.metrics['calibration']
        observed = calib['observed_delta']
        target = calib['target_delta']

        fig, ax = plt.subplots(figsize=(6, 5))

        # 判断是否接近目标
        color = COLORS['blue'] if calib['calibration_error'] < 0.01 else COLORS['red']

        # 绘制散点
        ax.scatter(0, observed, s=200, c=color, alpha=0.7,
                  edgecolors='black', linewidths=2)

        # 添加目标线（完美校准）
        ax.axhline(y=target, color=COLORS['green'], linestyle='--', linewidth=2,
                  label=f'Target $\\delta = {target:.2f}$')

        # 添加±10%容差区域
        ax.fill_between([-0.5, 0.5], target * 0.9, target * 1.1,
                       color=COLORS['green'], alpha=0.1, label='±10% tolerance')

        # 标注数值
        ax.text(0, observed, f'{observed:.3f}\\n±{calib["calibration_error"]:.3f}',
               ha='center', va='bottom' if observed < target else 'top',
               fontsize=10, fontweight='bold',
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

        # 设置标签和标题
        ax.set_xlim([-0.5, 0.5])
        ax.set_xticks([0])
        ax.set_xticklabels(['Safe-Regret MPC'])
        ax.set_ylabel('Observed Violation Rate ($\\widehat{\\delta}$)', fontweight='bold')
        ax.set_xlabel('Method', fontweight='bold')
        ax.set_title('Calibration Accuracy: Target vs. Observed Risk', fontweight='bold')
        ax.set_ylim([0, max(observed * 1.5, target * 2)])
        ax.legend(loc='upper right')
        ax.grid(axis='y', alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_dir / 'calibration_accuracy.png')
        plt.savefig(output_dir / 'calibration_accuracy.pdf')
        plt.close()

    def _plot_summary_dashboard(self, output_dir: Path):
        """7. Summary Dashboard（综合仪表板）"""
        fig = plt.figure(figsize=(16, 10))
        gs = fig.add_gridspec(3, 3, hspace=0.35, wspace=0.35)

        # 1. Satisfaction Probability (左上)
        ax1 = fig.add_subplot(gs[0, 0])
        sat = self.metrics['satisfaction_probability']
        p_sat = sat['p_sat'] * 100
        ci_lower = sat['ci_lower'] * 100
        ci_upper = sat['ci_upper'] * 100

        ax1.bar(['SR-MPC'], [p_sat],
               yerr=[[p_sat - ci_lower], [ci_upper - p_sat]],
               capsize=5, alpha=0.7, color=COLORS['blue'], error_kw={'linewidth': 2})
        ax1.text(0, p_sat, f'{p_sat:.1f}%', ha='center', va='bottom',
                fontsize=11, fontweight='bold')
        ax1.set_ylabel('Satisfaction (%)', fontweight='bold')
        ax1.set_title('(a) STL Satisfaction Rate', fontweight='bold')
        ax1.set_ylim([0, 105])
        ax1.grid(axis='y', alpha=0.3)

        # 2. Empirical Risk (中上)
        ax2 = fig.add_subplot(gs[0, 1])
        risk = self.metrics['empirical_risk']
        observed = risk['observed_delta']
        target = risk['target_delta']

        color = COLORS['blue'] if abs(observed - target) < 0.02 else COLORS['red']
        ax2.bar(['SR-MPC'], [observed], color=color, alpha=0.7)
        ax2.axhline(y=target, color=COLORS['green'], linestyle='--', linewidth=2)
        ax2.text(0, observed, f'{observed:.3f}', ha='center',
                va='bottom' if observed < target else 'top',
                fontsize=11, fontweight='bold')
        ax2.set_ylabel('Violation Rate', fontweight='bold')
        ax2.set_title('(b) Empirical Risk', fontweight='bold')
        ax2.set_ylim([0, max(observed * 1.5, target * 2)])
        ax2.grid(axis='y', alpha=0.3)

        # 3. Feasibility Rate (右上)
        ax3 = fig.add_subplot(gs[0, 2])
        feas = self.metrics['feasibility']
        rate = feas['feasibility_rate'] * 100
        target_rate = 99.5

        color = COLORS['blue'] if rate >= target_rate else COLORS['red']
        ax3.bar(['SR-MPC'], [rate], color=color, alpha=0.7)
        ax3.axhline(y=target_rate, color=COLORS['green'], linestyle='--', linewidth=2)
        ax3.text(0, rate, f'{rate:.1f}%', ha='center', va='bottom',
                fontsize=11, fontweight='bold')
        ax3.set_ylabel('Feasibility (%)', fontweight='bold')
        ax3.set_title('(c) Recursive Feasibility', fontweight='bold')
        ax3.set_ylim([rate * 0.95, 100.5])
        ax3.grid(axis='y', alpha=0.3)

        # 4. Computation Metrics (左中)
        ax4 = fig.add_subplot(gs[1, 0])
        comp = self.metrics['computation']
        stats_names = ['Median', 'Mean', 'P90', 'P95']
        stats_values = [comp['median_ms'], comp['mean_ms'],
                       comp['p90_ms'], comp['p95_ms']]

        x_pos = np.arange(len(stats_names))
        ax4.bar(x_pos, stats_values, color=COLORS['blue'], alpha=0.7)
        ax4.axhline(y=8, color=COLORS['green'], linestyle='--', linewidth=2)
        for i, val in enumerate(stats_values):
            ax4.text(i, val, f'{val:.1f}', ha='center', va='bottom',
                    fontsize=9, fontweight='bold')
        ax4.set_xticks(x_pos)
        ax4.set_xticklabels(stats_names)
        ax4.set_ylabel('Time (ms)', fontweight='bold')
        ax4.set_title('(d) Computation Time', fontweight='bold')
        ax4.grid(axis='y', alpha=0.3)

        # 5. Tracking Error Statistics (中中)
        ax5 = fig.add_subplot(gs[1, 1])
        track = self.metrics['tracking']
        stats_names = ['Mean', 'Std', 'RMSE', 'Max']
        stats_values = [track['mean_error_m'] * 100, track['std_error_m'] * 100,
                       track['rmse_m'] * 100, track['max_error_m'] * 100]

        x_pos = np.arange(len(stats_names))
        ax5.bar(x_pos, stats_values, color=COLORS['blue'], alpha=0.7)
        tube_radius_cm = self.metrics['metadata']['tube_radius'] * 100
        ax5.axhline(y=tube_radius_cm, color=COLORS['red'], linestyle='--', linewidth=2)
        for i, val in enumerate(stats_values):
            ax5.text(i, val, f'{val:.2f}', ha='center', va='bottom',
                    fontsize=9, fontweight='bold')
        ax5.set_xticks(x_pos)
        ax5.set_xticklabels(stats_names)
        ax5.set_ylabel('Error (cm)', fontweight='bold')
        ax5.set_title('(e) Tracking Error', fontweight='bold')
        ax5.grid(axis='y', alpha=0.3)

        # 6. Tube Occupancy (中右)
        ax6 = fig.add_subplot(gs[1, 2])
        occupancy = track['occupancy_rate'] * 100
        colors_pie = [COLORS['green'], COLORS['red']]
        sizes = [occupancy, 100 - occupancy]
        labels = [f'In tube\n({occupancy:.1f}%)', f'Out of tube\n({100-occupancy:.1f}%)']

        ax6.pie(sizes, labels=labels, colors=colors_pie, autopct='',
                startangle=90, textprops={'fontsize': 10, 'fontweight': 'bold'})
        ax6.set_title('(f) Tube Occupancy', fontweight='bold')

        # 7. Calibration Accuracy (左下)
        ax7 = fig.add_subplot(gs[2, 0])
        calib = self.metrics['calibration']
        observed = calib['observed_delta']
        target = calib['target_delta']

        color = COLORS['blue'] if calib['calibration_error'] < 0.01 else COLORS['red']
        ax7.scatter(0, observed, s=200, c=color, alpha=0.7,
                   edgecolors='black', linewidths=2)
        ax7.axhline(y=target, color=COLORS['green'], linestyle='--', linewidth=2)
        ax7.fill_between([-0.5, 0.5], target * 0.9, target * 1.1,
                        color=COLORS['green'], alpha=0.1)
        ax7.text(0, observed, f'{observed:.3f}', ha='center',
                va='bottom' if observed < target else 'top',
                fontsize=10, fontweight='bold',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        ax7.set_xlim([-0.5, 0.5])
        ax7.set_xticks([0])
        ax7.set_xticklabels(['SR-MPC'])
        ax7.set_ylabel('Observed $\\widehat{\\delta}$', fontweight='bold')
        ax7.set_title('(g) Calibration Accuracy', fontweight='bold')
        ax7.set_ylim([0, max(observed * 1.5, target * 2)])
        ax7.grid(axis='y', alpha=0.3)

        # 8. Regret (中下)
        ax8 = fig.add_subplot(gs[2, 1])
        regret = self.metrics['regret']
        normalized_regret = regret['normalized_regret']

        # 简化版本：只显示当前值
        ax8.bar(['SR-MPC'], [normalized_regret], color=COLORS['blue'], alpha=0.7)
        ax8.text(0, normalized_regret, f'{normalized_regret:.4f}',
                ha='center', va='bottom', fontsize=11, fontweight='bold')
        ax8.set_ylabel('Normalized Regret ($R_T/T$)', fontweight='bold')
        ax8.set_title('(h) Dynamic Regret', fontweight='bold')
        ax8.grid(axis='y', alpha=0.3)

        # 9. Summary Table (右下)
        ax9 = fig.add_subplot(gs[2, 2])
        ax9.axis('off')

        # 创建摘要表格
        summary_data = [
            ['Metric', 'Value', 'Target'],
            ['Satisfaction', f'{p_sat:.1f}%', 'N/A'],
            ['Risk', f'{observed:.3f}', f'{target:.2f}'],
            ['Feasibility', f'{rate:.1f}%', '99.5%'],
            ['Solve Time', f'{comp["median_ms"]:.1f} ms', '<8 ms'],
            ['Tracking Error', f'{track["mean_error_m"]*100:.2f} cm', '<18 cm'],
            ['Calibration', f'{calib["calibration_error"]:.3f}', '<0.01']
        ]

        table = ax9.table(cellText=summary_data, cellLoc='center',
                         loc='center', colWidths=[0.4, 0.3, 0.3])
        table.auto_set_font_size(False)
        table.set_fontsize(9)
        table.scale(1, 2)

        # 设置表头样式
        for i in range(3):
            table[(0, i)].set_facecolor('#4472C4')
            table[(0, i)].set_text_props(weight='bold', color='white')

        # 交替行颜色
        for i in range(1, len(summary_data)):
            for j in range(3):
                if i % 2 == 0:
                    table[(i, j)].set_facecolor('#E7E6E6')

        ax9.set_title('(i) Performance Summary', fontweight='bold', pad=20)

        # 总标题
        fig.suptitle('Safe-Regret MPC: Comprehensive Performance Evaluation',
                    fontsize=18, fontweight='bold', y=0.995)

        plt.savefig(output_dir / 'performance_dashboard.png', dpi=300)
        plt.savefig(output_dir / 'performance_dashboard.pdf')
        plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Generate Manuscript Figures for Safe-Regret MPC',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 generate_manuscript_figures.py \\
      --metrics test_results/safe_regret_20260408_120104/aggregated_metrics.json \\
      --output test_results/safe_regret_20260408_120104/metrics/
        """
    )

    parser.add_argument('--metrics', '-m', required=True,
                       help='Input aggregated metrics JSON file')
    parser.add_argument('--output', '-o', required=True,
                       help='Output directory for figures')

    args = parser.parse_args()

    # 检查输入文件
    metrics_file = Path(args.metrics)
    if not metrics_file.exists():
        print(f"❌ Error: Metrics file does not exist: {metrics_file}")
        return 1

    print("="*70)
    print("Manuscript Figure Generator")
    print("="*70)
    print(f"\nInput metrics: {metrics_file}")
    print(f"Output directory: {args.output}\n")

    # 加载metrics数据
    with open(metrics_file, 'r') as f:
        metrics_data = json.load(f)

    # 生成图表
    generator = ManuscriptFigureGenerator(metrics_data)
    generator.generate_all_figures(args.output)

    print("="*70)
    print("✅ Figure Generation Complete!")
    print("="*70)
    print("\nGenerated files:")
    print(f"  - {args.output}/satisfaction_probability.png")
    print(f"  - {args.output}/empirical_risk.png")
    print(f"  - {args.output}/computation_metrics.png")
    print(f"  - {args.output}/feasibility_rate.png")
    print(f"  - {args.output}/tracking_error_distribution.png")
    print(f"  - {args.output}/calibration_accuracy.png")
    print(f"  - {args.output}/performance_dashboard.png")
    print()

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
