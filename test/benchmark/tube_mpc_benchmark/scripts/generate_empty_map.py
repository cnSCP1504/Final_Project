#!/usr/bin/env python3
"""
Generate empty map PGM file for benchmark testing
"""

from PIL import Image
import os

def generate_empty_map(output_path, size_pixels=400, resolution=0.05):
    """
    Generate empty white map

    Args:
        output_path: Full path to output .pgm file
        size_pixels: Image size in pixels (square)
        resolution: Map resolution (m/pixel)
    """
    # Create white image (255 = free space)
    img = Image.new('L', (size_pixels, size_pixels), color=255)

    # Save as PGM
    img.save(output_path)
    print(f"Generated empty map: {output_path}")
    print(f"Size: {size_pixels}x{size_pixels} pixels")
    print(f"Physical size: {size_pixels * resolution:.2f}x{size_pixels * resolution:.2f}m")

if __name__ == '__main__':
    import sys

    # Default output path
    output_dir = os.path.join(os.path.dirname(__file__), '..', 'configs')
    output_path = os.path.join(output_dir, 'empty_map.pgm')

    # Generate map
    os.makedirs(output_dir, exist_ok=True)
    generate_empty_map(output_path, size_pixels=400, resolution=0.05)
