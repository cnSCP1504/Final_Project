#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from gazebo_msgs.srv import GetModelState
import math

class ObstacleVisualizer:
    def __init__(self):
        rospy.init_node('show_obstacles_in_gazebo', anonymous=True)

        # 障碍物名称列表
        self.obstacle_names = ['central_obstacle_1', 'central_obstacle_2', 'central_obstacle_3']

        # Gazebo服务
        self.get_model_state = rospy.ServiceProxy('/gazebo/get_model_state', GetModelState)

        rospy.loginfo("=" * 60)
        rospy.loginfo("障碍物位置查询工具")
        rospy.loginfo("=" * 60)
        rospy.loginfo("正在查询Gazebo中的障碍物位置...")
        rospy.loginfo("")

        self.show_obstacle_positions()

    def show_obstacle_positions(self):
        """查询并显示障碍物位置"""

        print("\n" + "=" * 60)
        print("障碍物在Gazebo中的实际位置")
        print("=" * 60)

        for i, obstacle_name in enumerate(self.obstacle_names, 1):
            try:
                # 获取模型状态
                state = self.get_model_state(model_name=obstacle_name)

                if state.success:
                    x = state.pose.position.x
                    y = state.pose.position.y
                    z = state.pose.position.z

                    # 计算到小车和目标的距离
                    robot_x, robot_y = -8.0, 0.0
                    goal_x, goal_y = 8.0, 0.0

                    dist_robot = math.sqrt((x - robot_x)**2 + (y - robot_y)**2)
                    dist_goal = math.sqrt((x - goal_x)**2 + (y - goal_y)**2)

                    print(f"\n{i}. {obstacle_name}")
                    print(f"   位置: ({x:.2f}, {y:.2f}, {z:.2f})")
                    print(f"   距离小车: {dist_robot:.2f} m")
                    print(f"   距离目标: {dist_goal:.2f} m")
                    print(f"   X坐标范围检查: {-4.0:.1f} <= {x:.2f} <= {4.0:.1f} {'✓' if -4.0 <= x <= 4.0 else '✗'}")

                else:
                    print(f"\n{i}. {obstacle_name}")
                    print(f"   ✗ 无法获取位置")

            except rospy.ServiceException as e:
                print(f"\n{i}. {obstacle_name}")
                print(f"   ✗ 查询失败: {e}")

        print("\n" + "=" * 60)
        print("\n💡 提示：")
        print("1. 在Gazebo中，使用鼠标滚轮缩放")
        print("2. 按住鼠标中键拖动来平移视角")
        print("3. 按住鼠标左键拖动来旋转视角")
        print("4. 观察地图中央区域（X在-4到4之间）的障碍物")
        print("\n障碍物应该在地面上，显示为灰色圆柱体或方块")
        print("=" * 60)

if __name__ == '__main__':
    try:
        node = ObstacleVisualizer()
    except rospy.ROSInterruptException:
        rospy.loginfo("\n查询完成")
    except Exception as e:
        rospy.logerr(f"发生错误: {e}")
