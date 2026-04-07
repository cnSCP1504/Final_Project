#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
测试地图可视化工具

显示当前使用的test_world.pgm地图内容
"""

import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
import os

def visualize_map(map_file=None):
    """可视化地图"""

    if map_file is None:
        map_file = "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/map/test_world.pgm"

    if not os.path.exists(map_file):
        print(f"❌ 地图文件不存在: {map_file}")
        return

    # 读取地图
    img = Image.open(map_file)
    map_data = np.array(img)

    # 读取YAML配置
    yaml_file = map_file.replace('.pgm', '.yaml')
    resolution = 0.05
    origin_x = -10.0
    origin_y = -10.0

    if os.path.exists(yaml_file):
        with open(yaml_file, 'r') as f:
            for line in f:
                if 'resolution:' in line:
                    resolution = float(line.split(':')[1].strip())
                elif 'origin:' in line:
                    coords = line.split(':')[1].strip().strip('[]').split(',')
                    origin_x = float(coords[0].strip())
                    origin_y = float(coords[1].strip())

    # 创建图形
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8))

    # 左图：原始地图
    ax1.imshow(map_data, cmap='gray', origin='upper',
               extent=[origin_x, origin_x + map_data.shape[1] * resolution,
                      origin_y + map_data.shape[0] * resolution, origin_y])
    ax1.set_title('原始地图 (Original Map)', fontsize=14, fontweight='bold')
    ax1.set_xlabel('X (m)')
    ax1.set_ylabel('Y (m)')
    ax1.grid(True, alpha=0.3)

    # 右图：带标注的地图
    ax2.imshow(map_data, cmap='gray', origin='upper',
               extent=[origin_x, origin_x + map_data.shape[1] * resolution,
                      origin_y + map_data.shape[0] * resolution, origin_y])
    ax2.set_title('测试地图布局 (Test Map Layout)', fontsize=14, fontweight='bold')
    ax2.set_xlabel('X (m)')
    ax2.set_ylabel('Y (m)')
    ax2.grid(True, alpha=0.3)

    # 添加货架标注
    from scipy import ndimage
    labeled, num_features = ndimage.label(map_data == 0)

    shelf_labels = [
        "shelf_south_1", "shelf_south_2", "shelf_south_3",
        "shelf_north_1", "shelf_north_2", "shelf_north_3"
    ]

    for i in range(1, min(num_features + 1, len(shelf_labels) + 1)):
        shelf_mask = (labeled == i)
        coords = np.argwhere(shelf_mask)

        if len(coords) > 0:
            center_y_px = int(np.mean(coords[:, 0]))
            center_x_px = int(np.mean(coords[:, 1]))

            world_x = origin_x + center_x_px * resolution
            world_y = origin_y + (map_data.shape[0] - center_y_px) * resolution

            # 标注货架
            ax2.plot(world_x, world_y, 'r+', markersize=15, markeredgewidth=2)
            ax2.text(world_x, world_y + 0.5, shelf_labels[i-1],
                    fontsize=8, ha='center', va='bottom',
                    bbox=dict(boxstyle='round,pad=0.3', facecolor='yellow', alpha=0.7))

    # 添加取货点标注（根据shelf_locations.yaml）
    pickup_points = [
        (-6.5, -7.0, "取货点01"),
        (-3.5, -7.0, "取货点02"),
        (0.0, -7.0, "取货点03"),
        (3.5, -7.0, "取货点04"),
        (6.5, -7.0, "取货点05"),
    ]

    for x, y, label in pickup_points:
        ax2.plot(x, y, 'go', markersize=10)
        ax2.text(x, y - 0.5, label, fontsize=7, ha='center', va='top',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='cyan', alpha=0.7))

    # 添加起点和终点
    ax2.plot(-8.0, 0.0, 'bs', markersize=12, label='起点(装货区)')
    ax2.plot(8.0, 0.0, 'rs', markersize=12, label='终点(卸货区)')
    ax2.legend(loc='upper right', fontsize=10)

    # 添加统计信息
    info_text = f"地图信息:\n"
    info_text += f"- 尺寸: {map_data.shape[1] * resolution:.1f}m × {map_data.shape[0] * resolution:.1f}m\n"
    info_text += f"- 分辨率: {resolution} m/pixel\n"
    info_text += f"- 货架数量: {num_features}\n"
    info_text += f"- 货架尺寸: 0.8m × 1.0m"

    ax2.text(0.02, 0.98, info_text, transform=ax2.transAxes,
            fontsize=9, verticalalignment='top',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    plt.tight_layout()

    # 保存图形
    output_file = "/tmp/test_world_map_visualization.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"✅ 地图可视化已保存: {output_file}")

    # 显示图形
    plt.show()

if __name__ == "__main__":
    visualize_map()
