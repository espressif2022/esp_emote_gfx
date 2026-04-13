#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Generate a compact numeric baseline summary from lobster_emote_export.inc."""

import argparse
import json
import re
from pathlib import Path

STATE_RE = re.compile(
    r'\{\s*"([^"]+)",\s*"([^"]+)",\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*([A-Z0-9_]+),\s*(\d+)\s*\}'
)


def extract_block(text: str, typename: str, symbol: str):
    pattern = re.compile(
        rf"static const {re.escape(typename)} {re.escape(symbol)} = \{{(.*?)\n\}};",
        re.S,
    )
    match = pattern.search(text)
    return match.group(1) if match else None


def extract_field(block: str, field: str):
    match = re.search(rf"\.{re.escape(field)}\s*=\s*([^,\n]+)\s*(?:,|$)", block, re.M)
    return match.group(1).strip() if match else None


def extract_curve_rows(text: str, symbol: str):
    pattern = re.compile(rf"static const int16_t {re.escape(symbol)}\[\d+\]\[\d+\] = \{{(.*?)\n\}};", re.S)
    match = pattern.search(text)
    if not match:
        return []
    rows = []
    for row in re.findall(r"\{([^{}]+)\}", match.group(1)):
        rows.append([float(v.strip()) for v in row.split(',')])
    return rows


def curve_bounds(points):
    xs = points[0::2]
    ys = points[1::2]
    return {
        "min_x": min(xs),
        "max_x": max(xs),
        "min_y": min(ys),
        "max_y": max(ys),
        "width": max(xs) - min(xs),
        "height": max(ys) - min(ys),
    }


def parse_states(text: str):
    states = {}
    for match in STATE_RE.finditer(text):
        name, name_cn, sm, hp, sd, su, an, look_x, look_y, pupil_shape, hold_ticks = match.groups()
        states[name] = {
            "name_cn": name_cn,
            "w_smile": int(sm),
            "w_happy": int(hp),
            "w_sad": int(sd),
            "w_surprise": int(su),
            "w_angry": int(an),
            "w_look_x": int(look_x),
            "w_look_y": int(look_y),
            "pupil_shape": pupil_shape,
            "hold_ticks": int(hold_ticks),
        }
    return states


def main():
    parser = argparse.ArgumentParser(description="Generate lobster export baseline JSON")
    parser.add_argument("input", help="Path to lobster_emote_export.inc")
    parser.add_argument("--output", required=True, help="Output JSON path")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    text = input_path.read_text(encoding="utf-8")

    meta_block = extract_block(text, "gfx_lobster_emote_export_meta_t", "s_lobster_export_meta")
    layout_block = extract_block(text, "gfx_lobster_emote_layout_t", "s_lobster_export_layout")
    semantics_block = extract_block(text, "gfx_lobster_emote_semantics_t", "s_lobster_export_semantics")

    meta_fields = [
        "version", "design_viewbox_x", "design_viewbox_y", "design_viewbox_w", "design_viewbox_h",
        "export_width", "export_height", "export_scale", "export_offset_x", "export_offset_y",
    ]
    layout_fields = [
        "eye_left_cx", "eye_left_cy", "eye_right_cx", "eye_right_cy", "pupil_left_cx", "pupil_left_cy",
        "pupil_right_cx", "pupil_right_cy", "mouth_cx", "mouth_cy", "antenna_left_cx", "antenna_left_cy",
        "antenna_right_cx", "antenna_right_cy",
    ]

    meta = {field: float(extract_field(meta_block, field)) if field == "export_scale" else int(float(extract_field(meta_block, field))) for field in meta_fields}
    layout = {field: int(float(extract_field(layout_block, field))) for field in layout_fields}

    semantics = {}
    if semantics_block is not None:
        for field in [
            "look_scale_x", "look_scale_y", "eye_scale_multiplier", "antenna_thickness_base",
            "timer_period_ms", "damping_div", "eye_segs", "pupil_segs", "antenna_segs"
        ]:
            value = extract_field(semantics_block, field)
            if value is None:
                continue
            semantics[field] = float(value.rstrip('f')) if any(ch in value for ch in '.f') else int(value)

    curves = {}
    for symbol in ["s_lobster_eye_white_base", "s_lobster_pupil_base", "s_lobster_mouth_base"]:
        rows = extract_curve_rows(text, symbol)
        if rows:
            curves[symbol] = {
                "base0_bounds": curve_bounds(rows[0]),
                "variants": len(rows),
            }

    baseline_states = parse_states(text)
    selected = {k: baseline_states[k] for k in ["00 neutral", "07 happy", "16 surprise", "21 angry"] if k in baseline_states}

    output = {
        "asset": str(input_path),
        "meta": meta,
        "layout": layout,
        "semantics": semantics,
        "curves": curves,
        "states": selected,
    }

    output_path.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
