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
import math

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
        'tube_mpc': {
            'name': 'Tube MPC',
            'params': '',  # 使用launch文件默认值
            'description': '仅使用Tube MPC（基线模型）'
        },
        'stl': {
            'name': 'Tube MPC + STL',
            'params': 'enable_stl:=true enable_dr:=false',
            'description': 'Tube MPC + STL监控'
        },
        'dr': {
            'name': 'Tube MPC + DR',
            'params': 'enable_stl:=false enable_dr:=true',
            'description': 'Tube MPC + DR约束收紧'
        },
        'safe_regret': {
            'name': 'Safe-Regret MPC',
            'params': 'enable_stl:=true enable_dr:=true',
            'description': '完整Safe-Regret MPC (STL+DR)'
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
    """目标到达监控器"""

    def __init__(self, goals, goal_radius=0.5, timeout=240):
        self.goals = goals
        self.goal_radius = goal_radius
        self.timeout = timeout

        self.current_goal_index = 0
        self.goals_reached = []
        self.robot_position = None
        self.start_time = None
        self.monitoring = False

        # Metrics
        self.metrics = {
            'test_start_time': None,
            'test_end_time': None,
            'test_status': 'RUNNING',
            'total_goals': len(goals),
            'goals_reached': [],
            'position_history': []
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

        # 检查目标到达
        if self.monitoring and self.current_goal_index < len(self.goals):
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
        self.metrics['test_status'] = 'SUCCESS'
        self.metrics['test_end_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        self.metrics['total_time'] = time.time() - self.start_time

        print("=" * 70)
        print("🎉 所有目标已完成！测试成功！")
        print(f"   总用时: {self.metrics['total_time']:.1f} 秒")
        print("=" * 70)

        self.monitoring = False

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

            self.monitoring = False
            return True

        return False

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
        print("=" * 70)

        rospy.init_node('goal_monitor', anonymous=True)
        rospy.Subscriber('/odom', Odometry, self.odom_callback)

        self.monitoring = True
        self.start_time = time.time()
        self.metrics['test_start_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        rate = rospy.Rate(10)
        while not rospy.is_shutdown() and self.monitoring:
            self.check_timeout()
            rate.sleep()

        return self.metrics['test_status'] == 'SUCCESS'

class AutomatedTestRunner:
    """自动化测试运行器"""

    def __init__(self, args):
        self.args = args
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

        # 构建launch命令 - 所有模型都使用safe_regret_mpc launch文件
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

        monitor = GoalMonitor(
            goals=goals,
            goal_radius=0.5,
            timeout=self.args.timeout
        )

        # 保存metrics
        metrics_file = test_dir / "metrics.json"

        try:
            success = monitor.start_monitoring()

            # 保存metrics
            with open(metrics_file, 'w') as f:
                json.dump(monitor.metrics, f, indent=2)

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
            if self.monitor_goals(shelf, test_dir):
                TestLogger.success("测试成功")
            else:
                TestLogger.warning("测试超时")
                test_result = "TIMEOUT"
        except Exception as e:
            TestLogger.error(f"测试过程出错: {e}")
            test_result = "ERROR"

        # 清理ROS进程
        self.cleanup_ros_processes()

        # 生成测试摘要
        self.generate_test_summary(shelf, test_dir, test_result)

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

  # 快速测试（无Gazebo和RViz）
  python3 run_automated_test.py --model tube_mpc --headless --shelves 2

  # 仅禁用RViz，保留Gazebo
  python3 run_automated_test.py --model tube_mpc --no-viz
        """
    )

    parser.add_argument('--model',
                       choices=['tube_mpc', 'stl', 'dr', 'safe_regret'],
                       default='tube_mpc',
                       help='模型类型 (默认: tube_mpc)')

    parser.add_argument('--shelves', dest='num_shelves',
                       type=int, default=5,
                       help='测试货架数量 (默认: 5, 范围: 1-5)')

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
    if args.num_shelves < 1 or args.num_shelves > 5:
        print("错误: 货架数量必须在1-5之间")
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
