#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from geometry_msgs.msg import PoseStamped
import random
import math
from gazebo_msgs.srv import SetModelState, GetModelState
from gazebo_msgs.msg import ModelState
import sys

class RandomObstacles:
    def __init__(self):
        rospy.init_node('random_obstacles', anonymous=True)

        # 障碍物名称列表
        self.obstacle_names = ['central_obstacle_1', 'central_obstacle_2', 'central_obstacle_3']

        # 地图边界 (20m x 20m, 坐标范围: -10 到 10)
        self.map_min_x = -10.0
        self.map_max_x = 10.0
        self.map_min_y = -10.0
        self.map_max_y = 10.0

        # 目标点安全半径 (不能在此范围内生成障碍物)
        self.safety_radius = rospy.get_param('~safety_radius', 0.6)

        # 当前目标点 (从auto_nav_goal.py读取的参数)
        self.current_goal_x = rospy.get_param('/auto_nav_goal/goal_x', 8.0)
        self.current_goal_y = rospy.get_param('/auto_nav_goal/goal_y', 0.0)

        # 小车初始位置（启动时需要避开）
        self.robot_start_x = rospy.get_param('/robot_start_x', -8.0)
        self.robot_start_y = rospy.get_param('/robot_start_y', 0.0)
        self.robot_safety_radius = rospy.get_param('~robot_safety_radius', 1.5)  # 小车周围安全半径

        # 订阅目标点话题（监听实际发送的目标）
        self.goal_sub = rospy.Subscriber('/move_base_simple/goal', PoseStamped, self.goal_callback)

        # Gazebo服务
        self.set_model_state = rospy.ServiceProxy('/gazebo/set_model_state', SetModelState)
        self.get_model_state = rospy.ServiceProxy('/gazebo/get_model_state', GetModelState)

        rospy.loginfo("=" * 60)
        rospy.loginfo("随机障碍物管理器启动")
        rospy.loginfo("=" * 60)
        rospy.loginfo(f"障碍物随机范围: X ∈ [-6, 6] m, Y ∈ [-2.5, 2.5] m")
        rospy.loginfo(f"目标点安全半径: {self.safety_radius} m")
        rospy.loginfo(f"小车安全半径: {self.robot_safety_radius} m")
        rospy.loginfo(f"小车初始位置: ({self.robot_start_x}, {self.robot_start_y})")
        rospy.loginfo(f"初始目标点: ({self.current_goal_x}, {self.current_goal_y})")
        rospy.loginfo("=" * 60)

        # 等待Gazebo服务可用
        rospy.loginfo("等待Gazebo服务...")
        rospy.wait_for_service('/gazebo/set_model_state')
        rospy.wait_for_service('/gazebo/get_model_state')
        rospy.loginfo("✓ Gazebo服务已连接")

        # 首次随机化障碍物（仅在启动时执行一次）
        self.randomize_obstacles()

        # 不再定时刷新，障碍物位置在启动后固定不变
        rospy.loginfo("=" * 60)
        rospy.loginfo("障碍物位置已固定，本次测试期间不再改变")
        rospy.loginfo("=" * 60)

    def goal_callback(self, msg):
        """监听目标点变化（仅记录，不刷新障碍物）"""
        self.current_goal_x = msg.pose.position.x
        self.current_goal_y = msg.pose.position.y
        rospy.loginfo(f"目标点更新: ({self.current_goal_x:.2f}, {self.current_goal_y:.2f})")
        rospy.loginfo("注意：障碍物位置已固定，不会随目标点变化")

    def is_safe_position(self, x, y):
        """检查位置是否安全（不在目标点和小车周围的安全半径内）"""
        # 计算到目标点的距离
        dx_goal = x - self.current_goal_x
        dy_goal = y - self.current_goal_y
        distance_to_goal = math.sqrt(dx_goal*dx_goal + dy_goal*dy_goal)

        # 计算到小车初始位置的距离
        dx_robot = x - self.robot_start_x
        dy_robot = y - self.robot_start_y
        distance_to_robot = math.sqrt(dx_robot*dx_robot + dy_robot*dy_robot)

        # 距离必须大于安全半径（同时避开目标点和小车）
        safe_from_goal = distance_to_goal > self.safety_radius
        safe_from_robot = distance_to_robot > self.robot_safety_radius

        return safe_from_goal and safe_from_robot

    def generate_random_position(self, obstacle_name):
        """为障碍物生成随机位置，避开目标点周围区域"""

        max_attempts = 100  # 最大尝试次数
        obstacle_size = 1.5  # 障碍物尺寸（用于边界检查）

        for attempt in range(max_attempts):
            # 生成随机位置（限制在指定范围内）
            x = random.uniform(-6.0, 6.0)  # X坐标限制在[-6, 6]范围内
            y = random.uniform(-2.5, 2.5)  # Y坐标限制在[-2.5, 2.5]范围内

            # 检查是否在安全范围内
            if self.is_safe_position(x, y):
                # 额外检查：避免障碍物离墙壁太近
                margin = 1.0  # 墙壁边距
                if (self.map_min_x + margin < x < self.map_max_x - margin and
                    self.map_min_y + margin < y < self.map_max_y - margin):
                    return x, y

        rospy.logwarn(f"警告: {obstacle_name} 经过 {max_attempts} 次尝试仍无法找到合适位置")
        # 如果实在找不到，返回一个默认位置（远离目标点）
        default_x = -self.current_goal_x if abs(self.current_goal_x) < 5 else 0
        default_y = -self.current_goal_y if abs(self.current_goal_y) < 5 else 0
        return default_x, default_y

    def randomize_obstacles(self):
        """随机刷新所有障碍物位置（仅在启动时执行一次）"""
        rospy.loginfo("\n" + "=" * 60)
        rospy.loginfo("开始随机放置障碍物（仅启动时执行一次）...")
        rospy.loginfo("=" * 60)
        rospy.loginfo(f"小车初始位置: ({self.robot_start_x:.2f}, {self.robot_start_y:.2f}) | 安全半径: {self.robot_safety_radius} m")
        rospy.loginfo(f"当前目标点: ({self.current_goal_x:.2f}, {self.current_goal_y:.2f}) | 安全半径: {self.safety_radius} m")
        rospy.loginfo("=" * 60)

        for i, obstacle_name in enumerate(self.obstacle_names, 1):
            # 生成新的随机位置
            new_x, new_y = self.generate_random_position(obstacle_name)

            # 创建模型状态消息
            model_state = ModelState()
            model_state.model_name = obstacle_name
            model_state.pose.position.x = new_x
            model_state.pose.position.y = new_y
            model_state.pose.position.z = 0.5  # 保持高度
            model_state.pose.orientation.x = 0
            model_state.pose.orientation.y = 0
            model_state.pose.orientation.z = 0
            model_state.pose.orientation.w = 1
            model_state.reference_frame = "world"

            # 计算到目标点和小车的距离
            dx_goal = new_x - self.current_goal_x
            dy_goal = new_y - self.current_goal_y
            dist_to_goal = math.sqrt(dx_goal*dx_goal + dy_goal*dy_goal)

            dx_robot = new_x - self.robot_start_x
            dy_robot = new_y - self.robot_start_y
            dist_to_robot = math.sqrt(dx_robot*dx_robot + dy_robot*dy_robot)

            try:
                # 设置障碍物位置
                self.set_model_state(model_state)

                # 短暂等待确保位置更新完成
                rospy.sleep(0.1)

                # 再次设置位置并设置零速度，确保障碍物不会滑动
                model_state.twist.linear.x = 0
                model_state.twist.linear.y = 0
                model_state.twist.linear.z = 0
                model_state.twist.angular.x = 0
                model_state.twist.angular.y = 0
                model_state.twist.angular.z = 0
                self.set_model_state(model_state)

                rospy.loginfo(f"✓ {obstacle_name}: ({new_x:.2f}, {new_y:.2f}) | 距离小车: {dist_to_robot:.2f}m | 距离目标: {dist_to_goal:.2f}m")
            except rospy.ServiceException as e:
                rospy.logerr(f"设置 {obstacle_name} 位置失败: {e}")

        rospy.loginfo("=" * 60)
        rospy.loginfo("障碍物随机放置完成！位置已固定，本次测试期间不再改变。\n")

    # 移除定时刷新功能 - 障碍物位置固定
    # def timer_callback(self, event):
    #     """定时器回调 - 定期刷新障碍物位置"""
    #     self.randomize_obstacles()

if __name__ == '__main__':
    try:
        node = RandomObstacles()
        rospy.loginfo("随机障碍物管理器运行中，按 Ctrl+C 退出...")
        rospy.spin()
    except rospy.ROSInterruptException:
        rospy.loginfo("\n随机障碍物管理器已关闭")
    except Exception as e:
        rospy.logerr(f"发生错误: {e}")
        sys.exit(1)
