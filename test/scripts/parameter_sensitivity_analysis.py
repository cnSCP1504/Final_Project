#!/usr/bin/env python3
"""
参数敏感性分析实验脚本
Parameter Sensitivity Analysis Experiment

分析三个关键参数对系统性能的影响:
1. MPC时域 N (mpc_steps): 10, 15, 20, 25, 30
2. STL温度 τ (temperature): 0.01, 0.05, 0.1, 0.5, 1.0
3. 残差窗口 M (sliding_window_size): 50, 100, 200, 300, 500

作者: Claude Code
日期: 2026-04-17
"""

import os
import sys
import json
import yaml
import subprocess
import time
import shutil
import statistics
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional
import argparse


class ParameterSensitivityAnalysis:
    """参数敏感性分析实验"""

    def __init__(self, output_dir: str = None):
        self.output_dir = Path(output_dir) if output_dir else Path("test_results/sensitivity_analysis")
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # 基础参数配置
        self.base_config = {
            'model': 'dr',  # 使用DR模式进行测试
            'shelves': 1,   # 每个配置测试1个货架
            'timeout': 120,
            'no_viz': True
        }

        # 参数范围定义
        self.parameter_ranges = {
            'mpc_steps': [10, 15, 20, 25, 30],           # 时域N
            'temperature': [0.01, 0.05, 0.1, 0.5, 1.0],  # STL温度τ
            'window_size': [50, 100, 200, 300, 500]      # 残差窗口M
        }

        # 参数文件路径
        self.param_files = {
            'mpc_steps': 'src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml',
            'temperature': 'src/tube_mpc_ros/stl_monitor/params/stl_monitor_params.yaml',
            'window_size': 'src/dr_tightening/params/dr_tightening_params.yaml'
        }

        # 结果存储
        self.results = {}

    def backup_param_files(self):
        """备份原始参数文件"""
        print("\n📦 备份原始参数文件...")
        backup_dir = self.output_dir / "param_backups"
        backup_dir.mkdir(exist_ok=True)

        for param_name, file_path in self.param_files.items():
            src = Path(file_path)
            dst = backup_dir / f"{src.name}.backup"
            shutil.copy(src, dst)
            print(f"  ✓ 备份: {src} -> {dst}")

    def restore_param_files(self):
        """恢复原始参数文件"""
        print("\n📦 恢复原始参数文件...")
        backup_dir = self.output_dir / "param_backups"

        for param_name, file_path in self.param_files.items():
            src = Path(file_path)
            dst = backup_dir / f"{src.name}.backup"
            if dst.exists():
                shutil.copy(dst, src)
                print(f"  ✓ 恢复: {dst} -> {src}")

    def modify_param(self, param_name: str, value: Any) -> bool:
        """修改参数文件中的指定参数"""
        file_path = Path(self.param_files[param_name])

        try:
            with open(file_path, 'r') as f:
                content = f.read()

            # 根据参数类型确定搜索模式
            if param_name == 'mpc_steps':
                # tube_mpc_params.yaml 格式: "mpc_steps: 20"
                import re
                new_content = re.sub(
                    r'(mpc_steps:\s*)\d+',
                    f'\\g<1>{value}',
                    content
                )
            elif param_name == 'temperature':
                # stl_monitor_params.yaml 格式: "temperature: 0.1"
                import re
                new_content = re.sub(
                    r'(temperature:\s*)[\d.]+',
                    f'\\g<1>{value}',
                    content
                )
            elif param_name == 'window_size':
                # dr_tightening_params.yaml 格式: "sliding_window_size: 200"
                import re
                new_content = re.sub(
                    r'(sliding_window_size:\s*)\d+',
                    f'\\g<1>{value}',
                    content
                )
            else:
                print(f"  ⚠️ 未知参数: {param_name}")
                return False

            with open(file_path, 'w') as f:
                f.write(new_content)

            return True

        except Exception as e:
            print(f"  ❌ 修改参数失败: {e}")
            return False

    def run_single_test(self, param_name: str, param_value: Any) -> Optional[Dict]:
        """运行单次测试"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        test_name = f"{param_name}_{param_value}_{timestamp}"

        print(f"\n{'='*60}")
        print(f"🧪 测试: {param_name} = {param_value}")
        print(f"{'='*60}")

        # 修改参数
        if not self.modify_param(param_name, param_value):
            return None

        # 重新编译
        print("  🔨 重新编译...")
        result = subprocess.run(
            ['catkin_make', '--only-pkg-with-deps', 'tube_mpc_ros', 'dr_tightening'],
            capture_output=True, text=True, cwd='/home/dixon/Final_Project/catkin'
        )
        if result.returncode != 0:
            print(f"  ❌ 编译失败: {result.stderr}")
            return None

        # 清理ROS进程
        print("  🧹 清理ROS进程...")
        subprocess.run(['killall', '-9', 'roslaunch', 'rosmaster', 'roscore',
                       'gzserver', 'gzclient', 'python'],
                      capture_output=True)
        time.sleep(2)

        # 运行测试
        print("  🚀 运行测试...")
        cmd = [
            'python3', 'test/scripts/run_automated_test.py',
            '--model', self.base_config['model'],
            '--shelves', str(self.base_config['shelves']),
            '--timeout', str(self.base_config['timeout']),
            '--no-viz'
        ]

        result = subprocess.run(
            cmd,
            capture_output=True, text=True,
            cwd='/home/dixon/Final_Project/catkin',
            timeout=180
        )

        if result.returncode != 0:
            print(f"  ⚠️ 测试返回非零: {result.returncode}")

        # 解析结果
        # 找到最新的测试结果目录
        test_results_dir = Path('test_results')
        dr_dirs = sorted(test_results_dir.glob('dr_*'), key=lambda x: x.stat().st_mtime, reverse=True)

        if not dr_dirs:
            print("  ❌ 未找到测试结果")
            return None

        latest_dir = dr_dirs[0]
        metrics_file = latest_dir / 'test_01_shelf_01' / 'metrics.json'

        if not metrics_file.exists():
            print(f"  ❌ 未找到指标文件: {metrics_file}")
            return None

        with open(metrics_file) as f:
            metrics_data = json.load(f)

        # 提取关键指标
        mm = metrics_data.get('manuscript_metrics', {})
        rd = metrics_data.get('manuscript_raw_data', {})

        result_data = {
            'param_name': param_name,
            'param_value': param_value,
            'test_dir': str(latest_dir),
            'timestamp': timestamp,
            'metrics': {
                'empirical_risk': mm.get('empirical_risk'),
                'calibration_error': mm.get('calibration_error'),
                'feasibility_rate': mm.get('feasibility_rate'),
                'median_solve_time': mm.get('median_solve_time'),
                'mean_tracking_error': mm.get('mean_tracking_error'),
                'tube_occupancy_rate': mm.get('tube_occupancy_rate'),
                'directional_calibration_error': mm.get('directional_calibration_error'),
                'safety_margin': mm.get('safety_margin'),
            },
            'raw_data': {
                'safety_violation_count': rd.get('safety_violation_count'),
                'safety_total_steps': rd.get('safety_total_steps'),
                'mpc_total_solves': rd.get('mpc_total_solves'),
            }
        }

        print(f"  ✅ 测试完成")
        print(f"     empirical_risk: {mm.get('empirical_risk', 'N/A')}")
        print(f"     feasibility_rate: {mm.get('feasibility_rate', 'N/A')}")
        print(f"     median_solve_time: {mm.get('median_solve_time', 'N/A')} ms")

        return result_data

    def analyze_parameter(self, param_name: str) -> Dict:
        """分析单个参数的敏感性"""
        print(f"\n{'#'*60}")
        print(f"# 参数敏感性分析: {param_name}")
        print(f"{'#'*60}")

        results = []
        param_values = self.parameter_ranges[param_name]

        for value in param_values:
            result = self.run_single_test(param_name, value)
            if result:
                results.append(result)
            time.sleep(5)  # 系统冷却时间

        return {
            'param_name': param_name,
            'param_values': param_values,
            'results': results,
            'summary': self._compute_summary(results)
        }

    def _compute_summary(self, results: List[Dict]) -> Dict:
        """计算敏感性分析摘要"""
        if not results:
            return {}

        summary = {}

        # 提取各指标的值列表
        metrics_keys = ['empirical_risk', 'feasibility_rate', 'median_solve_time',
                       'mean_tracking_error', 'calibration_error']

        for key in metrics_keys:
            values = [r['metrics'].get(key) for r in results if r['metrics'].get(key) is not None]
            if values:
                summary[key] = {
                    'mean': statistics.mean(values),
                    'std': statistics.stdev(values) if len(values) > 1 else 0,
                    'min': min(values),
                    'max': max(values),
                    'range': max(values) - min(values)
                }

        return summary

    def run_all_analyses(self):
        """运行所有参数敏感性分析"""
        print("\n" + "="*60)
        print("🔬 参数敏感性分析实验")
        print("="*60)
        print(f"开始时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"输出目录: {self.output_dir}")

        # 备份原始参数
        self.backup_param_files()

        try:
            # 分析每个参数
            for param_name in self.parameter_ranges.keys():
                self.results[param_name] = self.analyze_parameter(param_name)

            # 保存结果
            self.save_results()

            # 生成报告
            self.generate_report()

        finally:
            # 恢复原始参数
            self.restore_param_files()

        print("\n" + "="*60)
        print("✅ 参数敏感性分析完成")
        print("="*60)

    def save_results(self):
        """保存分析结果"""
        results_file = self.output_dir / "sensitivity_analysis_results.json"

        with open(results_file, 'w', encoding='utf-8') as f:
            json.dump(self.results, f, indent=2, ensure_ascii=False, default=str)

        print(f"\n💾 结果已保存: {results_file}")

    def generate_report(self):
        """生成敏感性分析报告"""
        report_file = self.output_dir / "sensitivity_analysis_report.md"

        with open(report_file, 'w', encoding='utf-8') as f:
            f.write("# 参数敏感性分析报告\n\n")
            f.write(f"**分析日期**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

            f.write("## 实验设计\n\n")
            f.write("分析三个关键参数对系统性能的影响:\n\n")

            f.write("| 参数 | 符号 | 测试范围 | 默认值 |\n")
            f.write("|------|------|----------|--------|\n")
            f.write("| MPC时域 | N | 10, 15, 20, 25, 30 | 20 |\n")
            f.write("| STL温度 | τ | 0.01, 0.05, 0.1, 0.5, 1.0 | 0.1 |\n")
            f.write("| 残差窗口 | M | 50, 100, 200, 300, 500 | 200 |\n\n")

            f.write("## 分析结果\n\n")

            for param_name, analysis in self.results.items():
                f.write(f"### {param_name}\n\n")

                summary = analysis.get('summary', {})
                results = analysis.get('results', [])

                if not results:
                    f.write("*无有效结果*\n\n")
                    continue

                # 结果表格
                f.write("| 参数值 | 经验风险 | 可行率 | 求解时间(ms) | 校准误差 |\n")
                f.write("|--------|----------|--------|--------------|----------|\n")

                for r in results:
                    m = r['metrics']
                    val = r['param_value']
                    emp = m.get('empirical_risk', 'N/A')
                    feas = m.get('feasibility_rate', 'N/A')
                    solve = m.get('median_solve_time', 'N/A')
                    cal = m.get('calibration_error', 'N/A')

                    emp_str = f"{emp*100:.2f}%" if isinstance(emp, (int, float)) else str(emp)
                    feas_str = f"{feas*100:.1f}%" if isinstance(feas, (int, float)) else str(feas)
                    solve_str = f"{solve:.1f}" if isinstance(solve, (int, float)) else str(solve)
                    cal_str = f"{cal*100:.2f}%" if isinstance(cal, (int, float)) else str(cal)

                    f.write(f"| {val} | {emp_str} | {feas_str} | {solve_str} | {cal_str} |\n")

                f.write("\n")

                # 敏感性指标
                if summary:
                    f.write("**敏感性指标**:\n\n")
                    for metric, stats in summary.items():
                        if isinstance(stats, dict) and 'range' in stats:
                            f.write(f"- {metric}: 变化范围 = {stats['range']:.4f}\n")
                    f.write("\n")

            # 结论
            f.write("## 结论与建议\n\n")
            f.write("根据敏感性分析结果，对参数调优提出以下建议:\n\n")

            # 分析每个参数的影响
            for param_name, analysis in self.results.items():
                summary = analysis.get('summary', {})
                if not summary:
                    continue

                # 找到变化最大的指标
                max_range_metric = max(
                    [(k, v['range']) for k, v in summary.items() if 'range' in v],
                    key=lambda x: x[1],
                    default=(None, 0)
                )

                if max_range_metric[0]:
                    f.write(f"- **{param_name}**: 主要影响 **{max_range_metric[0]}** (变化范围: {max_range_metric[1]:.4f})\n")

            f.write("\n---\n")
            f.write(f"*报告生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}*\n")

        print(f"📊 报告已生成: {report_file}")


def main():
    parser = argparse.ArgumentParser(description='参数敏感性分析实验')
    parser.add_argument('--output', '-o', type=str, default='test_results/sensitivity_analysis',
                       help='输出目录')
    parser.add_argument('--param', '-p', type=str, choices=['mpc_steps', 'temperature', 'window_size', 'all'],
                       default='all', help='要分析的参数')

    args = parser.parse_args()

    analysis = ParameterSensitivityAnalysis(output_dir=args.output)

    if args.param == 'all':
        analysis.run_all_analyses()
    else:
        analysis.backup_param_files()
        try:
            result = analysis.analyze_parameter(args.param)
            analysis.results[args.param] = result
            analysis.save_results()
            analysis.generate_report()
        finally:
            analysis.restore_param_files()


if __name__ == '__main__':
    main()
