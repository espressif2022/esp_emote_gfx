#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
"""
Rasterize from test_apps/UI/ip_svg_parts_readback.html:

  Eyes (left eye group):
    - Ellipse + pupil path → lobster_test_eye_tex.png, lobster_test_pupil_tex.png

  Body plate (no eyes, no antenna): claw-left, head, body, tail, claw-right, dots
    → lobster_test_body_tex.png (360×360, same viewBox as viewer)

Outputs: PNG previews, lobster_test_mesh_tex.inc (eyes), lobster_test_body_tex.inc (body BG)

RGB565 byte order matches scripts/image_converter.py:
  default (no flag)  = image_converter without --swap16  ([high_byte, low_byte] per pixel)
  --swap16           = image_converter with    --swap16 ([low_byte, high_byte] per pixel)
"""
from __future__ import annotations

import argparse
import os

import cairosvg
from PIL import Image, ImageDraw

# --- Body composite (ip_svg_parts_readback.html viewer, minus eye-left/right + antenna) ---
# viewBox matches LOBSTER_EXPORT design slice; raster 360×360 for gfx_img BG swap in test.
_BODY_SVG = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="120 150 1000 980">
  <defs>
    <linearGradient id="grad-claw-right" x1="572.14" y1="973.51" x2="832.91" y2="973.51" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#e94138"/>
      <stop offset=".62" stop-color="#f04f3e"/>
      <stop offset="1" stop-color="#f65b43"/>
    </linearGradient>
    <radialGradient id="grad-head" cx="564.75" cy="737.28" fx="564.75" fy="737.28" r="387.42" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#dc3336"/>
      <stop offset=".39" stop-color="#e3413a"/>
      <stop offset="1" stop-color="#f46144"/>
    </radialGradient>
    <linearGradient id="grad-claw-left" x1="160.47" y1="731.32" x2="406.47" y2="731.32" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#e94138"/>
      <stop offset=".62" stop-color="#f04f3e"/>
      <stop offset="1" stop-color="#f65b43"/>
    </linearGradient>
    <linearGradient id="grad-body" x1="824.65" y1="457.03" x2="906.86" y2="372.99" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#dc3336"/>
      <stop offset=".39" stop-color="#e3413a"/>
      <stop offset="1" stop-color="#f46144"/>
    </linearGradient>
    <linearGradient id="grad-tail" x1="921.29" y1="378.75" x2="996.95" y2="299.43" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#e94138"/>
      <stop offset=".62" stop-color="#f04f3e"/>
      <stop offset="1" stop-color="#f65b43"/>
    </linearGradient>
    <linearGradient id="grad-tail-stripe-a" x1="1015.28" y1="220.04" x2="899.96" y2="322.55" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#d42d36"/>
      <stop offset="1" stop-color="#d22e34"/>
    </linearGradient>
    <linearGradient id="grad-tail-stripe-b" x1="1077.43" y1="289.95" x2="962.1" y2="392.46" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="#d42d36"/>
      <stop offset="1" stop-color="#d22e34"/>
    </linearGradient>
  </defs>
  <rect x="120" y="150" width="1000" height="980" fill="#1d1b25" rx="26"/>
  <g id="part-claw-left">
    <path fill="url(#grad-claw-left)" d="M345.22,642.47c0-11.37.72-22.57,2.1-33.56.29-2.34-1.06-4.57-3.27-5.37-14.46-5.23-30.02-8.09-46.19-8.09-75.37,0-137.39,62.02-137.39,137.39,0,29.22,9.32,57.35,26.07,80.52,12.24,16.94,37,18.27,51.08,2.83l54.3-59.55,4.42,84.69c.89,16.98,16.95,29.1,33.48,25.1,30.23-7.32,56.69-24.76,75.55-48.46,1.46-1.84,1.47-4.45,0-6.27-37.59-46.16-60.14-105.05-60.14-169.22Z"/>
  </g>
  <g id="part-head">
    <path fill="url(#grad-head)" d="M698.92,823.06c33.31,0,63.97,10.67,88.39,28.59,1.92,1.41,4.55,1.42,6.46-.02,63.44-47.6,105.09-122.65,107.3-207.53,4.05-155.38-121.91-280.37-277.26-275.17-143.9,4.82-259.07,122.99-259.07,268.06,0,124.74,85.16,229.57,200.51,259.57,2.84.74,5.83-.49,7.31-3.02,24.59-42.02,71.95-70.47,126.35-70.47Z"/>
  </g>
  <g id="part-body">
    <path fill="url(#grad-body)" d="M900.97,539.29c-38.15-89.02-92.78-136.66-155.82-162.53-3.36-1.38-4.2-5.74-1.57-8.25l52.45-49.94c19.57-18.63,50.36-18.46,69.72.39l97.36,94.79c20.38,19.84,20.6,52.51.49,72.62l-54.5,54.5c-2.5,2.5-6.73,1.68-8.13-1.58Z"/>
  </g>
  <g id="part-tail">
    <path fill="url(#grad-tail)" d="M861,296.72c53.35,41.85,101.1,84.8,132.13,131.04,3.22,4.8,9.08,7.11,14.71,5.79l67.83-15.88c32.23-7.54,35.63-41.08,17.33-66.41-39.18-54.2-96.65-108.87-151.87-150.72-24.12-18.29-52.18-9.98-62.4,18.52l-22.24,62.02c-2.02,5.63-.21,11.93,4.5,15.62Z"/>
    <polygon fill="url(#grad-tail-stripe-a)" points="979.89 231.78 891.8 321.61 907.98 335.14 996.32 246.17 979.89 231.78"/>
    <polygon fill="url(#grad-tail-stripe-b)" points="1048.65 297.59 958.69 384.44 972.92 400.78 1062.75 313.32 1048.65 297.59"/>
  </g>
  <g id="part-claw-right">
    <path fill="url(#grad-claw-right)" d="M675.01,973.51h-83.14c-12.21,0-21.58-11.02-19.41-23.04,11.02-61.18,65.05-108.22,129.2-108.22,72,0,131.25,59.25,131.25,131.25s-59.25,131.25-131.25,131.25c-34.91,0-68.14-13.93-92.55-38.18-7.75-7.7-7.75-20.23.02-27.91l65.88-65.16Z"/>
  </g>
  <g id="part-dots">
    <circle fill="#f74c49" cx="872.27" cy="846.44" r="27.29"/>
    <circle fill="#f74c49" cx="915.29" cy="784.26" r="27.29"/>
    <circle fill="#f74c49" cx="939.83" cy="714.19" r="27.29"/>
  </g>
</svg>
"""

LOBSTER_TEST_BODY_W = 360
LOBSTER_TEST_BODY_H = 360

# --- SVG user space (left eye, ip_svg_parts_readback.html) ---
ELLIPSE_CX, ELLIPSE_CY = 478.47, 652.4
ELLIPSE_RX, ELLIPSE_RY = 58.27, 86.64
# Cubic Bezier (absolute points): M + c(relative)
P0 = (446.38, 665.79)
P1 = (446.38 + 5.69, 665.79 - 42.61)
P2 = (446.38 + 55.93, 665.79 - 40.58)
P3 = (446.38 + 61.22, 665.79 + 0.0)
STROKE_W_SVG = 20.0

# Crop box around left eye + margin for stroke (SVG units)
X0, Y0 = 412.0, 548.0
X1, Y1 = 544.0, 748.0

EYE_OUT_W, EYE_OUT_H = 64, 96
# Must match eye WxH: mesh_img maps UV 0..W-1, 0..H-1 onto a *closed* bezier; a thin SVG
# stroke mostly samples alpha=0. Use a solid filled ellipse so blended triangles hit opaque texels.
PUPIL_OUT_W, PUPIL_OUT_H = 64, 96


def map_pt(x: float, y: float, w: int, h: int) -> tuple[float, float]:
    return (x - X0) / (X1 - X0) * w, (y - Y0) / (Y1 - Y0) * h


def cubic(t: float, p0, p1, p2, p3) -> tuple[float, float]:
    u = 1.0 - t
    x = u**3 * p0[0] + 3 * u**2 * t * p1[0] + 3 * u * t**2 * p2[0] + t**3 * p3[0]
    y = u**3 * p0[1] + 3 * u**2 * t * p1[1] + 3 * u * t**2 * p2[1] + t**3 * p3[1]
    return x, y


def draw_eye_rgba() -> Image.Image:
    im = Image.new("RGBA", (EYE_OUT_W, EYE_OUT_H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    cx, cy = map_pt(ELLIPSE_CX, ELLIPSE_CY, EYE_OUT_W, EYE_OUT_H)
    rx = abs(map_pt(ELLIPSE_CX + ELLIPSE_RX, ELLIPSE_CY, EYE_OUT_W, EYE_OUT_H)[0] - cx)
    ry = abs(map_pt(ELLIPSE_CX, ELLIPSE_CY + ELLIPSE_RY, EYE_OUT_W, EYE_OUT_H)[1] - cy)
    dr.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(255, 255, 255, 255))
    return im


def draw_pupil_rgba() -> Image.Image:
    """
    Filled dark ellipse + bottom arc overlay.

    Runtime maps this texture onto a *closed* curve14 mesh; thin stroke-only art
    stays mostly transparent after triangulation — use a broad opaque fill.
    Arc matches readback path for silhouette; ellipse matches typical 'O' pupil coverage.
    """
    im = Image.new("RGBA", (PUPIL_OUT_W, PUPIL_OUT_H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)

    # Main eyeball: filled ellipse (SVG-ish proportions scaled to bitmap)
    cx = PUPIL_OUT_W * 0.5
    cy = PUPIL_OUT_H * 0.48
    rx = PUPIL_OUT_W * 0.36
    ry = PUPIL_OUT_H * 0.38
    dr.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(8, 8, 10, 255))

    # Readback lower lash arc (stroke) on top for shape cue
    n = 56
    pts = [cubic(i / (n - 1), P0, P1, P2, P3) for i in range(n)]
    pix = [map_pt(x, y, PUPIL_OUT_W, PUPIL_OUT_H) for x, y in pts]
    stroke = max(4.0, STROKE_W_SVG / (X1 - X0) * PUPIL_OUT_W)
    dr.line(pix, fill=(0, 0, 0, 255), width=int(round(stroke)), joint="curve")
    return im


def rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rgb565_to_bytes(v: int, swap16: bool) -> tuple[int, int]:
    """Same rules as scripts/image_converter.rgb565_to_bytes."""
    high_byte = (v >> 8) & 0xFF
    low_byte = v & 0xFF
    if swap16:
        return low_byte, high_byte
    return high_byte, low_byte


def image_to_rgb565a8(im: Image.Image, swap16: bool) -> tuple[bytes, int, int]:
    w, h = im.size
    im = im.convert("RGBA")
    px = im.load()
    rgb = bytearray(w * h * 2)
    alpha = bytearray(w * h)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            v = rgb565(r, g, b)
            b0, b1 = rgb565_to_bytes(v, swap16)
            rgb[i] = b0
            rgb[i + 1] = b1
            i += 2
            alpha[x + y * w] = a
    return bytes(rgb) + bytes(alpha), w, h


def raster_body_png(out_png: str) -> None:
    """360×360 body plate (readback paths, no eyes/antenna) for gfx_img BG."""
    cairosvg.svg2png(
        bytestring=_BODY_SVG.encode("utf-8"),
        write_to=out_png,
        output_width=LOBSTER_TEST_BODY_W,
        output_height=LOBSTER_TEST_BODY_H,
    )


def emit_c_inc(name: str, data: bytes, w: int, h: int, note: str) -> str:
    lines = [
        f"/* {name}: {w}x{h} RGB565A8 — {note} */",
        f"static const uint8_t s_{name}_map[] = {{",
    ]
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};")
    lines.append(f"static const gfx_image_dsc_t s_{name} = {{")
    lines.append("    .header.cf = GFX_COLOR_FORMAT_RGB565A8,")
    lines.append("    .header.magic = C_ARRAY_HEADER_MAGIC,")
    lines.append(f"    .header.w = {w},")
    lines.append(f"    .header.h = {h},")
    lines.append(f"    .header.stride = {w * 2},")
    lines.append(f"    .data_size = {len(data)},")
    lines.append(f"    .data = s_{name}_map,")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Rasterize readback lobster textures for test_apps.")
    parser.add_argument(
        "--swap16",
        action="store_true",
        help="RGB565 byte order: match scripts/image_converter.py --swap16 ([low, high] per pixel). "
        "Default matches image_converter without --swap16 ([high, low]).",
    )
    args = parser.parse_args()
    swap16 = bool(args.swap16)

    root = os.path.dirname(os.path.abspath(__file__))
    eye = draw_eye_rgba()
    pupil = draw_pupil_rgba()
    eye.save(os.path.join(root, "lobster_test_eye_tex.png"))
    pupil.save(os.path.join(root, "lobster_test_pupil_tex.png"))

    body_png = os.path.join(root, "lobster_test_body_tex.png")
    raster_body_png(body_png)
    body_im = Image.open(body_png).convert("RGBA")

    d_eye, w_e, h_e = image_to_rgb565a8(eye, swap16)
    d_pu, w_p, h_p = image_to_rgb565a8(pupil, swap16)
    d_body, w_b, h_b = image_to_rgb565a8(body_im, swap16)

    inc_path = os.path.join(root, "lobster_test_mesh_tex.inc")
    header = f"""/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "widget/gfx_img.h"

/* Eye white + pupil arc rasterized from test_apps/UI/ip_svg_parts_readback.html (part-eye-left). */
/* Regenerate: python3 test_apps/main/gen_lobster_readback_textures.py{" --swap16" if swap16 else ""} */

"""
    body_inc = "\n".join(
        [
            emit_c_inc("lobster_test_eye_tex", d_eye, w_e, h_e, "part-eye-left"),
            "",
            emit_c_inc("lobster_test_pupil_tex", d_pu, w_p, h_p, "part-eye-left pupil"),
        ]
    )
    with open(inc_path, "w", encoding="utf-8") as f:
        f.write(header + "\n" + body_inc + "\n")

    body_tex_path = os.path.join(root, "lobster_test_body_tex.inc")
    body_header = f"""/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "widget/gfx_img.h"

/* Body plate (claw/head/body/tail/dots, no eyes/antenna) from ip_svg_parts_readback.html — 360×360. */
/* Regenerate: python3 test_apps/main/gen_lobster_readback_textures.py{" --swap16" if swap16 else ""} */

"""
    with open(body_tex_path, "w", encoding="utf-8") as f:
        f.write(
            body_header
            + "\n"
            + emit_c_inc(
                "lobster_test_body_tex",
                d_body,
                w_b,
                h_b,
                "body plate (claw/head/body/tail/dots, no eyes/antenna)",
            )
            + "\n"
        )

    print(
        f"Wrote {inc_path}, {body_tex_path}, PNGs; eye {w_e}x{h_e}, pupil {w_p}x{h_p}, body {w_b}x{h_b}; "
        f"RGB565 swap16={'on' if swap16 else 'off'} (image_converter {'--swap16' if swap16 else 'default'})"
    )


if __name__ == "__main__":
    main()
