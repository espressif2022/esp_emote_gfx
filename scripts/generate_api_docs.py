#!/usr/bin/env python3
"""
Script to automatically extract API functions from header files
and generate API documentation table for README.md
"""

import os
import re
import glob
from pathlib import Path

def extract_functions_from_header(file_path):
    """Extract function declarations from a header file"""
    functions = []
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Remove comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    content = re.sub(r'//.*', '', content)
    
    # Find function declarations
    # Pattern to match function declarations
    pattern = r'^\s*([a-zA-Z_][a-zA-Z0-9_*\s]*?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*;'
    matches = re.findall(pattern, content, re.MULTILINE)
    
    for match in matches:
        return_type, func_name, params = match
        return_type = return_type.strip()
        params = params.strip()
        
        # Skip typedef function pointers
        if 'typedef' in return_type:
            continue
            
        # Clean up return type
        return_type = re.sub(r'\s+', ' ', return_type)
        
        # Clean up parameters
        params = re.sub(r'\s+', ' ', params)
        
        full_signature = f"{return_type} {func_name}({params})"
        functions.append({
            'name': func_name,
            'signature': full_signature,
            'file': os.path.basename(file_path)
        })
    
    return functions

def categorize_functions(functions):
    """Categorize functions by their purpose"""
    categories = {
        'Core Functions': [],
        'Object Management': [],
        'Label Widget': [],
        'Image Widget': [],
        'Animation Widget': [],
        'Timer System': [],
        'Utilities': []
    }
    
    for func in functions:
        name = func['name']
        if name.startswith('gfx_emote_'):
            categories['Core Functions'].append(func)
        elif name.startswith('gfx_obj_'):
            categories['Object Management'].append(func)
        elif name.startswith('gfx_label_'):
            categories['Label Widget'].append(func)
        elif name.startswith('gfx_img_'):
            categories['Image Widget'].append(func)
        elif name.startswith('gfx_anim_'):
            categories['Animation Widget'].append(func)
        elif name.startswith('gfx_timer_'):
            categories['Timer System'].append(func)
        else:
            categories['Utilities'].append(func)
    
    return categories

def generate_markdown_table(functions):
    """Generate markdown table for functions"""
    if not functions:
        return ""
    
    table = "| Function | Description |\n"
    table += "|----------|-------------|\n"
    
    for func in functions:
        # Simple description based on function name
        desc = generate_description(func['name'])
        table += f"| `{func['signature']}` | {desc} |\n"
    
    return table

def generate_description(func_name):
    """Generate simple description based on function name"""
    descriptions = {
        'gfx_emote_init': 'Initialize graphics context',
        'gfx_emote_deinit': 'Deinitialize graphics context',
        'gfx_emote_flush_ready': 'Check if flush is ready',
        'gfx_emote_get_user_data': 'Get user data from graphics context',
        'gfx_emote_get_screen_size': 'Get screen dimensions',
        'gfx_emote_lock': 'Lock render mutex',
        'gfx_emote_unlock': 'Unlock render mutex',
        'gfx_emote_set_bg_color': 'Set default background color',
        'gfx_emote_is_flushing_last': 'Check if flushing last block',
        'gfx_obj_set_pos': 'Set object position',
        'gfx_obj_set_size': 'Set object size',
        'gfx_obj_align': 'Align object relative to screen',
        'gfx_obj_set_visible': 'Set object visibility',
        'gfx_obj_get_visible': 'Get object visibility',
        'gfx_obj_get_pos': 'Get object position',
        'gfx_obj_get_size': 'Get object size',
        'gfx_obj_delete': 'Delete object',
        'gfx_label_create': 'Create a label object',
        'gfx_label_new_font': 'Create new FreeType font',
        'gfx_label_delete_font': 'Delete font and free resources',
        'gfx_label_set_text': 'Set label text',
        'gfx_label_set_text_fmt': 'Set formatted text',
        'gfx_label_set_color': 'Set text color',
        'gfx_label_set_bg_color': 'Set background color',
        'gfx_label_set_bg_enable': 'Enable/disable background',
        'gfx_label_set_opa': 'Set opacity (0-255)',
        'gfx_label_set_font': 'Set font',
        'gfx_label_set_text_align': 'Set text alignment',
        'gfx_label_set_long_mode': 'Set long text handling mode',
        'gfx_label_set_line_spacing': 'Set line spacing',
        'gfx_label_set_scroll_speed': 'Set scrolling speed',
        'gfx_label_set_scroll_loop': 'Set continuous scrolling',
        'gfx_img_create': 'Create an image object',
        'gfx_img_set_src': 'Set image source data (RGB565A8 format)',
        'gfx_anim_create': 'Create animation object',
        'gfx_anim_set_src': 'Set animation source data',
        'gfx_anim_set_segment': 'Configure animation segment',
        'gfx_anim_start': 'Start animation playback',
        'gfx_anim_stop': 'Stop animation playback',
        'gfx_anim_set_mirror': 'Set mirror display',
        'gfx_anim_set_auto_mirror': 'Set auto mirror alignment',
        'gfx_timer_create': 'Create a new timer',
        'gfx_timer_delete': 'Delete a timer',
        'gfx_timer_pause': 'Pause timer',
        'gfx_timer_resume': 'Resume timer',
        'gfx_timer_set_repeat_count': 'Set repeat count (-1 for infinite)',
        'gfx_timer_set_period': 'Set timer period (ms)',
        'gfx_timer_reset': 'Reset timer',
        'gfx_timer_tick_get': 'Get current system tick',
        'gfx_timer_tick_elaps': 'Calculate elapsed time',
        'gfx_timer_get_actual_fps': 'Get actual FPS',
        'gfx_color_hex': 'Convert hex color to gfx_color_t',
    }
    
    return descriptions.get(func_name, 'Function description')

def main():
    """Main function to generate API documentation"""
    script_dir = Path(__file__).parent
    include_dir = script_dir.parent / 'include'
    
    # Find all header files
    header_files = []
    for pattern in ['**/*.h']:
        header_files.extend(glob.glob(str(include_dir / pattern), recursive=True))
    
    # Extract functions from all headers
    all_functions = []
    for header_file in header_files:
        functions = extract_functions_from_header(header_file)
        all_functions.extend(functions)
    
    # Categorize functions
    categories = categorize_functions(all_functions)
    
    # Generate markdown
    output = "## API Reference\n\n"
    
    for category, functions in categories.items():
        if functions:
            output += f"### {category}\n\n"
            output += generate_markdown_table(functions)
            output += "\n"
    
    print(output)
    
    # Optionally write to file
    output_file = script_dir.parent / 'API_REFERENCE.md'
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(output)
    
    print(f"\nAPI reference written to: {output_file}")

if __name__ == '__main__':
    main()
