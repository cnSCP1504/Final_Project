#!/usr/bin/env python3
"""
Tube MPC Metrics Analysis Script (Simple version - no plots)

Analyzes metrics CSV files and generates text reports.
Usage: ./analyze_metrics_simple.py [csv_file]
"""

import pandas as pd
import numpy as np
import sys

def analyze_metrics(csv_file='/tmp/tube_mpc_metrics.csv'):
    """Analyze metrics and generate report"""

    # Load data
    print(f"Loading metrics from: {csv_file}")
    df = pd.read_csv(csv_file)

    print(f"\n{'='*60}")
    print(f"Tube MPC Performance Analysis Report")
    print(f"{'='*60}")
    print(f"Total time steps: {len(df)}")
    print(f"Duration: {df['timestamp'].max():.2f} seconds")
    print(f"Control frequency: {len(df) / df['timestamp'].max():.2f} Hz")

    # Tracking Error Analysis
    print(f"\n{'='*60}")
    print("Tracking Error Analysis")
    print(f"{'='*60}")
    print(f"Cross-Track Error (CTE):")
    print(f"  Mean:   {df['cte'].mean():.6f} m")
    print(f"  Std:    {df['cte'].std():.6f} m")
    print(f"  Max:    {df['cte'].abs().max():.6f} m")
    print(f"  RMS:    {np.sqrt((df['cte']**2).mean()):.6f} m")

    print(f"\nHeading Error (etheta):")
    print(f"  Mean:   {df['etheta'].mean():.6f} rad ({np.degrees(df['etheta'].mean()):.2f}°)")
    print(f"  Std:    {df['etheta'].std():.6f} rad ({np.degrees(df['etheta'].std()):.2f}°)")
    print(f"  Max:    {df['etheta'].abs().max():.6f} rad ({np.degrees(df['etheta'].abs().max()):.2f}°)")

    # MPC Performance
    print(f"\n{'='*60}")
    print("MPC Performance")
    print(f"{'='*60}")
    print(f"Solve Time:")
    print(f"  Mean:   {df['mpc_solve_time'].mean():.2f} ms")
    print(f"  Median: {df['mpc_solve_time'].median():.2f} ms")
    print(f"  Std:    {df['mpc_solve_time'].std():.2f} ms")
    print(f"  Max:    {df['mpc_solve_time'].max():.2f} ms")
    print(f"  P95:    {df['mpc_solve_time'].quantile(0.95):.2f} ms")

    print(f"\nFeasibility:")
    feasible_count = df['mpc_feasible'].sum()
    print(f"  Feasible solves:  {feasible_count}/{len(df)} ({100*feasible_count/len(df):.1f}%)")
    print(f"  Failed solves:    {len(df) - feasible_count}/{len(df)} ({100*(1-feasible_count/len(df)):.1f}%)")

    # Control Performance
    print(f"\n{'='*60}")
    print("Control Performance")
    print(f"{'='*60}")
    print(f"Linear Velocity:")
    print(f"  Mean:   {df['vel'].mean():.3f} m/s")
    print(f"  Std:    {df['vel'].std():.3f} m/s")
    print(f"  Max:    {df['vel'].max():.3f} m/s")

    print(f"\nAngular Velocity:")
    print(f"  Mean:   {df['angvel'].mean():.3f} rad/s ({np.degrees(df['angvel'].mean()):.2f}°/s)")
    print(f"  Std:    {df['angvel'].std():.3f} rad/s ({np.degrees(df['angvel'].std()):.2f}°/s)")
    print(f"  Max:    {df['angvel'].abs().max():.3f} rad/s ({np.degrees(df['angvel'].abs().max()):.2f}°/s)")

    # Safety Analysis
    print(f"\n{'='*60}")
    print("Safety Analysis")
    print(f"{'='*60}")
    print(f"Tube Violations:")
    violations = df['tube_violation'].sum()
    print(f"  Total violations:  {violations}/{len(df)} ({100*violations/len(df):.1f}%)")

    print(f"\nSafety Margin:")
    print(f"  Mean:   {df['safety_margin'].mean():.3f} m")
    print(f"  Min:    {df['safety_margin'].min():.3f} m")
    print(f"  Std:    {df['safety_margin'].std():.3f} m")

    print(f"\nSafety Violations:")
    safety_violations = df['safety_violation'].sum()
    print(f"  Total violations:  {safety_violations}/{len(df)} ({100*safety_violations/len(df):.1f}%)")

    # Regret Analysis
    print(f"\n{'='*60}")
    print("Regret Analysis")
    print(f"{'='*60}")
    print(f"Dynamic Regret:")
    print(f"  Cumulative: {df['regret_dynamic'].iloc[-1]:.2f}")
    print(f"  Average:    {df['regret_dynamic'].mean():.4f} per step")

    print(f"\nSafe Regret:")
    print(f"  Cumulative: {df['regret_safe'].iloc[-1]:.2f}")
    print(f"  Average:    {df['regret_safe'].mean():.4f} per step")

    # Cost Analysis
    print(f"\n{'='*60}")
    print("Cost Analysis")
    print(f"{'='*60}")
    print(f"Stage Cost:")
    print(f"  Mean:   {df['cost'].mean():.3f}")
    print(f"  Std:    {df['cost'].std():.3f}")
    print(f"  Min:    {df['cost'].min():.3f}")
    print(f"  Max:    {df['cost'].max():.3f}")

    # Save text report
    report_file = csv_file.replace('.csv', '_report.txt')
    with open(report_file, 'w') as f:
        f.write(f"Tube MPC Performance Analysis Report\n")
        f.write(f"{'='*60}\n\n")
        f.write(f"Data file: {csv_file}\n")
        f.write(f"Total time steps: {len(df)}\n")
        f.write(f"Duration: {df['timestamp'].max():.2f} seconds\n")
        f.write(f"Control frequency: {len(df) / df['timestamp'].max():.2f} Hz\n\n")

        # Write all sections
        f.write(f"\nTracking Error Analysis\n")
        f.write(f"{'-'*60}\n")
        f.write(f"Cross-Track Error (CTE):\n")
        f.write(f"  Mean:   {df['cte'].mean():.6f} m\n")
        f.write(f"  Std:    {df['cte'].std():.6f} m\n")
        f.write(f"  Max:    {df['cte'].abs().max():.6f} m\n")
        f.write(f"  RMS:    {np.sqrt((df['cte']**2).mean()):.6f} m\n\n")

        f.write(f"Heading Error (etheta):\n")
        f.write(f"  Mean:   {df['etheta'].mean():.6f} rad ({np.degrees(df['etheta'].mean()):.2f}°)\n")
        f.write(f"  Std:    {df['etheta'].std():.6f} rad ({np.degrees(df['etheta'].std()):.2f}°)\n")
        f.write(f"  Max:    {df['etheta'].abs().max():.6f} rad ({np.degrees(df['etheta'].abs().max()):.2f}°)\n\n")

        f.write(f"\nMPC Performance\n")
        f.write(f"{'-'*60}\n")
        f.write(f"Solve Time:\n")
        f.write(f"  Mean:   {df['mpc_solve_time'].mean():.2f} ms\n")
        f.write(f"  Median: {df['mpc_solve_time'].median():.2f} ms\n")
        f.write(f"  Std:    {df['mpc_solve_time'].std():.2f} ms\n")
        f.write(f"  Max:    {df['mpc_solve_time'].max():.2f} ms\n")
        f.write(f"  P95:    {df['mpc_solve_time'].quantile(0.95):.2f} ms\n\n")

        f.write(f"Feasibility:\n")
        f.write(f"  Feasible solves:  {feasible_count}/{len(df)} ({100*feasible_count/len(df):.1f}%)\n")
        f.write(f"  Failed solves:    {len(df) - feasible_count}/{len(df)} ({100*(1-feasible_count/len(df)):.1f}%)\n\n")

        f.write(f"\nControl Performance\n")
        f.write(f"{'-'*60}\n")
        f.write(f"Linear Velocity:\n")
        f.write(f"  Mean:   {df['vel'].mean():.3f} m/s\n")
        f.write(f"  Std:    {df['vel'].std():.3f} m/s\n")
        f.write(f"  Max:    {df['vel'].max():.3f} m/s\n\n")

        f.write(f"Angular Velocity:\n")
        f.write(f"  Mean:   {df['angvel'].mean():.3f} rad/s ({np.degrees(df['angvel'].mean()):.2f}°/s)\n")
        f.write(f"  Std:    {df['angvel'].std():.3f} rad/s ({np.degrees(df['angvel'].std()):.2f}°/s)\n")
        f.write(f"  Max:    {df['angvel'].abs().max():.3f} rad/s ({np.degrees(df['angvel'].abs().max()):.2f}°/s)\n\n")

        f.write(f"\nSafety Analysis\n")
        f.write(f"{'-'*60}\n")
        f.write(f"Tube Violations:\n")
        f.write(f"  Total violations:  {violations}/{len(df)} ({100*violations/len(df):.1f}%)\n\n")

        f.write(f"Safety Margin:\n")
        f.write(f"  Mean:   {df['safety_margin'].mean():.3f} m\n")
        f.write(f"  Min:    {df['safety_margin'].min():.3f} m\n")
        f.write(f"  Std:    {df['safety_margin'].std():.3f} m\n\n")

        f.write(f"Safety Violations:\n")
        f.write(f"  Total violations:  {safety_violations}/{len(df)} ({100*safety_violations/len(df):.1f}%)\n\n")

        f.write(f"\nRegret Analysis\n")
        f.write(f"{'-'*60}\n")
        f.write(f"Dynamic Regret:\n")
        f.write(f"  Cumulative: {df['regret_dynamic'].iloc[-1]:.2f}\n")
        f.write(f"  Average:    {df['regret_dynamic'].mean():.4f} per step\n\n")

        f.write(f"Safe Regret:\n")
        f.write(f"  Cumulative: {df['regret_safe'].iloc[-1]:.2f}\n")
        f.write(f"  Average:    {df['regret_safe'].mean():.4f} per step\n\n")

        f.write(f"\nCost Analysis\n")
        f.write(f"{'-'*60}\n")
        f.write(f"Stage Cost:\n")
        f.write(f"  Mean:   {df['cost'].mean():.3f}\n")
        f.write(f"  Std:    {df['cost'].std():.3f}\n")
        f.write(f"  Min:    {df['cost'].min():.3f}\n")
        f.write(f"  Max:    {df['cost'].max():.3f}\n\n")

    print(f"\n{'='*60}")
    print(f"Report saved to: {report_file}")
    print(f"{'='*60}")
    print("Analysis complete!")

if __name__ == '__main__':
    csv_file = sys.argv[1] if len(sys.argv) > 1 else '/tmp/tube_mpc_metrics.csv'
    analyze_metrics(csv_file)
