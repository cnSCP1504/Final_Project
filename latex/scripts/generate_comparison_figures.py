#!/usr/bin/env python3
"""
Generate comprehensive performance comparison tables and figures for four MPC methods
Based on test data from the usable folder
Includes Goal Success Rate metric
"""

import json
import os
import glob
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
plt.rcParams['font.sans-serif'] = ['DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

# Define MPC types and corresponding data paths
MPC_TYPES = {
    'Tube MPC': 'tube_mpc_*',
    'STL MPC': 'stl_*',
    'DR MPC': 'dr_*',
    'Safe-Regret MPC': 'safe_regret_*'
}

# Color scheme
COLORS = {
    'Tube MPC': '#3498db',
    'STL MPC': '#2ecc71',
    'DR MPC': '#e74c3c',
    'Safe-Regret MPC': '#9b59b6'
}

def calculate_success_rates(base_path, mpc_type, pattern):
    """Calculate goal and test success rates for a given MPC type"""
    folders = sorted(glob.glob(os.path.join(base_path, pattern)))
    run_goal_rates = []
    run_test_rates = []

    for folder in folders:
        test_folders = sorted(glob.glob(os.path.join(folder, 'test_*')))
        run_total_goals = 0
        run_reached_goals = 0
        run_success_tests = 0

        for test_folder in test_folders:
            metrics_file = os.path.join(test_folder, 'metrics.json')
            if os.path.exists(metrics_file):
                with open(metrics_file) as f:
                    d = json.load(f)
                    goals = d.get('total_goals', 0)
                    reached = len(d.get('goals_reached', []))
                    status = d.get('test_status', 'N/A')
                    run_total_goals += goals
                    run_reached_goals += reached
                    if status == 'SUCCESS':
                        run_success_tests += 1

        if run_total_goals > 0:
            goal_rate = run_reached_goals / run_total_goals * 100
            test_rate = run_success_tests / len(test_folders) * 100 if test_folders else 0
            run_goal_rates.append(goal_rate)
            run_test_rates.append(test_rate)

    return run_goal_rates, run_test_rates

def load_all_metrics(base_path):
    """Load metrics data for all MPC types"""
    all_data = {}

    for mpc_type, pattern in MPC_TYPES.items():
        folders = sorted(glob.glob(os.path.join(base_path, pattern)))
        metrics_list = []

        for folder in folders:
            json_path = os.path.join(folder, 'aggregated_manuscript_metrics.json')
            if os.path.exists(json_path):
                with open(json_path, 'r') as f:
                    data = json.load(f)
                    metrics_list.append(data)

        all_data[mpc_type] = metrics_list
        print(f"Loaded {len(metrics_list)} runs for {mpc_type}")

    return all_data

def compute_statistics(all_data, base_path):
    """Compute statistical metrics for each MPC type"""
    stats = {}

    for mpc_type, pattern in MPC_TYPES.items():
        metrics_list = all_data.get(mpc_type, [])
        if not metrics_list:
            continue

        # Extract individual metrics
        risks = [m['empirical_risk']['mean'] for m in metrics_list]
        feasibilities = [m['feasibility']['mean'] for m in metrics_list]
        track_errors = [m['tracking']['mean_error_mean'] for m in metrics_list]
        occupancies = [m['tracking']['occupancy_mean'] for m in metrics_list]
        solve_times = [m['computation']['median_of_medians'] for m in metrics_list]
        calibrations = [m['calibration']['mean_error'] for m in metrics_list]

        # Calculate success rates
        goal_rates, test_rates = calculate_success_rates(base_path, mpc_type, pattern)

        # Calculate statistics
        stats[mpc_type] = {
            'empirical_risk': {
                'mean': np.mean(risks),
                'std': np.std(risks),
                'min': np.min(risks),
                'max': np.max(risks),
                'values': risks
            },
            'feasibility': {
                'mean': np.mean(feasibilities),
                'std': np.std(feasibilities),
                'values': feasibilities
            },
            'tracking_error': {
                'mean': np.mean(track_errors),
                'std': np.std(track_errors),
                'min': np.min(track_errors),
                'max': np.max(track_errors),
                'values': track_errors
            },
            'tube_occupancy': {
                'mean': np.mean(occupancies),
                'std': np.std(occupancies),
                'values': occupancies
            },
            'solve_time': {
                'mean': np.mean(solve_times),
                'std': np.std(solve_times),
                'min': np.min(solve_times),
                'max': np.max(solve_times),
                'values': solve_times
            },
            'calibration_error': {
                'mean': np.mean(calibrations),
                'std': np.std(calibrations),
                'values': calibrations
            },
            'goal_success_rate': {
                'mean': np.mean(goal_rates) if goal_rates else 0,
                'std': np.std(goal_rates) if goal_rates else 0,
                'values': goal_rates
            },
            'test_success_rate': {
                'mean': np.mean(test_rates) if test_rates else 0,
                'std': np.std(test_rates) if test_rates else 0,
                'values': test_rates
            }
        }

    return stats

def generate_latex_table(stats):
    """Generate LaTeX format table"""
    latex_table = r"""
\begin{table}[htbp]
\centering
\caption{Comprehensive Performance Comparison of Four MPC Methods (10 independent runs, 10 shelf points each)}
\label{tab:mpc_comparison}
\begin{tabular}{lccccccc}
\toprule
\textbf{Method} & \textbf{Goal Success} & \textbf{Empirical Risk} $\hat{\delta}$ & \textbf{Feasibility} & \textbf{Tracking Error} & \textbf{Tube Occupancy} & \textbf{Solve Time} & \textbf{Calibration Error} \\
 & (\%) & (\%) & (\%) & (m) & (\%) & (ms) & \\
\midrule
"""

    for mpc_type in MPC_TYPES.keys():
        if mpc_type in stats:
            s = stats[mpc_type]
            latex_table += f"{mpc_type} & "
            latex_table += f"{s['goal_success_rate']['mean']:.1f}$\\pm${s['goal_success_rate']['std']:.1f} & "
            latex_table += f"{s['empirical_risk']['mean']*100:.2f}$\\pm${s['empirical_risk']['std']*100:.2f} & "
            latex_table += f"{s['feasibility']['mean']*100:.1f} & "
            latex_table += f"{s['tracking_error']['mean']:.3f}$\\pm${s['tracking_error']['std']:.3f} & "
            latex_table += f"{s['tube_occupancy']['mean']*100:.1f}$\\pm${s['tube_occupancy']['std']*100:.1f} & "
            latex_table += f"{s['solve_time']['mean']:.1f}$\\pm${s['solve_time']['std']:.1f} & "
            latex_table += f"{s['calibration_error']['mean']:.4f}$\\pm${s['calibration_error']['std']:.4f} \\\\\n"

    latex_table += r"""\bottomrule
\end{tabular}
\end{table}
"""
    return latex_table

def generate_markdown_table(stats):
    """Generate Markdown format table"""
    md_table = """
# Comprehensive Performance Comparison of Four MPC Methods

**Test Configuration**: Each method underwent 10 independent runs, with each run containing navigation tasks to 10 shelf points.

| Method | Goal Success Rate (%) | Empirical Risk $\hat{\\delta}$ (%) | Feasibility (%) | Tracking Error (m) | Tube Occupancy (%) | Solve Time (ms) | Calibration Error |
|--------|------|------|------|------|------|------|------|
"""

    for mpc_type in MPC_TYPES.keys():
        if mpc_type in stats:
            s = stats[mpc_type]
            md_table += f"| {mpc_type} | "
            md_table += f"{s['goal_success_rate']['mean']:.1f}±{s['goal_success_rate']['std']:.1f} | "
            md_table += f"{s['empirical_risk']['mean']*100:.2f}±{s['empirical_risk']['std']*100:.2f} | "
            md_table += f"{s['feasibility']['mean']*100:.1f} | "
            md_table += f"{s['tracking_error']['mean']:.3f}±{s['tracking_error']['std']:.3f} | "
            md_table += f"{s['tube_occupancy']['mean']*100:.1f}±{s['tube_occupancy']['std']*100:.1f} | "
            md_table += f"{s['solve_time']['mean']:.1f}±{s['solve_time']['std']:.1f} | "
            md_table += f"{s['calibration_error']['mean']:.4f}±{s['calibration_error']['std']:.4f} |\n"

    return md_table

def plot_comparison_bar(stats, output_dir):
    """Generate comparison bar chart"""
    fig, axes = plt.subplots(2, 4, figsize=(18, 10))

    metrics = [
        ('goal_success_rate', 'Goal Success Rate', '%', lambda x: x),
        ('empirical_risk', 'Empirical Risk', '%', lambda x: x*100),
        ('feasibility', 'Feasibility Rate', '%', lambda x: x*100),
        ('tracking_error', 'Tracking Error', 'm', lambda x: x),
        ('tube_occupancy', 'Tube Occupancy', '%', lambda x: x*100),
        ('solve_time', 'Solve Time', 'ms', lambda x: x),
        ('calibration_error', 'Calibration Error', '', lambda x: x)
    ]

    for idx, (metric_key, title, unit, transform) in enumerate(metrics):
        ax = axes[idx // 4, idx % 4]

        mpc_names = list(MPC_TYPES.keys())
        x = np.arange(len(mpc_names))
        width = 0.6

        means = [transform(stats[m][metric_key]['mean']) for m in mpc_names]
        stds = [transform(stats[m][metric_key]['std']) for m in mpc_names]
        colors = [COLORS[m] for m in mpc_names]

        bars = ax.bar(x, means, width, yerr=stds, capsize=5, color=colors, alpha=0.8)

        ax.set_ylabel(f'{title}' + (f' ({unit})' if unit else ''))
        ax.set_xticks(x)
        ax.set_xticklabels([m.replace(' ', '\n') for m in mpc_names], fontsize=9)
        ax.grid(axis='y', alpha=0.3)

        # Display values above bars
        for bar, mean, std in zip(bars, means, stds):
            height = bar.get_height()
            ax.annotate(f'{mean:.2f}',
                       xy=(bar.get_x() + bar.get_width()/2, height),
                       xytext=(0, 3 if std == 0 else std + 3),
                       textcoords="offset points",
                       ha='center', va='bottom', fontsize=8)

    # Hide the last empty subplot
    axes[1, 3].axis('off')

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'comparison_bar_chart.png'), dpi=150, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, 'comparison_bar_chart.pdf'), bbox_inches='tight')
    print(f"Saved: comparison_bar_chart.png/pdf")
    plt.close()

def plot_boxplots(all_data, base_path, output_dir):
    """Generate box plots"""
    fig, axes = plt.subplots(2, 4, figsize=(18, 10))

    metrics = [
        ('goal_success_rate', 'Goal Success Rate (%)', lambda: None),
        ('empirical_risk', 'Empirical Risk (%)', lambda x: x['empirical_risk']['mean']*100),
        ('feasibility', 'Feasibility Rate (%)', lambda x: x['feasibility']['mean']*100),
        ('tracking_error', 'Tracking Error (m)', lambda x: x['tracking']['mean_error_mean']),
        ('tube_occupancy', 'Tube Occupancy (%)', lambda x: x['tracking']['occupancy_mean']*100),
        ('solve_time', 'Solve Time (ms)', lambda x: x['computation']['median_of_medians']),
        ('calibration_error', 'Calibration Error', lambda x: x['calibration']['mean_error'])
    ]

    for idx, (metric_key, title, extractor) in enumerate(metrics):
        ax = axes[idx // 4, idx % 4]

        data_to_plot = []
        labels = []
        colors_list = []

        for mpc_type in MPC_TYPES.keys():
            if metric_key == 'goal_success_rate':
                # Get from stats calculation
                goal_rates, _ = calculate_success_rates(base_path, mpc_type, MPC_TYPES[mpc_type])
                if goal_rates:
                    data_to_plot.append(goal_rates)
                    labels.append(mpc_type.replace(' ', '\n'))
                    colors_list.append(COLORS[mpc_type])
            else:
                if mpc_type in all_data:
                    values = [extractor(m) for m in all_data[mpc_type]]
                    data_to_plot.append(values)
                    labels.append(mpc_type.replace(' ', '\n'))
                    colors_list.append(COLORS[mpc_type])

        if data_to_plot:
            bp = ax.boxplot(data_to_plot, patch_artist=True, labels=labels)

            for patch, color in zip(bp['boxes'], colors_list):
                patch.set_facecolor(color)
                patch.set_alpha(0.7)

        ax.set_ylabel(title)
        ax.grid(axis='y', alpha=0.3)
        ax.set_title(title, fontsize=11)

    # Hide the last empty subplot
    axes[1, 3].axis('off')

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'comparison_boxplot.png'), dpi=150, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, 'comparison_boxplot.pdf'), bbox_inches='tight')
    print(f"Saved: comparison_boxplot.png/pdf")
    plt.close()

def plot_trend_lines(all_data, base_path, output_dir):
    """Generate trend line plots (performance over 10 runs)"""
    fig, axes = plt.subplots(2, 4, figsize=(18, 10))

    metrics = [
        ('goal_success_rate', 'Goal Success Rate (%)', lambda: None),
        ('empirical_risk', 'Empirical Risk (%)', lambda x: x['empirical_risk']['mean']*100),
        ('feasibility', 'Feasibility Rate (%)', lambda x: x['feasibility']['mean']*100),
        ('tracking_error', 'Tracking Error (m)', lambda x: x['tracking']['mean_error_mean']),
        ('tube_occupancy', 'Tube Occupancy (%)', lambda x: x['tracking']['occupancy_mean']*100),
        ('solve_time', 'Solve Time (ms)', lambda x: x['computation']['median_of_medians']),
        ('calibration_error', 'Calibration Error', lambda x: x['calibration']['mean_error'])
    ]

    for idx, (metric_key, title, extractor) in enumerate(metrics):
        ax = axes[idx // 4, idx % 4]

        for mpc_type in MPC_TYPES.keys():
            if metric_key == 'goal_success_rate':
                goal_rates, _ = calculate_success_rates(base_path, mpc_type, MPC_TYPES[mpc_type])
                if goal_rates:
                    runs = range(1, len(goal_rates) + 1)
                    ax.plot(runs, goal_rates, 'o-', color=COLORS[mpc_type],
                           label=mpc_type, linewidth=2, markersize=6)
            else:
                if mpc_type in all_data:
                    values = [extractor(m) for m in all_data[mpc_type]]
                    runs = range(1, len(values) + 1)
                    ax.plot(runs, values, 'o-', color=COLORS[mpc_type],
                           label=mpc_type, linewidth=2, markersize=6)

        ax.set_xlabel('Run Number')
        ax.set_ylabel(title)
        ax.legend(fontsize=8, loc='best')
        ax.grid(alpha=0.3)
        ax.set_title(f'{title} over Runs', fontsize=11)

    # Hide the last empty subplot
    axes[1, 3].axis('off')

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'comparison_trend.png'), dpi=150, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, 'comparison_trend.pdf'), bbox_inches='tight')
    print(f"Saved: comparison_trend.png/pdf")
    plt.close()

def plot_radar_chart(stats, output_dir):
    """Generate spider/radar chart using bar chart style"""
    categories = [
        ('Goal Success', 'goal_success_rate', False),      # Higher is better
        ('Empirical Risk', 'empirical_risk', True),   # Lower is better
        ('Feasibility', 'feasibility', False),       # Higher is better
        ('Tracking Error', 'tracking_error', True),   # Lower is better
        ('Tube Occupancy', 'tube_occupancy', False),  # Higher is better
        ('Solve Time', 'solve_time', True),       # Lower is better
        ('Calibration', 'calibration_error', True) # Lower is better
    ]

    # Normalize data
    normalized = {}
    for cat_name, key, lower_better in categories:
        # Special handling for goal_success_rate which is already in percentage
        if key == 'goal_success_rate':
            values = [stats[m][key]['mean'] for m in MPC_TYPES.keys()]
        else:
            values = [stats[m][key]['mean'] for m in MPC_TYPES.keys()]

        min_val, max_val = min(values), max(values)

        for mpc_type in MPC_TYPES.keys():
            if mpc_type not in normalized:
                normalized[mpc_type] = {}

            val = stats[mpc_type][key]['mean']
            if max_val == min_val:
                norm_val = 1.0
            else:
                norm_val = (val - min_val) / (max_val - min_val)

            # Invert if lower is better
            if lower_better:
                norm_val = 1 - norm_val

            normalized[mpc_type][cat_name] = norm_val * 100  # Scale to 0-100

    # Create grouped bar chart instead of radar
    fig, ax = plt.subplots(figsize=(14, 8))

    cat_names = [cat[0] for cat in categories]
    x = np.arange(len(cat_names))
    width = 0.18
    multiplier = 0

    for mpc_type in MPC_TYPES.keys():
        values = [normalized[mpc_type][cat[0]] for cat in categories]
        offset = width * multiplier
        rects = ax.bar(x + offset, values, width, label=mpc_type, color=COLORS[mpc_type], alpha=0.8)
        multiplier += 1

    ax.set_ylabel('Normalized Score (0-100, higher is better)', fontsize=12)
    ax.set_xlabel('Performance Metrics', fontsize=12)
    ax.set_title('MPC Performance Comparison\n(Normalized Scores, Higher is Better)', fontsize=14)
    ax.set_xticks(x + width * 1.5)
    ax.set_xticklabels(cat_names, fontsize=10)
    ax.legend(loc='upper right', fontsize=10)
    ax.set_ylim(0, 110)
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'comparison_radar.png'), dpi=150, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, 'comparison_radar.pdf'), bbox_inches='tight')
    print(f"Saved: comparison_radar.png/pdf")
    plt.close()

def plot_risk_tracking_tradeoff(stats, output_dir):
    """Generate risk-tracking error trade-off plot"""
    fig, ax = plt.subplots(figsize=(10, 8))

    for mpc_type in MPC_TYPES.keys():
        risk = stats[mpc_type]['empirical_risk']['mean'] * 100
        tracking = stats[mpc_type]['tracking_error']['mean']
        risk_std = stats[mpc_type]['empirical_risk']['std'] * 100
        track_std = stats[mpc_type]['tracking_error']['std']

        ax.errorbar(tracking, risk,
                   xerr=track_std, yerr=risk_std,
                   fmt='o', color=COLORS[mpc_type],
                   markersize=15, capsize=5,
                   label=mpc_type, alpha=0.8)

        # Add labels
        ax.annotate(mpc_type.replace(' ', '\n'),
                   xy=(tracking, risk),
                   xytext=(10, 10),
                   textcoords='offset points',
                   fontsize=9,
                   ha='left')

    ax.set_xlabel('Tracking Error (m)', fontsize=12)
    ax.set_ylabel('Empirical Risk (%)', fontsize=12)
    ax.set_title('Risk-Tracking Error Trade-off', fontsize=14)
    ax.legend(fontsize=10, loc='upper left')
    ax.grid(alpha=0.3)

    # Add ideal region annotation
    ax.text(0.95, 0.05, 'Ideal Region\n(Bottom-Left)',
           transform=ax.transAxes,
           fontsize=10, va='bottom', ha='right',
           bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'risk_tracking_tradeoff.png'), dpi=150, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, 'risk_tracking_tradeoff.pdf'), bbox_inches='tight')
    print(f"Saved: risk_tracking_tradeoff.png/pdf")
    plt.close()

def main():
    base_path = '/home/dixon/Final_Project/catkin/usable'
    output_dir = '/home/dixon/Final_Project/catkin/latex/figures/comparison'

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    print("=" * 60)
    print("Loading test data...")
    print("=" * 60)
    all_data = load_all_metrics(base_path)

    print("\n" + "=" * 60)
    print("Computing statistics...")
    print("=" * 60)
    stats = compute_statistics(all_data, base_path)

    print("\n" + "=" * 60)
    print("Generating tables...")
    print("=" * 60)

    # Generate Markdown table
    md_table = generate_markdown_table(stats)
    with open(os.path.join(output_dir, 'comparison_table.md'), 'w') as f:
        f.write(md_table)
    print("Saved: comparison_table.md")

    # Generate LaTeX table
    latex_table = generate_latex_table(stats)
    with open(os.path.join(output_dir, 'comparison_table.tex'), 'w') as f:
        f.write(latex_table)
    print("Saved: comparison_table.tex")

    print("\n" + "=" * 60)
    print("Generating figures...")
    print("=" * 60)

    # Generate various plots
    plot_comparison_bar(stats, output_dir)
    plot_boxplots(all_data, base_path, output_dir)
    plot_trend_lines(all_data, base_path, output_dir)
    plot_radar_chart(stats, output_dir)
    plot_risk_tracking_tradeoff(stats, output_dir)

    print("\n" + "=" * 60)
    print("Done! All files saved to:")
    print(output_dir)
    print("=" * 60)

    # Print table preview
    print("\nTable Preview:")
    print(md_table)

if __name__ == '__main__':
    main()
