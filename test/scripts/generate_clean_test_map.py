#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
生成干净的测试地图 - 只包含货架本身，无额外安全半径

功能：
- 创建20m x 20m的空地图（从-10到+10）
- 只画出货架实体（0.8m x 5m），不添加安全半径
- 不画障碍物（墙壁）
- 不画出发点和终点标记
- 保存为PGM格式

使用方法：
    python3 generate_clean_test_map.py
"""

import numpy as np
from PIL import Image
import os

def create_clean_test_map():
    """创建干净的测试地图"""

    # 地图参数
    resolution = 0.05  # 5cm per pixel
    map_size = 20.0    # 20m x 20m
    origin_x = -10.0   # 从-10开始
    origin_y = -10.0   # 从-10开始

    # 计算图像尺寸
    width_px = int(map_size / resolution)
    height_px = int(map_size / resolution)

    print(f"创建地图:")
    print(f"  - 尺寸: {map_size}m x {map_size}m")
    print(f"  - 分辨率: {resolution} m/pixel")
    print(f"  - 图像: {width_px} x {height_px} pixels")
    print(f"  - 原点: ({origin_x}, {origin_y})")

    # 创建空白地图（全部为自由空间，白色=255）
    # PGM格式: 0=障碍物(黑), 254=自由空间(白), 205=未知(灰)
    map_data = np.full((height_px, width_px), 254, dtype=np.uint8)

    # 货架定义（根据shelf_locations.yaml和实际货架布局）
    # 注意：货架应该独立，不能相连
    # 货架尺寸: 0.8m x 1.0m (较小的独立货架)
    # 南侧货架 (Y=-7): 从西到东排列
    # 北侧货架 (Y=+7): 从东到西排列
    shelves = [
        # 南侧货架 (Y=-7) - 3个独立货架
        {"x": -8.0, "y": -7.0, "width": 0.8, "length": 1.0, "name": "shelf_south_1"},
        {"x": -5.0, "y": -7.0, "width": 0.8, "length": 1.0, "name": "shelf_south_2"},
        {"x": -2.0, "y": -7.0, "width": 0.8, "length": 1.0, "name": "shelf_south_3"},

        # 北侧货架 (Y=+7) - 3个独立货架
        {"x": 8.0, "y": 7.0, "width": 0.8, "length": 1.0, "name": "shelf_north_1"},
        {"x": 5.0, "y": 7.0, "width": 0.8, "length": 1.0, "name": "shelf_north_2"},
        {"x": 2.0, "y": 7.0, "width": 0.8, "length": 1.0, "name": "shelf_north_3"},
    ]

    # 在地图上画货架
    for shelf in shelves:
        # 货架中心位置（世界坐标）
        center_x = shelf["x"]
        center_y = shelf["y"]

        # 货架尺寸（假设货架是矩形，长边沿X轴）
        shelf_width = shelf["width"]   # 0.8m (Y方向)
        shelf_length = shelf["length"] # 5.0m (X方向)

        # 转换为像素坐标
        # 世界坐标 -> 像素坐标:
        # pixel_x = (world_x - origin_x) / resolution
        # pixel_y = height_px - (world_y - origin_y) / resolution

        center_px = int((center_x - origin_x) / resolution)
        center_py = int(height_px - (center_y - origin_y) / resolution)

        half_width_px = int((shelf_width / 2) / resolution)
        half_length_px = int((shelf_length / 2) / resolution)

        # 计算货架的像素范围
        x_min = center_px - half_length_px
        x_max = center_px + half_length_px
        y_min = center_py - half_width_px
        y_max = center_py + half_width_px

        # 确保在图像范围内
        x_min = max(0, x_min)
        x_max = min(width_px, x_max)
        y_min = max(0, y_min)
        y_max = min(height_px, y_max)

        # 画货架（黑色=0表示障碍物）
        map_data[y_min:y_max, x_min:x_max] = 0

        print(f"  ✓ 添加货架: {shelf['name']} at ({center_x}, {center_y})")

    # 保存为PGM文件
    output_dir = "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/map/"
    pgm_filename = output_dir + "test_world_clean.pgm"
    yaml_filename = output_dir + "test_world_clean.yaml"

    # 创建PIL图像并保存
    img = Image.fromarray(map_data, mode='L')
    img.save(pgm_filename)

    print(f"\n✅ PGM地图已保存: {pgm_filename}")

    # 创建YAML配置文件
    yaml_content = f"""image: test_world_clean.pgm
resolution: {resolution}
origin: [{origin_x}, {origin_y}, 0.000000]
negate: 0
occupied_thresh: 0.65
free_thresh: 0.196
"""

    with open(yaml_filename, 'w') as f:
        f.write(yaml_content)

    print(f"✅ YAML配置已保存: {yaml_filename}")

    # 生成使用说明
    print(f"\n" + "="*70)
    print("📝 地图信息:")
    print("="*70)
    print(f"地图文件: {pgm_filename}")
    print(f"配置文件: {yaml_filename}")
    print(f"\n地图内容:")
    print(f"  - 地图尺寸: {map_size}m x {map_size}m")
    print(f"  - 分辨率: {resolution} m/pixel ({width_px} x {height_px} pixels)")
    print(f"  - 货架数量: {len(shelves)}")
    print(f"  - 货架尺寸: 0.8m x 5.0m (无额外安全半径)")
    print(f"  - 障碍物: 无")
    print(f"  - 起点/终点标记: 无")
    print(f"\n使用方法:")
    print(f"  1. 修改launch文件中的地图路径:")
    print(f"     <arg name=\"map_file\" default=\"$(find tube_mpc_ros)/map/test_world_clean.yaml\"/>")
    print(f"  2. 或者创建符号链接:")
    print(f"     cd {output_dir}")
    print(f"     ln -sf test_world_clean.pgm test_world.pgm")
    print(f"     ln -sf test_world_clean.yaml test_world.yaml")
    print("="*70)

if __name__ == "__main__":
    create_clean_test_map()
