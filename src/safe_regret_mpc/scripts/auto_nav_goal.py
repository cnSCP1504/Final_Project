#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
自动导航目标发布器 - 物流任务模式

任务流程：
1. 取货点（Pickup Station S1）：可变坐标，由参数配置
2. 卸货点（Delivery Station S2）：固定坐标 (8.0, 0.0)

使用方法：
1. 默认模式：取货点为 (0.0, -7.0)，卸货点为 (8.0, 0.0)
2. 自定义取货点：通过ROS参数设置
   roslaunch safe_regret_mpc safe_regret_mpc_test.launch pickup_x:=3.0 pickup_y:=-7.0
"""

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

        # 读取任务参数
        self.pickup_x = rospy.get_param('~pickup_x', 0.0)
        self.pickup_y = rospy.get_param('~pickup_y', -7.0)
        self.pickup_yaw = rospy.get_param('~pickup_yaw', 0.0)

        self.delivery_x = rospy.get_param('~delivery_x', 8.0)
        self.delivery_y = rospy.get_param('~delivery_y', 0.0)
        self.delivery_yaw = rospy.get_param('~delivery_yaw', 0.0)

        rospy.loginfo("=" * 70)
        rospy.loginfo("物流任务参数配置")
        rospy.loginfo("=" * 70)
        rospy.loginfo(f"📦 取货点 S1 (Pickup Station):")
        rospy.loginfo(f"   坐标: ({self.pickup_x:.2f}, {self.pickup_y:.2f})")
        rospy.loginfo(f"   朝向: {math.degrees(self.pickup_yaw):.2f}°")
        rospy.loginfo(f"")
        rospy.loginfo(f"🏁 卸货点 S2 (Delivery Station - 固定):")
        rospy.loginfo(f"   坐标: ({self.delivery_x:.2f}, {self.delivery_y:.2f})")
        rospy.loginfo(f"   朝向: {math.degrees(self.delivery_yaw):.2f}°")
        rospy.loginfo("=" * 70)

        # 发布参数供其他节点使用
        rospy.set_param('/auto_nav_goal/pickup_x', self.pickup_x)
        rospy.set_param('/auto_nav_goal/pickup_y', self.pickup_y)
        rospy.set_param('/auto_nav_goal/delivery_x', self.delivery_x)
        rospy.set_param('/auto_nav_goal/delivery_y', self.delivery_y)
        rospy.set_param('/auto_nav_goal/goal_x', self.delivery_x)  # 兼容旧版本
        rospy.set_param('/auto_nav_goal/goal_y', self.delivery_y)

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

        rospy.loginfo(f"🎯 导航目标已发送: {description}")
        rospy.loginfo(f"   位置: x={goal_x:.2f}, y={goal_y:.2f}, z=0.0")
        rospy.loginfo(f"   朝向: yaw={goal_yaw:.2f} 弧度 ({math.degrees(goal_yaw):.2f} 度)")

    def wait_for_goal_reached(self, timeout=120.0, task_name="目标"):
        """
        等待目标到达
        :param timeout: 超时时间（秒）
        :param task_name: 任务名称（取货/卸货）
        :return: 是否成功到达
        """
        rospy.loginfo(f"⏳ 等待机器人到达{task_name}...")
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
                rospy.logwarn(f"⚠️  等待{task_name}超时 ({timeout}秒)")
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
                        rospy.loginfo(f"🚶 正在导航... 已等待 {elapsed} 秒, 距离{task_name}: {distance:.2f}m, 已移动: {moved:.2f}m")
                except:
                    rospy.loginfo(f"🚶 正在导航... 已等待 {elapsed} 秒")

            rate.sleep()

        if self.goal_reached:
            rospy.loginfo(f"✅ {task_name}已到达!")
            return True
        else:
            return False

    def execute_logistics_task(self):
        """
        执行物流任务：取货点 → 卸货点
        """
        rospy.loginfo("\n" + "=" * 70)
        rospy.loginfo("🚚 开始物流任务：取货 → 卸货")
        rospy.loginfo("=" * 70)

        # 任务1: 前往取货点 S1
        rospy.loginfo(f"\n📦 [任务 1/2] 前往取货点 S1 (Pickup Station)")
        rospy.loginfo("-" * 70)
        self.send_goal(self.pickup_x, self.pickup_y, self.pickup_yaw,
                      f"取货点 S1 - 位置({self.pickup_x:.2f}, {self.pickup_y:.2f})")

        success1 = self.wait_for_goal_reached(timeout=120.0, task_name="取货点 S1")

        if not success1:
            rospy.logwarn("⚠️  未能到达取货点 S1，物流任务中止")
            return False

        rospy.loginfo("📦 取货完成！等待2秒后前往卸货点...")
        rospy.sleep(2.0)

        # 任务2: 前往卸货点 S2
        rospy.loginfo(f"\n🏁 [任务 2/2] 前往卸货点 S2 (Delivery Station)")
        rospy.loginfo("-" * 70)
        self.send_goal(self.delivery_x, self.delivery_y, self.delivery_yaw,
                      f"卸货点 S2 - 位置({self.delivery_x:.2f}, {self.delivery_y:.2f})")

        success2 = self.wait_for_goal_reached(timeout=120.0, task_name="卸货点 S2")

        if not success2:
            rospy.logwarn("⚠️  未能到达卸货点 S2，物流任务未完成")
            return False

        rospy.loginfo("\n" + "=" * 70)
        rospy.loginfo("✅ 物流任务完成！取货 → 卸货 全部成功")
        rospy.loginfo("=" * 70)
        return True

if __name__ == '__main__':
    try:
        nav = AutoNavGoal()

        # 执行物流任务
        nav.execute_logistics_task()

    except rospy.ROSInterruptException:
        rospy.logerr("❌ 程序被中断!")
    except Exception as e:
        rospy.logerr(f"❌ 发生错误: {e}")
        import traceback
        traceback.print_exc()
