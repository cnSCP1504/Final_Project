#!/usr/bin/env python3

"""
Metrics Analysis and Visualization Tool for Tube MPC

This script analyzes the CSV output from MetricsCollector and generates
plots and statistics for Safe-Regret MPC evaluation.

Usage:
    python analyze_metrics.py [--csv CSV_FILE] [--output OUTPUT_DIR]

Example:
    python analyze_metrics.py --csv /tmp/tube_mpc_metrics.csv --output /tmp/metrics_analysis
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
import argparse
import os
from pathlib import Path

def load_metrics(csv_path):
    """Load metrics CSV file."""
    if not os.path.exists(csv_path):
        raise FileNotFoundError(f"Metrics file not found: {csv_path}")

    df = pd.read_csv(csv_path)
    print(f"✓ Loaded {len(df)} records from {csv_path}")
    return df

def compute_statistics(df):
    """Compute comprehensive statistics from metrics data."""
    stats = {
        # Safety metrics
        'empirical_risk': df['safety_violation'].mean(),
        'satisfaction_prob': 1 - df['safety_violation'].mean(),
        'safety_violation_count': df['safety_violation'].sum(),

        # Feasibility metrics
        'feasibility_rate': df['mpc_feasible'].mean(),
        'mpc_failures': (~df['mpc_feasible']).sum(),

        # Tracking performance
        'mean_tracking_error': df['tracking_error_norm'].mean(),
        'max_tracking_error': df['tracking_error_norm'].max(),
        'std_tracking_error': df['tracking_error_norm'].std(),
        'tube_violation_rate': (df['tube_violation'] > 0).mean(),

        # Computation
        'mean_solve_time_ms': df['mpc_solve_time'].mean(),
        'median_solve_time_ms': df['mpc_solve_time'].median(),
        'p90_solve_time_ms': df['mpc_solve_time'].quantile(0.9),
        'max_solve_time_ms': df['mpc_solve_time'].max(),

        # Regret
        'cumulative_dynamic_regret': df['regret_dynamic'].sum(),
        'average_dynamic_regret': df['regret_dynamic'].mean(),
        'cumulative_safe_regret': df['regret_safe'].sum(),
        'average_safe_regret': df['regret_safe'].mean(),

        # Total steps
        'total_steps': len(df),
    }

    # Compute Wilson confidence interval for satisfaction probability
    n = stats['total_steps']
    p = stats['satisfaction_prob']
    z = 1.96  # 95% CI
    center = (p + z*z/(2*n)) / (1 + z*z/n)
    margin = z * np.sqrt((p*(1-p) + z*z/(4*n))/n) / (1 + z*z/n)
    stats['satisfaction_ci_lower'] = center - margin
    stats['satisfaction_ci_upper'] = center + margin

    return stats

def print_statistics(stats_dict, target_delta=0.1):
    """Print formatted statistics summary."""
    print("\n" + "="*60)
    print("TUBE MPC METRICS ANALYSIS")
    print("="*60)

    print("\n--- Safety and Satisfaction ---")
    print(f"Empirical Risk (δ̂):          {stats_dict['empirical_risk']:.4f}")
    print(f"Satisfaction Probability:    {stats_dict['satisfaction_prob']:.4f}")
    print(f"95% CI:                      [{stats_dict['satisfaction_ci_lower']:.4f}, {stats_dict['satisfaction_ci_upper']:.4f}]")
    print(f"Target Risk (δ):             {target_delta:.4f}")
    print(f"Calibration Error:           {abs(stats_dict['empirical_risk'] - target_delta):.4f}")

    print("\n--- Feasibility ---")
    print(f"Total MPC Calls:             {stats_dict['total_steps']}")
    print(f"Successful Solves:           {stats_dict['total_steps'] - stats_dict['mpc_failures']}")
    print(f"Failed Solves:               {stats_dict['mpc_failures']}")
    print(f"Feasibility Rate:            {stats_dict['feasibility_rate']*100:.2f}%")

    print("\n--- Tracking Performance ---")
    print(f"Mean Tracking Error:         {stats_dict['mean_tracking_error']:.6f}")
    print(f"Max Tracking Error:          {stats_dict['max_tracking_error']:.6f}")
    print(f"Std Tracking Error:          {stats_dict['std_tracking_error']:.6f}")
    print(f"Tube Violation Rate:         {stats_dict['tube_violation_rate']*100:.2f}%")

    print("\n--- Computation Time ---")
    print(f"Mean Solve Time:             {stats_dict['mean_solve_time_ms']:.3f} ms")
    print(f"Median Solve Time:           {stats_dict['median_solve_time_ms']:.3f} ms")
    print(f"P90 Solve Time:              {stats_dict['p90_solve_time_ms']:.3f} ms")
    print(f"Max Solve Time:              {stats_dict['max_solve_time_ms']:.3f} ms")

    print("\n--- Regret Analysis ---")
    print(f"Cumulative Dynamic Regret:   {stats_dict['cumulative_dynamic_regret']:.4f}")
    print(f"Average Dynamic Regret:      {stats_dict['average_dynamic_regret']:.6f}")
    print(f"Cumulative Safe Regret:      {stats_dict['cumulative_safe_regret']:.4f}")
    print(f"Average Safe Regret:         {stats_dict['average_safe_regret']:.6f}")

    # Check o(T) regret growth
    T = stats_dict['total_steps']
    regret_per_sqrt_T = stats_dict['average_dynamic_regret'] / np.sqrt(T)
    print(f"Regret/√T:                   {regret_per_sqrt_T:.6f}")
    if regret_per_sqrt_T < 1.0:
        print(f"                             ✓ o(√T) growth confirmed")

    print("\n" + "="*60)

def create_plots(df, output_dir):
    """Create visualization plots."""
    os.makedirs(output_dir, exist_ok=True)

    # Set style
    sns.set_style("whitegrid")
    plt.rcParams['figure.figsize'] = (12, 6)

    # 1. Tracking Error Over Time
    plt.figure()
    plt.plot(df['timestamp'], df['tracking_error_norm'], linewidth=0.5)
    plt.fill_between(df['timestamp'],
                     df['tracking_error_norm'],
                     df['tube_radius'],
                     where=(df['tracking_error_norm'] > df['tube_radius']),
                     color='red', alpha=0.3, label='Tube Violation')
    plt.xlabel('Time (s)')
    plt.ylabel('Tracking Error Norm')
    plt.title('Tracking Error Over Time')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(f'{output_dir}/tracking_error.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_dir}/tracking_error.png")
    plt.close()

    # 2. MPC Solve Time
    plt.figure()
    plt.plot(df['timestamp'], df['mpc_solve_time'], linewidth=0.5, alpha=0.7)
    plt.axhline(df['mpc_solve_time'].mean(), color='r', linestyle='--',
                label=f"Mean: {df['mpc_solve_time'].mean():.2f} ms")
    plt.axhline(df['mpc_solve_time'].quantile(0.9), color='orange', linestyle='--',
                label=f"P90: {df['mpc_solve_time'].quantile(0.9):.2f} ms")
    plt.xlabel('Time (s)')
    plt.ylabel('Solve Time (ms)')
    plt.title('MPC Solve Time Over Time')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(f'{output_dir}/solve_time.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_dir}/solve_time.png")
    plt.close()

    # 3. Cumulative Regret
    plt.figure()
    cumulative_dynamic = df['regret_dynamic'].cumsum()
    cumulative_safe = df['regret_safe'].cumsum()
    plt.plot(df['timestamp'], cumulative_dynamic, label='Dynamic Regret', linewidth=2)
    plt.plot(df['timestamp'], cumulative_safe, label='Safe Regret', linewidth=2, alpha=0.7)
    # Add √T reference
    sqrt_T_ref = np.sqrt(np.arange(1, len(df)+1)) * cumulative_dynamic.iloc[-1] / np.sqrt(len(df))
    plt.plot(df['timestamp'], sqrt_T_ref, 'k--', alpha=0.5, label='√T Reference')
    plt.xlabel('Time (s)')
    plt.ylabel('Cumulative Regret')
    plt.title('Cumulative Regret Over Time')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(f'{output_dir}/cumulative_regret.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_dir}/cumulative_regret.png")
    plt.close()

    # 4. Safety Margin Distribution
    plt.figure()
    plt.hist(df['safety_margin'], bins=50, alpha=0.7, edgecolor='black')
    plt.axvline(0, color='r', linestyle='--', linewidth=2, label='Safety Boundary')
    plt.xlabel('Safety Margin h(x)')
    plt.ylabel('Frequency')
    plt.title('Safety Margin Distribution')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(f'{output_dir}/safety_margin.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_dir}/safety_margin.png")
    plt.close()

    # 5. Control Inputs
    fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True)
    ax1.plot(df['timestamp'], df['vel'], linewidth=0.5)
    ax1.set_ylabel('Velocity (m/s)')
    ax1.set_title('Control Inputs')
    ax1.grid(True, alpha=0.3)

    ax2.plot(df['timestamp'], df['angvel'], linewidth=0.5, color='orange')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Angular Velocity (rad/s)')
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(f'{output_dir}/control_inputs.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_dir}/control_inputs.png")
    plt.close()

    # 6. Feasibility and Safety Timeline
    fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(12, 6))

    # MPC Feasibility
    feasibility_timeline = df['mpc_feasible'].astype(int)
    ax1.plot(df['timestamp'], feasibility_timeline, linewidth=0.5)
    ax1.fill_between(df['timestamp'], 0, feasibility_timeline, where=(feasibility_timeline==0),
                     color='red', alpha=0.5, label='Infeasible')
    ax1.set_ylabel('Feasible (1/0)')
    ax1.set_title('MPC Feasibility Timeline')
    ax1.set_yticks([0, 1])
    ax1.set_yticklabels(['Infeasible', 'Feasible'])
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Safety Violations
    safety_timeline = df['safety_violation'].astype(int)
    ax2.plot(df['timestamp'], safety_timeline, linewidth=0.5, color='orange')
    ax2.fill_between(df['timestamp'], 0, safety_timeline, where=(safety_timeline==1),
                     color='red', alpha=0.5, label='Violation')
    ax2.set_ylabel('Safety Violation (1/0)')
    ax2.set_title('Safety Constraint Violations')
    ax2.set_xlabel('Time (s)')
    ax2.set_yticks([0, 1])
    ax2.set_yticklabels(['Safe', 'Violation'])
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(f'{output_dir}/feasibility_safety_timeline.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_dir}/feasibility_safety_timeline.png")
    plt.close()

def generate_report(df, stats_dict, output_dir, target_delta=0.1):
    """Generate a detailed text report."""
    report_path = f'{output_dir}/metrics_report.txt'

    with open(report_path, 'w') as f:
        f.write("="*70 + "\n")
        f.write("TUBE MPC PERFORMANCE ANALYSIS REPORT\n")
        f.write("="*70 + "\n\n")

        f.write(f"Analysis Date: {pd.Timestamp.now()}\n")
        f.write(f"Data Source: {df}\n")
        f.write(f"Total Records: {len(df)}\n")
        f.write(f"Duration: {df['timestamp'].max() - df['timestamp'].min():.2f} seconds\n\n")

        f.write("-"*70 + "\n")
        f.write("SUMMARY STATISTICS\n")
        f.write("-"*70 + "\n\n")

        f.write("Safety Metrics:\n")
        f.write(f"  Empirical Risk (δ̂):       {stats_dict['empirical_risk']:.6f}\n")
        f.write(f"  Satisfaction Probability:  {stats_dict['satisfaction_prob']:.6f}\n")
        f.write(f"  95% Confidence Interval:   [{stats_dict['satisfaction_ci_lower']:.6f}, ")
        f.write(f"{stats_dict['satisfaction_ci_upper']:.6f}]\n")
        f.write(f"  Target Risk (δ):           {target_delta:.6f}\n")
        f.write(f"  Calibration Error:         {abs(stats_dict['empirical_risk'] - target_delta):.6f}\n\n")

        f.write("Feasibility Metrics:\n")
        f.write(f"  Feasibility Rate:          {stats_dict['feasibility_rate']*100:.2f}%\n")
        f.write(f"  Total MPC Calls:           {stats_dict['total_steps']}\n")
        f.write(f"  Failed Solves:             {stats_dict['mpc_failures']}\n\n")

        f.write("Tracking Performance:\n")
        f.write(f"  Mean Tracking Error:       {stats_dict['mean_tracking_error']:.6f}\n")
        f.write(f"  Max Tracking Error:        {stats_dict['max_tracking_error']:.6f}\n")
        f.write(f"  Std Tracking Error:        {stats_dict['std_tracking_error']:.6f}\n")
        f.write(f"  Tube Violation Rate:       {stats_dict['tube_violation_rate']*100:.2f}%\n\n")

        f.write("Computation:\n")
        f.write(f"  Mean Solve Time:           {stats_dict['mean_solve_time_ms']:.3f} ms\n")
        f.write(f"  Median Solve Time:         {stats_dict['median_solve_time_ms']:.3f} ms\n")
        f.write(f"  P90 Solve Time:            {stats_dict['p90_solve_time_ms']:.3f} ms\n")
        f.write(f"  Max Solve Time:            {stats_dict['max_solve_time_ms']:.3f} ms\n\n")

        f.write("Regret Analysis:\n")
        f.write(f"  Cumulative Dynamic Regret: {stats_dict['cumulative_dynamic_regret']:.6f}\n")
        f.write(f"  Average Dynamic Regret:    {stats_dict['average_dynamic_regret']:.6f}\n")
        f.write(f"  Cumulative Safe Regret:    {stats_dict['cumulative_safe_regret']:.6f}\n")
        f.write(f"  Average Safe Regret:       {stats_dict['average_safe_regret']:.6f}\n\n")

        # Check against paper criteria
        f.write("-"*70 + "\n")
        f.write("ACCEPTANCE CRITERIA CHECK\n")
        f.write("-"*70 + "\n\n")

        checks = {
            "Satisfaction Probability > 0.90": stats_dict['satisfaction_prob'] > 0.90,
            "Calibration Error < 0.10": abs(stats_dict['empirical_risk'] - target_delta) < 0.10,
            "Feasibility Rate > 0.99": stats_dict['feasibility_rate'] > 0.99,
            "P90 Solve Time < 10 ms": stats_dict['p90_solve_time_ms'] < 10.0,
            "Regret Growth o(T)": (stats_dict['average_dynamic_regret'] / np.sqrt(stats_dict['total_steps'])) < 1.0,
        }

        for criterion, passed in checks.items():
            status = "✓ PASS" if passed else "✗ FAIL"
            f.write(f"  [{status}] {criterion}\n")

        f.write("\n" + "="*70 + "\n")

    print(f"✓ Saved: {report_path}")

def main():
    parser = argparse.ArgumentParser(description='Analyze Tube MPC metrics')
    parser.add_argument('--csv', type=str, default='/tmp/tube_mpc_metrics.csv',
                        help='Path to metrics CSV file')
    parser.add_argument('--output', type=str, default='/tmp/metrics_analysis',
                        help='Output directory for plots and reports')
    parser.add_argument('--target-delta', type=float, default=0.1,
                        help='Target risk level δ (default: 0.1)')

    args = parser.parse_args()

    print("=== Tube MPC Metrics Analysis Tool ===\n")

    # Load data
    try:
        df = load_metrics(args.csv)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1

    # Compute statistics
    stats = compute_statistics(df)

    # Print statistics
    print_statistics(stats, args.target_delta)

    # Create plots
    print("\nGenerating plots...")
    create_plots(df, args.output)

    # Generate report
    print("\nGenerating report...")
    generate_report(df, stats, args.output, args.target_delta)

    print(f"\n✓ Analysis complete! Results saved to: {args.output}")
    print("\nGenerated files:")
    print(f"  - {args.output}/tracking_error.png")
    print(f"  - {args.output}/solve_time.png")
    print(f"  - {args.output}/cumulative_regret.png")
    print(f"  - {args.output}/safety_margin.png")
    print(f"  - {args.output}/control_inputs.png")
    print(f"  - {args.output}/feasibility_safety_timeline.png")
    print(f"  - {args.output}/metrics_report.txt")

    return 0

if __name__ == '__main__':
    exit(main())
