#!/usr/bin/env python3
"""
直接速度测试 - 绕过MPC，直接发布速度到/cmd_vel
测试机器人硬件/Gazebo是否能响应速度命令
"""

import rospy
from geometry_msgs.msg import Twist
import time

def test_direct_velocity():
    rospy.init_node('direct_velocity_test')

    pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10)

    print("等待节点启动...")
    rospy.sleep(2.0)

    print("=" * 50)
    print("直接速度测试")
    print("=" * 50)

    # 测试1: 慢速 (0.1 m/s)
    print("\n测试1: 发布慢速 0.1 m/s (5秒)")
    twist = Twist()
    twist.linear.x = 0.1
    twist.angular.z = 0.0

    start = time.time()
    while time.time() - start < 5.0 and not rospy.is_shutdown():
        pub.publish(twist)
        rospy.sleep(0.1)

    rospy.sleep(1.0)

    # 测试2: 中速 (0.3 m/s)
    print("\n测试2: 发布中速 0.3 m/s (5秒)")
    twist.linear.x = 0.3

    start = time.time()
    while time.time() - start < 5.0 and not rospy.is_shutdown():
        pub.publish(twist)
        rospy.sleep(0.1)

    rospy.sleep(1.0)

    # 测试3: 快速 (0.5 m/s)
    print("\n测试3: 发布快速 0.5 m/s (5秒)")
    twist.linear.x = 0.5

    start = time.time()
    while time.time() - start < 5.0 and not rospy.is_shutdown():
        pub.publish(twist)
        rospy.sleep(0.1)

    rospy.sleep(1.0)

    # 测试4: 停止
    print("\n测试4: 停止")
    twist = Twist()
    twist.linear.x = 0.0
    twist.angular.z = 0.0

    for _ in range(10):
        pub.publish(twist)
        rospy.sleep(0.1)

    print("\n测试完成！")
    print("请观察机器人是否在Gazebo中移动：")
    print("  - 0.1 m/s时应该缓慢移动")
    print("  - 0.3 m/s时应该明显移动")
    print("  - 0.5 m/s时应该快速移动")

    if rospy.is_shutdown():
        return

    # 持续发布0.5 m/s直到用户中断
    print("\n持续发布 0.5 m/s (按Ctrl+C停止)...")
    twist.linear.x = 0.5
    rate = rospy.Rate(10)
    while not rospy.is_shutdown():
        pub.publish(twist)
        rate.sleep()

if __name__ == '__main__':
    try:
        test_direct_velocity()
    except rospy.ROSInterruptException:
        print("\n停止")
