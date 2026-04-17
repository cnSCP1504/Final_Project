#!/usr/bin/env python3
"""
敏感性分析结果汇总与可视化
Analyze and visualize sensitivity analysis results

生成:
1. 参数敏感性对比表格
2. 敏感性曲线图
3. Markdown报告
"""

import os
import sys
import json
import statistics
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any

try:
    import matplotlib.pyplot as plt
    import matplotlib
    matplotlib.use('Agg')  # 非交互式后端
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("⚠️ matplotlib未安装，跳过图表生成")


class SensitivityResultAnalyzer:
    """敏感性分析结果分析器"""

    def __init__(self, results_dir: str):
        self.results_dir = Path(results_dir)
        self.figures_dir = self.results_dir / "figures"
        self.figures_dir.mkdir(exist_ok=True)

        # 加载结果
        self.results = self._load_results()

    def _load_results(self) -> Dict:
        """加载测试结果"""
        results_file = self.results_dir / "sensitivity_analysis_results.json"

        if results_file.exists():
            with open(results_file) as f:
                return json.load(f)

        # 如果没有汇总文件，尝试从子目录加载
        results = {}
        for test_dir in self.results_dir.iterdir():
            if test_dir.is_dir() and (test_dir.name.startswith('mpc_steps_') or
                                       test_dir.name.startswith('temperature_') or
                                       test_dir.name.startswith('window_size_')):
                # 解析参数名和值
                parts = test_dir.name.split('_')
                if len(parts) >= 2:
                    param_name = '_'.join(parts[:-1])
                    param_value = parts[-1]

                    metrics_file = test_dir / 'test_01_shelf_01' / 'metrics.json'
                    if metrics_file.exists():
                        with open(metrics_file) as f:
                            metrics_data = json.load(f)

                        if param_name not in results:
                            results[param_name] = {'results': []}

                        mm = metrics_data.get('manuscript_metrics', {})
                        results[param_name]['results'].append({
                            'param_name': param_name,
                            'param_value': param_value,
                            'metrics': mm
                        })

        return results

    def generate_summary_table(self) -> str:
        """生成汇总表格"""
        lines = []
        lines.append("# 参数敏感性分析结果汇总\n")
        lines.append(f"**分析日期**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

        for param_name, analysis in self.results.items():
            lines.append(f"## {param_name}\n\n")

            results = analysis.get('results', [])
            if not results:
                lines.append("*无有效结果*\n\n")
                continue

            # 表头
            lines.append("| 参数值 | 经验风险(%) | 可行率(%) | 求解时间(ms) | 跟踪误差(m) | 校准误差(%) | 安全边际(%) |\n")
            lines.append("|--------|-------------|-----------|--------------|-------------|-------------|-------------|\n")

            for r in results:
                m = r.get('metrics', {})
                val = r.get('param_value', 'N/A')

                emp = m.get('empirical_risk')
                feas = m.get('feasibility_rate')
                solve = m.get('median_solve_time')
                track = m.get('mean_tracking_error')
                cal = m.get('calibration_error')
                margin = m.get('safety_margin')

                emp_str = f"{emp*100:.2f}" if isinstance(emp, (int, float)) else '-'
                feas_str = f"{feas*100:.1f}" if isinstance(feas, (int, float)) else '-'
                solve_str = f"{solve:.1f}" if isinstance(solve, (int, float)) else '-'
                track_str = f"{track:.4f}" if isinstance(track, (int, float)) else '-'
                cal_str = f"{cal*100:.2f}" if isinstance(cal, (int, float)) else '-'
                margin_str = f"{margin:.2f}" if isinstance(margin, (int, float)) else '-'

                lines.append(f"| {val} | {emp_str} | {feas_str} | {solve_str} | {track_str} | {cal_str} | {margin_str} |\n")

            lines.append("\n")

        return ''.join(lines)

    def compute_sensitivity_index(self) -> Dict:
        """计算敏感性指数"""
        sensitivity = {}

        for param_name, analysis in self.results.items():
            results = analysis.get('results', [])
            if len(results) < 2:
                continue

            # 计算各指标的变化范围
            metrics_to_analyze = [
                'empirical_risk', 'feasibility_rate', 'median_solve_time',
                'mean_tracking_error', 'calibration_error'
            ]

            sensitivity[param_name] = {}

            for metric in metrics_to_analyze:
                values = [r.get('metrics', {}).get(metric) for r in results]
                values = [v for v in values if isinstance(v, (int, float))]

                if len(values) >= 2:
                    param_values = [r.get('param_value') for r in results]

                    # 计算敏感性指数: 指标变化范围 / 参数变化范围
                    metric_range = max(values) - min(values)

                    # 标准化参数值范围
                    try:
                        param_numeric = [float(p) for p in param_values]
                        param_range = max(param_numeric) - min(param_numeric)
                    except:
                        param_range = len(set(param_values)) - 1  # 类别数 - 1

                    if param_range > 0:
                        sensitivity_index = metric_range / param_range
                    else:
                        sensitivity_index = 0

                    sensitivity[param_name][metric] = {
                        'range': metric_range,
                        'mean': statistics.mean(values),
                        'std': statistics.stdev(values) if len(values) > 1 else 0,
                        'sensitivity_index': sensitivity_index
                    }

        return sensitivity

    def generate_figures(self):
        """生成敏感性分析图表"""
        if not HAS_MATPLOTLIB:
            return

        for param_name, analysis in self.results.items():
            results = analysis.get('results', [])
            if len(results) < 2:
                continue

            # 提取数据
            param_values = []
            empirical_risks = []
            solve_times = []
            calibration_errors = []

            for r in results:
                m = r.get('metrics', {})
                try:
                    param_values.append(float(r.get('param_value', 0)))
                except:
                    continue

                emp = m.get('empirical_risk')
                solve = m.get('median_solve_time')
                cal = m.get('calibration_error')

                empirical_risks.append(emp * 100 if isinstance(emp, (int, float)) else None)
                solve_times.append(solve if isinstance(solve, (int, float)) else None)
                calibration_errors.append(cal * 100 if isinstance(cal, (int, float)) else None)

            # 创建图表
            fig, axes = plt.subplots(2, 2, figsize=(12, 10))
            fig.suptitle(f'Parameter Sensitivity: {param_name}', fontsize=14, fontweight='bold')

            # 1. 经验风险
            ax = axes[0, 0]
            valid_idx = [i for i, v in enumerate(empirical_risks) if v is not None]
            if valid_idx:
                ax.plot([param_values[i] for i in valid_idx],
                       [empirical_risks[i] for i in valid_idx],
                       'o-', color='#2E86AB', linewidth=2, markersize=8)
                ax.axhline(y=5, color='red', linestyle='--', label='Target δ=5%')
                ax.set_xlabel(param_name)
                ax.set_ylabel('Empirical Risk (%)')
                ax.set_title('Empirical Risk vs Parameter')
                ax.legend()
                ax.grid(True, alpha=0.3)

            # 2. 求解时间
            ax = axes[0, 1]
            valid_idx = [i for i, v in enumerate(solve_times) if v is not None]
            if valid_idx:
                ax.plot([param_values[i] for i in valid_idx],
                       [solve_times[i] for i in valid_idx],
                       's-', color='#A23B72', linewidth=2, markersize=8)
                ax.axhline(y=8, color='green', linestyle='--', label='Real-time budget (8ms)')
                ax.set_xlabel(param_name)
                ax.set_ylabel('Solve Time (ms)')
                ax.set_title('Computation Time vs Parameter')
                ax.legend()
                ax.grid(True, alpha=0.3)

            # 3. 校准误差
            ax = axes[1, 0]
            valid_idx = [i for i, v in enumerate(calibration_errors) if v is not None]
            if valid_idx:
                ax.plot([param_values[i] for i in valid_idx],
                       [calibration_errors[i] for i in valid_idx],
                       '^-', color='#F18F01', linewidth=2, markersize=8)
                ax.axhline(y=0, color='green', linestyle='--', label='Perfect calibration')
                ax.set_xlabel(param_name)
                ax.set_ylabel('Calibration Error (%)')
                ax.set_title('Calibration Error vs Parameter')
                ax.legend()
                ax.grid(True, alpha=0.3)

            # 4. 综合对比
            ax = axes[1, 1]
            ax.axis('off')

            # 创建数据表格
            sensitivity = self.compute_sensitivity_index()
            if param_name in sensitivity:
                table_data = []
                row_labels = []
                for metric, data in sensitivity[param_name].items():
                    table_data.append([
                        f"{data['mean']:.4f}",
                        f"{data['std']:.4f}",
                        f"{data['range']:.4f}",
                        f"{data['sensitivity_index']:.4f}"
                    ])
                    row_labels.append(metric)

                if table_data:
                    table = ax.table(
                        cellText=table_data,
                        rowLabels=row_labels,
                        colLabels=['Mean', 'Std', 'Range', 'Sensitivity'],
                        loc='center',
                        cellLoc='center'
                    )
                    table.auto_set_font_size(False)
                    table.set_fontsize(9)
                    table.scale(1.2, 1.5)
                    ax.set_title('Sensitivity Summary', pad=20)

            plt.tight_layout()
            plt.savefig(self.figures_dir / f'{param_name}_sensitivity.png', dpi=150, bbox_inches='tight')
            plt.close()

        print(f"📊 图表已保存到: {self.figures_dir}")

    def generate_report(self):
        """生成完整报告"""
        report_file = self.results_dir / "sensitivity_analysis_report.md"

        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(self.generate_summary_table())

            f.write("\n## 敏感性指数\n\n")
            f.write("敏感性指数 = 指标变化范围 / 参数变化范围\n")
            f.write("值越大表示参数对该指标影响越大\n\n")

            sensitivity = self.compute_sensitivity_index()
            for param_name, metrics in sensitivity.items():
                f.write(f"### {param_name}\n\n")
                f.write("| 指标 | 均值 | 标准差 | 变化范围 | 敏感性指数 |\n")
                f.write("|------|------|--------|----------|------------|\n")
                for metric, data in metrics.items():
                    f.write(f"| {metric} | {data['mean']:.4f} | {data['std']:.4f} | {data['range']:.4f} | {data['sensitivity_index']:.4f} |\n")
                f.write("\n")

            f.write("## 结论与建议\n\n")

            # 找出最敏感的参数-指标组合
            max_sensitivity = 0
            most_sensitive = None
            for param_name, metrics in sensitivity.items():
                for metric, data in metrics.items():
                    if data['sensitivity_index'] > max_sensitivity:
                        max_sensitivity = data['sensitivity_index']
                        most_sensitive = (param_name, metric)

            if most_sensitive:
                f.write(f"- **最敏感参数**: {most_sensitive[0]} 对 **{most_sensitive[1]}** 影响最大\n")
                f.write(f"  - 敏感性指数: {max_sensitivity:.4f}\n\n")

            f.write("### 参数调优建议\n\n")
            f.write("1. **MPC时域 (N)**:\n")
            f.write("   - 增大N可提高预测精度，但增加计算时间\n")
            f.write("   - 建议根据实时性要求选择合适的N值\n\n")

            f.write("2. **STL温度 (τ)**:\n")
            f.write("   - 较小的τ(0.01-0.1)提供更准确的robustness近似\n")
            f.write("   - 较大的τ(0.5-1.0)提供更平滑的优化曲面\n")
            f.write("   - 建议从τ=0.1开始调优\n\n")

            f.write("3. **残差窗口 (M)**:\n")
            f.write("   - 增大M可提高DR margin的统计可靠性\n")
            f.write("   - 但会增加内存和初始化时间\n")
            f.write("   - 建议M=200作为平衡点\n\n")

            f.write("---\n")
            f.write(f"*报告生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}*\n")

        print(f"📄 报告已保存: {report_file}")


def main():
    if len(sys.argv) < 2:
        print("用法: python3 analyze_sensitivity_results.py <results_dir>")
        print("示例: python3 analyze_sensitivity_results.py test_results/sensitivity_analysis")
        sys.exit(1)

    results_dir = sys.argv[1]

    if not os.path.exists(results_dir):
        print(f"错误: 目录不存在: {results_dir}")
        sys.exit(1)

    analyzer = SensitivityResultAnalyzer(results_dir)
    analyzer.generate_report()
    analyzer.generate_figures()

    print("\n✅ 分析完成!")


if __name__ == '__main__':
    main()
