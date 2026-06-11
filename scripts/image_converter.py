#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
PNG to GFX image converter.

Converts PNG images to one of the image payload formats supported by GFX:
RGB565, RGB565A8, RGB888, or RGB888A8. Supports both C file and binary output formats
and can process single files or batch process all PNG files in a directory.
"""

import argparse
import os
import sys
from PIL import Image
import re
import struct
import glob

GFX_COLOR_FORMAT_RGB565 = 0x04
GFX_COLOR_FORMAT_RGB565_SWAPPED = 0x05
GFX_COLOR_FORMAT_RGB565A8 = 0x0A
GFX_COLOR_FORMAT_RGB565A8_SWAPPED = 0x0B
GFX_COLOR_FORMAT_RGB888 = 0x0F
GFX_COLOR_FORMAT_RGB888A8 = 0x10

FORMAT_META = {
    'rgb565': {
        'name': 'RGB565',
        'cf': GFX_COLOR_FORMAT_RGB565,
        'cf_swapped': GFX_COLOR_FORMAT_RGB565_SWAPPED,
        'c_cf': 'GFX_COLOR_FORMAT_RGB565',
        'c_cf_swapped': 'GFX_COLOR_FORMAT_RGB565_SWAPPED',
        'pixel_size': 2,
        'has_alpha': False,
        'allow_swap16': True,
    },
    'rgb565a8': {
        'name': 'RGB565A8',
        'cf': GFX_COLOR_FORMAT_RGB565A8,
        'cf_swapped': GFX_COLOR_FORMAT_RGB565A8_SWAPPED,
        'c_cf': 'GFX_COLOR_FORMAT_RGB565A8',
        'c_cf_swapped': 'GFX_COLOR_FORMAT_RGB565A8_SWAPPED',
        'pixel_size': 2,
        'has_alpha': True,
        'allow_swap16': True,
    },
    'rgb888': {
        'name': 'RGB888',
        'cf': GFX_COLOR_FORMAT_RGB888,
        'c_cf': 'GFX_COLOR_FORMAT_RGB888',
        'pixel_size': 3,
        'has_alpha': False,
        'allow_swap16': False,
    },
    'rgb888a8': {
        'name': 'RGB888A8',
        'cf': GFX_COLOR_FORMAT_RGB888A8,
        'c_cf': 'GFX_COLOR_FORMAT_RGB888A8',
        'pixel_size': 3,
        'has_alpha': True,
        'allow_swap16': False,
    },
}

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565"""
    r = (r >> 3) & 0x1F
    g = (g >> 2) & 0x3F
    b = (b >> 3) & 0x1F
    return (r << 11) | (g << 5) | b

def rgb565_to_bytes(rgb565, swap16=False):
    """Convert RGB565 to image payload bytes.

    RGB565 stores high-byte, low-byte payload. RGB565_SWAPPED stores
    low-byte, high-byte payload. The byte order is represented by the image
    color format, not by a native-framebuffer flag.
    """
    high_byte = (rgb565 >> 8) & 0xFF
    low_byte = rgb565 & 0xFF

    if swap16:
        return [low_byte, high_byte]
    else:
        return [high_byte, low_byte]

def build_image_payload(pixels, width, height, output_format, swap16=False):
    """Build image bytes and metadata for the requested output format."""
    meta = FORMAT_META[output_format]

    if swap16 and not meta['allow_swap16']:
        raise ValueError('--swap16 is only valid for RGB565/RGB565A8 formats')

    color_data = []
    alpha_data = []

    for pixel in pixels:
        r, g, b, a = pixel

        if output_format in ('rgb888', 'rgb888a8'):
            color_data.extend([r, g, b])
            if meta['has_alpha']:
                alpha_data.append(a)
        else:
            rgb565 = rgb888_to_rgb565(r, g, b)
            color_data.extend(rgb565_to_bytes(rgb565, swap16))
            if meta['has_alpha']:
                alpha_data.append(a)

    final_data = color_data + alpha_data
    stride = width * meta['pixel_size']
    color_format = meta.get('c_cf_swapped') if swap16 else meta['c_cf']
    cf = meta.get('cf_swapped') if swap16 else meta['cf']
    format_name = meta['name']
    if swap16:
        format_name += '_SWAPPED'

    return {
        'data': final_data,
        'color_data': color_data,
        'alpha_data': alpha_data,
        'stride': stride,
        'color_format': color_format,
        'cf': cf,
        'format_name': format_name,
        'pixel_size': meta['pixel_size'],
        'has_alpha': meta['has_alpha'],
        'allow_swap16': meta['allow_swap16'],
    }

def format_array(data, indent=4, per_line=130):
    """Format data as C array with proper indentation and line breaks"""
    lines = []
    for i in range(0, len(data), per_line):
        line = ', '.join(f'0x{b:02x}' for b in data[i:i + per_line])
        lines.append(' ' * indent + line + ',')
    return '\n'.join(lines)

def generate_c_file(image_path, output_path, var_name, swap16=False, output_format='rgb565a8'):
    """Generate C file from PNG image

    Args:
        image_path: Input PNG file path
        output_path: Output C file path
        var_name: Variable name for the C array
        swap16: Emit RGB565_SWAPPED/RGB565A8_SWAPPED payload
        output_format: rgb565, rgb565a8, rgb888, or rgb888a8
    """

    # Open and convert image
    try:
        img = Image.open(image_path)
        if img.mode != 'RGBA':
            img = img.convert('RGBA')
    except Exception as e:
        print(f'Error opening image {image_path}: {e}')
        return False

    width, height = img.size
    pixels = list(img.getdata())

    payload = build_image_payload(pixels, width, height, output_format, swap16)
    final_data = payload['data']
    color_data = payload['color_data']
    alpha_data = payload['alpha_data']
    color_format = payload['color_format']
    format_name = payload['format_name']
    stride = payload['stride']

    # Generate C file content
    c_content = f"""#include "gfx.h"

const uint8_t {var_name}_map[] = {{
{format_array(final_data)}
}};

const gfx_image_dsc_t {var_name} = {{
    .header.cf = {color_format},
    .header.magic = GFX_IMAGE_HEADER_MAGIC,
    .header.flags = 0,
    .header.w = {width},
    .header.h = {height},
    .header.stride = {stride},
    .data_size = {len(final_data)},
    .data = {var_name}_map,
}};
"""

    # Write to file
    try:
        with open(output_path, 'w') as f:
            f.write(c_content)
        print(f'Successfully generated {output_path}')
        print(f'Format: {format_name}')
        print(f'Image size: {width}x{height}')
        print(f'Total data size: {len(final_data)} bytes')
        print(f'Color data: {len(color_data)} bytes ({width * height * payload["pixel_size"]} bytes)')
        if payload['has_alpha']:
            print(f'Alpha data: {len(alpha_data)} bytes ({width * height} bytes)')
        if payload['allow_swap16']:
            print(f"RGB565 swapped payload: {'yes' if swap16 else 'no'}")
        print(f'Stride: {stride} bytes per row')
        return True
    except Exception as e:
        print(f'Error writing file {output_path}: {e}')
        return False

def generate_bin_file(image_path, output_path, swap16=False, output_format='rgb565a8'):
    """Generate binary file from PNG image with header compatible with gfx_image_header_t structure

    Args:
        image_path: Input PNG file path
        output_path: Output binary file path
        swap16: Emit RGB565_SWAPPED/RGB565A8_SWAPPED payload
        output_format: rgb565, rgb565a8, rgb888, or rgb888a8
    """

    # Open and convert image
    try:
        img = Image.open(image_path)
        if img.mode != 'RGBA':
            img = img.convert('RGBA')
    except Exception as e:
        print(f'Error opening image {image_path}: {e}')
        return False

    width, height = img.size
    pixels = list(img.getdata())

    payload = build_image_payload(pixels, width, height, output_format, swap16)
    final_data = payload['data']
    color_data = payload['color_data']
    alpha_data = payload['alpha_data']
    cf = payload['cf']
    stride = payload['stride']
    format_name = payload['format_name']

    # Create gfx_image_header_t structure (12 bytes total)
    magic = 0x19  # GFX_IMAGE_HEADER_MAGIC
    flags = 0x0000
    reserved = 0x0000  # Reserved field

    # Pack gfx_image_header_t as bit fields in 3 uint32_t values
    # First uint32: magic(8) + cf(8) + flags(16)
    header_word1 = (magic & 0xFF) | ((cf & 0xFF) << 8) | ((flags & 0xFFFF) << 16)

    # Second uint32: w(16) + h(16)
    header_word2 = (width & 0xFFFF) | ((height & 0xFFFF) << 16)

    # Third uint32: stride(16) + reserved(16)
    header_word3 = (stride & 0xFFFF) | ((reserved & 0xFFFF) << 16)

    # Pack header structure - use little-endian for ESP32 compatibility
    # Layout: header_word1(4) + header_word2(4) + header_word3(4) = 12 bytes total
    header = struct.pack('<III', header_word1, header_word2, header_word3)

    # Write binary file: header (12 bytes) + image data
    try:
        with open(output_path, 'wb') as f:
            f.write(header)
            f.write(bytes(final_data))
        print(f'Successfully generated {output_path}')
        print(f'Format: {format_name}')
        print(f'Image size: {width}x{height}')
        print(f'Header size: {len(header)} bytes')
        print(f'Total data size: {len(final_data)} bytes')
        print(f'Color data: {len(color_data)} bytes ({width * height * payload["pixel_size"]} bytes)')
        if payload['has_alpha']:
            print(f'Alpha data: {len(alpha_data)} bytes ({width * height} bytes)')
        print(f'Stride: {stride} bytes per row')
        print('Data offset: 12 bytes')
        print(f'Total file size: {len(header) + len(final_data)} bytes')
        if payload['allow_swap16']:
            print(f"RGB565 swapped payload: {'yes' if swap16 else 'no'}")
        print(f'Header layout: magic=0x{magic:02x}, cf=0x{cf:02x}, flags=0x{flags:04x}')
        return True
    except Exception as e:
        print(f'Error writing file {output_path}: {e}')
        return False

def process_single_file(input_file, output_dir, bin_format, swap16, output_format):
    """Process a single PNG file"""
    # Determine output path and variable name from input filename
    base_name = os.path.splitext(os.path.basename(input_file))[0]

    if bin_format:
        # Output binary file
        output_path = os.path.join(output_dir, f'{base_name}.bin')
        return generate_bin_file(input_file, output_path, swap16, output_format)
    else:
        # Output C file
        output_path = os.path.join(output_dir, f'{base_name}.c')
        # Convert to valid C identifier
        var_name = re.sub(r'[^a-zA-Z0-9_]', '_', base_name)
        if var_name[0].isdigit():
            var_name = 'img_' + var_name
        return generate_c_file(input_file, output_path, var_name, swap16, output_format)

def find_png_files(input_path):
    """Find all PNG files in the given path"""
    png_files = []

    if os.path.isfile(input_path):
        # Single file
        if input_path.lower().endswith('.png'):
            png_files.append(input_path)
        else:
            print("Warning: Input file doesn't have .png extension")
            png_files.append(input_path)
    elif os.path.isdir(input_path):
        # Directory - find all PNG files
        png_pattern = os.path.join(input_path, '*.png')
        png_files = glob.glob(png_pattern)

        # Also search in subdirectories
        png_pattern_recursive = os.path.join(input_path, '**', '*.png')
        png_files.extend(glob.glob(png_pattern_recursive, recursive=True))

        # Remove duplicates and sort
        png_files = sorted(list(set(png_files)))

    return png_files

def main():
    parser = argparse.ArgumentParser(
        description='Convert PNG to GFX image format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert to RGB565A8 (with alpha) C file
  %(prog)s image.png

  # Convert to RGB565 (without alpha) C file
  %(prog)s image.png --format rgb565

  # Convert to RGB888 (without alpha) C file
  %(prog)s image.png --format rgb888

  # Convert to RGB888A8 (with alpha plane) C file
  %(prog)s image.png --format rgb888a8

  # Convert to binary format with RGB565_SWAPPED payload bytes
  %(prog)s image.png --bin --swap16

  # Batch convert all PNG files in directory
  %(prog)s images/ --output output/
        """
    )
    parser.add_argument('input', help='Input PNG file path or directory path')
    parser.add_argument('--output', '-o', help='Output directory (default: current directory)')
    parser.add_argument('--bin', action='store_true', help='Output binary format instead of C file')
    parser.add_argument('--swap16', action='store_true',
                        help='emit RGB565_SWAPPED/RGB565A8_SWAPPED payload bytes')
    parser.add_argument('--format', '-f', choices=['rgb565', 'rgb565a8', 'rgb888', 'rgb888a8'], default='rgb565a8',
                        help='Output format: rgb565, rgb565a8 (default), rgb888, or rgb888a8')

    args = parser.parse_args()

    # Validate input path
    if not os.path.exists(args.input):
        print(f"Error: Input path '{args.input}' does not exist")
        return 1

    # Set output directory
    output_dir = args.output if args.output else '.'
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if args.swap16 and args.format in ('rgb888', 'rgb888a8'):
        print('Error: --swap16 is only valid for rgb565/rgb565a8 formats')
        return 1

    # Find all PNG files
    png_files = find_png_files(args.input)

    if not png_files:
        print(f"No PNG files found in '{args.input}'")
        return 1

    print(f'Found {len(png_files)} PNG file(s) to process:')
    for png_file in png_files:
        print(f'  - {png_file}')
    print(f'Output format: {args.format.upper()}')
    print(f'Output type: {"Binary" if args.bin else "C file"}')
    if FORMAT_META[args.format]['allow_swap16']:
        print(f'RGB565 swapped payload: {"Enabled" if args.swap16 else "Disabled"}')
    print()

    # Process each PNG file
    success_count = 0
    for png_file in png_files:
        print(f'Processing: {png_file}')
        if process_single_file(png_file, output_dir, args.bin, args.swap16, args.format):
            success_count += 1
        print()  # Add blank line between files

    print(f'Processing complete: {success_count}/{len(png_files)} files processed successfully')

    if success_count == len(png_files):
        return 0
    else:
        return 1

if __name__ == '__main__':
    sys.exit(main())
