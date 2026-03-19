#!/usr/bin/env python3
"""
Simplified trajectory analysis script
Generates visualization from Tube MPC navigation data
"""

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import sys

# Set publication quality
plt.rcParams['figure.dpi'] = 150
plt.rcParams['font.size'] = 10

def analyze_trajectory(csv_file, output_file='trajectory_analysis.png'):
    """Generate trajectory analysis plot"""

    # Read data
    df = pd.read_csv(csv_file)
    print(f"✓ Loaded {len(df)} records from {csv_file}")

    # Create figure
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    fig.suptitle('Tube MPC Navigation Analysis', fontsize=14, fontweight='bold')

    # Convert to numpy arrays for compatibility
    x = df['x'].values if 'x' in df.columns else np.zeros(len(df))
    y = df['y'].values if 'y' in df.columns else np.zeros(len(df))
    elapsed = df['elapsed_time'].values if 'elapsed_time' in df.columns else np.arange(len(df)) / 10.0

    # 1. Trajectory plot
    ax1 = axes[0, 0]
    ax1.plot(x, y, 'b-', linewidth=2, label='Actual Path')
    ax1.plot(x[0], y[0], 'g^', markersize=15, label='Start')
    ax1.plot(x[-1], y[-1], 'r*', markersize=15, label='End')
    ax1.set_xlabel('X Position (m)')
    ax1.set_ylabel('Y Position (m)')
    ax1.set_title('Robot Trajectory', fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.axis('equal')
    ax1.legend()

    # 2. Position vs time
    ax2 = axes[0, 1]
    ax2.plot(elapsed, x, 'r-', linewidth=1.5, label='X')
    ax2.plot(elapsed, y, 'b-', linewidth=1.5, label='Y')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Position (m)')
    ax2.set_title('Position vs Time', fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend()

    # 3. Velocity
    ax3 = axes[1, 0]
    if 'linear_vel' in df.columns:
        vel = df['linear_vel'].values
        ax3.plot(elapsed, vel, 'g-', linewidth=1.5)
        ax3.set_xlabel('Time (s)')
        ax3.set_ylabel('Velocity (m/s)')
        ax3.set_title('Velocity Commands', fontweight='bold')
        ax3.grid(True, alpha=0.3)
    else:
        ax3.text(0.5, 0.5, 'No velocity data', ha='center', va='center')
        ax3.set_title('Velocity Commands')

    # 4. Distance from start
    ax4 = axes[1, 1]
    distances = np.sqrt(x**2 + y**2)
    ax4.plot(elapsed, distances, 'm-', linewidth=2)
    ax4.set_xlabel('Time (s)')
    ax4.set_ylabel('Distance (m)')
    ax4.set_title('Distance from Start', fontweight='bold')
    ax4.grid(True, alpha=0.3)

    # Statistics
    total_dist = np.sum(np.sqrt(np.diff(x)**2 + np.diff(y)**2))
    final_dist = distances[-1] if len(distances) > 0 else 0
    avg_vel = total_dist / elapsed[-1] if elapsed[-1] > 0 else 0

    stats_text = f"""
Statistics:
━━━━━━━━━━━━━
Distance: {total_dist:.2f} m
Time: {elapsed[-1]:.1f} s
Avg Vel: {avg_vel:.2f} m/s
Final Dist: {final_dist:.2f} m
"""

    plt.figtext(0.02, 0.02, stats_text, fontsize=9,
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
                family='monospace')

    plt.tight_layout()
    plt.savefig(output_file, bbox_inches='tight', dpi=150)
    print(f"✓ Saved plot to {output_file}")
    plt.close()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 simple_analyze.py <csv_file> [output_file]")
        sys.exit(1)

    csv_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'trajectory_analysis.png'

    analyze_trajectory(csv_file, output_file)
