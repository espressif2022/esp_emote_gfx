#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Package-side tools for the GSP demo scene.

The C exporter owns only the author scene -> raw .gsp compile step. This script
derives review/runtime side artifacts from that package:

    home.gsp -> home.inc
             -> home.manifest.md
             -> home_preview.bmp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct


GSP_MAGIC = 0x31505347
GSP_HEADER_SIZE = 56
GSP_OBJ_SIZE = 64
GSP_BLOB_SIZE = 20
GSP_ACTION_SIZE = 24
GSP_NO_PARENT = 0xFFFF
GSP_ACT_NO_TARGET = 0xFFFF

GSP_OBJ_CONTAINER = 1
GSP_OBJ_LABEL = 2
GSP_OBJ_BUTTON = 3
GSP_OBJ_IMAGE = 4
GSP_OBJ_LIST = 5
GSP_OBJ_WHEEL = 6
GSP_OBJ_LAYER = 7

GSP_CODEC_STORE = 0
GSP_CODEC_RLE16 = 1

GSP_F_TEXT = 1 << 0
GSP_F_FG_COLOR = 1 << 1
GSP_F_BG_COLOR = 1 << 2
GSP_F_BORDER = 1 << 3
GSP_F_RADIUS = 1 << 4
GSP_F_CALLBACK = 1 << 5
GSP_F_IMAGE = 1 << 6
GSP_F_NAME = 1 << 7
GSP_F_HIDDEN = 1 << 8
GSP_F_OPACITY = 1 << 9
GSP_F_ALIGN = 1 << 10
GSP_F_PARAMS = 1 << 11

HOME_FONT_CJK_REGULAR_PATH = 'fonts/NotoSansCJK-Regular.ttc'
HOME_FONTS = [
    {'id': 0, 'family': 'NotoSansCJK', 'path': HOME_FONT_CJK_REGULAR_PATH, 'size': 22, 'weight': 400, 'style': 0},
    {'id': 1, 'family': 'NotoSansCJK', 'path': HOME_FONT_CJK_REGULAR_PATH, 'size': 17, 'weight': 400, 'style': 0},
    {'id': 2, 'family': 'NotoSansCJK', 'path': HOME_FONT_CJK_REGULAR_PATH, 'size': 20, 'weight': 400, 'style': 0},
    {'id': 3, 'family': 'NotoSansCJK', 'path': HOME_FONT_CJK_REGULAR_PATH, 'size': 21, 'weight': 700, 'style': 0},
]


TYPE_NAMES = {
    GSP_OBJ_CONTAINER: 'container',
    GSP_OBJ_LABEL: 'label',
    GSP_OBJ_BUTTON: 'button',
    GSP_OBJ_IMAGE: 'image',
    GSP_OBJ_LIST: 'list',
    GSP_OBJ_WHEEL: 'wheel',
    GSP_OBJ_LAYER: 'layer',
}

EVENT_NAMES = {
    1: 'click',
    2: 'press',
    3: 'release',
    4: 'long',
    5: 'value',
}

ACTION_NAMES = {
    1: 'show',
    2: 'hide',
    3: 'toggle',
    4: 'set_text',
    5: 'set_bg_color',
    6: 'set_opacity',
    7: 'call',
    8: 'goto',
    9: 'back',
}

FLAG_NAMES = [
    (GSP_F_TEXT, 'TEXT'),
    (GSP_F_FG_COLOR, 'FG_COLOR'),
    (GSP_F_BG_COLOR, 'BG_COLOR'),
    (GSP_F_BORDER, 'BORDER'),
    (GSP_F_RADIUS, 'RADIUS'),
    (GSP_F_CALLBACK, 'CALLBACK'),
    (GSP_F_IMAGE, 'IMAGE'),
    (GSP_F_NAME, 'NAME'),
    (GSP_F_HIDDEN, 'HIDDEN'),
    (GSP_F_OPACITY, 'OPACITY'),
    (GSP_F_ALIGN, 'ALIGN'),
    (GSP_F_PARAMS, 'PARAMS'),
]


@dataclass
class Header:
    version: int
    screen_w: int
    screen_h: int
    screen_bg: int
    obj_count: int
    obj_off: int
    str_off: int
    blob_count: int
    blob_off: int
    total_size: int
    crc32: int
    action_count: int
    action_off: int


@dataclass
class Obj:
    idx: int
    type: int
    parent: int
    x: int
    y: int
    w: int
    h: int
    flags: int
    fg: int
    bg: int
    border: int
    border_width: int
    radius: int
    text_off: int
    callback_off: int
    name_off: int
    blob_idx: int
    params_off: int
    params_len: int
    opacity: int
    text_align: int
    font_id: int
    bind_id: int


@dataclass
class Blob:
    idx: int
    w: int
    h: int
    cf: int
    codec: int
    stride: int
    raw_size: int
    comp_size: int
    data_off: int


@dataclass
class Action:
    idx: int
    src: int
    event: int
    action: int
    target_idx: int
    target_name_off: int
    param_off: int
    param_len: int
    flags: int
    arg: int


def u16(buf: bytes, off: int) -> int:
    return struct.unpack_from('<H', buf, off)[0]


def i16(buf: bytes, off: int) -> int:
    return struct.unpack_from('<h', buf, off)[0]


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from('<I', buf, off)[0]


def read_cstr(buf: bytes, off: int) -> str | None:
    if off == 0 or off >= len(buf):
        return None
    end = buf.find(b'\0', off)
    if end < 0:
        return None
    return buf[off:end].decode('utf-8', errors='replace')


def md_escape(text: str) -> str:
    return text.replace('|', '\\|').replace('\n', '\\n')


def parse_header(pkg: bytes) -> Header:
    if len(pkg) < GSP_HEADER_SIZE:
        raise ValueError('package is shorter than GSP header')
    magic = u32(pkg, 0)
    if magic != GSP_MAGIC:
        raise ValueError(f'bad magic 0x{magic:08X}')
    total = u32(pkg, 36)
    if total != len(pkg):
        raise ValueError(f'total_size={total} does not match file size={len(pkg)}')
    return Header(
        version=u32(pkg, 4),
        screen_w=u16(pkg, 8),
        screen_h=u16(pkg, 10),
        screen_bg=u32(pkg, 12),
        obj_count=u32(pkg, 16),
        obj_off=u32(pkg, 20),
        str_off=u32(pkg, 24),
        blob_count=u32(pkg, 28),
        blob_off=u32(pkg, 32),
        total_size=total,
        crc32=u32(pkg, 40),
        action_count=u32(pkg, 44),
        action_off=u32(pkg, 48),
    )


def parse_objects(pkg: bytes, h: Header) -> list[Obj]:
    out: list[Obj] = []
    for idx in range(h.obj_count):
        off = h.obj_off + idx * GSP_OBJ_SIZE
        out.append(
            Obj(
                idx=idx,
                type=u16(pkg, off + 0),
                parent=u16(pkg, off + 2),
                x=i16(pkg, off + 4),
                y=i16(pkg, off + 6),
                w=u16(pkg, off + 8),
                h=u16(pkg, off + 10),
                flags=u32(pkg, off + 12),
                fg=u32(pkg, off + 16),
                bg=u32(pkg, off + 20),
                border=u32(pkg, off + 24),
                border_width=u16(pkg, off + 28),
                radius=u16(pkg, off + 30),
                text_off=u32(pkg, off + 32),
                callback_off=u32(pkg, off + 36),
                name_off=u32(pkg, off + 40),
                blob_idx=u32(pkg, off + 44),
                params_off=u32(pkg, off + 48),
                params_len=u16(pkg, off + 52),
                opacity=pkg[off + 54],
                text_align=pkg[off + 55],
                font_id=u16(pkg, off + 56),
                bind_id=u16(pkg, off + 58),
            )
        )
    return out


def parse_blobs(pkg: bytes, h: Header) -> list[Blob]:
    out: list[Blob] = []
    for idx in range(h.blob_count):
        off = h.blob_off + idx * GSP_BLOB_SIZE
        out.append(
            Blob(
                idx=idx,
                w=u16(pkg, off + 0),
                h=u16(pkg, off + 2),
                cf=pkg[off + 4],
                codec=pkg[off + 5],
                stride=u16(pkg, off + 6),
                raw_size=u32(pkg, off + 8),
                comp_size=u32(pkg, off + 12),
                data_off=u32(pkg, off + 16),
            )
        )
    return out


def parse_actions(pkg: bytes, h: Header) -> list[Action]:
    out: list[Action] = []
    if h.action_count == 0 or h.action_off == 0:
        return out
    for idx in range(h.action_count):
        off = h.action_off + idx * GSP_ACTION_SIZE
        out.append(
            Action(
                idx=idx,
                src=u16(pkg, off + 0),
                event=u16(pkg, off + 2),
                action=u16(pkg, off + 4),
                target_idx=u16(pkg, off + 6),
                target_name_off=u32(pkg, off + 8),
                param_off=u32(pkg, off + 12),
                param_len=u16(pkg, off + 16),
                flags=u16(pkg, off + 18),
                arg=u32(pkg, off + 20),
            )
        )
    return out


def first_blob_data(h: Header, blobs: list[Blob]) -> int:
    if not blobs:
        return h.total_size
    return min(b.data_off for b in blobs)


def flags_text(flags: int) -> str:
    names = [name for bit, name in FLAG_NAMES if flags & bit]
    return '|'.join(names) if names else 'none'


def type_name(value: int) -> str:
    return TYPE_NAMES.get(value, f'unknown({value})')


def event_name(value: int) -> str:
    return EVENT_NAMES.get(value, 'none')


def action_name(value: int) -> str:
    return ACTION_NAMES.get(value, 'none')


def codec_name(value: int) -> str:
    if value == GSP_CODEC_STORE:
        return 'store'
    if value == GSP_CODEC_RLE16:
        return 'rle16'
    return f'unknown({value})'


def object_label(pkg: bytes, obj: Obj) -> str:
    name = read_cstr(pkg, obj.name_off)
    text = read_cstr(pkg, obj.text_off)
    if name:
        return name
    if text:
        return text[:32]
    return type_name(obj.type)


def object_refs(pkg: bytes, obj: Obj) -> str:
    refs: list[str] = []
    name = read_cstr(pkg, obj.name_off)
    text = read_cstr(pkg, obj.text_off)
    callback = read_cstr(pkg, obj.callback_off)
    if name:
        refs.append(f'name@`{obj.name_off}` `{md_escape(name)}`')
    if text:
        refs.append(f'text@`{obj.text_off}` `{md_escape(text)}`')
    if obj.flags & GSP_F_IMAGE:
        refs.append(f'blob `{obj.blob_idx}`')
    if callback:
        refs.append(f'cb@`{obj.callback_off}` `{md_escape(callback)}`')
    if obj.flags & GSP_F_PARAMS:
        refs.append(f'params@`{obj.params_off}` `{obj.params_len}B`')
    return ', '.join(refs) if refs else 'none'


def parse_item_params(pkg: bytes, obj: Obj) -> str:
    if not (obj.flags & GSP_F_PARAMS) or obj.params_len < 12:
        return ''
    base = obj.params_off
    end = base + obj.params_len
    if base <= 0 or end > len(pkg):
        return 'invalid params bounds'
    item_count = u16(pkg, base + 0)
    selected = u16(pkg, base + 2)
    item_height = u16(pkg, base + 4)
    rows_or_page = u16(pkg, base + 6)
    flags = u16(pkg, base + 8)
    cursor = base + 12
    items: list[str] = []
    for _ in range(item_count):
        if cursor + 2 > end:
            items.append('<truncated>')
            break
        n = u16(pkg, cursor)
        cursor += 2
        if cursor + n > end:
            items.append('<truncated>')
            break
        items.append(pkg[cursor : cursor + n].decode('utf-8', errors='replace'))
        cursor += n
    selected_text = 'none' if selected == 0xFFFF else str(selected)
    item_text = ', '.join(f'`{md_escape(s)}`' for s in items)
    return (
        f'items={item_count}, selected={selected_text}, item_height={item_height}, '
        f'rows/page={rows_or_page}, flags=0x{flags:04X}, values=[{item_text}]'
    )


def object_notes(pkg: bytes, obj: Obj) -> str:
    notes: list[str] = []
    if obj.type == GSP_OBJ_CONTAINER and obj.parent == GSP_NO_PARENT:
        notes.append(f'screen background 0x{obj.bg & 0xFFFFFF:06X}')
    if obj.type == GSP_OBJ_LAYER:
        notes.append('layer')
        if obj.flags & GSP_F_HIDDEN:
            notes.append('initially hidden')
    if obj.type == GSP_OBJ_IMAGE:
        notes.append('uses baked image blob')
    if obj.type in (GSP_OBJ_LIST, GSP_OBJ_WHEEL):
        params = parse_item_params(pkg, obj)
        if params:
            notes.append(params)
    if obj.bind_id:
        notes.append(f'bind_id {obj.bind_id}')
    if obj.callback_off:
        cb = read_cstr(pkg, obj.callback_off) or '?'
        notes.append(f'C callback `{md_escape(cb)}`')
    return '; '.join(notes) if notes else '-'


def write_inc(path: Path, pkg: bytes, h: Header, prefix: str) -> None:
    with path.open('w', encoding='utf-8') as fp:
        fp.write(
            '/* Auto-generated by gsp_package_tools.py - do not edit.\n'
            ' * ITE-style GSP scene package include:\n'
            f' *   - {prefix}_fonts[]: runtime resource bindings for font_id\n'
            f' *   - {prefix}_scene_pkg[]: complete GSP1 binary scene package\n'
            ' *\n'
            ' * The package contains no native pointers. Object references are u16 indexes;\n'
            ' * strings and blob data are u32 byte offsets from the package base.\n'
            ' */\n'
            '#include "gfx/scene/gsp.h"\n\n'
            f'#define {prefix.upper()}_SCREEN_W       {h.screen_w}\n'
            f'#define {prefix.upper()}_SCREEN_H       {h.screen_h}\n'
            f'#define {prefix.upper()}_COLOR_FMT      0\n'
            f'#define {prefix.upper()}_OBJ_COUNT      {h.obj_count}\n'
            f'#define {prefix.upper()}_FONT_COUNT     {len(HOME_FONTS)}\n'
            f'#define {prefix.upper()}_ACTION_COUNT   {h.action_count}\n'
            f'#define {prefix.upper()}_FONT_CJK_REGULAR_PATH "{HOME_FONT_CJK_REGULAR_PATH}"\n\n'
        )
        fp.write(f'static const gsp_font_desc_t {prefix}_fonts[{prefix.upper()}_FONT_COUNT] = {{\n')
        for font in HOME_FONTS:
            fp.write(
                f"    {{ .id = {font['id']}, .family = \"{font['family']}\", "
                f'.path = {prefix.upper()}_FONT_CJK_REGULAR_PATH, '
                f".size_px = {font['size']}, .weight = {font['weight']}, .style = {font['style']} }},\n"
            )
        fp.write('};\n\n')

        fp.write(f'static const uint8_t {prefix}_scene_pkg[] = {{\n')
        for i, byte in enumerate(pkg):
            if i % 12 == 0:
                fp.write('    ')
            sep = '' if i + 1 == len(pkg) else ' '
            fp.write(f'0x{byte:02X},{sep}')
            if i % 12 == 11 or i + 1 == len(pkg):
                fp.write('\n')
        fp.write(f'}};\n\nstatic const size_t {prefix}_scene_pkg_len = sizeof({prefix}_scene_pkg);\n')


def decompress_blob(pkg: bytes, blob: Blob) -> bytes:
    comp = pkg[blob.data_off : blob.data_off + blob.comp_size]
    if blob.codec == GSP_CODEC_STORE:
        raw = comp
    elif blob.codec == GSP_CODEC_RLE16:
        out = bytearray()
        cursor = 0
        while cursor + 4 <= len(comp):
            count = u16(comp, cursor)
            pixel = comp[cursor + 2 : cursor + 4]
            cursor += 4
            out.extend(pixel * count)
        raw = bytes(out)
    else:
        raise ValueError(f'unsupported blob codec {blob.codec}')
    if len(raw) != blob.raw_size:
        raise ValueError(f'blob {blob.idx} raw size mismatch: {len(raw)} != {blob.raw_size}')
    return raw


def write_rgb565_bmp(path: Path, raw: bytes, w: int, h: int) -> None:
    row_bytes = w * 3
    row_padded = (row_bytes + 3) & ~3
    pixel_bytes = row_padded * h
    file_size = 14 + 40 + pixel_bytes
    with path.open('wb') as fp:
        fp.write(b'BM')
        fp.write(struct.pack('<IHHI', file_size, 0, 0, 54))
        fp.write(struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, pixel_bytes, 2835, 2835, 0, 0))
        pad = b'\0' * (row_padded - row_bytes)
        for y in range(h - 1, -1, -1):
            row = bytearray()
            for x in range(w):
                c = u16(raw, (y * w + x) * 2)
                r = ((c >> 11) & 0x1F) * 255 // 31
                g = ((c >> 5) & 0x3F) * 255 // 63
                b = (c & 0x1F) * 255 // 31
                row.extend((b, g, r))
            fp.write(row)
            fp.write(pad)


def write_preview(path: Path, pkg: bytes, blobs: list[Blob]) -> None:
    if not blobs:
        return
    blob = blobs[0]
    if blob.cf != 4:
        return
    raw = decompress_blob(pkg, blob)
    write_rgb565_bmp(path, raw, blob.w, blob.h)


def blob_users(objects: list[Obj], blob_idx: int) -> str:
    users = [f'object `{o.idx}`' for o in objects if (o.flags & GSP_F_IMAGE) and o.blob_idx == blob_idx]
    return ', '.join(users) if users else 'none'


def font_users(objects: list[Obj], font_id: int) -> str:
    users = [
        f'object `{o.idx}`'
        for o in objects
        if o.type in (GSP_OBJ_LABEL, GSP_OBJ_BUTTON, GSP_OBJ_LIST, GSP_OBJ_WHEEL) and o.font_id == font_id
    ]
    return ', '.join(users) if users else 'none'


def write_manifest(path: Path, source_name: str, inc_name: str, prefix: str, pkg: bytes, h: Header, objects: list[Obj], blobs: list[Blob], actions: list[Action]) -> None:
    blob_data = first_blob_data(h, blobs)
    params_off = h.action_off + h.action_count * GSP_ACTION_SIZE if h.action_count else h.blob_off + h.blob_count * GSP_BLOB_SIZE
    with path.open('w', encoding='utf-8') as fp:
        fp.write(
            f'# `{inc_name}` GSP Scene Package Manifest\n\n'
            f'This file is generated from `{source_name}` by `gsp_package_tools.py`. '
            'It explains the binary bytes that are wrapped by the companion `.inc`.\n\n'
            '## Export Summary\n\n'
            f'- Source package: `{source_name}`\n'
            f'- Package include: `{inc_name}`\n'
            f'- Runtime package symbol: `{prefix}_scene_pkg[]`\n'
            f'- Package length symbol: `{prefix}_scene_pkg_len`\n'
            f'- Runtime font binding symbol: `{prefix}_fonts[]`\n'
            f'- Source preview image: `{prefix}_preview.bmp`\n'
            f'- Format: GSP1 v{h.version}, little-endian, pointer-free runtime package\n'
            '- Loader: `gsp_load_with_fonts(home_scene_pkg, home_scene_pkg_len, ...)`\n\n'
        )

        fp.write(
            '## Header\n\n'
            '| Field | Value |\n'
            '|---|---:|\n'
            '| magic | `GSP1` |\n'
            f'| version | `{h.version}` |\n'
            f'| screen | `{h.screen_w} x {h.screen_h}` |\n'
            f'| screen_bg | `0x{h.screen_bg & 0xFFFFFF:06X}` |\n'
            f'| obj_count | `{h.obj_count}` |\n'
            f'| obj_table_off | `{h.obj_off}` |\n'
            f'| str_table_off | `{h.str_off}` |\n'
            f'| blob_count | `{h.blob_count}` |\n'
            f'| blob_table_off | `{h.blob_off}` |\n'
            f'| action_count | `{h.action_count}` |\n'
            f'| action_table_off | `{h.action_off}` |\n'
            f'| total_size | `{h.total_size}` |\n'
            f'| crc32 | `0x{h.crc32:08X}` |\n\n'
        )

        fp.write(
            '## Binary Layout\n\n'
            '| Region | Byte Range | Size | Notes |\n'
            '|---|---:|---:|---|\n'
            '| Header | `0..55` | `56` | Fixed GSP header (v4) |\n'
            f'| Object table | `{h.obj_off}..{h.obj_off + h.obj_count * GSP_OBJ_SIZE - 1}` | `{h.obj_count * GSP_OBJ_SIZE}` | `{h.obj_count} * 64B` object entries |\n'
            f'| Blob table | `{h.blob_off}..{h.blob_off + h.blob_count * GSP_BLOB_SIZE - 1}` | `{h.blob_count * GSP_BLOB_SIZE}` | `{h.blob_count} * 20B` blob entries |\n'
            f'| Action table | `{h.action_off}..{h.action_off + h.action_count * GSP_ACTION_SIZE - 1}` | `{h.action_count * GSP_ACTION_SIZE}` | `{h.action_count} * 24B` action entries |\n'
            f'| Params area | `{params_off}..{h.str_off - 1}` | `{h.str_off - params_off}` | Widget-private params blocks |\n'
            f'| String table | `{h.str_off}..{blob_data - 1}` | `{blob_data - h.str_off}` | NUL-terminated UTF-8 strings |\n'
            f'| Blob data | `{blob_data}..{h.total_size - 1}` | `{h.total_size - blob_data}` | Compressed image payloads |\n\n'
        )

        fp.write(
            '## Component Rules Used\n\n'
            '| ID | Type | Runtime create path |\n'
            '|---:|---|---|\n'
            '| `1` | container | `gfx_container_create()` |\n'
            '| `2` | label | `gfx_label_create()` |\n'
            '| `3` | button | `gfx_button_create()` |\n'
            '| `4` | image | `gfx_image_create()` + package blob source |\n'
            '| `5` | list | `gfx_list_create()` + params-v1 items |\n'
            '| `6` | wheel | `gfx_wheel_create()` + params-v1 items |\n'
            '| `7` | layer | `gfx_container_create()` + layer switching |\n\n'
            'Parent rule: entries are preorder; every non-root object must reference an earlier object index. `0xFFFF` means root.\n\n'
        )

        fp.write(
            '## Object Table\n\n'
            '| Idx | Role | Type | Parent | Rect | Flags | Name / Text / Callback / Blob / Params | Font | Bind | Notes |\n'
            '|---:|---|---|---:|---|---:|---|---:|---:|---|\n'
        )
        for obj in objects:
            parent = 'root' if obj.parent == GSP_NO_PARENT else f'`{obj.parent}`'
            role = md_escape(object_label(pkg, obj))
            refs = object_refs(pkg, obj)
            notes = md_escape(object_notes(pkg, obj))
            fp.write(
                f'| `{obj.idx}` | {role} | {type_name(obj.type)} | {parent} | '
                f'`({obj.x},{obj.y} {obj.w}x{obj.h})` | `0x{obj.flags:03X}` | '
                f'{refs} | `{obj.font_id}` | `{obj.bind_id}` | {notes}; flags: {flags_text(obj.flags)} |\n'
            )

        fp.write(
            '\n## Action Table (v4)\n\n'
            'Position-independent `event -> action` records (24B each). No inline structs, no pointers: '
            'targets are u16 object indexes or name offsets, params are u32 string offsets, colors are scalar `arg`.\n\n'
            '| Idx | Src Obj | Event | Action | Target | Param / Arg |\n'
            '|---:|---:|---|---|---|---|\n'
        )
        for action in actions:
            target_name = read_cstr(pkg, action.target_name_off)
            param = read_cstr(pkg, action.param_off)
            if target_name:
                target = f'name@`{action.target_name_off}` `{md_escape(target_name)}`'
            elif action.target_idx != GSP_ACT_NO_TARGET:
                target = f'obj `{action.target_idx}`'
            else:
                target = 'none'
            if param:
                param_text = f'param@`{action.param_off}` `{md_escape(param)}`'
            elif action.arg:
                param_text = f'arg `0x{action.arg & 0xFFFFFF:06X}`'
            else:
                param_text = 'none'
            fp.write(
                f'| `{action.idx}` | `{action.src}` | {event_name(action.event)} | '
                f'{action_name(action.action)} | {target} | {param_text} |\n'
            )

        fp.write('\n## String Table\n\n| Offset | String |\n|---:|---|\n')
        off = h.str_off
        while off < blob_data:
            text = read_cstr(pkg, off)
            if text is None:
                break
            fp.write(f'| `{off}` | `{md_escape(text)}` |\n')
            off += len(text.encode('utf-8')) + 1

        fp.write(
            '\n## Resource Bindings\n\n'
            'Fonts are currently outside the binary package as a C binding table in the generated `.inc`. Objects refer to these by `font_id`.\n\n'
            '| Font Id | Family | Path | Size | Weight | Style | Used By |\n'
            '|---:|---|---|---:|---:|---:|---|\n'
        )
        for font in HOME_FONTS:
            fp.write(
                f"| `{font['id']}` | `{font['family']}` | `{font['path']}` | "
                f"`{font['size']}` | `{font['weight']}` | `{font['style']}` | "
                f"{font_users(objects, font['id'])} |\n"
            )

        fp.write(
            '\nImages/blobs:\n\n'
            '| Blob Id | Size | Format | Codec | Raw | Compressed | Data Offset | Used By |\n'
            '|---:|---|---:|---|---:|---:|---:|---|\n'
        )
        for blob in blobs:
            fp.write(
                f'| `{blob.idx}` | `{blob.w}x{blob.h}` | `0x{blob.cf:02X}` | '
                f'`{codec_name(blob.codec)}` | `{blob.raw_size}` | `{blob.comp_size}` | '
                f'`{blob.data_off}` | {blob_users(objects, blob.idx)} |\n'
            )

        fp.write(
            '\n## Runtime Path\n\n'
            '```text\n'
            f'{inc_name}\n'
            f'  {prefix}_fonts[]      -> host/device font creation or binding\n'
            f'  {prefix}_scene_pkg[]  -> gsp_load_with_fonts()\n'
            '      header/CRC validation\n'
            '      object table scan\n'
            '      string offset resolution\n'
            '      runtime refs: object name / bind_id -> gfx_object_t\n'
            '      list/wheel params-v1 decode -> widget items\n'
            '      blob table resolution -> image blob decode/cache\n'
            '      gfx_*_create + setter\n'
            "      callback name binding: \"on_ok\" -> on_ok_cb\n"
            '      action table dispatch: Next buttons -> layer GOTO\n'
            '      render through normal GFX object tree\n'
            '```\n\n'
            '## Validation\n\n'
            'Run after export:\n\n'
            '```bash\n'
            'SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy GSP_HEADLESS=1 ./build-host-sdl/gfx_ai_scene_pkg_demo\n'
            'ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure\n'
            '```\n\n'
            'Latest result:\n\n'
            f'- Package size: `{h.total_size}` bytes\n'
            f'- CRC: `0x{h.crc32:08X}`\n'
            f'- Loaded objects: `{h.obj_count}`\n'
            f'- Image blobs: `{h.blob_count}`\n'
            f'- Self-check: expected `PASS ({h.obj_count} objects from {inc_name})`\n\n'
            '## Known Limits\n\n'
            '- Font descriptions are still C-side binding metadata, not binary package records.\n'
            '- Image preview is extracted from the first RGB565 blob in the package.\n'
            '- List/wheel params-v1 is currently a compact demo format; future widgets should define their own params profile.\n'
        )


def run(args: argparse.Namespace) -> None:
    source = Path(args.gsp)
    out_dir = Path(args.out_dir)
    # Default: put .inc next to the demo under examples/ai_scene_pkg/inc/
    inc_dir = Path(args.inc_dir) if args.inc_dir is not None else out_dir.parent / 'inc'
    out_dir.mkdir(parents=True, exist_ok=True)
    inc_dir.mkdir(parents=True, exist_ok=True)
    pkg = source.read_bytes()
    header = parse_header(pkg)
    objects = parse_objects(pkg, header)
    blobs = parse_blobs(pkg, header)
    actions = parse_actions(pkg, header)

    prefix = args.prefix
    inc_path = inc_dir / f'{prefix}.inc'
    inc_name = str(inc_path.relative_to(out_dir)) if inc_path.is_relative_to(out_dir) else inc_path.name
    write_inc(inc_path, pkg, header, prefix)
    write_manifest(out_dir / f'{prefix}.manifest.md', source.name, inc_name, prefix, pkg, header, objects, blobs, actions)
    write_preview(out_dir / f'{prefix}_preview.bmp', pkg, blobs)
    print(f"generated {inc_path}, {out_dir / (prefix + '.manifest.md')}, and {out_dir / (prefix + '_preview.bmp')}")

    if args.arn:
        from gsp_to_arn import gsp_to_arn

        arn, info = gsp_to_arn(pkg)
        arn_path = out_dir / f'{prefix}.arn'
        arn_path.write_bytes(arn)
        (out_dir / f'{prefix}.arn.md').write_text(
            f'# ARN export `{arn_path.name}`\n\n'
            f"- nodes: `{info['out_node_count']}` (from `{info['src_obj_count']}` GSP objs)\n"
            f"- size: `{info['arn_size']}` bytes\n"
            f"- skipped: `{info['skipped']}`\n",
            encoding='utf-8',
        )
        print(f"generated {arn_path} ({info['arn_size']} B, {info['out_node_count']} nodes)")


def main() -> None:
    parser = argparse.ArgumentParser(description='Generate GSP package side artifacts')
    parser.add_argument('gsp', help='input raw .gsp package')
    parser.add_argument('--out-dir', default='examples/ai_scene_pkg/gsp_export',
                        help='output directory for manifest/preview (default: gsp_export/)')
    parser.add_argument('--inc-dir', default=None,
                        help='output directory for generated .inc files (default: <out-dir>/../inc)')
    parser.add_argument('--prefix', default='home', help='symbol/file prefix')
    parser.add_argument('--arn', action='store_true',
                        help='also export ARN1 package (.arn) via gsp_to_arn')
    run(parser.parse_args())


if __name__ == '__main__':
    main()
