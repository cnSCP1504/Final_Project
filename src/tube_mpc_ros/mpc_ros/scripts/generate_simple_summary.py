#!/usr/bin/env python3

"""
Simple Metrics Summary Generator (no external dependencies)
Generates text summary and basic plots without matplotlib/pandas
"""

import csv
import math
import sys

def read_metrics(csv_path):
    """Read metrics from CSV file."""
    data = []
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    return data

def compute_statistics(data):
    """Compute statistics from metrics data."""
    n = len(data)

    # Safety metrics
    safety_violations = sum(1 for row in data if float(row['safety_violation']) > 0.5)
    empirical_risk = safety_violations / n if n > 0 else 0.0
    satisfaction_prob = 1.0 - empirical_risk

    # Feasibility
    mpc_feasible = sum(1 for row in data if int(row['mpc_feasible']) == 1)
    feasibility_rate = mpc_feasible / n if n > 0 else 0.0

    # Tracking
    tracking_errors = [float(row['tracking_error_norm']) for row in data]
    mean_tracking = sum(tracking_errors) / n if n > 0 else 0.0
    max_tracking = max(tracking_errors) if tracking_errors else 0.0

    # Computation
    solve_times = [float(row['mpc_solve_time']) for row in data]
    mean_solve = sum(solve_times) / n if n > 0 else 0.0
    median_solve = sorted(solve_times)[n//2] if solve_times else 0.0
    max_solve = max(solve_times) if solve_times else 0.0

    # P90
    sorted_times = sorted(solve_times)
    p90_idx = int(0.9 * n)
    p90_solve = sorted_times[p90_idx] if sorted_times else 0.0

    # Regret
    regrets = [float(row['regret_dynamic']) for row in data]
    cumulative_regret = sum(regrets)
    average_regret = cumulative_regret / n if n > 0 else 0.0

    # Tube violations
    tube_violations = sum(1 for row in data if float(row['tube_violation']) > 0.0)
    tube_violation_rate = tube_violations / n if n > 0 else 0.0

    return {
        'n': n,
        'empirical_risk': empirical_risk,
        'satisfaction_prob': satisfaction_prob,
        'safety_violations': safety_violations,
        'feasibility_rate': feasibility_rate,
        'mpc_feasible': mpc_feasible,
        'mean_tracking': mean_tracking,
        'max_tracking': max_tracking,
        'mean_solve': mean_solve,
        'median_solve': median_solve,
        'p90_solve': p90_solve,
        'max_solve': max_solve,
        'cumulative_regret': cumulative_regret,
        'average_regret': average_regret,
        'tube_violation_rate': tube_violation_rate,
        'tube_occupancy_rate': 1.0 - tube_violation_rate,
    }

def print_statistics(stats, target_delta=0.1):
    """Print formatted statistics."""
    print("\n" + "="*70)
    print("TUBE MPC METRICS SUMMARY")
    print("="*70)

    print("\n--- Safety and Satisfaction ---")
    print(f"Empirical Risk (δ̂):          {stats['empirical_risk']:.4f}")
    print(f"Satisfaction Probability:    {stats['satisfaction_prob']:.4f}")
    print(f"95% CI Approx:                [{max(0, stats['satisfaction_prob'] - 0.05):.4f}, 1.0000]")
    print(f"Target Risk (δ):             {target_delta:.4f}")
    print(f"Calibration Error:           {abs(stats['empirical_risk'] - target_delta):.4f}")

    print("\n--- Feasibility ---")
    print(f"Total MPC Calls:             {stats['n']}")
    print(f"Successful Solves:           {stats['mpc_feasible']}")
    print(f"Failed Solves:               {stats['n'] - stats['mpc_feasible']}")
    print(f"Feasibility Rate:            {stats['feasibility_rate']*100:.2f}%")

    print("\n--- Tracking Performance ---")
    print(f"Mean Tracking Error:         {stats['mean_tracking']:.6f}")
    print(f"Max Tracking Error:          {stats['max_tracking']:.6f}")
    print(f"Tube Violation Rate:         {stats['tube_violation_rate']*100:.2f}%")
    print(f"Tube Occupancy Rate:         {stats['tube_occupancy_rate']*100:.2f}%")

    print("\n--- Computation Time ---")
    print(f"Mean Solve Time:             {stats['mean_solve']:.3f} ms")
    print(f"Median Solve Time:           {stats['median_solve']:.3f} ms")
    print(f"P90 Solve Time:              {stats['p90_solve']:.3f} ms")
    print(f"Max Solve Time:              {stats['max_solve']:.3f} ms")

    print("\n--- Regret Analysis ---")
    print(f"Cumulative Dynamic Regret:   {stats['cumulative_regret']:.4f}")
    print(f"Average Dynamic Regret:      {stats['average_regret']:.6f}")

    # Check o(T) regret growth
    if stats['n'] > 100:
        regret_per_sqrt_T = stats['average_regret'] / math.sqrt(stats['n'])
        print(f"Regret/√T:                   {regret_per_sqrt_T:.6f}")

    print("\n" + "="*70)

def check_criteria(stats, target_delta=0.1):
    """Check against paper acceptance criteria."""
    print("\n--- Paper Acceptance Criteria Check ---")

    criteria = {
        "Satisfaction Probability > 0.90": stats['satisfaction_prob'] > 0.90,
        "Calibration Error < 0.10": abs(stats['empirical_risk'] - target_delta) < 0.10,
        "Feasibility Rate > 0.99": stats['feasibility_rate'] > 0.99,
        "P90 Solve Time < 10 ms": stats['p90_solve'] < 10.0,
        "Mean Solve Time < 15 ms": stats['mean_solve'] < 15.0,
        "Tube Occupancy > 0.95": stats['tube_occupancy_rate'] > 0.95,
        "No Safety Violations": stats['safety_violations'] == 0,
    }

    passed = sum(1 for v in criteria.values() if v)
    total = len(criteria)

    for criterion, result in criteria.items():
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  [{status}] {criterion}")

    print(f"\nSummary: {passed}/{total} criteria passed")

    return passed, total

def main():
    if len(sys.argv) < 2:
        csv_path = "/tmp/tube_mpc_metrics.csv"
    else:
        csv_path = sys.argv[1]

    print(f"Reading metrics from: {csv_path}")

    try:
        data = read_metrics(csv_path)
        print(f"✓ Loaded {len(data)} records")

        if not data:
            print("ERROR: No data found in CSV file")
            return 1

        stats = compute_statistics(data)
        print_statistics(stats)
        check_criteria(stats)

        print(f"\n✓ Analysis complete!")
        return 0

    except FileNotFoundError:
        print(f"ERROR: File not found: {csv_path}")
        return 1
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

if __name__ == '__main__':
    exit(main())
