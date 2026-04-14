#!/usr/bin/env python3
"""
重新生成现有测试结果的图表

用法:
    python3 test/scripts/regenerate_figures.py <test_result_dir>

示例:
    python3 test/scripts/regenerate_figures.py test_results/stl_20260409_101607/test_01_shelf_01
"""

import sys
import json
import pathlib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def regenerate_figures(test_dir):
    """为指定测试目录重新生成图表"""
    test_dir = pathlib.Path(test_dir)

    # 读取metrics.json
    metrics_file = test_dir / 'metrics.json'
    if not metrics_file.exists():
        print(f"❌ 未找到metrics.json: {metrics_file}")
        return False

    with open(metrics_file) as f:
        metrics = json.load(f)

    if 'manuscript_metrics' not in metrics:
        print(f"❌ metrics.json中没有manuscript_metrics")
        return False

    manuscript_metrics = metrics['manuscript_metrics']
    manuscript_raw_data = metrics.get('manuscript_raw_data', {})

    # 创建图表输出目录
    figures_dir = test_dir / 'figures'
    figures_dir.mkdir(exist_ok=True)

    # 设置matplotlib参数
    plt.rcParams['font.size'] = 10
    plt.rcParams['font.family'] = 'serif'
    plt.rcParams['figure.dpi'] = 100
    plt.rcParams['savefig.dpi'] = 300
    plt.rcParams['savefig.bbox'] = 'tight'

    # 色盲友好配色
    COLORS = {
        'blue': '#2E86AB',
        'red': '#C73E1D',
        'green': '#6A994E',
        'gray': '#808080'
    }

    # 获取shelf信息（从test_summary.txt）
    summary_file = test_dir / 'test_summary.txt'
    if summary_file.exists():
        with open(summary_file) as f:
            summary = f.read()
            # 尝试提取shelf ID
            for line in summary.split('\n'):
                if 'Shelf ID' in line or '货架ID' in line:
                    shelf_id = line.split(':')[1].strip().split()[0]
                    break
            else:
                shelf_id = test_dir.name.split('_')[-1]
    else:
        shelf_id = test_dir.name.split('_')[-1]

    shelf = {
        'id': shelf_id,
        'name': f'Shelf {shelf_id}',
        'x': manuscript_metrics.get('goal_x', 0),
        'y': manuscript_metrics.get('goal_y', 0)
    }

    # 创建综合仪表板（2x2子图）
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle(f'Test {shelf["id"]}: {shelf["name"]} - Performance Dashboard',
                fontsize=14, fontweight='bold')

    plots_generated = 0

    # 1. STL Robustness History (左上)
    if 'stl_robustness_history' in manuscript_raw_data and len(manuscript_raw_data['stl_robustness_history']) > 0:
        ax = axes[0, 0]
        history = manuscript_raw_data['stl_robustness_history']
        ax.plot(history, color=COLORS['blue'], linewidth=1.5)
        ax.axhline(y=0, color=COLORS['green'], linestyle='--', linewidth=2, label='Satisfaction threshold')
        ax.set_xlabel('Time step', fontweight='bold')
        ax.set_ylabel('STL Robustness', fontweight='bold')
        ax.set_title('(a) STL Robustness Over Time', fontweight='bold')
        ax.grid(alpha=0.3)
        ax.legend()
        plots_generated += 1
        print(f"✓ Plot 1: STL Robustness ({len(history)} samples)")
    else:
        print("✗ Skipped plot 1: No stl_robustness_history")
        # 隐藏子图
        axes[0, 0].set_visible(False)

    # 2. Tracking Error (右上)
    if 'tracking_error_norm_history' in manuscript_raw_data and len(manuscript_raw_data['tracking_error_norm_history']) > 0:
        ax = axes[0, 1]
        errors = manuscript_raw_data['tracking_error_norm_history']
        ax.plot(np.array(errors) * 100, color=COLORS['blue'], linewidth=1.5)  # 转为cm
        tube_radius_cm = manuscript_metrics.get('tube_radius', 0.18) * 100
        ax.axhline(y=tube_radius_cm, color=COLORS['red'], linestyle='--',
                  linewidth=2, label=f'Tube radius ({tube_radius_cm:.1f} cm)')
        ax.set_xlabel('Time step', fontweight='bold')
        ax.set_ylabel('Tracking Error (cm)', fontweight='bold')
        ax.set_title('(b) Tracking Error Over Time', fontweight='bold')
        ax.grid(alpha=0.3)
        ax.legend()
        plots_generated += 1
        print(f"✓ Plot 2: Tracking Error ({len(errors)} samples)")
    else:
        print("✗ Skipped plot 2: No tracking_error_norm_history")
        axes[0, 1].set_visible(False)

    # 3. MPC Solve Time (左下)
    if 'mpc_solve_times' in manuscript_raw_data and len(manuscript_raw_data['mpc_solve_times']) > 0:
        ax = axes[1, 0]
        solve_times = manuscript_raw_data['mpc_solve_times']
        ax.plot(solve_times, color=COLORS['blue'], linewidth=1.5, alpha=0.7)
        ax.axhline(y=8, color=COLORS['green'], linestyle='--',
                  linewidth=2, label='Real-time budget (8 ms)')
        ax.set_xlabel('Solve number', fontweight='bold')
        ax.set_ylabel('Solve Time (ms)', fontweight='bold')
        ax.set_title('(c) MPC Computation Time', fontweight='bold')
        ax.grid(alpha=0.3)
        ax.legend()
        plots_generated += 1
        print(f"✓ Plot 3: MPC Solve Time ({len(solve_times)} samples)")
    else:
        print("✗ Skipped plot 3: No mpc_solve_times")
        axes[1, 0].set_visible(False)

    # 4. Satisfaction Probability (右下)
    if 'satisfaction_probability' in manuscript_metrics:
        ax = axes[1, 1]
        sat = manuscript_metrics['satisfaction_probability']

        # 处理两种数据格式：嵌套字典或扁平值
        if isinstance(sat, dict):
            satisfied = sat.get('satisfied_steps', 0)
            total = sat.get('total_steps', 1)
        else:
            # 扁平格式：sat是概率值(0.0-1.0)，需要从total_steps计算
            total = manuscript_metrics.get('total_steps', manuscript_metrics.get('stl_total_steps', 1))
            satisfied = int(sat * total) if total > 0 else 0

        rate = (satisfied / total * 100) if total > 0 else 0

        colors_pie = [COLORS['green'], COLORS['red']]
        sizes = [rate, 100 - rate]
        labels = [f'Satisfied\n({rate:.1f}%)', f'Violated\n({100-rate:.1f}%)']

        ax.pie(sizes, labels=labels, colors=colors_pie, autopct='',
              startangle=90, textprops={'fontsize': 10, 'fontweight': 'bold'})
        ax.set_title(f'(d) STL Satisfaction Rate\n({satisfied}/{total} steps)',
                    fontweight='bold')
        plots_generated += 1
        print(f"✓ Plot 4: Satisfaction Probability ({satisfied}/{total} = {rate:.1f}%)")
    else:
        print("✗ Skipped plot 4: No satisfaction_probability")
        axes[1, 1].set_visible(False)

    # 调整布局并保存
    if plots_generated > 0:
        plt.tight_layout()
        dashboard_file = figures_dir / "performance_dashboard.png"
        plt.savefig(dashboard_file, dpi=300)
        plt.savefig(figures_dir / "performance_dashboard.pdf")
        plt.close()

        print(f"\n✓ Dashboard saved to: {dashboard_file}")
        print(f"✓ Total plots generated: {plots_generated}/4")
        return True
    else:
        print("❌ No plots generated (missing data)")
        plt.close()
        return False

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法: python3 regenerate_figures.py <test_result_dir>")
        print("\n示例:")
        print("  python3 regenerate_figures.py test_results/stl_20260409_101607/test_01_shelf_01")
        sys.exit(1)

    test_dir = sys.argv[1]
    success = regenerate_figures(test_dir)

    if success:
        print("\n✅ 图表重新生成成功！")
        sys.exit(0)
    else:
        print("\n❌ 图表生成失败！")
        sys.exit(1)
