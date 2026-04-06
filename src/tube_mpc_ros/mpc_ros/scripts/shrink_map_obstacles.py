#!/usr/bin/env python3
"""
Map Obstacle Shrinking Tool for test_world.pgm

This script shrinks obstacles (shelves) in the PGM map by a specified margin,
effectively reducing the safety buffer around obstacles.

Usage:
    python3 shrink_map_obstacles.py --margin 0.15  # Shrink by 15cm
    python3 shrink_map_obstacles.py --margin 0.10 --visualize  # Shrink by 10cm and show result
    python3 shrink_map_obstacles.py --margin 0.20 --backup  # Shrink by 20cm with backup
"""

import numpy as np
import matplotlib.pyplot as plt
import argparse
import os
import shutil
from datetime import datetime
from scipy import ndimage

def read_pgm(filename):
    """Read PGM file and return image data."""
    with open(filename, 'rb') as f:
        magic = f.readline().decode().strip()
        if magic != 'P5':
            raise ValueError('Not a P5 PGM file')

        width, height = map(int, f.readline().decode().split())
        max_val = int(f.readline().decode())

        data = np.fromfile(f, dtype=np.uint8)
        return data.reshape((height, width)), width, height, max_val

def write_pgm(filename, data, max_val=255):
    """Write image data to PGM file."""
    height, width = data.shape
    with open(filename, 'wb') as f:
        f.write(b'P5\n')
        f.write(f'{width} {height}\n'.encode())
        f.write(f'{max_val}\n'.encode())
        data.tofile(f)

def shrink_obstacles(map_data, margin_meters, resolution=0.05):
    """
    Shrink obstacles by specified margin in meters.

    Args:
        map_data: numpy array of pixel values
        margin_meters: margin to shrink in meters
        resolution: map resolution in meters/pixel (default: 0.05)

    Returns:
        Shrunk map data
    """
    # Convert margin to pixels
    margin_pixels = int(margin_meters / resolution)

    if margin_pixels <= 0:
        print("Warning: Margin is zero or negative, no shrinking applied.")
        return map_data

    print(f"Shrinking obstacles by {margin_meters}m ({margin_pixels} pixels)")

    # Create binary mask (obstacle vs free space)
    # Threshold: pixel value < 200 is considered obstacle
    obstacle_mask = map_data < 200

    # Count original obstacle pixels
    original_obstacle_count = np.sum(obstacle_mask)
    print(f"Original obstacle pixels: {original_obstacle_count}")

    # Use morphological erosion to shrink obstacles
    # This removes pixels from the edges of obstacles
    shrunk_mask = ndimage.binary_erosion(obstacle_mask, iterations=margin_pixels)

    # Count shrunk obstacle pixels
    shrunk_obstacle_count = np.sum(shrunk_mask)
    print(f"Shrunk obstacle pixels: {shrunk_obstacle_count}")
    print(f"Removed obstacle pixels: {original_obstacle_count - shrunk_obstacle_count}")

    # Create new map data
    new_map_data = map_data.copy()

    # Set eroded pixels to free space (254)
    new_map_data[~shrunk_mask & obstacle_mask] = 254

    return new_map_data

def visualize_results(original_data, shrunk_data, margin_meters, output_path):
    """Create visualization comparing original and shrunk maps."""
    fig, axes = plt.subplots(1, 3, figsize=(20, 6))

    # Original map
    axes[0].imshow(original_data, cmap='gray', origin='upper', vmin=0, vmax=255)
    axes[0].set_title('Original Map\\n(Before Shrinking)', fontsize=14)
    axes[0].set_xlabel('X (pixels)')
    axes[0].set_ylabel('Y (pixels)')

    # Shrunk map
    axes[1].imshow(shrunk_data, cmap='gray', origin='upper', vmin=0, vmax=255)
    axes[1].set_title(f'Shrunk Map\\n(Margin: {margin_meters}m)', fontsize=14)
    axes[1].set_xlabel('X (pixels)')
    axes[1].set_ylabel('Y (pixels)')

    # Difference map
    diff = shrunk_data.astype(np.int16) - original_data.astype(np.int16)
    # Show only where changes occurred (pixels that changed from obstacle to free)
    change_mask = (original_data < 200) & (shrunk_data >= 200)
    diff_visual = np.zeros((diff.shape[0], diff.shape[1], 3), dtype=np.uint8)
    diff_visual[:] = [200, 200, 200]  # Gray background
    diff_visual[original_data < 200] = [0, 0, 0]  # Black for unchanged obstacles
    diff_visual[change_mask] = [0, 255, 0]  # Green for removed obstacle pixels
    diff_visual[original_data >= 200] = [255, 255, 255]  # White for free space

    axes[2].imshow(diff_visual, origin='upper')
    axes[2].set_title('Change Map\\n(Green=Removed Obstacle Pixels)', fontsize=14)
    axes[2].set_xlabel('X (pixels)')
    axes[2].set_ylabel('Y (pixels)')

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"\\nVisualization saved to: {output_path}")

def main():
    parser = argparse.ArgumentParser(description='Shrink obstacles in PGM map file')
    parser.add_argument('--input', type=str,
                       default='/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/map/test_world.pgm',
                       help='Input PGM file path')
    parser.add_argument('--output', type=str,
                       default='/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/map/test_world.pgm',
                       help='Output PGM file path (default: overwrite input)')
    parser.add_argument('--margin', type=float, default=0.15,
                       help='Margin to shrink obstacles in meters (default: 0.15m = 15cm)')
    parser.add_argument('--resolution', type=float, default=0.05,
                       help='Map resolution in meters/pixel (default: 0.05)')
    parser.add_argument('--backup', action='store_true',
                       help='Create backup of original file')
    parser.add_argument('--visualize', action='store_true',
                       help='Create visualization of changes')
    parser.add_argument('--dry-run', action='store_true',
                       help='Show statistics without writing file')

    args = parser.parse_args()

    # Validate input file
    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        return 1

    print("=" * 70)
    print("Map Obstacle Shrinking Tool")
    print("=" * 70)
    print(f"Input file: {args.input}")
    print(f"Output file: {args.output}")
    print(f"Shrink margin: {args.margin}m ({int(args.margin / args.resolution)} pixels)")
    print(f"Map resolution: {args.resolution} m/pixel")
    print("=" * 70)

    # Create backup if requested
    if args.backup and args.input == args.output:
        backup_path = f"{args.input}.backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        print(f"\\nCreating backup: {backup_path}")
        shutil.copy2(args.input, backup_path)
        print("Backup created successfully.")

    # Read input file
    print(f"\\nReading map file: {args.input}")
    map_data, width, height, max_val = read_pgm(args.input)
    print(f"Map dimensions: {width}x{height} pixels")
    print(f"Map real size: {width*args.resolution:.1f}m x {height*args.resolution:.1f}m")

    # Shrink obstacles
    print("\\nShrinking obstacles...")
    shrunk_data = shrink_obstacles(map_data, args.margin, args.resolution)

    # Create visualization if requested
    if args.visualize:
        vis_path = f"/tmp/map_shrinking_{args.margin}m.png"
        print("\\nCreating visualization...")
        visualize_results(map_data, shrunk_data, args.margin, vis_path)

    # Dry run - don't write file
    if args.dry_run:
        print("\\n" + "=" * 70)
        print("DRY RUN - No file written (remove --dry-run to apply changes)")
        print("=" * 70)
        return 0

    # Write output file
    print(f"\\nWriting output file: {args.output}")
    write_pgm(args.output, shrunk_data, max_val)
    print("Output file written successfully.")

    print("\\n" + "=" * 70)
    print("Shrinking completed successfully!")
    print("=" * 70)

    # Provide next steps
    print("\\nNext steps:")
    print("1. Test the modified map:")
    print("   roslaunch safe_regret_mpc safe_regret_mpc_test.launch")
    print("\\n2. If issues occur, restore from backup:")
    if args.backup:
        print(f"   cp {backup_path} {args.output}")
    else:
        print(f"   cp {args.input}.backup_* {args.output}")

    return 0

if __name__ == '__main__':
    exit(main())
