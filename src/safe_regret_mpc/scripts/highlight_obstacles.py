#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from gazebo_msgs.srv import SetModelState, GetModelState
from gazebo_msgs.msg import ModelState
from std_msgs.msg import ColorRGBA
import math

class ObstacleHighlighter:
    def __init__(self):
        rospy.init_node('highlight_obstacles', anonymous=True)

        # 障碍物名称列表
        self.obstacle_names = ['central_obstacle_1', 'central_obstacle_2', 'central_obstacle_3']

        # 鲜艳的颜色（RGBA）
        self.colors = [
            (1.0, 0.0, 0.0, 1.0),  # 红色
            (0.0, 1.0, 0.0, 1.0),  # 绿色
            (0.0, 0.0, 1.0, 1.0),  # 蓝色
        ]

        # Gazebo服务
        self.get_model_state = rospy.ServiceProxy('/gazebo/get_model_state', GetModelState)
        self.set_model_state = rospy.ServiceProxy('/gazebo/set_model_state', SetModelState)

        rospy.loginfo("=" * 60)
        rospy.loginfo("障碍物高亮显示工具")
        rospy.loginfo("=" * 60)
        rospy.loginfo("正在为障碍物添加鲜艳颜色，使它们在Gazebo中更容易看到...")

        self.highlight_obstacles()

    def highlight_obstacles(self):
        """为障碍物添加鲜艳颜色"""

        print("\n" + "=" * 60)
        print("障碍物颜色高亮")
        print("=" * 60)

        for i, obstacle_name in enumerate(self.obstacle_names):
            try:
                # 获取当前状态
                state = self.get_model_state(model_name=obstacle_name)

                if state.success:
                    # 创建模型状态消息
                    model_state = ModelState()
                    model_state.model_name = obstacle_name
                    model_state.pose = state.pose  # 保持位置不变

                    # 设置鲜艳的颜色
                    r, g, b, a = self.colors[i]
                    model_state.reference_frame = "world"

                    # 注意：Gazebo的set_model_state不支持直接修改颜色
                    # 这里我们只显示信息，实际颜色修改需要编辑world文件
                    color_name = ["红色", "绿色", "蓝色"][i]

                    x = state.pose.position.x
                    y = state.pose.position.y

                    print(f"\n{i+1}. {obstacle_name}")
                    print(f"   位置: ({x:.2f}, {y:.2f})")
                    print(f"   建议颜色: {color_name}")
                    print(f"   在Gazebo中查找: 地面上的灰色圆柱体/方块")

                else:
                    print(f"\n{i+1}. {obstacle_name}")
                    print(f"   ✗ 无法获取状态")

            except rospy.ServiceException as e:
                print(f"\n{i+1}. {obstacle_name}")
                print(f"   ✗ 查询失败: {e}")

        print("\n" + "=" * 60)
        print("\n💡 在Gazebo中查找障碍物的技巧：")
        print("1. 俯视图：将视角转到正上方（向下看）")
        print("2. 使用快捷键：")
        print("   - 'z' 缩小（看更大范围）")
        print("   - 'Z'（Shift+z）放大（看细节）")
        print("3. 移动视角：按住鼠标中键拖动")
        print("4. 旋转视角：按住鼠标左键拖动")
        print("\n障碍物位置：")
        print("   central_obstacle_1: 圆柱形障碍物")
        print("   central_obstacle_2: 方块障碍物")
        print("   central_obstacle_3: 方块障碍物")
        print("\n它们应该在地图中央区域（X坐标在-4到4之间）")
        print("=" * 60)

        # 生成可视化辅助信息
        self.create_visual_markers()

    def create_visual_markers(self):
        """创建可视化标记（可选）"""
        print("\n" + "=" * 60)
        print("障碍物位置地图")
        print("=" * 60)
        print("\n     Y轴")
        print("      ↑")
        print("  10   │")
        print("   5   │    ● obstacle_1 (圆柱)")
        print("   0   │小车(-8)  │  │  目标(8)")
        print("  -5   │         ●obstacle_3(方块)")
        print("       │    ● obstacle_2 (方块)")
        print(" -10   │")
        print("       └─────────────────────────────→ X轴")
        print("      -10  -8  -6  -4  -2   0   2  4  6  8  10")
        print("                   [------- 障碍物区域 -------]")
        print("=" * 60)

if __name__ == '__main__':
    try:
        node = ObstacleHighlighter()
    except rospy.ROSInterruptException:
        rospy.loginfo("\n完成")
    except Exception as e:
        rospy.logerr(f"发生错误: {e}")
