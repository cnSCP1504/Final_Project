#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Odometry
import sys
import math

class AutoNavGoal:
    def __init__(self):
        rospy.init_node('auto_nav_goal', anonymous=True)

        # 创建发布者
        self.goal_pub = rospy.Publisher('/move_base_simple/goal', PoseStamped, queue_size=10)

        # 当前目标位置
        self.current_goal = None
        # 从参数服务器读取到达阈值，默认为0.5米（严格模式）
        self.goal_radius = rospy.get_param('~goal_radius', 0.5)
        rospy.loginfo(f"目标到达阈值设置为: {self.goal_radius} 米（严格模式）")

        # 订阅机器人位置以检测目标到达
        self.goal_reached = False
        self.odom_sub = rospy.Subscriber('/odom', Odometry, self.odom_callback)

        # 等待连接建立
        rospy.sleep(1.0)

    def odom_callback(self, msg):
        """监听机器人位置，检测是否到达目标"""
        if self.current_goal is not None and not self.goal_reached:
            # 获取机器人当前位置
            robot_x = msg.pose.pose.position.x
            robot_y = msg.pose.pose.position.y

            # 计算到目标的距离
            dx = self.current_goal['x'] - robot_x
            dy = self.current_goal['y'] - robot_y
            distance = math.sqrt(dx*dx + dy*dy)

            # 检查是否到达目标
            if distance < self.goal_radius:
                rospy.loginfo(f"✓ 目标已到达！距离: {distance:.2f} m < {self.goal_radius} m")
                self.goal_reached = True

    def send_goal(self, goal_x, goal_y, goal_yaw=0.0, description=""):
        """
        发送单个导航目标
        :param goal_x: 目标x坐标
        :param goal_y: 目标y坐标
        :param goal_yaw: 目标朝向角度(弧度)
        :param description: 目标描述
        """
        # 等待发布者准备就绪
        rospy.loginfo("等待move_base准备就绪...")
        while self.goal_pub.get_num_connections() == 0:
            if rospy.is_shutdown():
                rospy.logerr("ROS shutdown before goal could be sent!")
                sys.exit(1)
            rospy.sleep(0.5)

        rospy.loginfo("move_base已准备就绪，发送导航目标...")

        # 创建目标消息
        goal = PoseStamped()
        goal.header.stamp = rospy.Time.now()
        goal.header.frame_id = "map"

        # 设置目标位置
        goal.pose.position.x = goal_x
        goal.pose.position.y = goal_y
        goal.pose.position.z = 0.0

        # 设置目标朝向（四元数）
        goal.pose.orientation.x = 0.0
        goal.pose.orientation.y = 0.0
        goal.pose.orientation.z = math.sin(goal_yaw / 2.0)
        goal.pose.orientation.w = math.cos(goal_yaw / 2.0)

        # 保存当前目标位置（用于距离检测）
        self.current_goal = {'x': goal_x, 'y': goal_y}
        self.goal_reached = False

        # 发布目标
        self.goal_pub.publish(goal)

        rospy.loginfo(f"导航目标已发送: {description}")
        rospy.loginfo(f"  位置: x={goal_x}, y={goal_y}, z=0.0")
        rospy.loginfo(f"  朝向: yaw={goal_yaw} 弧度 ({math.degrees(goal_yaw):.2f} 度)")

    def wait_for_goal_reached(self, timeout=120.0):
        """
        等待目标到达
        :param timeout: 超时时间（秒）
        :return: 是否成功到达
        """
        rospy.loginfo("等待机器人到达目标...")
        self.goal_reached = False

        start_time = rospy.Time.now()
        rate = rospy.Rate(10)  # 10Hz

        # 获取初始机器人位置（用于计算移动距离）
        initial_odom = rospy.wait_for_message('/odom', Odometry, timeout=5.0)
        if initial_odom:
            initial_x = initial_odom.pose.pose.position.x
            initial_y = initial_odom.pose.pose.position.y

        while not self.goal_reached and not rospy.is_shutdown():
            if (rospy.Time.now() - start_time).to_sec() > timeout:
                rospy.logwarn(f"等待目标到达超时 ({timeout}秒)")
                return False

            # 每10秒打印一次状态
            elapsed = int((rospy.Time.now() - start_time).to_sec())
            if elapsed % 10 == 0 and elapsed > 0:
                # 获取当前位置并计算移动距离
                try:
                    current_odom = rospy.wait_for_message('/odom', Odometry, timeout=1.0)
                    if current_odom and self.current_goal:
                        robot_x = current_odom.pose.pose.position.x
                        robot_y = current_odom.pose.pose.position.y
                        dx = self.current_goal['x'] - robot_x
                        dy = self.current_goal['y'] - robot_y
                        distance = math.sqrt(dx*dx + dy*dy)

                        moved = math.sqrt((robot_x - initial_x)**2 + (robot_y - initial_y)**2)
                        rospy.loginfo(f"正在导航... 已等待 {elapsed} 秒, 距离目标: {distance:.2f}m, 已移动: {moved:.2f}m")
                except:
                    rospy.loginfo(f"正在导航... 已等待 {elapsed} 秒")

            rate.sleep()

        if self.goal_reached:
            rospy.loginfo("✓ 目标已到达!")
            return True
        else:
            return False

    def send_sequential_goals(self, goals):
        """
        发送一系列目标，按顺序执行
        :param goals: 目标列表，格式: [(x1, y1, yaw1, desc1), (x2, y2, yaw2, desc2), ...]
        """
        rospy.loginfo(f"开始自动导航任务，共 {len(goals)} 个目标")
        rospy.loginfo("=" * 60)

        for i, (x, y, yaw, desc) in enumerate(goals, 1):
            rospy.loginfo(f"\n[目标 {i}/{len(goals)}] {desc}")
            rospy.loginfo("-" * 60)

            self.send_goal(x, y, yaw, desc)
            success = self.wait_for_goal_reached(timeout=120.0)

            if not success:
                rospy.logwarn(f"目标 {i} 未能在超时时间内到达")
                if i < len(goals):
                    response = input("是否继续发送下一个目标？(y/n): ")
                    if response.lower() != 'y':
                        rospy.loginfo("导航任务中止")
                        break
            else:
                rospy.loginfo(f"目标 {i} 完成！")
                # 等待2秒再发送下一个目标
                if i < len(goals):
                    rospy.loginfo("等待2秒后发送下一个目标...")
                    rospy.sleep(2.0)

        rospy.loginfo("=" * 60)
        rospy.loginfo("所有导航任务完成!")

if __name__ == '__main__':
    try:
        nav = AutoNavGoal()

        # 定义测试目标序列
        # 目标1: (3.0, -7.0) - 第一个目标
        # 目标2: (8.0, 0.0) - 第二个目标，用于测试大角度转向
        goals = [
            (0.0, -7.0, 0.0, "第一个目标 - 位置(3.0, -7.0)"),
            (8.0, 0.0, 0.0, "第二个目标 - 位置(8.0, 0.0) - 测试大角度转向")
        ]

        # 从ROS参数服务器读取自定义目标（可选）
        if rospy.has_param('~custom_goals'):
            custom_goals = rospy.get_param('~custom_goals')
            if isinstance(custom_goals, list):
                goals = []
                for g in custom_goals:
                    goals.append((g['x'], g['y'], g.get('yaw', 0.0), g.get('desc', '')))

        # 执行顺序导航任务
        nav.send_sequential_goals(goals)

    except rospy.ROSInterruptException:
        rospy.logerr("程序被中断!")
    except Exception as e:
        rospy.logerr(f"发生错误: {e}")
