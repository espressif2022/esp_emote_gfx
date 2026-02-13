#!/usr/bin/env python3
"""
自动从 C 头文件生成 RST API 文档

使用方法:
    python docs/scripts/generate_api_docs.py

功能:
    1. 解析头文件中的 Doxygen 注释
    2. 生成对应的 RST 文档
    3. 支持结构体、枚举、函数、宏等
"""

import os
import re
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Optional, Dict

@dataclass
class DocItem:
    """文档项"""
    name: str
    kind: str  # function, struct, enum, typedef, macro
    brief: str = ""
    description: str = ""
    params: List[Dict] = field(default_factory=list)
    returns: str = ""
    code: str = ""
    notes: List[str] = field(default_factory=list)
    examples: List[str] = field(default_factory=list)

class HeaderParser:
    """解析 C 头文件"""
    
    def __init__(self, filepath: str):
        self.filepath = filepath
        with open(filepath, 'r', encoding='utf-8') as f:
            self.content = f.read()
        self.items: List[DocItem] = []
    
    def parse(self) -> List[DocItem]:
        """解析头文件"""
        self._parse_typedefs()
        self._parse_enums()
        self._parse_structs()
        self._parse_macros()
        self._parse_functions()
        return self.items
    
    def _find_comment_before(self, pos: int) -> Optional[str]:
        """查找位置前最近的 Doxygen 注释块"""
        before = self.content[:pos]
        
        # 向前查找，跳过空白
        search_pos = len(before) - 1
        while search_pos >= 0 and before[search_pos] in ' \t\n':
            search_pos -= 1
        
        if search_pos < 0:
            return None
        
        # 检查是否以 */ 结尾（块注释）
        if search_pos >= 1 and before[search_pos-1:search_pos+1] == '*/':
            # 向前找 /**
            start = before.rfind('/**', 0, search_pos)
            if start != -1:
                return before[start:search_pos+1]
        
        return None
    
    def _parse_doxygen_block(self, comment: str) -> Dict:
        """解析单个 Doxygen 注释块"""
        result = {
            'brief': '',
            'params': [],
            'returns': '',
            'notes': [],
            'examples': [],
        }
        
        if not comment:
            return result
        
        # 清理注释标记
        lines = []
        for line in comment.split('\n'):
            # 移除 /**, */, *, //
            line = re.sub(r'^\s*/?\*+/?', '', line)
            line = re.sub(r'\*/$', '', line)
            lines.append(line.strip())
        
        text = '\n'.join(lines).strip()
        
        # 提取 @brief
        match = re.search(r'@brief\s+(.+?)(?=\n\s*@|\n\s*\n|$)', text, re.DOTALL)
        if match:
            result['brief'] = ' '.join(match.group(1).split())
        
        # 提取 @param
        for match in re.finditer(r'@param(?:\[(\w+)\])?\s+(\w+)\s+(.+?)(?=\n\s*@|\n\s*\n|$)', text, re.DOTALL):
            direction = match.group(1) or 'in'
            name = match.group(2)
            desc = ' '.join(match.group(3).split())
            result['params'].append({'name': name, 'direction': direction, 'desc': desc})
        
        # 提取 @return
        match = re.search(r'@returns?\s+(.+?)(?=\n\s*@|\n\s*\n|$)', text, re.DOTALL)
        if match:
            result['returns'] = ' '.join(match.group(1).split())
        
        # 提取 @note
        for match in re.finditer(r'@note\s+(.+?)(?=\n\s*@|\n\s*\n|$)', text, re.DOTALL):
            result['notes'].append(' '.join(match.group(1).split()))
        
        # 提取 @code...@endcode
        for match in re.finditer(r'@code\s*(.+?)@endcode', text, re.DOTALL):
            code = match.group(1).strip()
            # 保持代码缩进
            result['examples'].append(code)
        
        return result
    
    def _parse_typedefs(self):
        """解析简单 typedef"""
        # typedef void *gfx_handle_t;
        pattern = r'typedef\s+(\w+)\s*\*?\s*(\w+_t)\s*;'
        for match in re.finditer(pattern, self.content):
            name = match.group(2)
            comment = self._find_comment_before(match.start())
            parsed = self._parse_doxygen_block(comment)
            
            self.items.append(DocItem(
                name=name,
                kind='typedef',
                brief=parsed['brief'],
                code=match.group(0).strip(),
            ))
        
        # 函数指针 typedef
        pattern = r'typedef\s+(\w+)\s*\(\s*\*\s*(\w+_t)\s*\)\s*\(([^)]*)\)\s*;'
        for match in re.finditer(pattern, self.content):
            name = match.group(2)
            comment = self._find_comment_before(match.start())
            parsed = self._parse_doxygen_block(comment)
            
            self.items.append(DocItem(
                name=name,
                kind='typedef',
                brief=parsed['brief'],
                code=match.group(0).strip(),
            ))
    
    def _parse_enums(self):
        """解析枚举"""
        pattern = r'typedef\s+enum\s*\{([^}]+)\}\s*(\w+)\s*;'
        for match in re.finditer(pattern, self.content):
            name = match.group(2)
            comment = self._find_comment_before(match.start())
            parsed = self._parse_doxygen_block(comment)
            
            self.items.append(DocItem(
                name=name,
                kind='enum',
                brief=parsed['brief'],
                code=match.group(0).strip(),
            ))
    
    def _parse_structs(self):
        """解析结构体"""
        pattern = r'typedef\s+struct\s*\{([\s\S]+?)\}\s*(\w+_t)\s*;'
        for match in re.finditer(pattern, self.content):
            name = match.group(2)
            comment = self._find_comment_before(match.start())
            parsed = self._parse_doxygen_block(comment)
            
            self.items.append(DocItem(
                name=name,
                kind='struct',
                brief=parsed['brief'],
                code=match.group(0).strip(),
            ))
    
    def _parse_macros(self):
        """解析宏定义"""
        pattern = r'#define\s+(\w+)\s*\([^)]*\)[^\n]*(?:\\\n[^\n]*)*'
        for match in re.finditer(pattern, self.content):
            name = match.group(1)
            if name.startswith('_'):
                continue
            
            comment = self._find_comment_before(match.start())
            parsed = self._parse_doxygen_block(comment)
            
            self.items.append(DocItem(
                name=name,
                kind='macro',
                brief=parsed['brief'],
                code=match.group(0).strip(),
            ))
    
    def _parse_functions(self):
        """解析函数声明"""
        # 匹配函数声明：返回类型 函数名(参数); 返回类型与函数名之间允许无空格（如 gfx_touch_t *gfx_touch_add）
        pattern = r'/\*\*[\s\S]*?\*/\s*\n\s*(\w+(?:\s*\*)?)\s*(\w+)\s*\(([^)]*)\)\s*;'
        
        for match in re.finditer(pattern, self.content):
            full_match = match.group(0)
            ret_type = match.group(1).strip()
            name = match.group(2)
            
            # 提取注释部分
            comment_end = full_match.find('*/')
            if comment_end != -1:
                comment = full_match[:comment_end+2]
            else:
                comment = None
            
            parsed = self._parse_doxygen_block(comment)
            
            # 构建函数签名
            func_sig = f"{ret_type} {name}({match.group(3).strip()});"
            
            self.items.append(DocItem(
                name=name,
                kind='function',
                brief=parsed['brief'],
                params=parsed['params'],
                returns=parsed['returns'],
                code=func_sig,
                notes=parsed['notes'],
                examples=parsed['examples'],
            ))


class RstGenerator:
    """生成 RST 文档"""
    
    def __init__(self, module_name: str, title: str):
        self.module_name = module_name
        self.title = title
        self.items: List[DocItem] = []
    
    def add_items(self, items: List[DocItem]):
        self.items.extend(items)
    
    def generate(self) -> str:
        """生成 RST 内容"""
        lines = []
        
        # 标题
        lines.append(self.title)
        lines.append('=' * len(self.title))
        lines.append('')
        
        # 按类型分组
        macros = [i for i in self.items if i.kind == 'macro']
        typedefs = [i for i in self.items if i.kind == 'typedef']
        enums = [i for i in self.items if i.kind == 'enum']
        structs = [i for i in self.items if i.kind == 'struct']
        functions = [i for i in self.items if i.kind == 'function']
        
        # 类型定义
        if typedefs or enums or structs:
            lines.append('Types')
            lines.append('-----')
            lines.append('')
            
            for item in typedefs:
                lines.extend(self._format_type(item))
            
            for item in enums:
                lines.extend(self._format_type(item))
            
            for item in structs:
                lines.extend(self._format_type(item))
        
        # 宏
        if macros:
            lines.append('Macros')
            lines.append('------')
            lines.append('')
            
            for item in macros:
                lines.extend(self._format_macro(item))
        
        # 函数
        if functions:
            lines.append('Functions')
            lines.append('---------')
            lines.append('')
            
            for item in functions:
                lines.extend(self._format_function(item))
        
        return '\n'.join(lines)
    
    def _format_type(self, item: DocItem) -> List[str]:
        lines = []
        lines.append(f'{item.name}')
        lines.append('~' * len(item.name))
        lines.append('')
        if item.brief:
            lines.append(item.brief)
            lines.append('')
        lines.append('.. code-block:: c')
        lines.append('')
        for code_line in item.code.split('\n'):
            lines.append(f'   {code_line}')
        lines.append('')
        return lines
    
    def _format_macro(self, item: DocItem) -> List[str]:
        lines = []
        lines.append(f'{item.name}()')
        lines.append('~' * (len(item.name) + 2))
        lines.append('')
        if item.brief:
            lines.append(item.brief)
            lines.append('')
        lines.append('.. code-block:: c')
        lines.append('')
        for code_line in item.code.split('\n'):
            lines.append(f'   {code_line}')
        lines.append('')
        return lines
    
    def _format_function(self, item: DocItem) -> List[str]:
        lines = []
        lines.append(f'{item.name}()')
        lines.append('~' * (len(item.name) + 2))
        lines.append('')
        if item.brief:
            lines.append(item.brief)
            lines.append('')
        
        lines.append('.. code-block:: c')
        lines.append('')
        lines.append(f'   {item.code}')
        lines.append('')
        
        if item.params:
            lines.append('**Parameters:**')
            lines.append('')
            for param in item.params:
                lines.append(f"* ``{param['name']}`` - {param['desc']}")
            lines.append('')
        
        if item.returns:
            lines.append('**Returns:**')
            lines.append('')
            lines.append(f'* {item.returns}')
            lines.append('')
        
        for note in item.notes:
            lines.append('**Note:**')
            lines.append('')
            lines.append(note)
            lines.append('')
        
        for example in item.examples:
            lines.append('**Example:**')
            lines.append('')
            lines.append('.. code-block:: c')
            lines.append('')
            for code_line in example.split('\n'):
                lines.append(f'   {code_line}')
            lines.append('')
        
        return lines


# 定义要处理的头文件映射
HEADER_MAPPING = {
    'include/core/gfx_core.h': ('api/core/gfx_core.rst', 'Core System (gfx_core)'),
    'include/core/gfx_types.h': ('api/core/gfx_types.rst', 'Type (gfx_types)'),
    'include/core/gfx_disp.h': ('api/core/gfx_disp.rst', 'Display (gfx_disp)'),
    'include/core/gfx_touch.h': ('api/core/gfx_touch.rst', 'Touch (gfx_touch)'),
    'include/core/gfx_obj.h': ('api/core/gfx_obj.rst', 'Object (gfx_obj)'),
    'include/core/gfx_timer.h': ('api/core/gfx_timer.rst', 'Timer (gfx_timer)'),
    'include/widget/gfx_img.h': ('api/widgets/gfx_img.rst', 'Image (gfx_img)'),
    'include/widget/gfx_label.h': ('api/widgets/gfx_label.rst', 'Label (gfx_label)'),
    'include/widget/gfx_anim.h': ('api/widgets/gfx_anim.rst', 'Animation (gfx_anim)'),
    'include/widget/gfx_qrcode.h': ('api/widgets/gfx_qrcode.rst', 'QR Code (gfx_qrcode)'),
}

# 各 API 子目录的 index 配置：(子目录名, 页面标题, 引言段落, “模块列表”小节标题)
INDEX_CONFIG = [
    (
        'core',
        'Core API Reference',
        'The core API provides the foundation for the graphics framework, including initialization, object management, and basic types.',
        'Core Modules',
    ),
    (
        'widgets',
        'Widget API Reference',
        'The widget API provides specialized functionality for different types of graphical elements.',
        'Widget Modules',
    ),
]


def md_to_rst_line(line: str) -> str:
    """Convert a single Markdown line to RST (headers and list items)."""
    s = line.rstrip()
    # [text](url) -> `text <url>`_
    s = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'`\1 <\2>`_', s)
    if s.startswith('# '):
        title = s[2:].strip()
        return title + '\n' + '=' * len(title)
    if s.startswith('## '):
        title = s[3:].strip()
        return title + '\n' + '-' * len(title)
    if s.startswith('### '):
        title = s[4:].strip()
        return title + '\n' + '~' * len(title)
    if s.startswith('- ') and not s.startswith('- [ ]'):
        return '* ' + s[2:]
    return s


def generate_changelog_rst(repo_root: Path, docs_dir: Path) -> bool:
    """Read CHANGELOG.md and write docs/changelog.rst (MD to RST conversion)."""
    md_path = repo_root / 'CHANGELOG.md'
    rst_path = docs_dir / 'changelog.rst'
    if not md_path.is_file():
        return False
    with open(md_path, 'r', encoding='utf-8') as f:
        md_lines = f.readlines()
    rst_lines = []
    for line in md_lines:
        if not line.strip():
            rst_lines.append('')
            continue
        rst_lines.append(md_to_rst_line(line))
    rst_path.parent.mkdir(parents=True, exist_ok=True)
    with open(rst_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(rst_lines))
        f.write('\n')
    return True


def update_index_rst(api_dir: Path, subdir: str, title: str, intro: str, modules_heading: str) -> bool:
    """
    扫描 api/<subdir>/ 下的 .rst 文件（排除 index.rst），生成 toctree 和模块列表，写入 index.rst。
    每个条目的描述取自对应 .rst 文件的第一行（标题行）。
    返回是否写入了文件。
    """
    index_dir = api_dir / subdir
    if not index_dir.is_dir():
        return False

    rst_files = sorted(
        f.stem for f in index_dir.glob('*.rst') if f.name != 'index.rst'
    )
    if not rst_files:
        return False

    # 从每个 .rst 读取第一行作为描述
    descriptions = {}
    for stem in rst_files:
        rst_path = index_dir / f'{stem}.rst'
        try:
            with open(rst_path, 'r', encoding='utf-8') as f:
                first = f.readline()
            descriptions[stem] = first.strip() if first else stem
        except OSError:
            descriptions[stem] = stem

    lines = [
        title,
        '=' * len(title),
        '',
        intro,
        '',
        '.. toctree::',
        '   :maxdepth: 2',
        '',
    ]
    for stem in rst_files:
        lines.append(f'   {stem}')
    lines.extend(['', modules_heading, '-' * len(modules_heading), ''])
    for stem in rst_files:
        desc = descriptions[stem]
        lines.append(f'* :doc:`{stem}` - {desc}')
    lines.append('')

    index_path = index_dir / 'index.rst'
    with open(index_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    return True


def main():
    parser = argparse.ArgumentParser(description='从 C 头文件生成 RST 文档')
    parser.add_argument('--output-dir', '-o', default='docs',
                       help='输出目录 (默认: docs)')
    parser.add_argument('--dry-run', '-n', action='store_true',
                       help='只显示将要生成的文件，不实际写入')
    parser.add_argument('--header', '-H', 
                       help='只处理指定的头文件')
    args = parser.parse_args()
    
    # 确定项目根目录
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent.parent
    
    print("=" * 50)
    print("  ESP Emote GFX API 文档生成器")
    print("=" * 50)
    print()
    
    generated = 0
    for header_path, (rst_path, title) in HEADER_MAPPING.items():
        if args.header and args.header not in header_path:
            continue
        
        full_header_path = repo_root / header_path
        full_rst_path = repo_root / args.output_dir / rst_path
        
        if not full_header_path.exists():
            print(f"⚠ 跳过: {header_path} (文件不存在)")
            continue
        
        print(f"处理: {header_path}")
        print(f"  → {rst_path}")
        
        # 解析头文件
        parser_obj = HeaderParser(str(full_header_path))
        items = parser_obj.parse()
        
        # 统计各类型数量
        counts = {}
        for item in items:
            counts[item.kind] = counts.get(item.kind, 0) + 1
        
        count_str = ', '.join(f"{v} {k}" for k, v in counts.items())
        print(f"  发现: {count_str}")
        
        # 生成 RST
        generator = RstGenerator(header_path, title)
        generator.add_items(items)
        rst_content = generator.generate()
        
        if args.dry_run:
            print(f"  [dry-run] 将生成 {len(rst_content)} 字节")
        else:
            full_rst_path.parent.mkdir(parents=True, exist_ok=True)
            with open(full_rst_path, 'w', encoding='utf-8') as f:
                f.write(rst_content)
            print(f"  ✓ 已生成")
        
        generated += 1
        print()

    # 根据 api/core 和 api/widgets 下的 .rst 自动更新 index.rst
    api_dir = repo_root / args.output_dir / 'api'
    if not args.dry_run and api_dir.is_dir():
        for subdir, title, intro, modules_heading in INDEX_CONFIG:
            index_dir = api_dir / subdir
            if index_dir.is_dir():
                if update_index_rst(api_dir, subdir, title, intro, modules_heading):
                    print(f"更新索引: api/{subdir}/index.rst")
                print()

    # 从 CHANGELOG.md 生成 docs/changelog.rst
    docs_dir = repo_root / args.output_dir
    if not args.dry_run:
        if generate_changelog_rst(repo_root, docs_dir):
            print("更新: changelog.rst (来自 CHANGELOG.md)")
        print()

    print("=" * 50)
    print(f"完成! 共处理 {generated} 个文件")
    print("=" * 50)


if __name__ == '__main__':
    main()
