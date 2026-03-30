#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from geometry_msgs.msg import PoseStamped
import sys

def send_navigation_goal(goal_x, goal_y, goal_yaw):
    """
    发送2D导航目标到move_base
    :param goal_x: 目标x坐标
    :param goal_y: 目标y坐标
    :param goal_yaw: 目标朝向角度(弧度)
    """
    rospy.init_node('auto_nav_goal', anonymous=True)

    # 创建发布者，发布到move_base_simple/goal话题
    goal_pub = rospy.Publisher('/move_base_simple/goal', PoseStamped, queue_size=10)

    # 等待发布者准备就绪
    rospy.sleep(1.0)  # 给系统一些时间初始化

    # 等待move_base准备就绪
    rospy.loginfo("等待move_base准备就绪...")
    while goal_pub.get_num_connections() == 0:
        if rospy.is_shutdown():
            rospy.logerr("ROS shutdown before goal could be sent!")
            sys.exit(1)
        rospy.sleep(0.5)

    rospy.loginfo("move_base已准备就绪，发送导航目标...")

    # 创建目标消息
    goal = PoseStamped()

    # 设置header
    goal.header.stamp = rospy.Time.now()
    goal.header.frame_id = "map"

    # 设置目标位置 (x, y, z)
    goal.pose.position.x = goal_x
    goal.pose.position.y = goal_y
    goal.pose.position.z = 0.0

    # 设置目标朝向 (四元数)
    # 将yaw角度转换为四元数
    import math
    goal.pose.orientation.x = 0.0
    goal.pose.orientation.y = 0.0
    goal.pose.orientation.z = math.sin(goal_yaw / 2.0)
    goal.pose.orientation.w = math.cos(goal_yaw / 2.0)

    # 发布目标
    goal_pub.publish(goal)

    rospy.loginfo(f"导航目标已发送:")
    rospy.loginfo(f"  位置: x={goal_x}, y={goal_y}, z=0.0")
    rospy.loginfo(f"  朝向: yaw={goal_yaw} 弧度 ({math.degrees(goal_yaw):.2f} 度)")

    # 等待一段时间确保消息被接收
    rospy.sleep(1.0)

    rospy.loginfo("导航目标发送完成!")

if __name__ == '__main__':
    try:
        # 从ROS参数服务器读取目标坐标，如果没有参数则使用默认值
        goal_x = rospy.get_param('~goal_x', 3.0)
        goal_y = rospy.get_param('~goal_y', -7.0)
        goal_yaw = rospy.get_param('~goal_yaw', 0.0)  # 弧度

        send_navigation_goal(goal_x, goal_y, goal_yaw)

    except rospy.ROSInterruptException:
        rospy.logerr("程序被中断!")
