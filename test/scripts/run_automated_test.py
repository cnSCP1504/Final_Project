#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
自动化Baseline测试系统 - Python版本
Automated Baseline Testing System - Python Version

功能：
- 循环测试不同货架位置作为取货点
- 支持选择不同的baseline模型
- 自动检测两次目标到达后重启并切换货架
- 收集并保存详细测试metrics到结构化文件夹
- 生成可视化报告和统计数据

使用方法：
    python3 run_automated_test.py [OPTIONS]

示例：
    # 测试所有货架，使用tube_mpc模型
    python3 run_automated_test.py --model tube_mpc

    # 测试前3个货架，使用STL增强模型
    python3 run_automated_test.py --model stl --shelves 3

    # 测试纯MPC基准（论文对照）
    python3 run_automated_test.py --model mpc --shelves 5

    # 🔥 测试所有模型，每个模型测试10个货架（完整测试）
    python3 run_automated_test.py --model all --shelves 10

    # 快速测试所有模型（每个模型只测试1个货架）
    python3 run_automated_test.py --model all --shelves 1 --no-viz
"""

import os
import sys
import yaml
import json
import subprocess
import time
import signal
import argparse
from datetime import datetime
from pathlib import Path
import threading
import rospy
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Twist
import math
import numpy as np
from collections import defaultdict

# 添加数据处理和绘图功能
import matplotlib
matplotlib.use('Agg')  # 使用非交互式后端
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# 抑制中文字体缺失警告
import warnings
warnings.filterwarnings("ignore", category=RuntimeWarning, module='matplotlib')
warnings.filterwarnings("ignore", category=RuntimeWarning, module='matplotlib.backends')
from typing import Dict, List, Tuple
from scipy import stats

class ManuscriptMetricsCollector:
    """
    Manuscript性能指标收集器

    收集manuscript中要求的7类关键指标：
    1. Satisfaction probability (STL robustness)
    2. Empirical risk (safety violations)
    3. Dynamic/Safe regret
    4. Recursive feasibility rate
    5. Computation metrics (solve time, failures)
    6. Tracking error and tube occupancy
    7. Calibration accuracy
    """

    def __init__(self, output_dir=None):
        self.output_dir = Path(output_dir) if output_dir else None
        self.data = {
            # 1. STL Robustness Data
            'stl_robustness_history': [],      # STL robustness values
            'stl_budget_history': [],           # Budget values
            'stl_satisfaction_count': 0,        # Steps with rho >= 0
            'stl_total_steps': 0,               # Total steps evaluated

            # 2. Safety/Empirical Risk Data
            'safety_violations': [],            # Steps where h(x) < 0
            'dr_margins_history': [],           # DR margin values
            'tracking_error_history': [],       # Tracking error norms
            'safety_violation_count': 0,        # Count of safety violations
            'safety_total_steps': 0,            # Total steps monitored

            # 3. Regret Data (需要参考策略数据)
            'instantaneous_cost': [],           # Instantaneous cost l(x,u)
            'reference_cost': [],               # Reference policy cost
            'dynamic_regret': [],               # Dynamic regret values
            'safe_regret': [],                  # Safe regret values
            'tracking_contribution': [],        # Regret from tracking error
            'nominal_contribution': [],         # Regret from nominal policy

            # 4. Feasibility Data
            'mpc_feasible_count': 0,            # Feasible MPC solves
            'mpc_infeasible_count': 0,          # Infeasible MPC solves
            'mpc_total_solves': 0,              # Total MPC solve attempts
            'feasibility_rate': 0.0,            # Feasibility rate

            # 5. Computation Metrics
            'mpc_solve_times': [],              # Individual solve times (ms)
            'mpc_solve_time_median': 0.0,       # Median solve time
            'mpc_solve_time_p90': 0.0,          # 90th percentile solve time
            'mpc_failure_count': 0,             # Failed solve attempts

            # 6. Tracking Error & Tube Data
            'tracking_error_norm_history': [],  # ||e_t||_2 values
            'tube_violations': [],              # Steps with e_t not in E
            'tube_violation_count': 0,          # Count of tube violations
            'tube_occupancy_rate': 0.0,         # Fraction of time in tube

            # 7. Calibration Data
            'target_delta': 0.05,               # Target risk level
            'observed_violation_rate': 0.0,     # Actual violation rate
            'calibration_error': 0.0,           # |observed - target|
        }

        # ROS订阅者
        self.subscribers = {}

        # 统计辅助数据
        self.start_time = None
        self.step_count = 0

    def reset(self):
        """重置所有数据收集器"""
        for key in self.data:
            if isinstance(self.data[key], list):
                self.data[key] = []
            elif isinstance(self.data[key], (int, float)):
                self.data[key] = 0
        self.start_time = None
        self.step_count = 0

    def setup_ros_subscribers(self, model_name='tube_mpc'):
        """设置所有需要的ROS话题订阅

        Args:
            model_name: 模型名称，用于判断哪些话题可用
                - mpc: 纯MPC，只发布基础话题（/mpc_trajectory, /cmd_vel）
                - tube_mpc, stl, dr, safe_regret: 发布完整的metrics话题
        """
        self.model_name = model_name

        try:
            # 1. STL Robustness (from stl_monitor)
            # 注意：stl_monitor只在stl和safe_regret模式下启用
            if model_name in ['stl', 'safe_regret']:
                try:
                    from std_msgs.msg import Float32
                    # 关键修复：指定queue_size=10，避免消息队列积压
                    self.subscribers['stl_robustness'] = rospy.Subscriber(
                        '/stl_monitor/robustness', Float32, self.stl_robustness_callback, queue_size=10)
                    self.subscribers['stl_budget'] = rospy.Subscriber(
                        '/stl_monitor/budget', Float32, self.stl_budget_callback, queue_size=10)
                except Exception as e:
                    TestLogger.warning(f"无法订阅STL话题: {e}")
            else:
                TestLogger.info(f"  STL话题禁用（模型: {model_name}）")

            # 2. DR Margins (from dr_tightening)
            # 注意：DR只在dr和safe_regret模式下启用
            if model_name in ['dr', 'safe_regret']:
                try:
                    from std_msgs.msg import Float64MultiArray
                    self.subscribers['dr_margins'] = rospy.Subscriber(
                        '/dr_margins', Float64MultiArray, self.dr_margins_callback, queue_size=10)
                except Exception as e:
                    TestLogger.warning(f"无法订阅DR话题: {e}")
            else:
                TestLogger.info(f"  DR话题禁用（模型: {model_name}）")

            # 3. Tracking Error (from tube_mpc)
            # 注意：tube_mpc, stl, dr, safe_regret都发布tracking_error
            # 纯MPC没有这个话题
            if model_name in ['tube_mpc', 'stl', 'dr', 'safe_regret']:
                try:
                    from std_msgs.msg import Float64MultiArray
                    self.subscribers['tracking_error'] = rospy.Subscriber(
                        '/tube_mpc/tracking_error', Float64MultiArray, self.tracking_error_callback, queue_size=10)
                except Exception as e:
                    TestLogger.warning(f"无法订阅tracking_error话题: {e}")
            else:
                TestLogger.info(f"  Tracking Error话题禁用（模型: {model_name}，纯MPC无此功能）")

            # 4. MPC Solver Stats (from tube_mpc MetricsCollector)
            # 注意：纯MPC没有这个话题
            if model_name in ['tube_mpc', 'stl', 'dr', 'safe_regret']:
                try:
                    from std_msgs.msg import Float64
                    self.subscribers['mpc_solve_time'] = rospy.Subscriber(
                        '/mpc_metrics/solve_time_ms', Float64, self.mpc_solve_time_callback, queue_size=10)
                    self.subscribers['mpc_feasibility'] = rospy.Subscriber(
                        '/mpc_metrics/feasibility_rate', Float64, self.mpc_feasibility_callback, queue_size=10)
                except Exception as e:
                    TestLogger.warning(f"无法订阅MPC solver话题: {e}")
            else:
                TestLogger.info(f"  MPC Metrics话题禁用（模型: {model_name}，纯MPC无此功能）")

            # 5. Tube Boundaries (for tube occupancy)
            # 纯MPC没有这个话题
            if model_name in ['tube_mpc', 'stl', 'dr', 'safe_regret']:
                try:
                    from geometry_msgs.msg import PolygonStamped
                    self.subscribers['tube_boundaries'] = rospy.Subscriber(
                        '/tube_boundaries', PolygonStamped, self.tube_boundaries_callback, queue_size=10)
                except Exception as e:
                    TestLogger.warning(f"无法订阅tube_boundaries话题: {e}")
            else:
                TestLogger.info(f"  Tube Boundaries话题禁用（模型: {model_name}）")

            # 6. Regret Metrics (from safe_regret reference_planner)
            # 只有safe_regret模式有这个话题
            if model_name in ['safe_regret']:
                try:
                    from std_msgs.msg import Float64MultiArray
                    self.subscribers['regret_metrics'] = rospy.Subscriber(
                        '/safe_regret/regret_metrics', Float64MultiArray, self.regret_metrics_callback, queue_size=10)
                except Exception as e:
                    TestLogger.warning(f"无法订阅regret_metrics话题: {e}")
            else:
                TestLogger.info(f"  Regret Metrics话题禁用（模型: {model_name}）")

            TestLogger.success(f"Manuscript Metrics订阅者设置完成（模型: {model_name}）")

        except Exception as e:
            TestLogger.error(f"设置ROS订阅者时出错: {e}")

    def shutdown_subscribers(self):
        """关闭所有订阅者"""
        for name, sub in self.subscribers.items():
            try:
                sub.unregister()
            except:
                pass
        self.subscribers.clear()

    # === 回调函数 ===

    def stl_robustness_callback(self, msg):
        """STL鲁棒性回调"""
        rho = msg.data
        self.data['stl_robustness_history'].append(rho)
        self.data['stl_total_steps'] += 1
        if rho >= 0:
            self.data['stl_satisfaction_count'] += 1

    def stl_budget_callback(self, msg):
        """STL预算回调"""
        self.data['stl_budget_history'].append(msg.data)

    def dr_margins_callback(self, msg):
        """DR边界回调"""
        if len(msg.data) > 0:
            # ✅ 记录horizon（从第一次更新推断）
            if 'dr_horizon' not in self.data:
                # dr_margins包含horizon+1个值 [t=0, t=1, ..., t=horizon]
                self.data['dr_horizon'] = len(msg.data) - 1

            # 保存所有margin值
            self.data['dr_margins_history'].extend(msg.data)

    def tracking_error_callback(self, msg):
        """跟踪误差回调 - 接收Float64MultiArray

        tube_mpc发布的消息格式：
        [0]: cte           - Cross-track error（横向位置误差）
        [1]: etheta         - Heading error（角度误差，弧度）
        [2]: error_norm     - 6D误差范数（包含位置和角度）
        [3]: tube_radius    - 当前tube半径
        """
        if len(msg.data) >= 4:
            cte = msg.data[0]          # Cross-track error（位置误差）
            etheta = msg.data[1]        # Heading error（角度误差）
            error_norm_6d = msg.data[2] # 6D误差范数
            tube_radius = msg.data[3]   # Tube半径

            # ✅ 修复：使用正确的数据格式
            # tracking_error_history: [cte, etheta, error_norm_6d, tube_radius]
            self.data['tracking_error_history'].append([cte, etheta, error_norm_6d, tube_radius])

            # ⚠️ 注意：error_norm_6d始终为0（Tube MPC的_e_current从未被更新）
            # 所以tracking_error_norm_history改为记录综合误差：sqrt(cte² + etheta²)
            combined_error = (cte**2 + etheta**2)**0.5
            self.data['tracking_error_norm_history'].append(combined_error)

            # ✅ 修复：使用综合误差检查tube约束
            # 因为error_norm_6d始终为0，我们用sqrt(cte² + etheta²)代替
            if combined_error > tube_radius:
                self.data['tube_violations'].append(combined_error)
                self.data['tube_violation_count'] += 1

            # ✅ 关键修复：DR安全约束应该使用位置误差(cte)，而不是6D误差范数！
            # 原因：DR安全函数h(x)只约束位置(x, y)，不约束角度
            # 参考：NavigationSafetyFunction::evaluate() 只使用 state[0], state[1]
            self.data['safety_total_steps'] += 1

            # 获取当前时间步(t=0)的DR margin
            if len(self.data['dr_margins_history']) > 0:
                horizon = self.data.get('dr_horizon', 20)
                latest_t0_margin_index = -(horizon + 1)
                latest_dr_margin = self.data['dr_margins_history'][latest_t0_margin_index]

                # ✅ 使用位置误差(cte)和DR margin比较
                # cte = cross-track error = 垂直于路径的位置偏差
                # 这是DR约束真正需要约束的量
                if abs(cte) > latest_dr_margin:
                    self.data['safety_violation_count'] += 1
                    self.data['safety_violations'].append(abs(cte))
        else:
            TestLogger.warning(f"tracking_error数据长度不足: {len(msg.data)}")

    def mpc_solve_time_callback(self, msg):
        """MPC求解时间回调"""
        solve_time = msg.data  # 毫秒
        self.data['mpc_solve_times'].append(solve_time)

    def mpc_feasibility_callback(self, msg):
        """MPC可行性回调 (1.0 = feasible, 0.0 = infeasible)"""
        feasible = msg.data > 0.5
        self.data['mpc_total_solves'] += 1
        if feasible:
            self.data['mpc_feasible_count'] += 1
        else:
            self.data['mpc_infeasible_count'] += 1
            self.data['mpc_failure_count'] += 1

    def tube_boundaries_callback(self, msg):
        """Tube边界回调（用于高级分析）"""
        # 这里可以保存tube的多边形形状用于可视化
        pass

    def regret_metrics_callback(self, msg):
        """Regret指标回调 - 接收Float64MultiArray"""
        # msg.data格式:
        # [0] safe_regret
        # [1] dynamic_regret
        # [2] tracking_contribution
        # [3] nominal_contribution
        # [4] horizon_T
        if len(msg.data) >= 2:
            self.data['safe_regret'].append(msg.data[0])
            self.data['dynamic_regret'].append(msg.data[1])
            if len(msg.data) >= 4:
                self.data['tracking_contribution'].append(msg.data[2])
                self.data['nominal_contribution'].append(msg.data[3])
        else:
            TestLogger.warning(f"regret_metrics数据长度不足: {len(msg.data)}")

    # === 计算指标 ===

    def compute_final_metrics(self):
        """计算所有最终指标"""
        metrics = {}

        # 1. Satisfaction Probability
        if self.data['stl_total_steps'] > 0:
            metrics['satisfaction_probability'] = (
                self.data['stl_satisfaction_count'] / self.data['stl_total_steps']
            )
        else:
            metrics['satisfaction_probability'] = None

        # 2. Empirical Risk (安全违反率)
        # ✅ 修复：使用tube_violation_count而不是safety_violation_count
        if self.data['safety_total_steps'] > 0:
            metrics['empirical_risk'] = (
                self.data['tube_violation_count'] / self.data['safety_total_steps']
            )
        else:
            metrics['empirical_risk'] = None

        # 3. Dynamic & Safe Regret (平均值)
        if len(self.data['dynamic_regret']) > 0:
            metrics['avg_dynamic_regret'] = np.mean(self.data['dynamic_regret'])
        else:
            metrics['avg_dynamic_regret'] = None

        if len(self.data['safe_regret']) > 0:
            metrics['avg_safe_regret'] = np.mean(self.data['safe_regret'])
        else:
            metrics['avg_safe_regret'] = None

        # 4. Recursive Feasibility Rate
        if self.data['mpc_total_solves'] > 0:
            metrics['feasibility_rate'] = (
                self.data['mpc_feasible_count'] / self.data['mpc_total_solves']
            )
        else:
            metrics['feasibility_rate'] = None

        # 5. Computation Metrics
        if len(self.data['mpc_solve_times']) > 0:
            solve_times = np.array(self.data['mpc_solve_times'])
            metrics['median_solve_time'] = np.median(solve_times)
            metrics['p90_solve_time'] = np.percentile(solve_times, 90)
            metrics['mean_solve_time'] = np.mean(solve_times)
            metrics['std_solve_time'] = np.std(solve_times)
        else:
            metrics['median_solve_time'] = None
            metrics['p90_solve_time'] = None
            metrics['mean_solve_time'] = None
            metrics['std_solve_time'] = None

        metrics['mpc_failure_count'] = self.data['mpc_failure_count']

        # 6. Tracking Error & Tube Occupancy
        if len(self.data['tracking_error_norm_history']) > 0:
            errors = np.array(self.data['tracking_error_norm_history'])
            metrics['mean_tracking_error'] = np.mean(errors)
            metrics['std_tracking_error'] = np.std(errors)
            metrics['max_tracking_error'] = np.max(errors)
            metrics['min_tracking_error'] = np.min(errors)

            # Tube occupancy rate
            metrics['tube_occupancy_rate'] = 1.0 - (
                self.data['tube_violation_count'] / len(errors)
            )
        else:
            metrics['mean_tracking_error'] = None
            metrics['std_tracking_error'] = None
            metrics['max_tracking_error'] = None
            metrics['min_tracking_error'] = None
            metrics['tube_occupancy_rate'] = None

        metrics['tube_violation_count'] = self.data['tube_violation_count']

        # 7. Calibration Accuracy
        # ✅ 修复：使用tube_violation_count而不是safety_violation_count
        # tube_violation_count在所有模式下都有值，safety_violation_count可能为0
        if self.data['safety_total_steps'] > 0:
            metrics['observed_violation_rate'] = (
                self.data['tube_violation_count'] / self.data['safety_total_steps']
            )
            metrics['calibration_error'] = abs(
                metrics['observed_violation_rate'] - self.data['target_delta']
            )
        else:
            metrics['observed_violation_rate'] = None
            metrics['calibration_error'] = None

        # 附加统计信息
        # ✅ 修复：使用safety_total_steps而不是stl_total_steps
        # safety_total_steps在所有模式下都有值，stl_total_steps只在STL启用时才有值
        metrics['total_steps'] = max(self.data['stl_total_steps'], self.data['safety_total_steps'])
        metrics['total_mpc_solves'] = self.data['mpc_total_solves']

        return metrics

    def get_raw_data(self):
        """获取原始数据用于详细分析"""
        return self.data.copy()

    def print_summary(self):
        """打印指标摘要"""
        metrics = self.compute_final_metrics()

        print("\n" + "=" * 70)
        print("📊 MANUSCRIPT性能指标摘要 / MANUSCRIPT PERFORMANCE METRICS SUMMARY")
        print("=" * 70)

        # 1. Satisfaction Probability
        if metrics['satisfaction_probability'] is not None:
            print(f"\n1️⃣  Satisfaction Probability (STL满足率):")
            print(f"   - ρ(φ) ≥ 0: {self.data['stl_satisfaction_count']}/{self.data['stl_total_steps']} steps")
            print(f"   - 概率: {metrics['satisfaction_probability']:.3f}")
        else:
            print(f"\n1️⃣  Satisfaction Probability: 无数据")

        # 2. Empirical Risk
        if metrics['empirical_risk'] is not None:
            print(f"\n2️⃣  Empirical Risk (经验风险):")
            # ✅ 修复：使用tube_violation_count
            print(f"   - 违反次数: {self.data['tube_violation_count']}/{self.data['safety_total_steps']}")
            print(f"   - 违反率: {metrics['empirical_risk']:.4f}")
            print(f"   - 目标风险 δ: {self.data['target_delta']:.2f}")
        else:
            print(f"\n2️⃣  Empirical Risk: 无数据")

        # 3. Regret
        if metrics['avg_dynamic_regret'] is not None:
            print(f"\n3️⃣  Regret (遗憾):")
            print(f"   - Dynamic Regret/T: {metrics['avg_dynamic_regret']:.4f}")
        else:
            print(f"\n3️⃣  Regret: 无数据")

        # 4. Feasibility Rate
        if metrics['feasibility_rate'] is not None:
            print(f"\n4️⃣  Recursive Feasibility Rate (递归可行率):")
            print(f"   - 可行/总求解: {self.data['mpc_feasible_count']}/{self.data['mpc_total_solves']}")
            print(f"   - 可行率: {metrics['feasibility_rate']:.3f}")
        else:
            print(f"\n4️⃣  Recursive Feasibility Rate: 无数据")

        # 5. Computation Metrics
        if metrics['median_solve_time'] is not None:
            print(f"\n5️⃣  Computation Metrics (计算性能):")
            print(f"   - 中位数求解时间: {metrics['median_solve_time']:.2f} ms")
            print(f"   - P90求解时间: {metrics['p90_solve_time']:.2f} ms")
            print(f"   - 失败次数: {metrics['mpc_failure_count']}")
        else:
            print(f"\n5️⃣  Computation Metrics: 无数据")

        # 6. Tracking Error & Tube
        if metrics['mean_tracking_error'] is not None:
            print(f"\n6️⃣  Tracking Error & Tube (跟踪误差与管状约束):")
            print(f"   - 平均误差: {metrics['mean_tracking_error']:.4f} m")
            print(f"   - 标准差: {metrics['std_tracking_error']:.4f} m")
            print(f"   - 最大误差: {metrics['max_tracking_error']:.4f} m")
            print(f"   - Tube占用率: {metrics['tube_occupancy_rate']:.3f}")
            print(f"   - Tube违反次数: {metrics['tube_violation_count']}")
        else:
            print(f"\n6️⃣  Tracking Error & Tube: 无数据")

        # 7. Calibration
        if metrics['calibration_error'] is not None:
            print(f"\n7️⃣  Calibration Accuracy (校准精度):")
            print(f"   - 观察违反率: {metrics['observed_violation_rate']:.4f}")
            print(f"   - 目标风险: {self.data['target_delta']:.2f}")
            print(f"   - 校准误差: {metrics['calibration_error']:.4f}")
        else:
            print(f"\n7️⃣  Calibration Accuracy: 无数据")

        print("\n" + "=" * 70)


class Colors:
    """终端颜色代码"""
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    PURPLE = '\033[0;35m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'

class TestLogger:
    """测试日志记录器"""

    @staticmethod
    def info(message):
        print(f"{Colors.BLUE}[INFO]{Colors.NC} {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} - {message}")

    @staticmethod
    def success(message):
        print(f"{Colors.GREEN}[SUCCESS]{Colors.NC} {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} - {message}")

    @staticmethod
    def warning(message):
        print(f"{Colors.YELLOW}[WARNING]{Colors.NC} {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} - {message}")

    @staticmethod
    def error(message):
        print(f"{Colors.RED}[ERROR]{Colors.NC} {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} - {message}")

    @staticmethod
    def test(message):
        print(f"{Colors.PURPLE}[TEST]{Colors.NC} {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} - {message}")

class ShelfConfig:
    """货架配置管理器"""

    def __init__(self, config_file):
        self.config_file = config_file
        self.shelves = []
        self.load_config()

    def load_config(self):
        """加载货架配置"""
        try:
            with open(self.config_file, 'r') as f:
                config = yaml.safe_load(f)
                self.shelves = config.get('shelves', [])
            TestLogger.info(f"加载货架配置: {len(self.shelves)} 个货架")
        except Exception as e:
            TestLogger.error(f"加载货架配置失败: {e}")
            raise

    def get_shelf(self, index):
        """获取指定索引的货架"""
        if 0 <= index < len(self.shelves):
            return self.shelves[index]
        return None

    def get_all_shelves(self):
        """获取所有货架"""
        return self.shelves

class ModelConfig:
    """模型配置管理器"""

    MODELS = {
        'mpc': {
            'name': 'Pure MPC (Baseline)',
            'description': '纯MPC控制器（论文基准对照）',
            'launch_package': 'mpc_ros',
            'launch_file': 'mpc_baseline_test.launch',  # 使用新的launch文件
            'params': '',
            'is_mpc_ros': True,
            # mpc_baseline_test.launch的参数映射
            'position_args': {
                'x': 'pickup_x',
                'y': 'pickup_y',
                'yaw': 'pickup_yaw'
            }
        },
        'tube_mpc': {
            'name': 'Tube MPC',
            'params': '',  # 使用launch文件默认值
            'description': '仅使用Tube MPC（基线模型）',
            'is_mpc_ros': False
        },
        'stl': {
            'name': 'Tube MPC + STL',
            'params': 'enable_stl:=true enable_dr:=false',
            'description': 'Tube MPC + STL监控',
            'is_mpc_ros': False
        },
        'dr': {
            'name': 'Tube MPC + DR',
            'params': 'enable_stl:=false enable_dr:=true',
            'description': 'Tube MPC + DR约束收紧',
            'is_mpc_ros': False
        },
        'safe_regret': {
            'name': 'Safe-Regret MPC',
            'params': 'enable_stl:=true enable_dr:=true enable_reference_planner:=true',
            'description': '完整Safe-Regret MPC (STL+DR+Reference)',
            'is_mpc_ros': False
        }
    }

    @staticmethod
    def get_model_params(model_name):
        """获取模型参数"""
        if model_name in ModelConfig.MODELS:
            return ModelConfig.MODELS[model_name]
        return None

    @staticmethod
    def list_models():
        """列出所有可用模型"""
        return ModelConfig.MODELS.keys()

class ResultsManager:
    """测试结果管理器"""

    def __init__(self, base_dir, model_name):
        self.base_dir = Path(base_dir)
        self.model_name = model_name
        self.test_results_dir = self.create_results_directory()

    def create_results_directory(self):
        """创建结果目录"""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        result_dir = self.base_dir / f"{self.model_name}_{timestamp}"
        result_dir.mkdir(parents=True, exist_ok=True)
        TestLogger.success(f"创建结果目录: {result_dir}")
        return result_dir

    def create_test_directory(self, test_index, shelf_id):
        """创建单个测试目录"""
        test_dir = self.test_results_dir / f"test_{test_index:02d}_{shelf_id}"
        test_dir.mkdir(parents=True, exist_ok=True)
        (test_dir / "logs").mkdir(exist_ok=True)
        (test_dir / "rosbag").mkdir(exist_ok=True)
        return test_dir

class GoalMonitor:
    """目标到达监控器（集成Manuscript Metrics收集）"""

    # 类变量：跟踪ROS节点是否已初始化（避免重复初始化）
    _node_initialized = False
    # 移除全局订阅者，改为实例变量（修复第二次测试回调绑定问题）
    # _subscriber = None  # 不再使用全局订阅者

    def __init__(self, goals, goal_radius=0.5, timeout=240, launch_process=None, test_dir=None, model_name='tube_mpc'):
        self.goals = goals
        self.goal_radius = goal_radius
        self.timeout = timeout
        self.launch_process = launch_process  # 添加launch进程引用
        self.test_dir = test_dir  # 添加测试目录
        self.model_name = model_name  # 添加模型名称

        self.current_goal_index = 0
        self.goals_reached = []
        self.robot_position = None
        self.start_time = None
        self.monitoring = False

        # 实例变量：odom订阅者（每次测试创建新的）
        self.odom_subscriber = None

        # Manuscript Metrics收集器（稍后在start_monitoring中设置订阅者）
        self.manuscript_metrics = ManuscriptMetricsCollector(test_dir) if test_dir else None

        # 基础Metrics
        self.metrics = {
            'test_start_time': None,
            'test_end_time': None,
            'test_status': 'RUNNING',
            'total_goals': len(goals),
            'goals_reached': [],
            'position_history': [],
            'ros_died': False,  # 添加ROS死亡标志
            'manuscript_metrics': {}  # 添加manuscript指标
        }

    def reset(self, goals, goal_radius=0.5, timeout=240, launch_process=None, test_dir=None, model_name='tube_mpc'):
        """重置监控器状态（用于复用实例）"""
        self.goals = goals
        self.goal_radius = goal_radius
        self.timeout = timeout
        self.launch_process = launch_process
        self.test_dir = test_dir  # 更新测试目录
        self.model_name = model_name  # 更新模型名称

        self.current_goal_index = 0
        self.goals_reached = []
        self.robot_position = None
        self.start_time = None
        self.monitoring = False

        # 关键修复：注销旧的订阅者
        if self.odom_subscriber is not None:
            self.odom_subscriber.unregister()
            self.odom_subscriber = None

        # 重置Manuscript Metrics收集器
        if self.manuscript_metrics:
            # 🔥 关键修复：先关闭旧订阅者，避免订阅者状态不一致
            self.manuscript_metrics.shutdown_subscribers()
            # 然后重置数据
            self.manuscript_metrics.reset()
            # 如果test_dir更新了，需要更新收集器的output_dir
            if test_dir:
                self.manuscript_metrics.output_dir = Path(test_dir)

        # 重置Metrics
        self.metrics = {
            'test_start_time': None,
            'test_end_time': None,
            'test_status': 'RUNNING',
            'total_goals': len(goals),
            'goals_reached': [],
            'position_history': [],
            'ros_died': False
        }

    def odom_callback(self, msg):
        """里程计回调"""
        x = msg.pose.pose.position.x
        y = msg.pose.pose.position.y

        # 提取朝向
        quaternion = (
            msg.pose.pose.orientation.x,
            msg.pose.pose.orientation.y,
            msg.pose.pose.orientation.z,
            msg.pose.pose.orientation.w
        )
        euler = self.euler_from_quaternion(quaternion)
        yaw = euler[2]

        self.robot_position = (x, y, yaw)

        # 记录位置历史
        if self.monitoring and self.start_time:
            elapsed = time.time() - self.start_time
            if int(elapsed) > len(self.metrics['position_history']):
                self.metrics['position_history'].append({
                    'time': elapsed,
                    'x': x,
                    'y': y,
                    'yaw': yaw
                })

        # 检查目标到达（添加调试日志）
        if self.monitoring and self.current_goal_index < len(self.goals):
            # 每5秒打印一次状态（避免日志过多）
            elapsed = time.time() - self.start_time if self.start_time else 0
            if int(elapsed) % 5 == 0 and int(elapsed) > 0:
                goal = self.goals[self.current_goal_index]
                distance = math.sqrt((goal[0] - x)**2 + (goal[1] - y)**2)
                print(f"📍 [目标 {self.current_goal_index + 1}/{len(self.goals)}] "
                      f"当前位置: ({x:.2f}, {y:.2f}), "
                      f"目标: ({goal[0]:.2f}, {goal[1]:.2f}), "
                      f"距离: {distance:.2f}m, "
                      f"阈值: {self.goal_radius}m")

            self.check_goal_reached()

    def euler_from_quaternion(self, quaternion):
        """四元数转欧拉角"""
        x, y, z, w = quaternion
        t0 = +2.0 * (w * x + y * z)
        t1 = +1.0 - 2.0 * (x * x + y * y)
        roll_x = math.atan2(t0, t1)

        t2 = +2.0 * (w * y - z * x)
        t2 = +1.0 if t2 > +1.0 else t2
        t2 = -1.0 if t2 < -1.0 else t2
        pitch_y = math.asin(t2)

        t3 = +2.0 * (w * z + x * y)
        t4 = +1.0 - 2.0 * (y * y + z * z)
        yaw_z = math.atan2(t3, t4)

        return roll_x, pitch_y, yaw_z

    def check_goal_reached(self):
        """检查目标到达"""
        if not self.robot_position:
            return

        goal = self.goals[self.current_goal_index]
        goal_x, goal_y, goal_yaw = goal

        robot_x, robot_y, robot_yaw = self.robot_position

        # 计算距离
        distance = math.sqrt((goal_x - robot_x)**2 + (goal_y - robot_y)**2)

        # 检查是否到达
        if distance < self.goal_radius:
            self.on_goal_reached(goal, distance)

    def on_goal_reached(self, goal, distance):
        """目标到达处理"""
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        elapsed = time.time() - self.start_time

        goal_info = {
            'goal_index': self.current_goal_index,
            'goal_position': {'x': goal[0], 'y': goal[1], 'yaw': goal[2]},
            'robot_position': {'x': self.robot_position[0],
                              'y': self.robot_position[1],
                              'yaw': self.robot_position[2]},
            'distance': distance,
            'time_elapsed': elapsed,
            'timestamp': timestamp
        }

        self.goals_reached.append(goal_info)
        self.metrics['goals_reached'].append(goal_info)

        print("=" * 70)
        print(f"✅ 目标 {self.current_goal_index + 1}/{len(self.goals)} 已到达!")
        print(f"   目标位置: ({goal[0]:.2f}, {goal[1]:.2f})")
        print(f"   机器人位置: ({self.robot_position[0]:.2f}, {self.robot_position[1]:.2f})")
        print(f"   距离: {distance:.3f} m")
        print(f"   用时: {elapsed:.1f} 秒")
        print("=" * 70)

        self.current_goal_index += 1

        if self.current_goal_index >= len(self.goals):
            self.on_all_goals_reached()

    def on_all_goals_reached(self):
        """所有目标到达"""
        print("=" * 70)
        print("🎉 所有目标已完成！测试成功！")
        print(f"   已完成目标: {len(self.goals_reached)}/{len(self.goals)}")
        print(f"   总用时: {time.time() - self.start_time:.1f} 秒")
        print("=" * 70)

        # 设置监控标志为False（关键！）
        self.metrics['test_status'] = 'SUCCESS'
        self.metrics['test_end_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        self.metrics['total_time'] = time.time() - self.start_time

        print(f"🔴 设置 monitoring = False (当前值: {self.monitoring})")
        self.monitoring = False
        print(f"🔴 monitoring 已设置为: {self.monitoring}")
        print("=" * 70)

    def check_timeout(self):
        """检查超时"""
        if not self.monitoring or not self.start_time:
            return False

        elapsed = time.time() - self.start_time
        if elapsed > self.timeout:
            self.metrics['test_status'] = 'TIMEOUT'
            self.metrics['test_end_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            self.metrics['total_time'] = elapsed

            print("=" * 70)
            print(f"⏰ 测试超时 ({self.timeout} 秒)")
            print(f"   已完成目标: {len(self.goals_reached)}/{len(self.goals)}")
            print("=" * 70)
            print(f"🔴 设置 monitoring = False (超时)")
            self.monitoring = False
            return True

        return False

    def check_launch_process(self):
        """检查launch进程是否还活着"""
        if not self.launch_process:
            return True  # 如果没有launch进程引用，假设还活着

        # 检查进程是否还在运行
        poll_result = self.launch_process.poll()
        if poll_result is not None:
            # 进程已经死了
            self.metrics['test_status'] = 'ROS_DIED'
            self.metrics['test_end_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            self.metrics['total_time'] = time.time() - self.start_time
            self.metrics['ros_died'] = True
            self.metrics['launch_exit_code'] = poll_result

            print("=" * 70)
            print("❌ ROS进程已崩溃！")
            print(f"   Launch进程退出码: {poll_result}")
            print(f"   已完成目标: {len(self.goals_reached)}/{len(self.goals)}")
            print(f"   运行时间: {time.time() - self.start_time:.1f} 秒")
            print("=" * 70)

            self.monitoring = False
            return False

        return True

    def start_monitoring(self):
        """开始监控"""
        print("=" * 70)
        print("🚀 目标监控器启动")
        print("=" * 70)
        print(f"目标点数量: {len(self.goals)}")
        for i, goal in enumerate(self.goals):
            print(f"  目标 {i+1}: ({goal[0]:.2f}, {goal[1]:.2f}), yaw={goal[2]:.2f}")
        print(f"到达阈值: {self.goal_radius} m")
        print(f"超时时间: {self.timeout} 秒")
        if self.launch_process:
            print(f"Launch进程PID: {self.launch_process.pid}")
        print("=" * 70)

        # 关键修复：只在第一次调用时初始化ROS节点
        if not GoalMonitor._node_initialized:
            print("📡 初始化ROS节点...")
            rospy.init_node('goal_monitor', anonymous=True)
            GoalMonitor._node_initialized = True
            print("✓ ROS节点已初始化")
        else:
            print("✓ ROS节点已存在，跳过初始化")

        # 关键修复：每次测试都创建新的订阅者（修复回调绑定问题）
        # 先注销旧的订阅者（如果存在）
        if self.odom_subscriber is not None:
            print("📡 注销旧的odom订阅者...")
            self.odom_subscriber.unregister()
            self.odom_subscriber = None

        # 创建新的订阅者（绑定到当前实例的回调）
        print("📡 创建新的odom订阅者...")
        # 关键修复：指定queue_size=10，避免消息队列积压导致性能问题
        self.odom_subscriber = rospy.Subscriber('/odom', Odometry, self.odom_callback, queue_size=10)
        print("✓ 订阅者已创建（绑定到当前实例）")

        # 设置Manuscript Metrics订阅者
        if self.manuscript_metrics:
            print("📊 设置Manuscript Metrics订阅者...")
            self.manuscript_metrics.setup_ros_subscribers(self.model_name)  # 传入模型名称
            print("✓ Manuscript Metrics订阅者已设置")
        else:
            print("⚠️  Manuscript Metrics收集器未初始化")

        self.monitoring = True
        self.start_time = time.time()
        self.metrics['test_start_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        print("🔵 开始监控循环...")
        rate = rospy.Rate(10)  # 10Hz = 每0.1秒检查一次
        loop_count = 0

        while not rospy.is_shutdown() and self.monitoring:
            loop_count += 1

            # 检查超时
            if self.check_timeout():
                print("🔴 检测到超时，退出监控循环")
                break

            # 检查launch进程健康（关键修复！）
            if not self.check_launch_process():
                print("❌ 检测到ROS进程崩溃，停止监控")
                break

            # 每1秒打印一次状态（避免日志过多）
            if loop_count % 10 == 0:
                elapsed = time.time() - self.start_time
                print(f"🔄 监控中... 已用时: {elapsed:.1f}s, "
                      f"目标进度: {self.current_goal_index}/{len(self.goals)}, "
                      f"monitoring={self.monitoring}")

            # 🔥 关键修复：使用较短的sleep时间，以便更快响应 monitoring=False
            # 不要使用 rate.sleep()，因为它可能阻塞较长时间
            time.sleep(0.05)  # 50ms，比 rate.sleep() 更快响应

        # 📊 计算Manuscript Metrics（在监控循环结束后）
        print("📊 计算Manuscript性能指标...")
        self.metrics['manuscript_metrics'] = self.manuscript_metrics.compute_final_metrics()

        # 打印Manuscript Metrics摘要
        self.manuscript_metrics.print_summary()

        # 保存原始数据到文件
        raw_data = self.manuscript_metrics.get_raw_data()
        self.metrics['manuscript_raw_data'] = raw_data

        print(f"🔵 监控循环退出 (loop_count={loop_count}, monitoring={self.monitoring})")
        print(f"🔵 测试状态: {self.metrics['test_status']}")
        print(f"🔵 准备返回... (success={self.metrics['test_status'] == 'SUCCESS'})")
        return self.metrics['test_status'] == 'SUCCESS'

class AutomatedTestRunner:
    """自动化测试运行器"""

    def __init__(self, args):
        self.args = args
        # 关键修复：使用单一的GoalMonitor实例，避免回调函数绑定问题
        self.goal_monitor = None
        self.workspace_dir = Path(args.workspace)
        self.shelf_config = ShelfConfig(args.shelf_config)
        self.model_config = ModelConfig.get_model_params(args.model)
        self.results_manager = ResultsManager(args.results_dir, args.model)

        self.launch_process = None
        self.monitor_thread = None
        self.monitor_result = None

    def cleanup_ros_processes(self):
        """清理ROS进程"""
        TestLogger.info("清理ROS进程...")

        # 停止launch进程
        if self.launch_process:
            self.launch_process.terminate()
            try:
                self.launch_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.launch_process.kill()
            self.launch_process = None

        # 杀死所有ROS进程
        subprocess.run(['killall', '-9', 'roslaunch', 'rosmaster', 'roscore',
                       'gzserver', 'gzclient', 'python'],
                      stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)

        time.sleep(3)
        TestLogger.success("ROS进程清理完成")

    def launch_ros_system(self, shelf, test_dir):
        """启动ROS系统"""
        TestLogger.info(f"启动ROS系统...")
        TestLogger.info(f"  取货点: ({shelf['x']}, {shelf['y']})")
        TestLogger.info(f"  货架ID: {shelf['id']}")
        TestLogger.info(f"  模型: {self.args.model}")

        # 设置ROS环境变量
        env = os.environ.copy()

        # Source catkin workspace setup
        setup_bash = self.workspace_dir / "devel" / "setup.bash"
        if setup_bash.exists():
            TestLogger.info(f"Sourcing workspace setup: {setup_bash}")
            # 读取setup.bash并设置环境变量
            try:
                result = subprocess.run(
                    ['bash', '-c', f'source {setup_bash} && env'],
                    capture_output=True,
                    text=True,
                    cwd=self.workspace_dir
                )
                # 解析环境变量
                for line in result.stdout.split('\n'):
                    if '=' in line and not line.startswith('_'):
                        key, value = line.split('=', 1)
                        env[key] = value
            except Exception as e:
                TestLogger.warning(f"无法source workspace setup: {e}")

        # 构建launch命令 - 根据模型类型选择不同的launch文件
        is_mpc_ros = self.model_config.get('is_mpc_ros', False)

        if is_mpc_ros:
            # ✅ MPC纯基准：使用原始mpc_ros的nav_gazebo.launch（已验证工作正常）
            launch_cmd_str = (
                f"source {self.workspace_dir}/devel/setup.bash && "
                f"roslaunch mpc_ros nav_gazebo.launch "
                f"controller:=mpc "
                f"gui:={str(self.args.visualization).lower()}"
            )
        else:
            # safe_regret_mpc包使用统一的launch文件
            launch_cmd_str = (
                f"source {self.workspace_dir}/devel/setup.bash && "
                f"roslaunch safe_regret_mpc safe_regret_mpc_test.launch "
                f"pickup_x:={shelf['x']} "
                f"pickup_y:={shelf['y']} "
                f"pickup_yaw:={shelf['yaw']} "
                f"use_gazebo:={str(self.args.use_gazebo).lower()} "
                f"enable_visualization:={str(self.args.visualization).lower()} "
                f"debug_mode:=true "
                f"{self.model_config['params']}"
            )

        TestLogger.info(f"Launch命令: {launch_cmd_str}")

        # 启动launch文件
        log_file = test_dir / "logs" / "launch.log"
        with open(log_file, 'w') as f:
            self.launch_process = subprocess.Popen(
                ['bash', '-c', launch_cmd_str],
                stdout=f,
                stderr=subprocess.STDOUT,
                env=env,
                cwd=str(self.workspace_dir)
            )

        TestLogger.info(f"ROS系统启动中 (PID: {self.launch_process.pid})...")

        # 等待系统初始化
        time.sleep(10)

        # 检查是否启动成功
        if self.launch_process.poll() is not None:
            TestLogger.error("ROS系统启动失败")
            return False

        TestLogger.success("ROS系统启动成功")
        return True

    def monitor_goals(self, shelf, test_dir):
        """监控目标到达"""
        pickup_goal = (shelf['x'], shelf['y'], shelf['yaw'])
        delivery_goal = (8.0, 0.0, 0.0)  # 固定卸货点

        goals = [pickup_goal, delivery_goal]

        # 关键修复：复用单一的GoalMonitor实例，避免回调函数绑定问题
        if self.goal_monitor is None:
            # 第一次创建
            TestLogger.info("创建GoalMonitor实例...")
            self.goal_monitor = GoalMonitor(
                goals=goals,
                goal_radius=0.5,
                timeout=self.args.timeout,
                launch_process=self.launch_process,
                test_dir=test_dir,  # 传入测试目录
                model_name=self.args.model  # 传入模型名称
            )
        else:
            # 后续测试：重置现有实例
            TestLogger.info("重置GoalMonitor实例...")
            self.goal_monitor.reset(
                goals=goals,
                goal_radius=0.5,
                timeout=self.args.timeout,
                launch_process=self.launch_process,
                test_dir=test_dir,  # 传入测试目录
                model_name=self.args.model  # 传入模型名称
            )

        # 保存metrics
        metrics_file = test_dir / "metrics.json"

        try:
            success = self.goal_monitor.start_monitoring()

            # 保存metrics
            with open(metrics_file, 'w') as f:
                json.dump(self.goal_monitor.metrics, f, indent=2)

            # 如果ROS死了，返回False
            if self.goal_monitor.metrics.get('ros_died', False):
                TestLogger.error("ROS进程在测试中崩溃")
                return False

            return success

        except Exception as e:
            TestLogger.error(f"监控过程出错: {e}")
            return False

    def run_single_test(self, test_index, shelf):
        """运行单个测试"""
        TestLogger.test("=" * 70)
        TestLogger.test(f"开始测试 {test_index}/{self.args.num_shelves}: {shelf['name']}")
        TestLogger.test("=" * 70)

        # 创建测试目录
        test_dir = self.results_manager.create_test_directory(test_index, shelf['id'])

        # 启动ROS系统
        if not self.launch_ros_system(shelf, test_dir):
            self.generate_test_summary(shelf, test_dir, "FAILED (启动失败)")
            return False

        # 等待系统稳定
        time.sleep(5)

        # 监控目标到达
        TestLogger.info("等待机器人完成取货和卸货任务...")
        test_result = "SUCCESS"

        try:
            TestLogger.info("🔵 开始调用 monitor_goals()...")
            monitor_success = self.monitor_goals(shelf, test_dir)
            TestLogger.info(f"🔵 monitor_goals() 返回: {monitor_success}")

            if monitor_success:
                TestLogger.success("测试成功")
            else:
                # 检查是否是ROS崩溃
                metrics_file = test_dir / "metrics.json"
                if metrics_file.exists():
                    with open(metrics_file, 'r') as f:
                        metrics = json.load(f)
                        if metrics.get('ros_died', False):
                            TestLogger.error(f"ROS进程崩溃 (退出码: {metrics.get('launch_exit_code', 'unknown')})")
                            test_result = "ROS_DIED"
                        else:
                            TestLogger.warning("测试超时")
                            test_result = "TIMEOUT"
                else:
                    TestLogger.warning("测试超时")
                    test_result = "TIMEOUT"
        except Exception as e:
            TestLogger.error(f"测试过程出错: {e}")
            test_result = "ERROR"

        # 清理ROS进程
        TestLogger.info("🔵 开始清理ROS进程...")

        # 关键修复：先注销ROS订阅者，避免订阅者累积
        if self.goal_monitor is not None:
            TestLogger.info("📡 注销GoalMonitor订阅者...")
            # 注销odom订阅者
            if self.goal_monitor.odom_subscriber is not None:
                self.goal_monitor.odom_subscriber.unregister()
                self.goal_monitor.odom_subscriber = None
            # 注销manuscript metrics订阅者
            if self.goal_monitor.manuscript_metrics is not None:
                self.goal_monitor.manuscript_metrics.shutdown_subscribers()
            TestLogger.success("✓ 订阅者已注销")

        self.cleanup_ros_processes()
        TestLogger.info("🔵 ROS进程清理完成")

        # 生成测试摘要
        TestLogger.info("🔵 开始生成测试摘要...")
        self.generate_test_summary(shelf, test_dir, test_result)
        TestLogger.info("🔵 测试摘要生成完成")

        # 🔥 新增：处理单个测试的manuscript指标并生成图表
        # 即使测试超时或失败，也尝试处理数据（只要manuscript数据存在）
        TestLogger.info("📊 开始处理manuscript指标...")
        try:
            self.process_single_test_metrics(test_dir, shelf)
        except Exception as e:
            TestLogger.warning(f"  Manuscript指标处理失败: {e}")
            import traceback
            TestLogger.warning(f"  错误详情: {traceback.format_exc()}")
            TestLogger.warning(f"  继续执行后续测试...")

        TestLogger.test(f"测试 {test_index}/{self.args.num_shelves} 完成: {test_result}")

        # 等待系统完全关闭
        time.sleep(self.args.interval)

        return test_result == "SUCCESS"

    def generate_test_summary(self, shelf, test_dir, test_result):
        """生成测试摘要"""
        summary_file = test_dir / "test_summary.txt"

        with open(summary_file, 'w') as f:
            f.write("=" * 65 + "\n")
            f.write("           测试摘要 / Test Summary\n")
            f.write("=" * 65 + "\n\n")

            f.write(f"测试时间 / Test Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

            f.write("模型配置 / Model Configuration:\n")
            f.write(f"  - 模型类型 / Model Type: {self.args.model}\n")
            f.write(f"  - 货架ID / Shelf ID: {shelf['id']}\n")
            f.write(f"  - 货架名称 / Shelf Name: {shelf['name']}\n\n")

            f.write(f"测试结果 / Test Result: {test_result}\n\n")

            # 如果是ROS崩溃，添加详细信息
            if test_result == "ROS_DIED":
                f.write("❌ ROS进程崩溃详情 / ROS Process Crash Details:\n")
                metrics_file = test_dir / "metrics.json"
                if metrics_file.exists():
                    with open(metrics_file, 'r') as mf:
                        metrics = json.load(mf)
                        f.write(f"  - 退出码 / Exit Code: {metrics.get('launch_exit_code', 'unknown')}\n")
                        f.write(f"  - 运行时间 / Runtime: {metrics.get('total_time', 0):.1f}秒\n")
                        f.write(f"  - 已完成目标 / Goals Reached: {len(metrics.get('goals_reached', []))}/{metrics.get('total_goals', 0)}\n")

                        # 尝试读取launch.log的最后几行
                        log_file = test_dir / "logs" / "launch.log"
                        if log_file.exists():
                            f.write("\n  - Launch日志最后20行 / Last 20 lines of launch.log:\n")
                            try:
                                with open(log_file, 'r') as lf:
                                    lines = lf.readlines()
                                    last_lines = lines[-20:] if len(lines) > 20 else lines
                                    for line in last_lines:
                                        f.write(f"    {line.rstrip()}\n")
                            except Exception as e:
                                f.write(f"    [无法读取日志: {e}]\n")
                f.write("\n")

            f.write("测试参数 / Test Parameters:\n")
            f.write(f"  - 超时时间 / Timeout: {self.args.timeout}秒\n")
            f.write(f"  - Gazebo启用 / Gazebo Enabled: {self.args.use_gazebo}\n")
            f.write(f"  - 可视化 / Visualization: {self.args.visualization}\n\n")

            f.write("文件位置 / File Locations:\n")
            f.write(f"  - Launch日志 / Launch Log: logs/launch.log\n")
            f.write(f"  - 测试指标 / Metrics: metrics.json\n\n")

            f.write("=" * 65 + "\n")

        TestLogger.success(f"测试摘要已生成: {summary_file}")

    def generate_final_report(self, successful_tests):
        """生成最终报告"""
        report_file = self.results_manager.test_results_dir / "final_report.txt"

        with open(report_file, 'w') as f:
            f.write("=" * 65 + "\n")
            f.write("      自动化Baseline测试最终报告\n")
            f.write("   Automated Baseline Testing Final Report\n")
            f.write("=" * 65 + "\n\n")

            f.write(f"测试时间 / Test Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

            f.write("模型配置 / Model Configuration:\n")
            f.write(f"  - 模型类型 / Model Type: {self.args.model}\n")
            f.write(f"  - 模型名称 / Model Name: {self.model_config['name']}\n\n")

            f.write("测试统计 / Test Statistics:\n")
            f.write(f"  - 总测试数 / Total Tests: {self.args.num_shelves}\n")
            f.write(f"  - 成功测试 / Successful Tests: {successful_tests}\n")
            f.write(f"  - 失败测试 / Failed Tests: {self.args.num_shelves - successful_tests}\n")
            success_rate = successful_tests * 100.0 / self.args.num_shelves
            f.write(f"  - 成功率 / Success Rate: {success_rate:.1f}%\n\n")

            f.write("测试参数 / Test Parameters:\n")
            f.write(f"  - 超时时间 / Timeout: {self.args.timeout}秒/测试\n")
            f.write(f"  - Gazebo启用 / Gazebo Enabled: {self.args.use_gazebo}\n")
            f.write(f"  - 可视化 / Visualization: {self.args.visualization}\n\n")

            f.write("结果目录 / Results Directory:\n")
            f.write(f"  {self.results_manager.test_results_dir}\n\n")

            f.write("=" * 65 + "\n")

        TestLogger.success(f"最终报告已生成: {report_file}")

        # 打印报告内容
        with open(report_file, 'r') as f:
            print(f.read())

        # 🔥 新增：聚合所有测试的manuscript指标并生成综合图表
        TestLogger.info("📊 开始聚合所有测试的manuscript指标...")
        try:
            self.aggregate_all_tests_metrics()
        except Exception as e:
            TestLogger.error(f"  聚合manuscript指标失败: {e}")
            TestLogger.error(f"  请检查测试数据是否完整")

    def process_single_test_metrics(self, test_dir: Path, shelf: Dict):
        """
        处理单个测试的manuscript指标并生成图表

        Args:
            test_dir: 测试目录路径
            shelf: 货架信息字典
        """
        TestLogger.info(f"📊 处理测试 {shelf['id']} 的manuscript指标...")

        # 读取metrics.json
        metrics_file = test_dir / "metrics.json"
        if not metrics_file.exists():
            TestLogger.warning(f"  未找到metrics.json，跳过数据处理")
            return

        with open(metrics_file, 'r') as f:
            metrics = json.load(f)

        # 检查是否有manuscript指标
        if 'manuscript_metrics' not in metrics:
            TestLogger.warning(f"  未找到manuscript_metrics，跳过数据处理")
            return

        manuscript_metrics = metrics['manuscript_metrics']
        manuscript_raw_data = metrics.get('manuscript_raw_data', {})

        # 创建图表输出目录
        figures_dir = test_dir / "figures"
        figures_dir.mkdir(exist_ok=True)

        # 生成单测试指标报告
        self._generate_single_test_report(manuscript_metrics, test_dir, shelf)

        # 生成单测试图表（传递raw_data用于绘图）
        self._generate_single_test_figures(manuscript_metrics, manuscript_raw_data, figures_dir, shelf)

        TestLogger.success(f"  ✓ 测试 {shelf['id']} 数据处理完成")

    def _generate_single_test_report(self, metrics: Dict, test_dir: Path, shelf: Dict):
        """生成单个测试的指标报告（处理扁平数据格式）"""
        report_file = test_dir / "manuscript_metrics_report.txt"

        with open(report_file, 'w') as f:
            f.write("=" * 70 + "\n")
            f.write(f"Manuscript性能指标报告 - 测试 {shelf['id']}: {shelf['name']}\n")
            f.write("=" * 70 + "\n\n")

            f.write(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"货架位置: ({shelf['x']}, {shelf['y']})\n\n")

            # 辅助函数：安全获取值
            def get_value(data, key, default=0, is_float=False):
                """从嵌套或扁平数据中获取值"""
                if key in data:
                    val = data[key]
                    if isinstance(val, dict):
                        return val.get(default, 0) if isinstance(default, str) else val.get(default, 0)
                    else:
                        return float(val) if is_float else val
                return default

            # 1. Satisfaction Probability（扁平格式）
            if 'satisfaction_probability' in metrics:
                sat = metrics['satisfaction_probability']
                if sat is not None:  # ✅ 检查是否为None
                    f.write("1. STL Satisfaction Probability\n")
                    if isinstance(sat, dict):
                        f.write(f"   满足步数: {sat.get('satisfied_steps', 0)}/{sat.get('total_steps', 0)}\n")
                        f.write(f"   最终鲁棒性: {sat.get('final_robustness', 0):.4f}\n")
                        f.write(f"   平均鲁棒性: {sat.get('mean_robustness', 0):.4f}\n")
                    else:
                        # 扁平格式，直接是概率值
                        f.write(f"   满足概率: {float(sat):.4f}\n")
                    f.write("\n")
                else:
                    f.write("1. STL Satisfaction Probability\n")
                    f.write("   满足概率: N/A (STL未启用)\n\n")

            # 2. Empirical Risk（扁平格式：observed_violation_rate）
            f.write("2. Empirical Risk (安全约束违反)\n")
            if 'tube_violation_count' in metrics:
                f.write(f"   违反次数: {metrics['tube_violation_count']}\n")
            if 'total_steps' in metrics:
                f.write(f"   总步数: {metrics['total_steps']}\n")
            if 'observed_violation_rate' in metrics:
                f.write(f"   观察风险: {metrics['observed_violation_rate']:.4f}\n")
            f.write(f"   目标风险 δ: 0.05\n\n")

            # 3. Regret（扁平格式：avg_dynamic_regret）
            if 'avg_dynamic_regret' in metrics and metrics['avg_dynamic_regret'] is not None:
                f.write("3. Dynamic Regret\n")
                f.write(f"   归一化遗憾: {metrics['avg_dynamic_regret']:.4f}\n\n")

            # 4. Feasibility（扁平格式：feasibility_rate）
            if 'feasibility_rate' in metrics:
                f.write("4. MPC Feasibility\n")
                rate = metrics['feasibility_rate']
                if 'total_mpc_solves' in metrics:
                    total = metrics['total_mpc_solves']
                    feasible = int(rate * total)
                    f.write(f"   可行求解: {feasible}/{total}\n")
                f.write(f"   可行率: {float(rate)*100:.2f}%\n\n")

            # 5. Computation（扁平格式：median_solve_time, mean_solve_time等）
            f.write("5. Computation Time\n")
            if 'median_solve_time' in metrics and metrics['median_solve_time'] is not None:
                f.write(f"   中位数: {metrics['median_solve_time']:.2f} ms\n")
            if 'mean_solve_time' in metrics and metrics['mean_solve_time'] is not None:
                f.write(f"   平均值: {metrics['mean_solve_time']:.2f} ms\n")
            if 'p90_solve_time' in metrics and metrics['p90_solve_time'] is not None:
                f.write(f"   P90: {metrics['p90_solve_time']:.2f} ms\n")
            if 'mpc_failure_count' in metrics and metrics['mpc_failure_count'] is not None:
                f.write(f"   失败次数: {metrics['mpc_failure_count']}\n")
            f.write("\n")

            # 6. Tracking Error（扁平格式：mean_tracking_error等）
            f.write("6. Tracking Error\n")
            if 'mean_tracking_error' in metrics and metrics['mean_tracking_error'] is not None:
                f.write(f"   平均误差: {metrics['mean_tracking_error']*100:.2f} cm\n")
            if 'max_tracking_error' in metrics and metrics['max_tracking_error'] is not None:
                f.write(f"   最大误差: {metrics['max_tracking_error']*100:.2f} cm\n")
            if 'std_tracking_error' in metrics and metrics['std_tracking_error'] is not None:
                f.write(f"   标准差: {metrics['std_tracking_error']*100:.2f} cm\n")
            if 'tube_occupancy_rate' in metrics and metrics['tube_occupancy_rate'] is not None:
                f.write(f"   管占用量: {metrics['tube_occupancy_rate']*100:.2f}%\n")
            f.write("\n")

            # 7. Calibration（扁平格式：calibration_error）
            f.write("7. Calibration Accuracy\n")
            f.write(f"   目标风险 δ: 0.05\n")
            if 'observed_violation_rate' in metrics and metrics['observed_violation_rate'] is not None:
                f.write(f"   观察风险: {metrics['observed_violation_rate']:.4f}\n")
            if 'calibration_error' in metrics and metrics['calibration_error'] is not None:
                f.write(f"   校准误差: {metrics['calibration_error']:.4f}\n")
                is_well = metrics['calibration_error'] < 0.01
                f.write(f"   是否校准良好: {'是' if is_well else '否'}\n")
            f.write("\n")

            f.write("=" * 70 + "\n")

        TestLogger.success(f"  ✓ 指标报告已生成: {report_file}")

    def _generate_single_test_figures(self, metrics: Dict, raw_data: Dict, figures_dir: Path, shelf: Dict):
        """生成单个测试的图表

        Args:
            metrics: manuscript_metrics字典（包含聚合指标）
            raw_data: manuscript_raw_data字典（包含历史数据数组）
            figures_dir: 图表输出目录
            shelf: 货架信息
        """
        # 设置matplotlib参数
        plt.rcParams['font.size'] = 10
        plt.rcParams['font.family'] = ['sans-serif']  # 使用sans-serif，通常有更好的字体支持
        plt.rcParams['axes.unicode_minus'] = False  # 正确显示负号
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

        # 创建综合仪表板（2x2子图）
        fig, axes = plt.subplots(2, 2, figsize=(12, 10))
        fig.suptitle(f'Test {shelf["id"]}: {shelf["name"]} - Performance Dashboard',
                    fontsize=14, fontweight='bold')

        # 1. STL Robustness History (左上)
        if 'stl_robustness_history' in raw_data and len(raw_data['stl_robustness_history']) > 0:
            ax = axes[0, 0]
            history = raw_data['stl_robustness_history']
            ax.plot(history, color=COLORS['blue'], linewidth=1.5)
            ax.axhline(y=0, color=COLORS['green'], linestyle='--', linewidth=2, label='Satisfaction threshold')
            ax.set_xlabel('Time step', fontweight='bold')
            ax.set_ylabel('STL Robustness', fontweight='bold')
            ax.set_title('(a) STL Robustness Over Time', fontweight='bold')
            ax.grid(alpha=0.3)
            ax.legend()

        # 2. Tracking Error (右上)
        if 'tracking_error_norm_history' in raw_data and len(raw_data['tracking_error_norm_history']) > 0:
            ax = axes[0, 1]
            errors = raw_data['tracking_error_norm_history']
            ax.plot(np.array(errors) * 100, color=COLORS['blue'], linewidth=1.5)  # 转为cm
            tube_radius_cm = metrics.get('tube_radius', 0.18) * 100
            ax.axhline(y=tube_radius_cm, color=COLORS['red'], linestyle='--',
                      linewidth=2, label=f'Tube radius ({tube_radius_cm:.1f} cm)')
            ax.set_xlabel('Time step', fontweight='bold')
            ax.set_ylabel('Tracking Error (cm)', fontweight='bold')
            ax.set_title('(b) Tracking Error Over Time', fontweight='bold')
            ax.grid(alpha=0.3)
            ax.legend()

        # 3. MPC Solve Time (左下)
        if 'mpc_solve_times' in raw_data and len(raw_data['mpc_solve_times']) > 0:
            ax = axes[1, 0]
            solve_times = raw_data['mpc_solve_times']
            ax.plot(solve_times, color=COLORS['blue'], linewidth=1.5, alpha=0.7)
            ax.axhline(y=8, color=COLORS['green'], linestyle='--',
                      linewidth=2, label='Real-time budget (8 ms)')
            ax.set_xlabel('Solve number', fontweight='bold')
            ax.set_ylabel('Solve Time (ms)', fontweight='bold')
            ax.set_title('(c) MPC Computation Time', fontweight='bold')
            ax.grid(alpha=0.3)
            ax.legend()

        # 4. Violation Comparison (右下) - ✅ 新增：Safety vs Tube Violation
        ax = axes[1, 1]

        # 获取数据
        tube_violations = metrics.get('tube_violation_count', 0)
        total_steps = metrics.get('total_steps', metrics.get('safety_total_steps', 1))

        # ✅ 关键修复：使用dr_margins_history判断是否有safety约束
        # tube_mpc和stl模式没有DR约束，dr_margins_history为空或不存在
        # dr和safe_regret模式有DR约束，dr_margins_history有数据
        has_dr_margins = (
            'dr_margins_history' in raw_data and
            len(raw_data['dr_margins_history']) > 0
        )

        if has_dr_margins:
            safety_violations = raw_data.get('safety_violation_count', 0)
            safety_total = raw_data.get('safety_total_steps', total_steps)
            safety_rate = (safety_violations / safety_total * 100) if safety_total > 0 else 0
            safety_label = f'Safety\n({safety_violations}/{safety_total} = {safety_rate:.1f}%)'
            safety_value = safety_violations
        else:
            safety_label = 'Safety\n(N/A - 无DR约束)'
            safety_value = 0  # 用于绘图，但标签会显示N/A

        tube_rate = (tube_violations / total_steps * 100) if total_steps > 0 else 0
        tube_label = f'Tube\n({tube_violations}/{total_steps} = {tube_rate:.1f}%)'

        # 创建柱状图
        categories = ['Safety Violation', 'Tube Violation']
        values = [safety_value, tube_violations]
        colors_bar = [COLORS['red'] if has_dr_margins else COLORS['gray'], COLORS['blue']]
        labels_display = [safety_label, tube_label]

        bars = ax.bar(categories, values, color=colors_bar, alpha=0.7, edgecolor='black', linewidth=1.5)

        # 添加数值标签
        for i, (bar, label) in enumerate(zip(bars, labels_display)):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   label,
                   ha='center', va='bottom', fontweight='bold', fontsize=10)

        ax.set_ylabel('Violation Count', fontweight='bold')
        ax.set_title('(d) Safety vs Tube Violations', fontweight='bold')
        ax.grid(axis='y', alpha=0.3)

        # 添加说明文字
        if not has_dr_margins:
            ax.text(0.5, 0.95, '⚠️ Safety约束未启用（无DR）',
                   transform=ax.transAxes, ha='center', va='top',
                   fontsize=9, style='italic', color=COLORS['red'])

        plt.tight_layout()
        dashboard_file = figures_dir / "performance_dashboard.png"
        plt.savefig(dashboard_file, dpi=300)
        plt.savefig(figures_dir / "performance_dashboard.pdf")
        plt.close()

        TestLogger.success(f"  ✓ 性能仪表板已生成: {dashboard_file}")

    def aggregate_all_tests_metrics(self):
        """
        聚合所有测试的manuscript指标并生成综合报告和图表
        """
        TestLogger.info("=" * 70)
        TestLogger.info("📊 聚合所有测试的manuscript指标...")
        TestLogger.info("=" * 70)

        test_results_dir = self.results_manager.test_results_dir

        # 收集所有测试的metrics
        all_tests_data = []

        for test_dir in sorted(test_results_dir.glob("test_*")):
            metrics_file = test_dir / "metrics.json"
            if metrics_file.exists():
                with open(metrics_file, 'r') as f:
                    test_data = json.load(f)
                    if 'manuscript_metrics' in test_data:
                        all_tests_data.append(test_data['manuscript_metrics'])
                        TestLogger.info(f"  ✓ 加载测试数据: {test_dir.name}")

        if not all_tests_data:
            TestLogger.warning("  未找到任何manuscript指标数据")
            return

        TestLogger.info(f"  共加载 {len(all_tests_data)} 个测试的数据")

        # 计算聚合指标
        aggregated_metrics = self._compute_aggregated_metrics(all_tests_data)

        # 保存聚合指标
        aggregated_file = test_results_dir / "aggregated_manuscript_metrics.json"
        with open(aggregated_file, 'w') as f:
            json.dump(aggregated_metrics, f, indent=2)

        TestLogger.success(f"  ✓ 聚合指标已保存: {aggregated_file}")

        # 生成聚合报告
        self._generate_aggregated_report(aggregated_metrics, test_results_dir)

        # 生成聚合图表
        self._generate_aggregated_figures(aggregated_metrics, test_results_dir)

        TestLogger.success("📊 所有测试数据处理完成！")

    def _compute_aggregated_metrics(self, all_tests_data: List[Dict]) -> Dict:
        """计算聚合的manuscript指标（处理扁平数据结构）"""
        aggregated = {
            'metadata': {
                'total_tests': len(all_tests_data),
                'computation_time': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                'model': self.args.model
            }
        }

        # 1. Satisfaction Probability（扁平格式：直接是float值）
        all_satisfied = []
        for test in all_tests_data:
            if 'satisfaction_probability' in test:
                sat = test['satisfaction_probability']
                # 处理扁平格式（直接值）或嵌套格式（字典）
                if isinstance(sat, dict):
                    satisfied = sat.get('satisfied_steps', 0)
                    total = sat.get('total_steps', 1)
                    if total > 0:
                        all_satisfied.append(satisfied / total)
                elif sat is not None:  # ✅ 检查是否为None
                    # 扁平格式，直接是概率值
                    all_satisfied.append(float(sat))

        if all_satisfied:
            aggregated['satisfaction_probability'] = {
                'mean': float(np.mean(all_satisfied)),
                'std': float(np.std(all_satisfied)),
                'min': float(np.min(all_satisfied)),
                'max': float(np.max(all_satisfied)),
                'median': float(np.median(all_satisfied))
            }

        # 2. Empirical Risk（扁平格式）
        all_risks = []
        total_violations = 0
        total_steps = 0

        for test in all_tests_data:
            if 'observed_violation_rate' in test:
                # 扁平格式
                risk = test['observed_violation_rate']
                all_risks.append(float(risk))
                if 'tube_violation_count' in test:
                    total_violations += test['tube_violation_count']
            elif 'empirical_risk' in test:
                # 嵌套格式
                risk = test['empirical_risk']
                if isinstance(risk, dict):
                    observed_risk = risk.get('observed_risk', 0)
                    all_risks.append(float(observed_risk))
                    total_violations += risk.get('violation_count', 0)
                    total_steps += risk.get('total_steps', 0)
                else:
                    all_risks.append(float(risk))

            if 'total_steps' in test:
                total_steps += test['total_steps']

        aggregated_risk = total_violations / total_steps if total_steps > 0 else 0

        if all_risks:
            aggregated['empirical_risk'] = {
                'mean': float(np.mean(all_risks)),
                'std': float(np.std(all_risks)),
                'min': float(np.min(all_risks)),
                'max': float(np.max(all_risks)),
                'aggregated_risk': float(aggregated_risk),
                'total_violations': total_violations,
                'total_steps': total_steps
            }

        # 3. Regret（扁平格式：avg_dynamic_regret）
        all_regrets = []
        for test in all_tests_data:
            if 'avg_dynamic_regret' in test and test['avg_dynamic_regret'] is not None:
                all_regrets.append(float(test['avg_dynamic_regret']))
            elif 'regret' in test:
                regret = test['regret']
                if isinstance(regret, dict):
                    all_regrets.append(regret.get('normalized_regret', 0))
                else:
                    all_regrets.append(float(regret))

        if all_regrets:
            aggregated['regret'] = {
                'mean': float(np.mean(all_regrets)),
                'std': float(np.std(all_regrets)),
                'min': float(np.min(all_regrets)),
                'max': float(np.max(all_regrets)),
                'median': float(np.median(all_regrets))
            }

        # 4. Feasibility（扁平格式：feasibility_rate）
        all_feasibility = []
        total_feasible = 0
        total_solves = 0

        for test in all_tests_data:
            if 'feasibility_rate' in test:
                # 扁平格式
                rate = test['feasibility_rate']
                all_feasibility.append(float(rate))
                if 'total_mpc_solves' in test:
                    total_solves += test['total_mpc_solves']
                    total_feasible += int(rate * test['total_mpc_solves'])
            elif 'feasibility' in test:
                # 嵌套格式
                feas = test['feasibility']
                if isinstance(feas, dict):
                    rate = feas.get('feasibility_rate', 0)
                    all_feasibility.append(float(rate))
                    total_feasible += feas.get('feasible_count', 0)
                    total_solves += feas.get('total_solves', 0)
                else:
                    all_feasibility.append(float(feas))

        aggregated_feasibility = total_feasible / total_solves if total_solves > 0 else 0

        if all_feasibility:
            aggregated['feasibility'] = {
                'mean': float(np.mean(all_feasibility)),
                'std': float(np.std(all_feasibility)),
                'min': float(np.min(all_feasibility)),
                'max': float(np.max(all_feasibility)),
                'aggregated_rate': float(aggregated_feasibility),
                'total_feasible': total_feasible,
                'total_solves': total_solves
            }

        # 5. Computation（扁平格式：median_solve_time, mean_solve_time等）
        all_medians = []
        all_means = []
        total_failures = 0

        for test in all_tests_data:
            if 'median_solve_time' in test and test['median_solve_time'] is not None:
                # 扁平格式
                all_medians.append(float(test['median_solve_time']))
                if 'mean_solve_time' in test and test['mean_solve_time'] is not None:
                    all_means.append(float(test['mean_solve_time']))
                if 'mpc_failure_count' in test and test['mpc_failure_count'] is not None:
                    total_failures += test['mpc_failure_count']
            elif 'computation' in test:
                # 嵌套格式
                comp = test['computation']
                if isinstance(comp, dict):
                    all_medians.append(comp.get('median_ms', 0))
                    all_means.append(comp.get('mean_ms', 0))
                    total_failures += comp.get('failure_count', 0)

        if all_medians:
            aggregated['computation'] = {
                'median_of_medians': float(np.median(all_medians)),
                'mean_median': float(np.mean(all_medians)),
                'std_median': float(np.std(all_medians)),
                'min_median': float(np.min(all_medians)),
                'max_median': float(np.max(all_medians)),
                'mean_of_means': float(np.mean(all_means)) if all_means else 0,
                'total_failures': total_failures
            }

        # 6. Tracking Error（扁平格式：mean_tracking_error等）
        all_mean_errors = []
        all_max_errors = []
        all_occupancy = []

        for test in all_tests_data:
            if 'mean_tracking_error' in test and test['mean_tracking_error'] is not None:
                # 扁平格式
                all_mean_errors.append(float(test['mean_tracking_error']))
                if 'max_tracking_error' in test and test['max_tracking_error'] is not None:
                    all_max_errors.append(float(test['max_tracking_error']))
                if 'tube_occupancy_rate' in test and test['tube_occupancy_rate'] is not None:
                    all_occupancy.append(float(test['tube_occupancy_rate']))
            elif 'tracking' in test:
                # 嵌套格式
                track = test['tracking']
                if isinstance(track, dict):
                    all_mean_errors.append(track.get('mean_error_m', 0))
                    all_max_errors.append(track.get('max_error_m', 0))
                    all_occupancy.append(track.get('occupancy_rate', 0))

        if all_mean_errors:
            aggregated['tracking'] = {
                'mean_error_mean': float(np.mean(all_mean_errors)),
                'mean_error_std': float(np.std(all_mean_errors)),
                'max_error_mean': float(np.mean(all_max_errors)) if all_max_errors else 0,
                'occupancy_mean': float(np.mean(all_occupancy)),
                'occupancy_std': float(np.std(all_occupancy))
            }

        # 7. Calibration（扁平格式：calibration_error）
        all_calibration_errors = []

        for test in all_tests_data:
            if 'calibration_error' in test and test['calibration_error'] is not None:
                # 扁平格式
                all_calibration_errors.append(float(test['calibration_error']))
            elif 'calibration' in test:
                # 嵌套格式
                calib = test['calibration']
                if isinstance(calib, dict):
                    all_calibration_errors.append(calib.get('calibration_error', 0))
                else:
                    all_calibration_errors.append(float(calib))

        if all_calibration_errors:
            aggregated['calibration'] = {
                'mean_error': float(np.mean(all_calibration_errors)),
                'std_error': float(np.std(all_calibration_errors)),
                'min_error': float(np.min(all_calibration_errors)),
                'max_error': float(np.max(all_calibration_errors)),
                'well_calibrated_count': sum(1 for e in all_calibration_errors if e < 0.01),
                'total_tests': len(all_calibration_errors)
            }

        return aggregated

    def _generate_aggregated_report(self, metrics: Dict, output_dir: Path):
        """生成聚合指标报告"""
        report_file = output_dir / "aggregated_manuscript_metrics_report.txt"

        with open(report_file, 'w', encoding='utf-8') as f:
            f.write("=" * 70 + "\n")
            f.write("聚合Manuscript性能指标报告\n")
            f.write("Aggregated Manuscript Performance Metrics Report\n")
            f.write("=" * 70 + "\n\n")

            f.write(f"生成时间: {metrics['metadata']['computation_time']}\n")
            f.write(f"模型: {metrics['metadata']['model']}\n")
            f.write(f"测试数量: {metrics['metadata']['total_tests']}\n\n")

            # 1. Satisfaction Probability
            if 'satisfaction_probability' in metrics:
                sat = metrics['satisfaction_probability']
                f.write("1. STL Satisfaction Probability (聚合统计)\n")
                f.write(f"   均值: {sat['mean']*100:.2f}%\n")
                f.write(f"   标准差: {sat['std']*100:.2f}%\n")
                f.write(f"   中位数: {sat['median']*100:.2f}%\n")
                f.write(f"   范围: [{sat['min']*100:.2f}%, {sat['max']*100:.2f}%]\n\n")

            # 2. Empirical Risk
            if 'empirical_risk' in metrics:
                risk = metrics['empirical_risk']
                f.write("2. Empirical Risk (聚合统计)\n")
                f.write(f"   聚合风险: {risk['aggregated_risk']:.4f}\n")
                f.write(f"   均值: {risk['mean']:.4f} ± {risk['std']:.4f}\n")
                f.write(f"   总违反次数: {risk['total_violations']}\n")
                f.write(f"   总步数: {risk['total_steps']}\n\n")

            # 3. Regret
            if 'regret' in metrics:
                regret = metrics['regret']
                f.write("3. Dynamic Regret (聚合统计)\n")
                f.write(f"   均值: {regret['mean']:.4f} ± {regret['std']:.4f}\n")
                f.write(f"   中位数: {regret['median']:.4f}\n")
                f.write(f"   范围: [{regret['min']:.4f}, {regret['max']:.4f}]\n\n")

            # 4. Feasibility
            if 'feasibility' in metrics:
                feas = metrics['feasibility']
                f.write("4. MPC Feasibility (聚合统计)\n")
                f.write(f"   聚合可行率: {feas['aggregated_rate']*100:.2f}%\n")
                f.write(f"   均值: {feas['mean']*100:.2f}% ± {feas['std']*100:.2f}%\n")
                f.write(f"   总可行求解: {feas['total_feasible']}/{feas['total_solves']}\n\n")

            # 5. Computation
            if 'computation' in metrics:
                comp = metrics['computation']
                f.write("5. Computation Time (聚合统计)\n")
                f.write(f"   中位数的中位数: {comp['median_of_medians']:.2f} ms\n")
                f.write(f"   均值: {comp['mean_median']:.2f} ± {comp['std_median']:.2f} ms\n")
                f.write(f"   范围: [{comp['min_median']:.2f}, {comp['max_median']:.2f}] ms\n")
                f.write(f"   总失败次数: {comp['total_failures']}\n\n")

            # 6. Tracking Error
            if 'tracking' in metrics:
                track = metrics['tracking']
                f.write("6. Tracking Error (聚合统计)\n")
                f.write(f"   平均误差均值: {track['mean_error_mean']*100:.2f} ± {track['mean_error_std']*100:.2f} cm\n")
                if 'max_error_mean' in track:
                    f.write(f"   最大误差均值: {track['max_error_mean']*100:.2f} cm\n")
                f.write(f"   管占用量: {track['occupancy_mean']*100:.2f}% ± {track['occupancy_std']*100:.2f}%\n\n")

            # 7. Calibration
            if 'calibration' in metrics:
                calib = metrics['calibration']
                f.write("7. Calibration Accuracy (聚合统计)\n")
                f.write(f"   校准误差均值: {calib['mean_error']:.4f} ± {calib['std_error']:.4f}\n")
                f.write(f"   校准良好测试数: {calib['well_calibrated_count']}/{calib['total_tests']}\n\n")

            f.write("=" * 70 + "\n")

        TestLogger.success(f"  ✓ 聚合报告已生成: {report_file}")

    def _generate_aggregated_figures(self, metrics: Dict, output_dir: Path):
        """生成聚合图表"""
        # 创建图表输出目录
        figures_dir = output_dir / "aggregated_figures"
        figures_dir.mkdir(exist_ok=True)

        # 设置matplotlib参数
        plt.rcParams['font.size'] = 10
        plt.rcParams['font.family'] = ['sans-serif']  # 使用sans-serif，通常有更好的字体支持
        plt.rcParams['axes.unicode_minus'] = False  # 正确显示负号
        plt.rcParams['axes.labelsize'] = 12
        plt.rcParams['axes.labelweight'] = 'bold'
        plt.rcParams['axes.titlesize'] = 14
        plt.rcParams['axes.titleweight'] = 'bold'
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

        # 创建综合仪表板 (3x3 子图)
        fig = plt.figure(figsize=(16, 12))
        gs = fig.add_gridspec(3, 3, hspace=0.35, wspace=0.35)

        fig.suptitle(f'Aggregated Performance Metrics - {self.args.model}\n({metrics["metadata"]["total_tests"]} tests)',
                    fontsize=16, fontweight='bold')

        # 1. Satisfaction Probability (左上)
        if 'satisfaction_probability' in metrics:
            ax = fig.add_subplot(gs[0, 0])
            sat = metrics['satisfaction_probability']

            data = [sat['mean'] * 100]
            yerr = [[sat['mean'] - sat['min']], [sat['max'] - sat['mean']]]

            ax.bar(['SR-MPC'], data, yerr=yerr, capsize=5,
                  color=COLORS['blue'], alpha=0.7, error_kw={'linewidth': 2})
            ax.text(0, sat['mean'] * 100, f'{sat["mean"]*100:.1f}%',
                   ha='center', va='bottom', fontsize=12, fontweight='bold')
            ax.set_ylabel('Satisfaction Rate (%)', fontweight='bold')
            ax.set_title('(a) STL Satisfaction', fontweight='bold')
            ax.set_ylim([0, 105])
            ax.grid(axis='y', alpha=0.3)

        # 2. Empirical Risk (中上)
        if 'empirical_risk' in metrics:
            ax = fig.add_subplot(gs[0, 1])
            risk = metrics['empirical_risk']

            observed = risk['aggregated_risk']
            target = 0.05
            color = COLORS['blue'] if abs(observed - target) < 0.02 else COLORS['red']

            ax.bar(['SR-MPC'], [observed], color=color, alpha=0.7)
            ax.axhline(y=target, color=COLORS['green'], linestyle='--', linewidth=2,
                      label=f'Target $\\delta = {target:.2f}$')
            ax.text(0, observed, f'{observed:.3f}', ha='center', va='bottom' if observed < target else 'top',
                   fontsize=12, fontweight='bold')
            ax.set_ylabel('Violation Rate', fontweight='bold')
            ax.set_title('(b) Empirical Risk', fontweight='bold')
            ax.set_ylim([0, max(observed * 1.5, target * 2)])
            ax.legend(loc='upper right')
            ax.grid(axis='y', alpha=0.3)

        # 3. Feasibility (右上)
        if 'feasibility' in metrics:
            ax = fig.add_subplot(gs[0, 2])
            feas = metrics['feasibility']
            rate = feas['aggregated_rate'] * 100
            target_rate = 99.5

            color = COLORS['blue'] if rate >= target_rate else COLORS['red']
            ax.bar(['SR-MPC'], [rate], color=color, alpha=0.7)
            ax.axhline(y=target_rate, color=COLORS['green'], linestyle='--', linewidth=2,
                      label=f'Target ({target_rate:.1f}%)')
            ax.text(0, rate, f'{rate:.1f}%', ha='center', va='bottom',
                   fontsize=12, fontweight='bold')
            ax.set_ylabel('Feasibility (%)', fontweight='bold')
            ax.set_title('(c) Recursive Feasibility', fontweight='bold')
            ax.set_ylim([rate * 0.95, 100.5])
            ax.legend(loc='lower right')
            ax.grid(axis='y', alpha=0.3)

        # 4. Computation (左中)
        if 'computation' in metrics:
            ax = fig.add_subplot(gs[1, 0])
            comp = metrics['computation']

            stats_names = ['Median', 'Mean', 'Min', 'Max']
            stats_values = [comp['median_of_medians'], comp['mean_median'],
                           comp['min_median'], comp['max_median']]

            x_pos = np.arange(len(stats_names))
            ax.bar(x_pos, stats_values, color=COLORS['blue'], alpha=0.7)
            ax.axhline(y=8, color=COLORS['green'], linestyle='--', linewidth=2,
                      label='Real-time (8 ms)')

            for i, val in enumerate(stats_values):
                ax.text(i, val, f'{val:.1f}', ha='center', va='bottom',
                       fontsize=9, fontweight='bold')

            ax.set_xticks(x_pos)
            ax.set_xticklabels(stats_names)
            ax.set_ylabel('Time (ms)', fontweight='bold')
            ax.set_title('(d) Computation Time', fontweight='bold')
            ax.legend(loc='upper left')
            ax.grid(axis='y', alpha=0.3)

        # 5. Tracking Error (中中)
        if 'tracking' in metrics:
            ax = fig.add_subplot(gs[1, 1])
            track = metrics['tracking']

            stats_names = ['Mean', 'Max']
            stats_values = [track['mean_error_mean'] * 100, track.get('max_error_mean', track['mean_error_mean']) * 100]
            # 修复：yerr应该是一个1D数组，每个值对应一个bar
            # Mean error有std，Max error没有std（设为0）
            yerr = [track['mean_error_std'] * 100, 0]

            x_pos = np.arange(len(stats_names))
            ax.bar(x_pos, stats_values, yerr=yerr, capsize=5,
                  color=COLORS['blue'], alpha=0.7, error_kw={'linewidth': 2})

            tube_radius_cm = 18
            ax.axhline(y=tube_radius_cm, color=COLORS['red'], linestyle='--',
                      linewidth=2, label=f'Tube radius ({tube_radius_cm} cm)')

            for i, val in enumerate(stats_values):
                ax.text(i, val, f'{val:.2f}', ha='center', va='bottom',
                       fontsize=9, fontweight='bold')

            ax.set_xticks(x_pos)
            ax.set_xticklabels(stats_names)
            ax.set_ylabel('Error (cm)', fontweight='bold')
            ax.set_title('(e) Tracking Error', fontweight='bold')
            ax.legend(loc='upper left')
            ax.grid(axis='y', alpha=0.3)

        # 6. Tube Occupancy (中右)
        if 'tracking' in metrics:
            ax = fig.add_subplot(gs[1, 2])
            track = metrics['tracking']
            occupancy = track['occupancy_mean'] * 100

            colors_pie = [COLORS['green'], COLORS['red']]
            sizes = [occupancy, 100 - occupancy]
            labels = [f'In tube\n({occupancy:.1f}%)', f'Out of tube\n({100-occupancy:.1f}%)']

            ax.pie(sizes, labels=labels, colors=colors_pie, autopct='',
                  startangle=90, textprops={'fontsize': 10, 'fontweight': 'bold'})
            ax.set_title(f'(f) Tube Occupancy\n(Mean ± {track["occupancy_std"]*100:.1f}%)',
                        fontweight='bold')

        # 7. Regret (左下)
        if 'regret' in metrics:
            ax = fig.add_subplot(gs[2, 0])
            regret = metrics['regret']

            stats_names = ['Mean', 'Median']
            stats_values = [regret['mean'], regret['median']]

            x_pos = np.arange(len(stats_names))
            ax.bar(x_pos, stats_values, color=COLORS['blue'], alpha=0.7)

            for i, val in enumerate(stats_values):
                ax.text(i, val, f'{val:.4f}', ha='center', va='bottom',
                       fontsize=9, fontweight='bold')

            ax.set_xticks(x_pos)
            ax.set_xticklabels(stats_names)
            ax.set_ylabel('Normalized Regret ($R_T/T$)', fontweight='bold')
            ax.set_title('(g) Dynamic Regret', fontweight='bold')
            ax.grid(axis='y', alpha=0.3)

        # 8. Calibration (中下)
        if 'calibration' in metrics:
            ax = fig.add_subplot(gs[2, 1])
            calib = metrics['calibration']

            mean_error = calib['mean_error']
            target_delta = 0.05
            color = COLORS['blue'] if mean_error < 0.01 else COLORS['red']

            ax.scatter(0, mean_error, s=200, c=color, alpha=0.7,
                      edgecolors='black', linewidths=2)
            ax.axhline(y=0, color=COLORS['green'], linestyle='--', linewidth=2,
                      label='Perfect calibration')
            ax.fill_between([-0.5, 0.5], -0.01, 0.01,
                          color=COLORS['green'], alpha=0.1, label='±1% tolerance')
            ax.text(0, mean_error, f'{mean_error:.3f}', ha='center',
                   va='bottom' if mean_error < 0 else 'top',
                   fontsize=10, fontweight='bold',
                   bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
            ax.set_xlim([-0.5, 0.5])
            ax.set_xticks([0])
            ax.set_xticklabels(['SR-MPC'])
            ax.set_ylabel('Calibration Error', fontweight='bold')
            ax.set_title('(h) Calibration Accuracy', fontweight='bold')
            ax.set_ylim([0, max(mean_error * 1.5, 0.02)])
            ax.legend(loc='upper right')
            ax.grid(axis='y', alpha=0.3)

        # 9. Summary Table (右下)
        ax = fig.add_subplot(gs[2, 2])
        ax.axis('off')

        # 创建摘要表格
        summary_data = [['Metric', 'Mean Value', 'Target']]

        if 'satisfaction_probability' in metrics:
            sat = metrics['satisfaction_probability']
            summary_data.append(['Satisfaction', f'{sat["mean"]*100:.1f}%', 'N/A'])

        if 'empirical_risk' in metrics:
            risk = metrics['empirical_risk']
            summary_data.append(['Risk', f'{risk["aggregated_risk"]:.3f}', '0.05'])

        if 'feasibility' in metrics:
            feas = metrics['feasibility']
            summary_data.append(['Feasibility', f'{feas["aggregated_rate"]*100:.1f}%', '99.5%'])

        if 'computation' in metrics:
            comp = metrics['computation']
            summary_data.append(['Solve Time', f'{comp["median_of_medians"]:.1f} ms', '<8 ms'])

        if 'tracking' in metrics:
            track = metrics['tracking']
            summary_data.append(['Tracking Error', f'{track["mean_error_mean"]*100:.2f} cm', '<18 cm'])

        if 'calibration' in metrics:
            calib = metrics['calibration']
            summary_data.append(['Calibration Error', f'{calib["mean_error"]:.3f}', '<0.01'])

        table = ax.table(cellText=summary_data, cellLoc='center',
                        loc='center', colWidths=[0.4, 0.3, 0.3])
        table.auto_set_font_size(False)
        table.set_fontsize(9)
        table.scale(1, 2)

        # 设置表头样式
        for i in range(3):
            table[(0, i)].set_facecolor('#4472C4')
            table[(0, i)].set_text_props(weight='bold', color='white')

        # 交替行颜色
        for i in range(1, len(summary_data)):
            for j in range(3):
                if i % 2 == 0:
                    table[(i, j)].set_facecolor('#E7E6E6')

        ax.set_title('(i) Performance Summary', fontweight='bold', pad=20)

        # 保存图表
        plt.savefig(figures_dir / "aggregated_performance_dashboard.png", dpi=300)
        plt.savefig(figures_dir / "aggregated_performance_dashboard.pdf")
        plt.close()

        TestLogger.success(f"  ✓ 聚合性能仪表板已生成")
        TestLogger.success(f"    图表目录: {figures_dir}")

    def run_tests(self):
        """运行所有测试"""
        TestLogger.test("=" * 70)
        TestLogger.test("  自动化Baseline测试系统启动")
        TestLogger.test("  Automated Baseline Testing System")
        TestLogger.test("=" * 70)

        TestLogger.info("测试配置:")
        TestLogger.info(f"  模型: {self.args.model} - {self.model_config['description']}")
        TestLogger.info(f"  货架数量: {self.args.num_shelves}")
        TestLogger.info(f"  超时时间: {self.args.timeout}秒")
        TestLogger.info(f"  Gazebo: {self.args.use_gazebo}")
        TestLogger.info(f"  可视化: {self.args.visualization}")

        # 清理之前的ROS进程
        self.cleanup_ros_processes()

        # 执行测试循环
        successful_tests = 0

        for i in range(self.args.num_shelves):
            shelf = self.shelf_config.get_shelf(i)
            if not shelf:
                TestLogger.error(f"无效的货架索引: {i}")
                continue

            if self.run_single_test(i + 1, shelf):
                successful_tests += 1

        # 生成最终报告
        self.generate_final_report(successful_tests)

        TestLogger.test("=" * 70)
        TestLogger.test("  所有测试完成")
        TestLogger.test(f"  成功率: {successful_tests}/{self.args.num_shelves}")
        TestLogger.test("=" * 70)

        return successful_tests == self.args.num_shelves

def main():
    parser = argparse.ArgumentParser(
        description='自动化Baseline测试系统 - Python版本',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 测试所有货架，使用tube_mpc模型（默认启用Gazebo和RViz）
  python3 run_automated_test.py --model tube_mpc

  # 测试前3个货架，使用STL增强模型
  python3 run_automated_test.py --model stl --shelves 3

  # 测试纯MPC基准（论文对照，使用mpc_ros包）
  python3 run_automated_test.py --model mpc --shelves 5

  # 🔥 测试所有模型，每个模型测试10个货架（完整测试，包含纯MPC基准）
  python3 run_automated_test.py --model all --shelves 10

  # 快速测试所有模型（每个模型只测试1个货架）
  python3 run_automated_test.py --model all --shelves 1 --no-viz

  # 快速测试（无Gazebo和RViz）
  python3 run_automated_test.py --model tube_mpc --headless --shelves 2

  # 仅禁用RViz，保留Gazebo
  python3 run_automated_test.py --model tube_mpc --no-viz
        """
    )

    parser.add_argument('--model',
                       choices=['mpc', 'tube_mpc', 'stl', 'dr', 'safe_regret', 'all'],
                       default='tube_mpc',
                       help='模型类型 (默认: tube_mpc, "all"测试所有模型)')

    parser.add_argument('--shelves', dest='num_shelves',
                       type=int, default=10,
                       help='测试货架数量 (默认: 10, 范围: 1-10)')

    parser.add_argument('--timeout',
                       type=int, default=240,
                       help='单次测试超时时间，秒 (默认: 240)')

    parser.add_argument('--no-gazebo',
                       action='store_true',
                       help='禁用Gazebo可视化（默认启用）')

    parser.add_argument('--no-viz',
                       action='store_true',
                       help='禁用RViz可视化（默认启用）')

    parser.add_argument('--headless',
                       action='store_true',
                       help='无头模式（不显示Gazebo和RViz窗口）')

    parser.add_argument('--interval',
                       type=int, default=5,
                       help='测试间隔时间，秒 (默认: 5)')

    parser.add_argument('--workspace',
                       default='/home/dixon/Final_Project/catkin',
                       help='Catkin工作空间路径')

    parser.add_argument('--shelf-config',
                       default='/home/dixon/Final_Project/catkin/test/scripts/shelf_locations.yaml',
                       help='货架配置文件路径')

    parser.add_argument('--results-dir',
                       default='/home/dixon/Final_Project/catkin/test_results',
                       help='测试结果保存目录')

    args = parser.parse_args()

    # 验证货架数量
    if args.num_shelves < 1 or args.num_shelves > 10:
        print("错误: 货架数量必须在1-10之间")
        sys.exit(1)

    # 设置参数
    # 默认启用Gazebo和RViz，除非明确禁用
    args.use_gazebo = not args.no_gazebo
    args.visualization = not args.no_viz

    # 无头模式同时禁用Gazebo和RViz
    if args.headless:
        args.use_gazebo = False
        args.visualization = False

    TestLogger.info(f"可视化配置: Gazebo={args.use_gazebo}, RViz={args.visualization}")

    # ✅ 处理 "all" 模式：循环测试所有模型
    if args.model == 'all':
        all_models = ['mpc', 'tube_mpc', 'stl', 'dr', 'safe_regret']
        TestLogger.info("="*70)
        TestLogger.info("🚀 ALL MODE: 测试所有模型")
        TestLogger.info("="*70)
        TestLogger.info(f"模型列表: {', '.join(all_models)}")
        TestLogger.info(f"每个模型测试货架数: {args.num_shelves}")
        TestLogger.info(f"总测试数: {len(all_models)} × {args.num_shelves} = {len(all_models) * args.num_shelves}")
        TestLogger.info("="*70)

        # 记录所有模型的测试结果
        all_results = {}
        all_success = True

        for model in all_models:
            TestLogger.info("")
            TestLogger.info("="*70)
            TestLogger.info(f"📊 开始测试模型: {model} ({all_models.index(model)+1}/{len(all_models)})")
            TestLogger.info("="*70)

            # 为每个模型创建独立的参数副本
            import copy
            model_args = copy.deepcopy(args)
            model_args.model = model

            # 创建测试运行器
            runner = AutomatedTestRunner(model_args)

            # 运行测试
            try:
                success = runner.run_tests()
                all_results[model] = 'SUCCESS' if success else 'FAILED'
                if not success:
                    all_success = False
            except KeyboardInterrupt:
                TestLogger.warning(f"模型 {model} 测试被用户中断")
                all_results[model] = 'INTERRUPTED'
                all_success = False
                break
            except Exception as e:
                TestLogger.error(f"模型 {model} 测试过程出错: {e}")
                all_results[model] = 'ERROR'
                all_success = False
                import traceback
                traceback.print_exc()

            # 清理ROS进程
            runner.cleanup_ros_processes()

            # 等待一段时间再开始下一个模型测试
            if model != all_models[-1]:  # 不是最后一个模型
                TestLogger.info(f"⏳ 等待 {args.interval} 秒后开始下一个模型测试...")
                time.sleep(args.interval)

        # 打印所有模型的测试结果汇总
        TestLogger.info("")
        TestLogger.info("="*70)
        TestLogger.info("📈 ALL MODE 测试结果汇总")
        TestLogger.info("="*70)
        for model, result in all_results.items():
            status_icon = "✅" if result == "SUCCESS" else "❌"
            TestLogger.info(f"  {status_icon} {model}: {result}")
        TestLogger.info("="*70)

        final_result = "✅ 全部成功" if all_success else "❌ 部分失败"
        TestLogger.info(f"总体结果: {final_result}")
        TestLogger.info("="*70)

        sys.exit(0 if all_success else 1)

    # 单个模型模式
    # 创建测试运行器
    runner = AutomatedTestRunner(args)

    # 运行测试
    try:
        success = runner.run_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        TestLogger.warning("测试被用户中断")
        runner.cleanup_ros_processes()
        sys.exit(1)
    except Exception as e:
        TestLogger.error(f"测试过程出错: {e}")
        import traceback
        traceback.print_exc()
        runner.cleanup_ros_processes()
        sys.exit(1)

if __name__ == '__main__':
    main()
