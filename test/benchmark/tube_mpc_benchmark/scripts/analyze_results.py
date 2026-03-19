#!/usr/bin/env python3
"""
Post-processing analysis tool for Tube MPC benchmark data
Generates publication-quality figures for manuscripts
"""

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from matplotlib.gridspec import GridSpec
import argparse
import os

# Set publication quality parameters
plt.rcParams['figure.dpi'] = 300
plt.rcParams['savefig.dpi'] = 300
plt.rcParams['font.size'] = 10
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.serif'] = ['Times New Roman']

class TubeMPCAnalyzer:
    def __init__(self, csv_file=None):
        """Initialize analyzer with data file"""
        if csv_file and os.path.exists(csv_file):
            self.df = pd.read_csv(csv_file)
            print(f"✓ Loaded {len(self.df)} records from {csv_file}")
        else:
            self.df = None
            print("No data file provided or file not found")

    def generate_trajectory_plot(self, output_file='trajectory_analysis.png'):
        """Generate comprehensive trajectory analysis plot"""
        if self.df is None or 'x' not in self.df.columns:
            print("No trajectory data available")
            return

        fig = plt.figure(figsize=(16, 10))
        gs = GridSpec(3, 3, figure=fig, hspace=0.3, wspace=0.3)

        fig.suptitle('Tube MPC Navigation Analysis', fontsize=16, fontweight='bold')

        # 1. Main trajectory plot
        ax1 = fig.add_subplot(gs[0:2, 0:2])
        ax1.plot(self.df['x'].values, self.df['y'].values, 'b-', linewidth=2, label='Actual Path')
        ax1.plot(self.df['x'].iloc[0], self.df['y'].iloc[0], 'g^', markersize=15, label='Start')
        ax1.plot(self.df['x'].iloc[-1], self.df['y'].iloc[-1], 'r*', markersize=15, label='End')
        ax1.set_xlabel('X Position (m)', fontsize=12)
        ax1.set_ylabel('Y Position (m)', fontsize=12)
        ax1.set_title('Robot Trajectory', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3)
        ax1.axis('equal')
        ax1.legend()

        # Add goal if available
        if 'goal_x' in self.df.columns and 'goal_y' in self.df.columns:
            goal_x = self.df['goal_x'].iloc[0]
            goal_y = self.df['goal_y'].iloc[0]
            if not pd.isna(goal_x) and not pd.isna(goal_y):
                ax1.plot(goal_x, goal_y, 'rx', markersize=12, markeredgewidth=3)
                ax1.plot(goal_x, goal_y, 'r+', markersize=20, label='Goal')
                ax1.legend()

        # 2. Position vs time
        ax2 = fig.add_subplot(gs[0, 2])
        ax2.plot(self.df['elapsed_time'], self.df['x'], 'r-', linewidth=1.5, label='X')
        ax2.plot(self.df['elapsed_time'], self.df['y'], 'b-', linewidth=1.5, label='Y')
        ax2.set_xlabel('Time (s)', fontsize=10)
        ax2.set_ylabel('Position (m)', fontsize=10)
        ax2.set_title('Position vs Time', fontsize=12, fontweight='bold')
        ax2.grid(True, alpha=0.3)
        ax2.legend()

        # 3. Velocity components
        ax3 = fig.add_subplot(gs[1, 2])
        if 'linear_vel' in self.df.columns:
            ax3.plot(self.df['elapsed_time'], self.df['linear_vel'], 'b-', linewidth=1.5, label='Linear')
        if 'angular_vel' in self.df.columns:
            ax3_twin = ax3.twinx()
            ax3_twin.plot(self.df['elapsed_time'], self.df['angular_vel'], 'r-', linewidth=1.5, label='Angular')
            ax3_twin.set_ylabel('Angular Vel (rad/s)', fontsize=10)
            ax3_twin.legend(loc='upper right')

        ax3.set_xlabel('Time (s)', fontsize=10)
        ax3.set_ylabel('Linear Vel (m/s)', fontsize=10)
        ax3.set_title('Velocity Commands', fontsize=12, fontweight='bold')
        ax3.grid(True, alpha=0.3)
        ax3.legend(loc='upper left')

        # 4. Distance from start
        ax4 = fig.add_subplot(gs[2, 0])
        distances = np.sqrt(self.df['x']**2 + self.df['y']**2)
        ax4.plot(self.df['elapsed_time'], distances, 'g-', linewidth=2)
        ax4.set_xlabel('Time (s)', fontsize=10)
        ax4.set_ylabel('Distance (m)', fontsize=10)
        ax4.set_title('Distance from Start', fontsize=12, fontweight='bold')
        ax4.grid(True, alpha=0.3)

        # 5. Instantaneous velocity
        ax5 = fig.add_subplot(gs[2, 1])
        if len(distances) > 1:
            dt = np.diff(self.df['elapsed_time'])
            dx = np.diff(distances)
            inst_vel = np.divide(dx, dt, out=np.zeros_like(dx), where=dt!=0)
            ax5.plot(self.df['elapsed_time'][1:], inst_vel, 'm-', linewidth=2)
        ax5.set_xlabel('Time (s)', fontsize=10)
        ax5.set_ylabel('Velocity (m/s)', fontsize=10)
        ax5.set_title('Instantaneous Velocity', fontsize=12, fontweight='bold')
        ax5.grid(True, alpha=0.3)

        # 6. Statistics summary
        ax6 = fig.add_subplot(gs[2, 2])
        ax6.axis('off')

        # Calculate statistics
        total_dist = distances.iloc[-1] if len(distances) > 0 else 0
        total_time = self.df['elapsed_time'].iloc[-1] if len(self.df) > 0 else 0
        avg_vel = total_dist / total_time if total_time > 0 else 0

        stats_text = f"""
Statistics:
━━━━━━━━━━━━━━━━━━━━━
Total Distance: {total_dist:.2f} m
Total Time: {total_time:.1f} s
Avg Velocity: {avg_vel:.2f} m/s

Final Position:
  X: {self.df['x'].iloc[-1]:.2f} m
  Y: {self.df['y'].iloc[-1]:.2f} m

Path Quality:
  Points: {len(self.df)}
  Sample Rate: {len(self.df)/total_time:.1f} Hz
"""

        ax6.text(0.1, 0.5, stats_text, transform=ax6.transAxes,
                fontsize=10, verticalalignment='center',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
                family='monospace')

        plt.savefig(output_file, bbox_inches='tight', dpi=300)
        print(f"✓ Saved trajectory plot to {output_file}")
        plt.close()

    def generate_performance_comparison(self, controllers_data, output_file='controller_comparison.png'):
        """Generate controller comparison plots

        Args:
            controllers_data: dict of {controller_name: csv_file}
        """
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Controller Performance Comparison', fontsize=16, fontweight='bold')

        # Collect data
        all_data = {}
        for name, csv_file in controllers_data.items():
            if os.path.exists(csv_file):
                df = pd.read_csv(csv_file)
                all_data[name] = df

        if not all_data:
            print("No valid data files found")
            return

        # 1. Success rate comparison
        ax1 = axes[0, 0]
        success_rates = []
        names = []
        for name, df in all_data.items():
            if 'success' in df.columns:
                rate = df['success'].mean() * 100
                success_rates.append(rate)
                names.append(name)

        bars = ax1.bar(names, success_rates, color=['#2ecc71', '#3498db', '#e74c3c', '#f39c12'])
        ax1.set_ylabel('Success Rate (%)', fontsize=12)
        ax1.set_title('Navigation Success Rate', fontsize=14, fontweight='bold')
        ax1.set_ylim(0, 100)
        ax1.grid(True, alpha=0.3, axis='y')

        # Add value labels on bars
        for bar, rate in zip(bars, success_rates):
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height,
                    f'{rate:.1f}%', ha='center', va='bottom', fontsize=11, fontweight='bold')

        # 2. Completion time distribution
        ax2 = axes[0, 1]
        for name, df in all_data.items():
            if 'elapsed_time' in df.columns:
                df['elapsed_time'].hist(alpha=0.6, label=name, ax=ax2, bins=20)

        ax2.set_xlabel('Completion Time (s)', fontsize=12)
        ax2.set_ylabel('Frequency', fontsize=12)
        ax2.set_title('Completion Time Distribution', fontsize=14, fontweight='bold')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        # 3. Position error comparison
        ax3 = axes[1, 0]
        position_errors = []
        for name, df in all_data.items():
            if 'position_error' in df.columns:
                errors = df[df['success'] == True]['position_error']
                if len(errors) > 0:
                    position_errors.append(errors)
                else:
                    position_errors.append(pd.Series([np.nan]))
            else:
                position_errors.append(pd.Series([np.nan]))

        bp = ax3.boxplot(position_errors, labels=names, patch_artist=True)
        colors = ['#2ecc71', '#3498db', '#e74c3c', '#f39c12']
        for patch, color in zip(bp['boxes'], colors[:len(names)]):
            patch.set_facecolor(color)

        ax3.set_ylabel('Position Error (m)', fontsize=12)
        ax3.set_title('Final Position Error', fontsize=14, fontweight='bold')
        ax3.grid(True, alpha=0.3, axis='y')

        # 4. Performance summary table
        ax4 = axes[1, 1]
        ax4.axis('off')

        # Build summary table
        summary_data = []
        for name, df in all_data.items():
            row = {'Controller': name}
            if 'success' in df.columns:
                row['Success %'] = f"{df['success'].mean()*100:.1f}"
            if 'elapsed_time' in df.columns:
                successful = df[df['success'] == True]['elapsed_time']
                row['Avg Time'] = f"{successful.mean():.1f}" if len(successful) > 0 else "N/A"
            if 'position_error' in df.columns:
                successful = df[df['success'] == True]['position_error']
                row['Avg Error'] = f"{successful.mean():.3f}" if len(successful) > 0 else "N/A"
            summary_data.append(row)

        # Create table
        table_data = []
        headers = ['Controller', 'Success %', 'Avg Time', 'Avg Error']
        for row in summary_data:
            table_data.append([row.get(h, 'N/A') for h in headers])

        table = ax4.table(cellText=table_data, colLabels=headers,
                         cellLoc='center', loc='center',
                         bbox=[0, 0, 1, 1])
        table.auto_set_font_size(False)
        table.set_fontsize(11)
        table.scale(1, 2)

        # Style header row
        for i in range(len(headers)):
            table[(0, i)].set_facecolor('#4472C4')
            table[(0, i)].set_text_props(weight='bold', color='white')

        ax4.set_title('Performance Summary', fontsize=14, fontweight='bold', pad=20)

        plt.tight_layout()
        plt.savefig(output_file, bbox_inches='tight', dpi=300)
        print(f"✓ Saved comparison plot to {output_file}")
        plt.close()

    def generate_mpc_metrics_plot(self, mpc_csv_file='/tmp/tube_mpc_metrics.csv', output_file='mpc_metrics.png'):
        """Generate MPC-specific metrics visualization"""
        if not os.path.exists(mpc_csv_file):
            print(f"MPC metrics file not found: {mpc_csv_file}")
            return

        df = pd.read_csv(mpc_csv_file)

        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Tube MPC Performance Metrics', fontsize=16, fontweight='bold')

        # 1. Solve time distribution
        ax1 = axes[0, 0]
        if 'solve_time_ms' in df.columns:
            df['solve_time_ms'].hist(bins=30, ax=ax1, color='steelblue', edgecolor='black')
            ax1.axvline(df['solve_time_ms'].mean(), color='red', linestyle='--', linewidth=2, label=f"Mean: {df['solve_time_ms'].mean():.1f} ms")
            ax1.set_xlabel('Solve Time (ms)', fontsize=12)
            ax1.set_ylabel('Frequency', fontsize=12)
            ax1.set_title('MPC Optimization Time', fontsize=14, fontweight='bold')
            ax1.legend()
            ax1.grid(True, alpha=0.3)

        # 2. Empirical risk
        ax2 = axes[0, 1]
        if 'empirical_risk' in df.columns:
            ax2.plot(df.index, df['empirical_risk'], 'b-', linewidth=1.5)
            ax2.set_xlabel('Time Step', fontsize=12)
            ax2.set_ylabel('Empirical Risk', fontsize=12)
            ax2.set_title('Data-Driven Risk Evolution', fontsize=14, fontweight='bold')
            ax2.grid(True, alpha=0.3)

        # 3. Feasibility rate
        ax3 = axes[1, 0]
        if 'feasibility_rate' in df.columns:
            ax3.plot(df.index, df['feasibility_rate'] * 100, 'g-', linewidth=1.5)
            ax3.set_xlabel('Time Step', fontsize=12)
            ax3.set_ylabel('Feasibility Rate (%)', fontsize=12)
            ax3.set_title('Optimization Feasibility', fontsize=14, fontweight='bold')
            ax3.set_ylim(0, 105)
            ax3.grid(True, alpha=0.3)

        # 4. Tracking error
        ax4 = axes[1, 1]
        if 'tracking_error' in df.columns:
            ax4.plot(df.index, df['tracking_error'], 'r-', linewidth=1.5, label='Tracking Error')
            if 'tube_radius' in df.columns:
                ax4.axhline(df['tube_radius'].iloc[0], color='blue', linestyle='--', linewidth=2, label='Tube Radius')
            ax4.set_xlabel('Time Step', fontsize=12)
            ax4.set_ylabel('Error (m)', fontsize=12)
            ax4.set_title('Tracking Performance', fontsize=14, fontweight='bold')
            ax4.legend()
            ax4.grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_file, bbox_inches='tight', dpi=300)
        print(f"✓ Saved MPC metrics plot to {output_file}")
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Analyze Tube MPC benchmark results')
    parser.add_argument('--csv', type=str, help='CSV file with trajectory data')
    parser.add_argument('--mpc-csv', type=str, default='/tmp/tube_mpc_metrics.csv', help='MPC metrics CSV file')
    parser.add_argument('--output', type=str, default='tube_mpc_analysis.png', help='Output file name')
    parser.add_argument('--compare', nargs='+', help='CSV files for controller comparison')

    args = parser.parse_args()

    analyzer = TubeMPCAnalyzer(args.csv)

    # Generate trajectory plot
    if args.csv:
        analyzer.generate_trajectory_plot(args.output)

    # Generate MPC metrics plot
    analyzer.generate_mpc_metrics_plot(args.mpc_csv)

    # Generate comparison plot
    if args.compare and len(args.compare) > 1:
        comparison_files = {
            f'Controller_{i+1}': csv_file
            for i, csv_file in enumerate(args.compare)
        }
        analyzer.generate_performance_comparison(comparison_files, 'controller_comparison.png')

if __name__ == '__main__':
    main()
