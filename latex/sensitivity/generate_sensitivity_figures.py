#!/usr/bin/env python3
"""
Generate figures and CSV files from sensitivity analysis report.
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import os

# Fix for Chinese font display
# Use SimHei or a fallback that supports Chinese
try:
    # Try to use a Chinese font
    plt.rcParams['font.sans-serif'] = ['SimHei', 'WenQuanYi Micro Hei', 'Noto Sans CJK SC', 'DejaVu Sans']
    plt.rcParams['axes.unicode_minus'] = False
except:
    pass

# Output directory
OUTPUT_DIR = "/home/dixon/Final_Project/catkin/latex/sensitivity"

# Data from the report

# 4.1 MPC预测时域 N 数据
N_values = [10, 15, 20, 25, 30]
N_data = {
    'N': N_values,
    '经验风险(%)': [1.78, 1.57, 2.51, 1.56, 2.88],
    '可行性率(%)': [100.0, 100.0, 100.0, 100.0, 100.0],
    '求解时间(ms)': [12.0, 12.0, 13.0, 13.0, 13.0],
    '跟踪误差(m)': [0.388, 0.409, 0.698, 0.405, 0.448],
    '校准误差(%)': [3.22, 3.43, 2.49, 3.44, 2.12]
}

# 4.2 STL温度参数 τ 数据
tau_values = [0.01, 0.05, 0.1, 0.5, 1.0]
tau_data = {
    'τ': tau_values,
    '经验风险(%)': [2.93, 1.75, 1.58, 0.79, 2.56],
    '可行性率(%)': [100.0, 100.0, 100.0, 100.0, 100.0],
    '求解时间(ms)': [13.0, 13.0, 13.0, 13.0, 13.0],
    '跟踪误差(m)': [0.457, 0.431, 0.412, 0.427, 0.439],
    '校准误差(%)': [2.07, 3.25, 3.42, 4.21, 2.44]
}

# 4.3 DR残差窗口大小 M 数据
M_values = [50, 100, 200, 300, 500]
M_data = {
    'M': M_values,
    '经验风险(%)': [1.51, 1.66, 1.75, 1.58, 1.55],
    '可行性率(%)': [100.0, 100.0, 100.0, 100.0, 100.0],
    '求解时间(ms)': [13.0, 13.0, 13.0, 12.0, 13.0],
    '跟踪误差(m)': [0.465, 0.443, 0.394, 0.404, 0.426],
    '校准误差(%)': [3.49, 3.34, 3.25, 3.42, 3.45]
}

# 敏感性指标汇总
N_sensitivity = {
    '指标': ['经验风险', '可行性率', '求解时间', '跟踪误差', '校准误差'],
    '均值': ['2.06%', '100.0%', '12.6ms', '0.469m', '2.94%'],
    '标准差': ['0.60%', '0.0%', '0.55ms', '0.129m', '0.60%'],
    '最小值': ['1.56%', '100.0%', '12.0ms', '0.388m', '2.12%'],
    '最大值': ['2.88%', '100.0%', '13.0ms', '0.698m', '3.44%'],
    '变化范围': ['1.33%', '0.0%', '1.0ms', '0.310m', '1.33%']
}

tau_sensitivity = {
    '指标': ['经验风险', '可行性率', '求解时间', '跟踪误差', '校准误差'],
    '均值': ['1.92%', '100.0%', '13.0ms', '0.433m', '3.08%'],
    '标准差': ['0.85%', '0.0%', '0.0ms', '0.017m', '0.85%'],
    '最小值': ['0.79%', '100.0%', '13.0ms', '0.412m', '2.07%'],
    '最大值': ['2.93%', '100.0%', '13.0ms', '0.457m', '4.21%'],
    '变化范围': ['2.14%', '0.0%', '0.0ms', '0.045m', '2.14%']
}

M_sensitivity = {
    '指标': ['经验风险', '可行性率', '求解时间', '跟踪误差', '校准误差'],
    '均值': ['1.61%', '100.0%', '12.8ms', '0.426m', '3.39%'],
    '标准差': ['0.10%', '0.0%', '0.45ms', '0.029m', '0.10%'],
    '最小值': ['1.51%', '100.0%', '12.0ms', '0.394m', '3.25%'],
    '最大值': ['1.75%', '100.0%', '13.0ms', '0.465m', '3.49%'],
    '变化范围': ['0.24%', '0.0%', '1.0ms', '0.071m', '0.24%']
}

# 参数敏感性排序
ranking_data = {
    '排序': [1, 2, 3],
    '参数': ['τ (温度)', 'N (时域)', 'M (窗口)'],
    '经验风险影响': ['2.14% (最大)', '1.33%', '0.24% (最小)'],
    '跟踪误差影响': ['0.045m', '0.310m (最大)', '0.071m'],
    '综合影响': ['★★★★★', '★★★★☆', '★★☆☆☆']
}

# 最优参数组合
optimal_params = {
    '参数': ['N (mpc_steps)', 'τ (temperature)', 'M (window_size)'],
    '推荐值': ['10', '0.5', '50'],
    '理由': ['跟踪误差最小，求解最快', '经验风险最低，安全性能最优', '经验风险最低，计算效率高']
}

# 调优效果
tuning_effect = {
    '指标': ['经验风险', '跟踪误差', '求解时间'],
    '调优前': ['~2.5%', '~0.5m', '~13ms'],
    '调优后预期': ['~0.8%', '~0.4m', '~12ms'],
    '改善幅度': ['-68%', '-20%', '-8%']
}


def save_csv_files():
    """Save all tables as CSV files."""
    # Detailed data tables
    pd.DataFrame(N_data).to_csv(os.path.join(OUTPUT_DIR, 'N_detailed_data.csv'), index=False, encoding='utf-8-sig')
    pd.DataFrame(tau_data).to_csv(os.path.join(OUTPUT_DIR, 'tau_detailed_data.csv'), index=False, encoding='utf-8-sig')
    pd.DataFrame(M_data).to_csv(os.path.join(OUTPUT_DIR, 'M_detailed_data.csv'), index=False, encoding='utf-8-sig')

    # Sensitivity summary tables
    pd.DataFrame(N_sensitivity).to_csv(os.path.join(OUTPUT_DIR, 'N_sensitivity_summary.csv'), index=False, encoding='utf-8-sig')
    pd.DataFrame(tau_sensitivity).to_csv(os.path.join(OUTPUT_DIR, 'tau_sensitivity_summary.csv'), index=False, encoding='utf-8-sig')
    pd.DataFrame(M_sensitivity).to_csv(os.path.join(OUTPUT_DIR, 'M_sensitivity_summary.csv'), index=False, encoding='utf-8-sig')

    # Ranking and recommendations
    pd.DataFrame(ranking_data).to_csv(os.path.join(OUTPUT_DIR, 'parameter_ranking.csv'), index=False, encoding='utf-8-sig')
    pd.DataFrame(optimal_params).to_csv(os.path.join(OUTPUT_DIR, 'optimal_parameters.csv'), index=False, encoding='utf-8-sig')
    pd.DataFrame(tuning_effect).to_csv(os.path.join(OUTPUT_DIR, 'tuning_effect.csv'), index=False, encoding='utf-8-sig')

    print("CSV files saved successfully!")


def generate_figures():
    """Generate all figures as PNG files."""
    plt.rcParams['font.size'] = 12
    plt.rcParams['axes.labelsize'] = 14
    plt.rcParams['axes.titlesize'] = 16
    plt.rcParams['legend.fontsize'] = 10
    plt.rcParams['figure.figsize'] = (10, 6)

    # Color scheme
    colors = {
        'N': '#2E86AB',
        'tau': '#A23B72',
        'M': '#F18F01'
    }

    # ==================== Figure 1: Empirical Risk vs Parameters ====================
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    # N vs Empirical Risk
    ax = axes[0]
    ax.plot(N_values, N_data['经验风险(%)'], 'o-', color=colors['N'], linewidth=2, markersize=10)
    ax.axhline(y=5.0, color='r', linestyle='--', label='Target Risk delta=5%')
    ax.set_xlabel('MPC Horizon N')
    ax.set_ylabel('Empirical Risk (%)')
    ax.set_title('(a) Empirical Risk vs MPC Horizon')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(0, 4)

    # τ vs Empirical Risk
    ax = axes[1]
    ax.plot(tau_values, tau_data['经验风险(%)'], 's-', color=colors['tau'], linewidth=2, markersize=10)
    ax.axhline(y=5.0, color='r', linestyle='--', label='Target Risk delta=5%')
    ax.set_xlabel('STL Temperature tau')
    ax.set_ylabel('Empirical Risk (%)')
    ax.set_title('(b) Empirical Risk vs STL Temperature')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(0, 4)
    ax.set_xscale('log')

    # M vs Empirical Risk
    ax = axes[2]
    ax.plot(M_values, M_data['经验风险(%)'], '^-', color=colors['M'], linewidth=2, markersize=10)
    ax.axhline(y=5.0, color='r', linestyle='--', label='Target Risk delta=5%')
    ax.set_xlabel('DR Residual Window M')
    ax.set_ylabel('Empirical Risk (%)')
    ax.set_title('(c) Empirical Risk vs DR Window')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(0, 4)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig1_empirical_risk_comparison.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 1 saved: fig1_empirical_risk_comparison.png")

    # ==================== Figure 2: Tracking Error vs Parameters ====================
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    # N vs Tracking Error
    ax = axes[0]
    ax.plot(N_values, N_data['跟踪误差(m)'], 'o-', color=colors['N'], linewidth=2, markersize=10)
    ax.axhline(y=0.5, color='g', linestyle='--', label='Safety Threshold 0.5m')
    ax.set_xlabel('MPC Horizon N')
    ax.set_ylabel('Tracking Error (m)')
    ax.set_title('(a) Tracking Error vs MPC Horizon')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(0.3, 0.8)

    # τ vs Tracking Error
    ax = axes[1]
    ax.plot(tau_values, tau_data['跟踪误差(m)'], 's-', color=colors['tau'], linewidth=2, markersize=10)
    ax.axhline(y=0.5, color='g', linestyle='--', label='Safety Threshold 0.5m')
    ax.set_xlabel('STL Temperature tau')
    ax.set_ylabel('Tracking Error (m)')
    ax.set_title('(b) Tracking Error vs STL Temperature')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(0.3, 0.8)
    ax.set_xscale('log')

    # M vs Tracking Error
    ax = axes[2]
    ax.plot(M_values, M_data['跟踪误差(m)'], '^-', color=colors['M'], linewidth=2, markersize=10)
    ax.axhline(y=0.5, color='g', linestyle='--', label='Safety Threshold 0.5m')
    ax.set_xlabel('DR Residual Window M')
    ax.set_ylabel('Tracking Error (m)')
    ax.set_title('(c) Tracking Error vs DR Window')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(0.3, 0.8)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig2_tracking_error_comparison.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 2 saved: fig2_tracking_error_comparison.png")

    # ==================== Figure 3: Solve Time vs Parameters ====================
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    # N vs Solve Time
    ax = axes[0]
    bars = ax.bar(N_values, N_data['求解时间(ms)'], color=colors['N'], alpha=0.8, width=3)
    ax.axhline(y=50, color='r', linestyle='--', label='Real-time Threshold 50ms')
    ax.set_xlabel('MPC Horizon N')
    ax.set_ylabel('Solve Time (ms)')
    ax.set_title('(a) Solve Time vs MPC Horizon')
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend()
    ax.set_ylim(0, 60)

    # τ vs Solve Time
    ax = axes[1]
    ax.bar(range(len(tau_values)), tau_data['求解时间(ms)'], color=colors['tau'], alpha=0.8)
    ax.axhline(y=50, color='r', linestyle='--', label='Real-time Threshold 50ms')
    ax.set_xlabel('STL Temperature tau')
    ax.set_ylabel('Solve Time (ms)')
    ax.set_title('(b) Solve Time vs STL Temperature')
    ax.set_xticks(range(len(tau_values)))
    ax.set_xticklabels(tau_values)
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend()
    ax.set_ylim(0, 60)

    # M vs Solve Time
    ax = axes[2]
    ax.bar(range(len(M_values)), M_data['求解时间(ms)'], color=colors['M'], alpha=0.8)
    ax.axhline(y=50, color='r', linestyle='--', label='Real-time Threshold 50ms')
    ax.set_xlabel('DR Residual Window M')
    ax.set_ylabel('Solve Time (ms)')
    ax.set_title('(c) Solve Time vs DR Window')
    ax.set_xticks(range(len(M_values)))
    ax.set_xticklabels(M_values)
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend()
    ax.set_ylim(0, 60)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig3_solve_time_comparison.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 3 saved: fig3_solve_time_comparison.png")

    # ==================== Figure 4: Sensitivity Range Comparison ====================
    fig, ax = plt.subplots(figsize=(12, 6))

    metrics = ['Empirical Risk Range (%)', 'Tracking Error Range (m x10)', 'Solve Time Range (ms)']
    N_ranges = [1.33, 0.31*10, 1.0]
    tau_ranges = [2.14, 0.045*10, 0.0]
    M_ranges = [0.24, 0.071*10, 1.0]

    x = np.arange(len(metrics))
    width = 0.25

    bars1 = ax.bar(x - width, N_ranges, width, label='N (MPC Horizon)', color=colors['N'], alpha=0.8)
    bars2 = ax.bar(x, tau_ranges, width, label='tau (STL Temperature)', color=colors['tau'], alpha=0.8)
    bars3 = ax.bar(x + width, M_ranges, width, label='M (DR Window)', color=colors['M'], alpha=0.8)

    ax.set_xlabel('Performance Metric')
    ax.set_ylabel('Variation Range')
    ax.set_title('Parameter Sensitivity Comparison - Variation Range by Metric')
    ax.set_xticks(x)
    ax.set_xticklabels(metrics)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')

    # Add value labels
    for bars in [bars1, bars2, bars3]:
        for bar in bars:
            height = bar.get_height()
            ax.annotate(f'{height:.2f}',
                        xy=(bar.get_x() + bar.get_width() / 2, height),
                        xytext=(0, 3),
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=10)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig4_sensitivity_range_comparison.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 4 saved: fig4_sensitivity_range_comparison.png")

    # ==================== Figure 5: Parameter Ranking Bar Chart ====================
    fig, ax = plt.subplots(figsize=(10, 6))

    params = ['tau', 'N', 'M']
    risk_ranges = [2.14, 1.33, 0.24]
    track_ranges = [0.045, 0.310, 0.071]

    x = np.arange(len(params))
    width = 0.35

    bars1 = ax.bar(x - width/2, risk_ranges, width, label='Empirical Risk Range (%)', color=colors['tau'], alpha=0.8)
    bars2 = ax.bar(x + width/2, [t*10 for t in track_ranges], width, label='Tracking Error Range (m x10)', color=colors['N'], alpha=0.8)

    ax.set_xlabel('Parameter')
    ax.set_ylabel('Variation Range')
    ax.set_title('Parameter Sensitivity Ranking')
    ax.set_xticks(x)
    ax.set_xticklabels(['tau (Temperature)\nHighest Sensitivity', 'N (Horizon)\nMedium Sensitivity', 'M (Window)\nLowest Sensitivity'])
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')

    # Add value labels
    for bar in bars1:
        height = bar.get_height()
        ax.annotate(f'{height:.2f}%',
                    xy=(bar.get_x() + bar.get_width() / 2, height),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=11)

    for bar in bars2:
        height = bar.get_height()
        ax.annotate(f'{height/10:.3f}m',
                    xy=(bar.get_x() + bar.get_width() / 2, height),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=11)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig5_parameter_ranking.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 5 saved: fig5_parameter_ranking.png")

    # ==================== Figure 6: Calibration Error Analysis ====================
    fig, ax = plt.subplots(figsize=(10, 6))

    x = np.arange(3)
    width = 0.15

    # Calibration error for each parameter value
    N_cal = N_data['校准误差(%)']
    tau_cal = tau_data['校准误差(%)']
    M_cal = M_data['校准误差(%)']

    # Plot grouped bar chart
    for i, (param_name, cal_data, color) in enumerate([
        ('N', N_cal, colors['N']),
        ('tau', tau_cal, colors['tau']),
        ('M', M_cal, colors['M'])
    ]):
        offsets = [-2*width, -width, 0, width, 2*width]
        for j, (val, offset) in enumerate(zip(cal_data, offsets)):
            if i == 0:
                label = f'{param_name}={N_values[j]}'
            elif i == 1:
                label = f'{param_name}={tau_values[j]}'
            else:
                label = f'{param_name}={M_values[j]}'
            ax.bar(x[i] + offset, val, width, color=color, alpha=0.6 + j*0.08, label=label if i == 0 else None)

    ax.axhline(y=5.0, color='r', linestyle='--', linewidth=2, label='Target Risk delta=5%')
    ax.set_xlabel('Parameter Type')
    ax.set_ylabel('Calibration Error (%)')
    ax.set_title('Calibration Error Analysis - |Empirical Risk - Target Risk|')
    ax.set_xticks(x)
    ax.set_xticklabels(['N (MPC Horizon)', 'tau (STL Temp)', 'M (DR Window)'])
    ax.grid(True, alpha=0.3, axis='y')
    ax.set_ylim(0, 6)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig6_calibration_error_analysis.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 6 saved: fig6_calibration_error_analysis.png")

    # ==================== Figure 7: Comprehensive Comparison Heatmap ====================
    fig, ax = plt.subplots(figsize=(10, 8))

    # Create heatmap data
    params = ['N=10', 'N=15', 'N=20', 'N=25', 'N=30',
              'tau=0.01', 'tau=0.05', 'tau=0.1', 'tau=0.5', 'tau=1.0',
              'M=50', 'M=100', 'M=200', 'M=300', 'M=500']

    metrics = ['Empirical Risk', 'Tracking Error', 'Solve Time', 'Calibration Error']

    # Normalize all values (lower is better)
    all_risk = N_data['经验风险(%)'] + tau_data['经验风险(%)'] + M_data['经验风险(%)']
    all_track = N_data['跟踪误差(m)'] + tau_data['跟踪误差(m)'] + M_data['跟踪误差(m)']
    all_time = N_data['求解时间(ms)'] + tau_data['求解时间(ms)'] + M_data['求解时间(ms)']
    all_cal = N_data['校准误差(%)'] + tau_data['校准误差(%)'] + M_data['校准误差(%)']

    heatmap_data = np.array([
        [v/max(all_risk) for v in all_risk],
        [v/max(all_track) for v in all_track],
        [v/max(all_time) for v in all_time],
        [v/max(all_cal) for v in all_cal]
    ])

    im = ax.imshow(heatmap_data, cmap='RdYlGn_r', aspect='auto')

    ax.set_xticks(np.arange(len(params)))
    ax.set_yticks(np.arange(len(metrics)))
    ax.set_xticklabels(params, rotation=45, ha='right')
    ax.set_yticklabels(metrics)

    # Add colorbar
    cbar = ax.figure.colorbar(im, ax=ax)
    cbar.ax.set_ylabel('Normalized Value (Lower is Better)', rotation=-90, va="bottom")

    # Add text annotations
    for i in range(len(metrics)):
        for j in range(len(params)):
            val = heatmap_data[i, j]
            text = ax.text(j, i, f'{val:.2f}',
                          ha="center", va="center", color="black" if val < 0.7 else "white", fontsize=8)

    ax.set_title('Parameter Configuration Performance Heatmap')
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig7_comprehensive_heatmap.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 7 saved: fig7_comprehensive_heatmap.png")

    # ==================== Figure 8: Bar Chart - Empirical Risk by Parameter ====================
    fig, ax = plt.subplots(figsize=(12, 6))

    all_params = [f'N={v}' for v in N_values] + [f'tau={v}' for v in tau_values] + [f'M={v}' for v in M_values]
    all_risks = N_data['经验风险(%)'] + tau_data['经验风险(%)'] + M_data['经验风险(%)']
    all_colors = [colors['N']]*5 + [colors['tau']]*5 + [colors['M']]*5

    bars = ax.bar(range(len(all_params)), all_risks, color=all_colors, alpha=0.8)

    ax.axhline(y=5.0, color='r', linestyle='--', linewidth=2, label='Target Risk delta=5%')
    ax.axhline(y=min(all_risks), color='g', linestyle=':', linewidth=2, label=f'Min Risk {min(all_risks):.2f}%')

    ax.set_xlabel('Parameter Configuration')
    ax.set_ylabel('Empirical Risk (%)')
    ax.set_title('Empirical Risk Comparison - All Parameter Configurations')
    ax.set_xticks(range(len(all_params)))
    ax.set_xticklabels(all_params, rotation=45, ha='right')
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend()

    # Highlight best parameter
    best_idx = all_risks.index(min(all_risks))
    bars[best_idx].set_edgecolor('green')
    bars[best_idx].set_linewidth(3)

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, 'fig8_empirical_risk_bar_chart.png'), dpi=300, bbox_inches='tight')
    plt.close()
    print("Figure 8 saved: fig8_empirical_risk_bar_chart.png")

    print("\nAll figures generated successfully!")


if __name__ == '__main__':
    print("=" * 60)
    print("Generating figures and CSV files from sensitivity analysis")
    print("=" * 60)

    # Save CSV files
    print("\n[1/2] Saving CSV files...")
    save_csv_files()

    # Generate figures
    print("\n[2/2] Generating figures...")
    generate_figures()

    print("\n" + "=" * 60)
    print("All files saved to:", OUTPUT_DIR)
    print("=" * 60)
