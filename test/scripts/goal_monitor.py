#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
目标到达监控器 - Goal Reach Monitor
用于自动化测试中检测机器人是否到达目标点

功能：
- 监听/odom话题检测机器人位置
- 检测目标到达事件（距离阈值判定）
- 记录到达时间和位置
- 支持多个目标点的序列监控
- 生成详细的metrics数据

使用方法：
    python3 goal_monitor.py --goals x1,y1 x2,y2 --radius 0.5 --output metrics.json
"""

import rospy
import sys
import json
import time
from datetime import datetime
from nav_msgs.msg import Odometry
import argparse
import math

class GoalMonitor:
    def __init__(self, goals, goal_radius=0.5, output_file=None, timeout=240):
        """
        初始化目标监控器

        Args:
            goals: 目标点列表 [(x1, y1, yaw1), (x2, y2, yaw2), ...]
            goal_radius: 到达判定半径（米）
            output_file: 输出文件路径
            timeout: 超时时间（秒）
        """
        self.goals = goals
        self.goal_radius = goal_radius
        self.output_file = output_file
        self.timeout = timeout

        self.current_goal_index = 0
        self.goals_reached = []
        self.robot_position = None
        self.start_time = None
        self.monitoring = False

        # Metrics数据
        self.metrics = {
            'test_start_time': None,
            'test_end_time': None,
            'test_status': 'RUNNING',
            'total_goals': len(goals),
            'goals_reached': [],
            'position_history': [],
            'timeout': timeout
        }

    def odom_callback(self, msg):
        """里程计回调函数"""
        # 更新机器人位置
        x = msg.pose.pose.position.x
        y = msg.pose.pose.position.y

        # 提取朝向（yaw）
        quaternion = (
            msg.pose.pose.orientation.x,
            msg.pose.pose.orientation.y,
            msg.pose.pose.orientation.z,
            msg.pose.pose.orientation.w
        )
        euler = self.euler_from_quaternion(quaternion)
        yaw = euler[2]

        self.robot_position = (x, y, yaw)

        # 记录位置历史（每秒记录一次）
        if self.monitoring and self.start_time:
            elapsed = time.time() - self.start_time
            if int(elapsed) > len(self.metrics['position_history']):
                self.metrics['position_history'].append({
                    'time': elapsed,
                    'x': x,
                    'y': y,
                    'yaw': yaw
                })

        # 检查是否到达当前目标
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
        """检查是否到达当前目标"""
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

        rospy.loginfo("=" * 70)
        rospy.loginfo(f"✅ 目标 {self.current_goal_index + 1}/{len(self.goals)} 已到达!")
        rospy.loginfo(f"   目标位置: ({goal[0]:.2f}, {goal[1]:.2f})")
        rospy.loginfo(f"   机器人位置: ({self.robot_position[0]:.2f}, {self.robot_position[1]:.2f})")
        rospy.loginfo(f"   距离: {distance:.3f} m")
        rospy.loginfo(f"   用时: {elapsed:.1f} 秒")
        rospy.loginfo("=" * 70)

        # 移动到下一个目标
        self.current_goal_index += 1

        # 检查是否所有目标都已到达
        if self.current_goal_index >= len(self.goals):
            self.on_all_goals_reached()

    def on_all_goals_reached(self):
        """所有目标到达处理"""
        self.metrics['test_status'] = 'SUCCESS'
        self.metrics['test_end_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        self.metrics['total_time'] = time.time() - self.start_time

        rospy.loginfo("=" * 70)
        rospy.loginfo("🎉 所有目标已完成！测试成功！")
        rospy.loginfo(f"   总用时: {self.metrics['total_time']:.1f} 秒")
        rospy.loginfo("=" * 70)

        # 保存metrics
        if self.output_file:
            self.save_metrics()

        # 停止监控
        self.monitoring = False

    def check_timeout(self):
        """检查是否超时"""
        if not self.monitoring or not self.start_time:
            return False

        elapsed = time.time() - self.start_time
        if elapsed > self.timeout:
            self.metrics['test_status'] = 'TIMEOUT'
            self.metrics['test_end_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            self.metrics['total_time'] = elapsed

            rospy.logwarn("=" * 70)
            rospy.logwarn(f"⏰ 测试超时 ({self.timeout} 秒)")
            rospy.logwarn(f"   已完成目标: {len(self.goals_reached)}/{len(self.goals)}")
            rospy.logwarn("=" * 70)

            # 保存metrics
            if self.output_file:
                self.save_metrics()

            # 停止监控
            self.monitoring = False
            return True

        return False

    def save_metrics(self):
        """保存metrics到文件"""
        try:
            with open(self.output_file, 'w') as f:
                json.dump(self.metrics, f, indent=2)
            rospy.loginfo(f"✅ Metrics已保存到: {self.output_file}")
        except Exception as e:
            rospy.logerr(f"❌ 保存metrics失败: {e}")

    def start_monitoring(self):
        """开始监控"""
        rospy.loginfo("=" * 70)
        rospy.loginfo("🚀 目标监控器启动")
        rospy.loginfo("=" * 70)
        rospy.loginfo(f"目标点数量: {len(self.goals)}")
        for i, goal in enumerate(self.goals):
            rospy.loginfo(f"  目标 {i+1}: ({goal[0]:.2f}, {goal[1]:.2f}), yaw={goal[2]:.2f}")
        rospy.loginfo(f"到达阈值: {self.goal_radius} m")
        rospy.loginfo(f"超时时间: {self.timeout} 秒")
        rospy.loginfo("=" * 70)

        # 订阅里程计
        rospy.Subscriber('/odom', Odometry, self.odom_callback)

        # 开始监控
        self.monitoring = True
        self.start_time = time.time()
        self.metrics['test_start_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        # 监控循环
        rate = rospy.Rate(10)  # 10Hz
        while not rospy.is_shutdown() and self.monitoring:
            self.check_timeout()
            rate.sleep()

        return self.metrics['test_status'] == 'SUCCESS'

def parse_goals(goal_strings):
    """解析目标点字符串"""
    goals = []
    for goal_str in goal_strings:
        parts = goal_str.split(',')
        if len(parts) >= 2:
            x = float(parts[0])
            y = float(parts[1])
            yaw = float(parts[2]) if len(parts) >= 3 else 0.0
            goals.append((x, y, yaw))
    return goals

def main():
    parser = argparse.ArgumentParser(description='目标到达监控器')
    parser.add_argument('--goals', nargs='+', required=True,
                       help='目标点列表，格式: x1,y1,yaw1 x2,y2,yaw2')
    parser.add_argument('--radius', type=float, default=0.5,
                       help='到达判定半径（米），默认: 0.5')
    parser.add_argument('--timeout', type=int, default=240,
                       help='超时时间（秒），默认: 240')
    parser.add_argument('--output', type=str, default=None,
                       help='输出JSON文件路径')

    args = parser.parse_args()

    # 解析目标点
    goals = parse_goals(args.goals)
    if not goals:
        print("❌ 错误: 未提供有效的目标点")
        sys.exit(1)

    # 初始化ROS节点
    rospy.init_node('goal_monitor', anonymous=True)

    # 创建监控器
    monitor = GoalMonitor(
        goals=goals,
        goal_radius=args.radius,
        output_file=args.output,
        timeout=args.timeout
    )

    # 开始监控
    try:
        success = monitor.start_monitoring()
        sys.exit(0 if success else 1)
    except rospy.ROSInterruptException:
        rospy.loginfo("监控器被中断")
        sys.exit(1)

if __name__ == '__main__':
    main()
