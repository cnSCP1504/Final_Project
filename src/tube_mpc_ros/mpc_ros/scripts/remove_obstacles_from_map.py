#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
移除地图中的障碍物，创建部分已知地图
用于Phase 6实验：模拟部分已知环境
"""

import numpy as np
from PIL import Image
import sys

def remove_obstacles_from_map(input_path, output_path):
    """
    移除地图中的三个障碍物区域
    障碍物位置：
    1. (0, 2) - 圆形障碍物，半径0.8m
    2. (-2, -2) - 方形障碍物，1.5x1.5m
    3. (2, -2) - 方形障碍物，1.5x1.5m
    """

    # 读取原始地图
    img = Image.open(input_path)
    img_array = np.array(img)

    # 地图参数
    resolution = 0.05  # 5cm per pixel
    origin_x = -10.0
    origin_y = -10.0

    # 世界坐标转像素坐标的函数
    def world_to_pixel(wx, wy):
        px = int((wx - origin_x) / resolution)
        py = int((wy - origin_y) / resolution)
        return px, py

    # 创建空白区域（将障碍物区域设为自由空间）
    # 障碍物1: (0, 2), 圆形，半径0.8m
    cx, cy = world_to_pixel(0, 2)
    radius_pixels = int(0.8 / resolution)
    y, x = np.ogrid[:img_array.shape[0], :img_array.shape[1]]
    mask1 = (x - cx)**2 + (y - cy)**2 <= radius_pixels**2
    img_array[mask1] = 254  # 白色表示自由空间

    # 障碍物2: (-2, -2), 方形，1.5x1.5m
    cx, cy = world_to_pixel(-2, -2)
    size_pixels = int(1.5 / resolution)
    x_start = max(0, cx - size_pixels // 2)
    x_end = min(img_array.shape[1], cx + size_pixels // 2)
    y_start = max(0, cy - size_pixels // 2)
    y_end = min(img_array.shape[0], cy + size_pixels // 2)
    img_array[y_start:y_end, x_start:x_end] = 254

    # 障碍物3: (2, -2), 方形，1.5x1.5m
    cx, cy = world_to_pixel(2, -2)
    size_pixels = int(1.5 / resolution)
    x_start = max(0, cx - size_pixels // 2)
    x_end = min(img_array.shape[1], cx + size_pixels // 2)
    y_start = max(0, cy - size_pixels // 2)
    y_end = min(img_array.shape[0], cy + size_pixels // 2)
    img_array[y_start:y_end, x_start:x_end] = 254

    # 保存处理后的地图
    result_img = Image.fromarray(img_array)
    result_img.save(output_path)
    print(f"✅ 部分已知地图已创建: {output_path}")
    print(f"   原始地图: {input_path}")
    print(f"   移除障碍物: 3个")
    print(f"   图像尺寸: {img_array.shape[1]}x{img_array.shape[0]}")

if __name__ == '__main__':
    input_map = "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/map/test_world.pgm"
    output_map = "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/map/test_world_no_obstacles.pgm"

    try:
        remove_obstacles_from_map(input_map, output_map)
    except Exception as e:
        print(f"❌ 错误: {e}")
        sys.exit(1)
