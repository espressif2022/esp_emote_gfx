#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
PNG to RGB565/RGB565A8 C file converter
Converts PNG images to RGB565 or RGB565A8 format with optional byte swapping
RGB565: Pure RGB565 format without alpha channel
RGB565A8: RGB565 with separate alpha channel
Supports both C file and binary output formats
Can process single files or batch process all PNG files in a directory
"""

import argparse
import os
import sys
from PIL import Image
import re
import struct
import glob

EMBED_BLOCK_BEGIN_TEMPLATE = '/* BEGIN GENERATED IMAGE ASSET: {symbol} */'
EMBED_BLOCK_END_TEMPLATE = '/* END GENERATED IMAGE ASSET: {symbol} */'

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565"""
    r = (r >> 3) & 0x1F
    g = (g >> 2) & 0x3F
    b = (b >> 3) & 0x1F
    return (r << 11) | (g << 5) | b

def rgb565_to_bytes(rgb565, swap16=False):
    """Convert RGB565 to bytes, optionally swapping byte order"""
    high_byte = (rgb565 >> 8) & 0xFF
    low_byte = rgb565 & 0xFF

    if swap16:
        return [low_byte, high_byte]
    else:
        return [high_byte, low_byte]

def format_array(data, indent=4, per_line=130):
    """Format data as C array with proper indentation and line breaks"""
    lines = []
    for i in range(0, len(data), per_line):
        line = ', '.join(f'0x{b:02x}' for b in data[i:i + per_line])
        lines.append(' ' * indent + line + ',')
    return '\n'.join(lines)

def sanitize_var_name(base_name):
    """Convert file base name to a valid C identifier."""
    var_name = re.sub(r'[^a-zA-Z0-9_]', '_', base_name)
    if var_name and var_name[0].isdigit():
        var_name = 'img_' + var_name
    return var_name

def convert_image_data(image_path, swap16=False, use_alpha=True):
    """Convert image into byte arrays and metadata."""
    try:
        img = Image.open(image_path)
        if img.mode != 'RGBA':
            img = img.convert('RGBA')
    except Exception as e:
        print(f'Error opening image {image_path}: {e}')
        return None

    width, height = img.size
    pixels = list(img.getdata())

    rgb565_data = []
    alpha_data = []

    for pixel in pixels:
        r, g, b, a = pixel
        rgb565 = rgb888_to_rgb565(r, g, b)
        rgb565_bytes = rgb565_to_bytes(rgb565, swap16)
        rgb565_data.extend(rgb565_bytes)

        if use_alpha:
            alpha_data.append(a)

    if use_alpha:
        final_data = rgb565_data + alpha_data
        color_format = 'GFX_COLOR_FORMAT_RGB565A8'
        format_name = 'RGB565A8'
    else:
        final_data = rgb565_data
        color_format = 'GFX_COLOR_FORMAT_RGB565'
        format_name = 'RGB565'

    return {
        'width': width,
        'height': height,
        'rgb565_data': rgb565_data,
        'alpha_data': alpha_data,
        'final_data': final_data,
        'color_format': color_format,
        'format_name': format_name,
    }

def build_c_content(var_name, image_info):
    """Build C source content for an image descriptor."""
    return f"""#include "gfx.h"

const uint8_t {var_name}_map[] = {{
{format_array(image_info['final_data'])}
}};

const gfx_image_dsc_t {var_name} = {{
    .header.cf = {image_info['color_format']},
    .header.magic = C_ARRAY_HEADER_MAGIC,
    .header.w = {image_info['width']},
    .header.h = {image_info['height']},
    .data_size = {len(image_info['final_data'])},
    .data = {var_name}_map,
}};
"""

def build_embedded_inc_block(var_name, image_info):
    """Build an embeddable .inc block containing image data."""
    block_begin = EMBED_BLOCK_BEGIN_TEMPLATE.format(symbol=var_name)
    block_end = EMBED_BLOCK_END_TEMPLATE.format(symbol=var_name)
    return f"""{block_begin}
#include "gfx.h"

#define LOBSTER_USE_EXPORTED_BG 1
#define LOBSTER_EXPORTED_BG_SYMBOL {var_name}

static const uint8_t {var_name}_map[] = {{
{format_array(image_info['final_data'])}
}};

static const gfx_image_dsc_t {var_name} = {{
    .header.cf = {image_info['color_format']},
    .header.magic = C_ARRAY_HEADER_MAGIC,
    .header.w = {image_info['width']},
    .header.h = {image_info['height']},
    .data_size = {len(image_info['final_data'])},
    .data = {var_name}_map,
}};
{block_end}
"""

def upsert_embedded_inc_block(inc_content, block_content, var_name):
    """Replace an existing generated image block or append a new one."""
    begin_marker = re.escape(EMBED_BLOCK_BEGIN_TEMPLATE.format(symbol=var_name))
    end_marker = re.escape(EMBED_BLOCK_END_TEMPLATE.format(symbol=var_name))
    pattern = re.compile(rf'\n?{begin_marker}.*?{end_marker}\n?', re.DOTALL)

    if pattern.search(inc_content):
        updated = pattern.sub(f'\n{block_content}\n', inc_content).rstrip() + '\n'
    else:
        updated = inc_content.rstrip() + '\n\n' + block_content + '\n'

    return updated

def generate_embedded_inc_file(image_path, inc_path, var_name, swap16=False, use_alpha=True):
    """Embed generated image data into an existing .inc file."""
    image_info = convert_image_data(image_path, swap16, use_alpha)
    if image_info is None:
        return False

    try:
        with open(inc_path, 'r') as f:
            inc_content = f.read()
    except Exception as e:
        print(f'Error reading inc file {inc_path}: {e}')
        return False

    block_content = build_embedded_inc_block(var_name, image_info)
    merged_content = upsert_embedded_inc_block(inc_content, block_content, var_name)

    try:
        with open(inc_path, 'w') as f:
            f.write(merged_content)
        print(f'Successfully embedded image into {inc_path}')
        print(f'Format: {image_info["format_name"]}')
        print(f'Image size: {image_info["width"]}x{image_info["height"]}')
        print(f'Total data size: {len(image_info["final_data"])} bytes')
        print(f'RGB565 data: {len(image_info["rgb565_data"])} bytes ({image_info["width"] * image_info["height"] * 2} bytes)')
        if use_alpha:
            print(f'Alpha data: {len(image_info["alpha_data"])} bytes ({image_info["width"] * image_info["height"]} bytes)')
        print(f"Swap16: {'enabled' if swap16 else 'disabled'}")
        print(f'Embedded symbol: {var_name}')
        return True
    except Exception as e:
        print(f'Error writing inc file {inc_path}: {e}')
        return False

def generate_c_file(image_path, output_path, var_name, swap16=False, use_alpha=True):
    """Generate C file from PNG image

    Args:
        image_path: Input PNG file path
        output_path: Output C file path
        var_name: Variable name for the C array
        swap16: Enable byte swapping for RGB565
        use_alpha: True for RGB565A8, False for RGB565
    """

    image_info = convert_image_data(image_path, swap16, use_alpha)
    if image_info is None:
        return False

    c_content = build_c_content(var_name, image_info)

    # Write to file
    try:
        with open(output_path, 'w') as f:
            f.write(c_content)
        print(f'Successfully generated {output_path}')
        print(f'Format: {image_info["format_name"]}')
        print(f'Image size: {image_info["width"]}x{image_info["height"]}')
        print(f'Total data size: {len(image_info["final_data"])} bytes')
        print(f'RGB565 data: {len(image_info["rgb565_data"])} bytes ({image_info["width"] * image_info["height"] * 2} bytes)')
        if use_alpha:
            print(f'Alpha data: {len(image_info["alpha_data"])} bytes ({image_info["width"] * image_info["height"]} bytes)')
        print(f"Swap16: {'enabled' if swap16 else 'disabled'}")
        return True
    except Exception as e:
        print(f'Error writing file {output_path}: {e}')
        return False

def generate_bin_file(image_path, output_path, swap16=False, use_alpha=True):
    """Generate binary file from PNG image with header compatible with gfx_image_header_t structure

    Args:
        image_path: Input PNG file path
        output_path: Output binary file path
        swap16: Enable byte swapping for RGB565
        use_alpha: True for RGB565A8, False for RGB565
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

    # Convert to RGB565 format
    rgb565_data = []
    alpha_data = []

    for pixel in pixels:
        r, g, b, a = pixel

        # Convert RGB to RGB565
        rgb565 = rgb888_to_rgb565(r, g, b)

        # Add RGB565 bytes (2 bytes) to RGB565 array
        rgb565_bytes = rgb565_to_bytes(rgb565, swap16)
        rgb565_data.extend(rgb565_bytes)

        # Add Alpha byte (1 byte) to Alpha array if needed
        if use_alpha:
            alpha_data.append(a)

    # Combine data based on format
    if use_alpha:
        # RGB565A8: RGB565 first, then Alpha
        final_data = rgb565_data + alpha_data
        cf = 0x0A  # GFX_COLOR_FORMAT_RGB565A8
        stride = width * 2  # Stride is only for RGB565 data
        format_name = 'RGB565A8'
    else:
        # RGB565: Only RGB565 data
        final_data = rgb565_data
        cf = 0x04  # GFX_COLOR_FORMAT_RGB565
        stride = width * 2
        format_name = 'RGB565'

    # Create gfx_image_header_t structure (12 bytes total)
    magic = 0x19  # C_ARRAY_HEADER_MAGIC
    flags = 0x0000  # No special flags
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
        print(f'RGB565 data: {len(rgb565_data)} bytes ({width * height * 2} bytes)')
        if use_alpha:
            print(f'Alpha data: {len(alpha_data)} bytes ({width * height} bytes)')
        print(f'Stride: {stride} bytes per row')
        print(f'Data offset: 12 bytes')
        print(f'Total file size: {len(header) + len(final_data)} bytes')
        print(f"Swap16: {'enabled' if swap16 else 'disabled'}")
        print(f'Header layout: magic=0x{magic:02x}, cf=0x{cf:02x}, flags=0x{flags:04x}')
        return True
    except Exception as e:
        print(f'Error writing file {output_path}: {e}')
        return False

def process_single_file(input_file, output_dir, bin_format, swap16, use_alpha):
    """Process a single PNG file"""
    # Determine output path and variable name from input filename
    base_name = os.path.splitext(os.path.basename(input_file))[0]

    if bin_format:
        # Output binary file
        output_path = os.path.join(output_dir, f'{base_name}.bin')
        return generate_bin_file(input_file, output_path, swap16, use_alpha)
    else:
        # Output C file
        output_path = os.path.join(output_dir, f'{base_name}.c')
        var_name = sanitize_var_name(base_name)
        return generate_c_file(input_file, output_path, var_name, swap16, use_alpha)

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
        description='Convert PNG to RGB565 or RGB565A8 format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert to RGB565A8 (with alpha) C file
  %(prog)s image.png

  # Convert to RGB565 (without alpha) C file
  %(prog)s image.png --format rgb565

  # Convert to binary format with byte swapping
  %(prog)s image.png --bin --swap16

  # Batch convert all PNG files in directory
  %(prog)s images/ --output output/
        """
    )
    parser.add_argument('input', help='Input PNG file path or directory path')
    parser.add_argument('--output', '-o', help='Output directory (default: current directory)')
    parser.add_argument('--bin', action='store_true', help='Output binary format instead of C file')
    parser.add_argument('--swap16', action='store_true', help='Enable byte swapping for RGB565')
    parser.add_argument('--format', '-f', choices=['rgb565', 'rgb565a8'], default='rgb565a8',
                        help='Output format: rgb565 (no alpha) or rgb565a8 (with alpha, default)')
    parser.add_argument('--embed-into-inc', help='Embed image data into an existing .inc file instead of writing a standalone .c file')
    parser.add_argument('--symbol', help='Override generated C symbol name when used with C or embedded .inc output')

    args = parser.parse_args()

    # Validate input path
    if not os.path.exists(args.input):
        print(f"Error: Input path '{args.input}' does not exist")
        return 1

    # Set output directory
    output_dir = args.output if args.output else '.'
    if not args.embed_into_inc and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Determine if alpha channel should be included
    use_alpha = (args.format == 'rgb565a8')

    # Find all PNG files
    png_files = find_png_files(args.input)

    if not png_files:
        print(f"No PNG files found in '{args.input}'")
        return 1

    print(f'Found {len(png_files)} PNG file(s) to process:')
    for png_file in png_files:
        print(f'  - {png_file}')
    print(f'Output format: {args.format.upper()}')
    if args.embed_into_inc:
        print(f'Output type: Embedded inc ({args.embed_into_inc})')
    else:
        print(f'Output type: {"Binary" if args.bin else "C file"}')
    print(f'Byte swap: {"Enabled" if args.swap16 else "Disabled"}')
    print()

    if args.embed_into_inc:
        if args.bin:
            print('Error: --embed-into-inc cannot be used together with --bin')
            return 1
        if len(png_files) != 1:
            print('Error: --embed-into-inc only supports a single input PNG file')
            return 1

        base_name = os.path.splitext(os.path.basename(png_files[0]))[0]
        var_name = args.symbol if args.symbol else f's_{sanitize_var_name(base_name)}_embedded'
        if generate_embedded_inc_file(png_files[0], args.embed_into_inc, var_name, args.swap16, use_alpha):
            print('\nProcessing complete: 1/1 files processed successfully')
            return 0
        return 1

    # Process each PNG file
    success_count = 0
    for png_file in png_files:
        print(f'Processing: {png_file}')
        if args.symbol and len(png_files) > 1:
            print('Error: --symbol only supports single-file conversion')
            return 1

        if args.symbol and not args.bin:
            output_path = os.path.join(output_dir, f'{os.path.splitext(os.path.basename(png_file))[0]}.c')
            ok = generate_c_file(png_file, output_path, args.symbol, args.swap16, use_alpha)
        else:
            ok = process_single_file(png_file, output_dir, args.bin, args.swap16, use_alpha)

        if ok:
            success_count += 1
        print()  # Add blank line between files

    print(f'Processing complete: {success_count}/{len(png_files)} files processed successfully')

    if success_count == len(png_files):
        return 0
    else:
        return 1

if __name__ == '__main__':
    sys.exit(main())
