#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Host-side GSP1 → ARN1 exporter (mirrors src/scene/gsp_to_arena.c).

Usage:
  python3 examples/ai_scene_pkg/gsp_export/gsp_to_arn.py \\
    examples/ai_scene_pkg/gsp_export/home.gsp \\
    --out examples/ai_scene_pkg/gsp_export/home.arn
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from gsp_package_tools import (  # noqa: E402
    GSP_CODEC_RLE16,
    GSP_CODEC_STORE,
    GSP_F_BG_COLOR,
    GSP_F_CALLBACK,
    GSP_F_FG_COLOR,
    GSP_F_HIDDEN,
    GSP_F_IMAGE,
    GSP_F_NAME,
    GSP_F_PARAMS,
    GSP_F_TEXT,
    GSP_NO_PARENT,
    GSP_OBJ_BUTTON,
    GSP_OBJ_CONTAINER,
    GSP_OBJ_IMAGE,
    GSP_OBJ_LABEL,
    GSP_OBJ_LAYER,
    GSP_OBJ_LIST,
    GSP_OBJ_WHEEL,
    Blob,
    parse_blobs,
    parse_header,
    parse_objects,
    read_cstr,
    u16,
)

GSP_ITEM_PARAMS_F_CYCLIC = 1 << 0

ARENA_MAGIC = 0x314E5241
ARENA_VERSION = 1
ARENA_NO_NODE = 0xFFFFFFFF

ARENA_NODE_CONTAINER = 1
ARENA_NODE_LABEL = 2
ARENA_NODE_BUTTON = 3
ARENA_NODE_IMAGE = 4
ARENA_NODE_LIST = 5
ARENA_NODE_WHEEL = 6

ARENA_F_VISIBLE = 1 << 0
ARENA_F_BG = 1 << 1
ARENA_F_CLICKABLE = 1 << 2

ARENA_IMG_FMT_RGB565 = 0
ARENA_ITEMS_F_CYCLIC = 1 << 0
GFX_COLOR_FORMAT_RGB565 = 0x04


def decode_blob_rgb565(pkg: bytes, blob: Blob) -> tuple[int, int, bytes] | None:
    if blob.cf != GFX_COLOR_FORMAT_RGB565 or blob.w == 0 or blob.h == 0:
        return None
    expect = blob.w * blob.h * 2
    if blob.raw_size != expect:
        return None
    data = pkg[blob.data_off : blob.data_off + blob.comp_size]
    if blob.codec == GSP_CODEC_STORE:
        if blob.comp_size != blob.raw_size:
            return None
        return blob.w, blob.h, data
    if blob.codec == GSP_CODEC_RLE16:
        out = bytearray()
        ci = 0
        while ci + 4 <= len(data) and len(out) + 2 <= expect:
            run = u16(data, ci)
            px = data[ci + 2 : ci + 4]
            ci += 4
            for _ in range(run):
                if len(out) + 2 > expect:
                    break
                out.extend(px)
        if len(out) != expect:
            return None
        return blob.w, blob.h, bytes(out)
    return None


def parse_items(pkg: bytes, obj) -> tuple[list[str], int, int, int] | None:
    if not (obj.flags & GSP_F_PARAMS) or obj.params_len < 12:
        return None
    p = pkg[obj.params_off : obj.params_off + obj.params_len]
    item_count = u16(p, 0)
    selected = u16(p, 2)
    item_height = u16(p, 4)
    flags = u16(p, 8)
    cur = 12
    items: list[str] = []
    for _ in range(item_count):
        if cur + 2 > len(p):
            return None
        ln = u16(p, cur)
        cur += 2
        if cur + ln > len(p):
            return None
        items.append(p[cur : cur + ln].decode('utf-8', errors='replace'))
        cur += ln
    return items, selected, item_height, flags


def pack_arn(descs: list[dict]) -> bytes:
    str_blob = bytearray()
    name_rel: list[int | None] = []
    action_rel: list[int | None] = []

    for d in descs:
        if d.get('name'):
            name_rel.append(len(str_blob))
            str_blob.extend(d['name'].encode('utf-8') + b'\0')
        else:
            name_rel.append(None)
        if d.get('action') and d['type'] == ARENA_NODE_BUTTON:
            action_rel.append(len(str_blob))
            str_blob.extend(d['action'].encode('utf-8') + b'\0')
        else:
            action_rel.append(None)

    blob_payload = bytearray()
    blob_rel: list[int | None] = []
    for d in descs:
        if d['type'] == ARENA_NODE_IMAGE and d.get('pixels'):
            blob_rel.append(len(blob_payload))
            w, h, px = d['pixels']
            blob_payload.extend(struct.pack('<HHHH', w, h, ARENA_IMG_FMT_RGB565, 0))
            blob_payload.extend(px)
        elif d['type'] in (ARENA_NODE_LIST, ARENA_NODE_WHEEL) and d.get('items'):
            blob_rel.append(len(blob_payload))
            items = d['items']
            blob_payload.extend(
                struct.pack(
                    '<HHHHI',
                    len(items),
                    d.get('selected', 0xFFFF),
                    d.get('item_height', 0),
                    d.get('items_flags', 0),
                    d.get('text_rgb', 0xF3F7FA),
                )
            )
            for s in items:
                b = s.encode('utf-8')
                blob_payload.extend(struct.pack('<H', len(b)))
                blob_payload.extend(b)
        else:
            blob_rel.append(None)

    nodes_off = 24
    str_off = nodes_off + len(descs) * 32
    blob_base = str_off + len(str_blob)
    total = blob_base + len(blob_payload)

    node_offs = [nodes_off + i * 32 for i in range(len(descs))]
    first_child = [ARENA_NO_NODE] * len(descs)
    next_sibling = [ARENA_NO_NODE] * len(descs)
    root_off = ARENA_NO_NODE

    for i, d in enumerate(descs):
        p = d['parent']
        if p < 0:
            if root_off == ARENA_NO_NODE:
                root_off = node_offs[i]
            continue
        if first_child[p] == ARENA_NO_NODE:
            first_child[p] = node_offs[i]

    for i, d in enumerate(descs):
        p = d['parent']
        if p < 0:
            continue
        for j in range(i + 1, len(descs)):
            if descs[j]['parent'] == p:
                next_sibling[i] = node_offs[j]
                break

    prev_root = None
    for i, d in enumerate(descs):
        if d['parent'] >= 0:
            continue
        if prev_root is not None:
            next_sibling[prev_root] = node_offs[i]
        prev_root = i

    buf = bytearray(total)
    struct.pack_into(
        '<IHHIIII',
        buf,
        0,
        ARENA_MAGIC,
        ARENA_VERSION,
        len(descs),
        nodes_off,
        str_off,
        total,
        root_off,
    )
    buf[str_off : str_off + len(str_blob)] = str_blob
    if blob_payload:
        buf[blob_base : blob_base + len(blob_payload)] = blob_payload

    for i, d in enumerate(descs):
        name_off = 0 if name_rel[i] is None else str_off + name_rel[i]
        if blob_rel[i] is not None:
            reserved = blob_base + blob_rel[i]
        elif action_rel[i] is not None:
            reserved = str_off + action_rel[i]
        else:
            reserved = 0
        struct.pack_into(
            '<HHhhHHIIIII',
            buf,
            node_offs[i],
            d['type'],
            d['flags'],
            d['x'],
            d['y'],
            d['w'],
            d['h'],
            d.get('bg_rgb', 0) & 0xFFFFFFFF,
            name_off,
            first_child[i],
            next_sibling[i],
            reserved,
        )
    return bytes(buf)


def gsp_to_arn(pkg: bytes) -> tuple[bytes, dict]:
    h = parse_header(pkg)
    objects = parse_objects(pkg, h)
    blobs = parse_blobs(pkg, h)
    blob_by_idx = {b.idx: b for b in blobs}

    convertible = {
        GSP_OBJ_CONTAINER,
        GSP_OBJ_LAYER,
        GSP_OBJ_LABEL,
        GSP_OBJ_BUTTON,
        GSP_OBJ_IMAGE,
        GSP_OBJ_LIST,
        GSP_OBJ_WHEEL,
    }
    mapping: dict[int, int] = {}
    out_i = 0
    skipped = 0
    for obj in objects:
        if obj.type in convertible:
            mapping[obj.idx] = out_i
            out_i += 1
        else:
            skipped += 1

    descs: list[dict] = []
    for obj in objects:
        if obj.idx not in mapping:
            continue
        text = read_cstr(pkg, obj.text_off) if obj.flags & GSP_F_TEXT else None
        cb = read_cstr(pkg, obj.callback_off) if obj.flags & GSP_F_CALLBACK else None
        name = read_cstr(pkg, obj.name_off) if obj.flags & GSP_F_NAME else None
        flags = ARENA_F_VISIBLE
        if obj.flags & GSP_F_HIDDEN:
            flags &= ~ARENA_F_VISIBLE
        parent = -1
        if obj.parent != GSP_NO_PARENT and obj.parent in mapping:
            parent = mapping[obj.parent]

        d: dict = {
            'type': ARENA_NODE_CONTAINER,
            'flags': flags,
            'x': obj.x,
            'y': obj.y,
            'w': obj.w,
            'h': obj.h,
            'bg_rgb': 0,
            'name': None,
            'parent': parent,
        }

        if obj.type in (GSP_OBJ_CONTAINER, GSP_OBJ_LAYER):
            d['type'] = ARENA_NODE_CONTAINER
            d['name'] = name
            if obj.flags & GSP_F_BG_COLOR:
                d['flags'] |= ARENA_F_BG
                d['bg_rgb'] = obj.bg
        elif obj.type == GSP_OBJ_LABEL:
            d['type'] = ARENA_NODE_LABEL
            d['name'] = text or name
            d['bg_rgb'] = obj.fg if (obj.flags & GSP_F_FG_COLOR) else 0xF3F7FA
        elif obj.type == GSP_OBJ_BUTTON:
            d['type'] = ARENA_NODE_BUTTON
            d['name'] = text or name
            d['action'] = cb
            d['flags'] |= ARENA_F_CLICKABLE | ARENA_F_BG
            d['bg_rgb'] = obj.bg if (obj.flags & GSP_F_BG_COLOR) else 0x2F8CFF
        elif obj.type == GSP_OBJ_IMAGE:
            d['type'] = ARENA_NODE_IMAGE
            d['name'] = name
            if obj.flags & GSP_F_IMAGE and obj.blob_idx in blob_by_idx:
                decoded = decode_blob_rgb565(pkg, blob_by_idx[obj.blob_idx])
                if decoded:
                    d['pixels'] = decoded
        else:
            d['type'] = ARENA_NODE_LIST if obj.type == GSP_OBJ_LIST else ARENA_NODE_WHEEL
            d['name'] = name
            d['flags'] |= ARENA_F_CLICKABLE | ARENA_F_BG
            d['bg_rgb'] = obj.bg if (obj.flags & GSP_F_BG_COLOR) else 0x1A1F2E
            d['text_rgb'] = obj.fg if (obj.flags & GSP_F_FG_COLOR) else 0xF3F7FA
            parsed = parse_items(pkg, obj)
            if parsed:
                items, selected, item_h, iflags = parsed
                d['items'] = items
                d['selected'] = selected
                d['item_height'] = item_h
                d['items_flags'] = (
                    ARENA_ITEMS_F_CYCLIC
                    if (obj.type == GSP_OBJ_WHEEL and (iflags & GSP_ITEM_PARAMS_F_CYCLIC))
                    else 0
                )
        descs.append(d)

    arn = pack_arn(descs)
    info = {
        'screen_w': h.screen_w,
        'screen_h': h.screen_h,
        'src_obj_count': h.obj_count,
        'out_node_count': len(descs),
        'skipped': skipped,
        'arn_size': len(arn),
    }
    return arn, info


def main() -> None:
    parser = argparse.ArgumentParser(description='Export GSP package to ARN1')
    parser.add_argument('gsp', help='input .gsp')
    parser.add_argument('--out', required=True, help='output .arn path')
    parser.add_argument('--manifest', default=None, help='optional .md summary')
    args = parser.parse_args()

    pkg = Path(args.gsp).read_bytes()
    arn, info = gsp_to_arn(pkg)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(arn)

    lines = [
        f'# ARN export from `{Path(args.gsp).name}`',
        '',
        f"- screen: `{info['screen_w']}x{info['screen_h']}`",
        f"- GSP objects: `{info['src_obj_count']}` → ARN nodes: `{info['out_node_count']}`",
        f"- skipped: `{info['skipped']}`",
        f"- ARN size: `{info['arn_size']}` bytes",
        '',
        'Load with `arena_load` / `arena_scene_attach`.',
        '',
    ]
    if args.manifest:
        Path(args.manifest).write_text('\n'.join(lines), encoding='utf-8')
    print(
        f"wrote {out} ({info['arn_size']} B, "
        f"{info['out_node_count']} nodes, skipped {info['skipped']})"
    )


if __name__ == '__main__':
    main()
