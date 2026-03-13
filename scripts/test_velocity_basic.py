#!/usr/bin/env python3
"""
最基础的速度测试节点 - 绕过所有MPC逻辑
直接向/cmd_vel发布恒定速度命令
"""

import rospy
from geometry_msgs.msg import Twist
import sys

def velocity_publisher():
    try:
        rospy.init_node('test_velocity_publisher')

        # 检查/cmd_vel话题
        print("正在检查/cmd_vel话题...")
        topic_list = rospy.get_published_topics()
        cmd_vel_found = any('/cmd_vel' in str(topic) for topic in topic_list)

        if not cmd_vel_found:
            print("❌ 错误: /cmd_vel话题不存在!")
            print("当前话题列表:")
            for topic in topic_list:
                if 'cmd' in topic or 'vel' in topic:
                    print(f"  {topic}")
            return

        print("✅ /cmd_vel话题存在")

        # 创建发布者
        pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10)

        # 等待节点启动
        rospy.sleep(2.0)

        print("✅ 节点已启动")
        print("正在发布测试速度命令...")
        print("  linear.x = 0.3 m/s")
        print("  angular.z = 0.0 rad/s")
        print("按 Ctrl+C 停止")
        print("-" * 50)

        rate = rospy.Rate(10)  # 10Hz
        twist = Twist()

        # 设置恒定速度
        twist.linear.x = 0.3
        twist.linear.y = 0.0
        twist.linear.z = 0.0
        twist.angular.x = 0.0
        twist.angular.y = 0.0
        twist.angular.z = 0.0

        msg_count = 0

        start_time = rospy.Time.now()

        while not rospy.is_shutdown():
            pub.publish(twist)
            msg_count += 1

            # 每秒报告一次
            if msg_count % 10 == 0:
                elapsed = (rospy.Time.now() - start_time).to_sec()
                print(f"[{elapsed:.1f}s] 已发布 {msg_count} 条消息, "
                      f"速度: linear.x={twist.linear.x:.2f} m/s")

            rate.sleep()

    except rospy.ROSInterruptException:
        print("\n✅ 正常停止")
    except Exception as e:
        print(f"❌ 错误: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    velocity_publisher()
