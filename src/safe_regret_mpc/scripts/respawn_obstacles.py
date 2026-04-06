#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from gazebo_msgs.srv import DeleteModel, SpawnModel
from geometry_msgs.msg import Pose
import random
import math

class ObstacleSpawner:
    def __init__(self):
        rospy.init_node('obstacle_spawner', anonymous=True)

        # 障碍物配置（与world文件中的定义一致）
        self.obstacles_config = {
            'central_obstacle_1': {
                'type': 'cylinder',
                'radius': 0.8,
                'length': 1.0,
                'color': (0.6, 0.6, 0.6, 1),  # RGBA
                'initial_pose': (0, 2, 0.5)
            },
            'central_obstacle_2': {
                'type': 'box',
                'size': (1.5, 1.5, 1.0),
                'color': (0.5, 0.5, 0.5, 1),
                'initial_pose': (-2, -2, 0.5)
            },
            'central_obstacle_3': {
                'type': 'box',
                'size': (1.5, 1.5, 1.0),
                'color': (0.5, 0.5, 0.5, 1),
                'initial_pose': (2, -2, 0.5)
            }
        }

        # 地图参数
        self.map_min_x = -10.0
        self.map_max_x = 10.0
        self.map_min_y = -10.0
        self.map_max_y = 10.0

        # 安全参数
        self.safety_radius = rospy.get_param('~safety_radius', 0.6)
        self.robot_safety_radius = rospy.get_param('~robot_safety_radius', 1.5)
        self.robot_start_x = rospy.get_param('/robot_start_x', -8.0)
        self.robot_start_y = rospy.get_param('/robot_start_y', 0.0)
        self.current_goal_x = rospy.get_param('/auto_nav_goal/goal_x', 8.0)
        self.current_goal_y = rospy.get_param('/auto_nav_goal/goal_y', 0.0)

        # Gazebo服务
        self.delete_model = rospy.ServiceProxy('/gazebo/delete_model', DeleteModel)
        self.spawn_model = rospy.ServiceProxy('/gazebo/spawn_sdf_model', SpawnModel)

        rospy.loginfo("=" * 60)
        rospy.loginfo("障碍物重新生成器")
        rospy.loginfo("=" * 60)
        rospy.loginfo(f"小车初始位置: ({self.robot_start_x}, {self.robot_start_y})")
        rospy.loginfo(f"当前目标点: ({self.current_goal_x}, {self.current_goal_y})")
        rospy.loginfo("=" * 60)

        # 等待服务
        rospy.loginfo("等待Gazebo服务...")
        rospy.wait_for_service('/gazebo/delete_model')
        rospy.wait_for_service('/gazebo/spawn_sdf_model')
        rospy.loginfo("✓ Gazebo服务已连接")

        # 重新生成障碍物
        self.respawn_obstacles()

    def is_safe_position(self, x, y):
        """检查位置是否安全"""
        # 计算到目标点的距离
        dx_goal = x - self.current_goal_x
        dy_goal = y - self.current_goal_y
        distance_to_goal = math.sqrt(dx_goal*dx_goal + dy_goal*dy_goal)

        # 计算到小车初始位置的距离
        dx_robot = x - self.robot_start_x
        dy_robot = y - self.robot_start_y
        distance_to_robot = math.sqrt(dx_robot*dx_robot + dy_robot*dy_robot)

        # 距离必须大于安全半径
        safe_from_goal = distance_to_goal > self.safety_radius
        safe_from_robot = distance_to_robot > self.robot_safety_radius

        return safe_from_goal and safe_from_robot

    def generate_random_position(self, obstacle_name):
        """生成安全的随机位置"""
        max_attempts = 100
        obstacle_size = 1.5

        for attempt in range(max_attempts):
            # X坐标限制在[-4, 4]
            x = random.uniform(-4.0, 4.0)
            # Y坐标在[-10, 10]
            y = random.uniform(self.map_min_y + obstacle_size/2, self.map_max_y - obstacle_size/2)

            # 检查安全性
            if self.is_safe_position(x, y):
                # 避免离墙壁太近
                margin = 1.0
                if (self.map_min_x + margin < x < self.map_max_x - margin and
                    self.map_min_y + margin < y < self.map_max_y - margin):
                    return x, y

        rospy.logwarn(f"警告: {obstacle_name} 经过 {max_attempts} 次尝试仍无法找到合适位置")
        return 0.0, 0.0

    def create_sdf_model(self, name, config):
        """创建SDF模型字符串"""
        if config['type'] == 'cylinder':
            r, g, b, a = config['color']
            radius = config['radius']
            length = config['length']

            sdf = f"""<?xml version='1.0'?>
<sdf version='1.6'>
  <model name='{name}'>
    <static>0</static>
    <pose>0 0 0.5 0 0 0</pose>
    <link name='link'>
      <collision name='collision'>
        <geometry>
          <cylinder>
            <radius>{radius}</radius>
            <length>{length}</length>
          </cylinder>
        </geometry>
      </collision>
      <visual name='visual'>
        <geometry>
          <cylinder>
            <radius>{radius}</radius>
            <length>{length}</length>
          </cylinder>
        </geometry>
        <material>
          <ambient>{r} {g} {b} {a}</ambient>
          <diffuse>{r} {g} {b} {a}</diffuse>
        </material>
      </visual>
    </link>
  </model>
</sdf>"""
        else:  # box
            r, g, b, a = config['color']
            sx, sy, sz = config['size']

            sdf = f"""<?xml version='1.0'?>
<sdf version='1.6'>
  <model name='{name}'>
    <static>0</static>
    <pose>0 0 0.5 0 0 0</pose>
    <link name='link'>
      <collision name='collision'>
        <geometry>
          <box>
            <size>{sx} {sy} {sz}</size>
          </box>
        </geometry>
      </collision>
      <visual name='visual'>
        <geometry>
          <box>
            <size>{sx} {sy} {sz}</size>
          </box>
        </geometry>
        <material>
          <ambient>{r} {g} {b} {a}</ambient>
          <diffuse>{r} {g} {b} {a}</diffuse>
        </material>
      </visual>
    </link>
  </model>
</sdf>"""

        return sdf

    def respawn_obstacles(self):
        """删除旧障碍物，在新位置重新生成"""
        rospy.loginfo("\n" + "=" * 60)
        rospy.loginfo("开始重新生成障碍物...")
        rospy.loginfo("=" * 60)

        for name, config in self.obstacles_config.items():
            try:
                # 1. 删除旧模型
                rospy.loginfo(f"删除旧模型: {name}")
                self.delete_model(name)
                rospy.sleep(0.2)  # 等待删除完成

                # 2. 生成新的随机位置
                new_x, new_y = self.generate_random_position(name)

                # 3. 创建SDF模型
                sdf_model = self.create_sdf_model(name, config)

                # 4. 设置新位置的pose
                pose = Pose()
                pose.position.x = new_x
                pose.position.y = new_y
                pose.position.z = 0.5
                pose.orientation.x = 0
                pose.orientation.y = 0
                pose.orientation.z = 0
                pose.orientation.w = 1

                # 5. 生成新模型
                rospy.loginfo(f"生成新模型: {name} 在 ({new_x:.2f}, {new_y:.2f})")
                self.spawn_model(name, sdf_model, "", pose, "world")

                # 计算距离
                dx_goal = new_x - self.current_goal_x
                dy_goal = new_y - self.current_goal_y
                dist_goal = math.sqrt(dx_goal*dx_goal + dy_goal*dy_goal)

                dx_robot = new_x - self.robot_start_x
                dy_robot = new_y - self.robot_start_y
                dist_robot = math.sqrt(dx_robot*dx_robot + dy_robot*dy_robot)

                rospy.loginfo(f"✓ {name}: ({new_x:.2f}, {new_y:.2f}) | 距离小车: {dist_robot:.2f}m | 距离目标: {dist_goal:.2f}m")

                rospy.sleep(0.2)  # 等待生成完成

            except rospy.ServiceException as e:
                rospy.logerr(f"处理 {name} 时出错: {e}")

        rospy.loginfo("=" * 60)
        rospy.loginfo("障碍物重新生成完成！")
        rospy.loginfo("=" * 60)

if __name__ == '__main__':
    try:
        node = ObstacleSpawner()
        rospy.loginfo("障碍物已重新生成，节点退出")
    except rospy.ROSInterruptException:
        rospy.loginfo("\n节点被中断")
    except Exception as e:
        rospy.logerr(f"发生错误: {e}")
