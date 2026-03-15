#!/usr/bin/env python3
"""
直接发布大速度测试 - 验证Gazebo机器人是否能响应
"""

import rospy
from geometry_msgs.msg import Twist
import sys

def test_large_velocity():
    rospy.init_node('test_large_velocity')

    print("等待2秒...")
    rospy.sleep(2.0)

    pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10)

    print("\n" + "="*60)
    print("测试1: 检查/cmd_vel话题连接")
    print("="*60)
    n = pub.get_num_connections()
    print(f"/cmd_vel 连接数: {n}")
    if n == 0:
        print("❌ 错误: 没有节点订阅/cmd_vel！")
        print("   这意味着Gazebo插件没有正确连接。")
        return False

    print("\n" + "="*60)
    print("测试2: 发布 1.0 m/s 速度（10秒）")
    print("="*60)

    twist = Twist()
    twist.linear.x = 1.0
    twist.angular.z = 0.0

    print("发布命令: linear.x = 1.0 m/s")
    print("请在Gazebo中观察机器人是否移动...")
    print("按Ctrl+C停止\n")

    rate = rospy.Rate(10)  # 10Hz
    start_time = rospy.Time.now()
    count = 0

    while not rospy.is_shutdown():
        elapsed = (rospy.Time.now() - start_time).to_sec()

        if elapsed > 10.0:
            print("\n✓ 测试完成")
            break

        pub.publish(twist)
        count += 1

        if count % 10 == 0:  # 每秒输出一次
            print(f"[{elapsed:.1f}s] 已发布 {count} 条命令")

        rate.sleep()

    # 停止
    print("\n停止机器人...")
    twist.linear.x = 0.0
    for _ in range(10):
        pub.publish(twist)
        rospy.sleep(0.1)

    print("\n" + "="*60)
    print("结果分析")
    print("="*60)
    print("如果机器人在Gazebo中移动了:")
    print("  ✅ 问题在于MPC速度计算")
    print("  ✅ 需要提高最小速度或修改MPC参数")
    print("")
    print("如果机器人完全不动:")
    print("  ❌ Gazebo插件配置问题")
    print("  ❌ 可能需要检查:")
    print("     - diff_drive插件的maxWheelVelocity")
    print("     - wheelTorque和wheelAcceleration参数")
    print("     - 机器人物理属性（质量、惯性）")
    print("="*60)

    return True

if __name__ == '__main__':
    try:
        result = test_large_velocity()
        sys.exit(0 if result else 1)
    except rospy.ROSInterruptException:
        print("\n用户中断")
        sys.exit(0)
