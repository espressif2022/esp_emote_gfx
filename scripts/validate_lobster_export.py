#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Validate standardized lobster export .inc assets.
"""

import argparse
import re
import sys
from pathlib import Path


REQUIRED_LAYOUT_FIELDS = (
    "eye_left_cx",
    "eye_left_cy",
    "eye_right_cx",
    "eye_right_cy",
    "pupil_left_cx",
    "pupil_left_cy",
    "pupil_right_cx",
    "pupil_right_cy",
    "mouth_cx",
    "mouth_cy",
    "antenna_left_cx",
    "antenna_left_cy",
    "antenna_right_cx",
    "antenna_right_cy",
)

REQUIRED_META_FIELDS = (
    "version",
    "design_viewbox_x",
    "design_viewbox_y",
    "design_viewbox_w",
    "design_viewbox_h",
    "export_width",
    "export_height",
    "export_scale",
    "export_offset_x",
    "export_offset_y",
)


def extract_block(text, typename, symbol):
    pattern = re.compile(
        rf"static const {re.escape(typename)} {re.escape(symbol)} = \{{(.*?)\n\}};",
        re.S,
    )
    match = pattern.search(text)
    return match.group(1) if match else None


def extract_field(block, field):
    match = re.search(rf"\.{re.escape(field)}\s*=\s*([^,\n]+)\s*(?:,|$)", block, re.M)
    return match.group(1).strip() if match else None


def main():
    parser = argparse.ArgumentParser(description="Validate lobster export .inc asset")
    parser.add_argument("input", help="Path to lobster_emote_export.inc")
    args = parser.parse_args()

    path = Path(args.input)
    if not path.exists():
        print(f"ERROR: file not found: {path}")
        return 1

    text = path.read_text()
    errors = []

    meta_block = extract_block(text, "gfx_lobster_emote_export_meta_t", "s_lobster_export_meta")
    layout_block = extract_block(text, "gfx_lobster_emote_layout_t", "s_lobster_export_layout")

    if meta_block is None:
        errors.append("missing s_lobster_export_meta")
    if layout_block is None:
        errors.append("missing s_lobster_export_layout")

    meta_values = {}
    if meta_block is not None:
        for field in REQUIRED_META_FIELDS:
            value = extract_field(meta_block, field)
            if value is None:
                errors.append(f"missing export_meta field: {field}")
            else:
                meta_values[field] = value

    if layout_block is not None:
        for field in REQUIRED_LAYOUT_FIELDS:
            if extract_field(layout_block, field) is None:
                errors.append(f"missing layout field: {field}")

    if "static const gfx_lobster_emote_state_t s_lobster_expr_sequence[]" not in text:
        errors.append("missing s_lobster_expr_sequence")

    if "#define LOBSTER_EXPORTED_BG_SYMBOL" not in text:
        errors.append("missing embedded background symbol macro")

    header_w = re.search(r"\.header\.w\s*=\s*(\d+),", text)
    header_h = re.search(r"\.header\.h\s*=\s*(\d+),", text)
    if header_w is None or header_h is None:
        errors.append("missing embedded background size")
    elif meta_values:
        export_w = re.sub(r"[^0-9]", "", meta_values["export_width"])
        export_h = re.sub(r"[^0-9]", "", meta_values["export_height"])
        if header_w.group(1) != export_w or header_h.group(1) != export_h:
            errors.append(
                f"embedded background size {header_w.group(1)}x{header_h.group(1)} "
                f"does not match export_meta {export_w}x{export_h}"
            )

    if meta_values:
        version = re.sub(r"[^0-9]", "", meta_values["version"])
        if version != "2":
            errors.append(f"unsupported export_meta.version: {meta_values['version']}")
        if "static const gfx_lobster_emote_semantics_t s_lobster_export_semantics" not in text:
            errors.append("missing s_lobster_export_semantics")

    if errors:
        print(f"Validation failed for {path}:")
        for error in errors:
            print(f"  - {error}")
        return 1

    export_w = meta_values.get("export_width", "?")
    export_h = meta_values.get("export_height", "?")
    print(f"Validation OK: {path}")
    print(f"  export size: {export_w}x{export_h}")
    print("  sections: export_meta, layout, semantics, sequence, embedded image")
    return 0


if __name__ == "__main__":
    sys.exit(main())
