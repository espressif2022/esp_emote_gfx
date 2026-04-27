#!/usr/bin/env python3
"""
lottie2inc.py — Convert a Lottie JSON animation to the gfx_sm_scene .inc format.

Usage:
    python3 lottie2inc.py face_emote_demo.json -o face_emote_out.inc
    python3 lottie2inc.py face_emote_demo.json -o face_emote_out.inc --prefix s_fe

The Lottie file may contain a custom "__gfx_sm" root field that stores:
  - viewbox, layout hints, segment kind/stroke_width per layer

If "__gfx_sm" is absent, the script auto-detects segment kinds:
  - Closed path + fill  → BEZIER_FILL
  - Closed path + stroke → BEZIER_LOOP
  - Open path           → BEZIER_STRIP

Poses are sampled at Lottie marker times.  Each marker maps to one clip:
  - marker.cm = "english_name|中文名"  (| separator optional)
  - marker.dr = hold_ticks
"""

import argparse
import json
import sys
from pathlib import Path

KIND_C = {
    "CAPSULE":      "GFX_SM_SEG_CAPSULE",
    "RING":         "GFX_SM_SEG_RING",
    "BEZIER_STRIP": "GFX_SM_SEG_BEZIER_STRIP",
    "BEZIER_LOOP":  "GFX_SM_SEG_BEZIER_LOOP",
    "BEZIER_FILL":  "GFX_SM_SEG_BEZIER_FILL",
}

# ── Lottie helpers ──────────────────────────────────────────────────

def lottie_shape_to_ctrl_pts(shape):
    """
    Convert Lottie shape {v, i, o, c} to gfx_sm flat control-point list.

    Lottie stores: anchor vertices (v), in-tangents (i, relative), out-tangents (o, relative).
    gfx_sm stores: cubic Bezier control points in n=3k+1 format:
        [p0, cp1, cp2, p1, cp3, cp4, p2, ...]

    For a closed path with N Lottie vertices → N cubic segments → 3N+1 control points.
    For an open path with N Lottie vertices → N-1 cubic segments → 3(N-1)+1 control points.
    """
    v = shape["v"]
    i_tan = shape["i"]
    o_tan = shape["o"]
    closed = shape.get("c", False)
    n = len(v)
    seg_count = n if closed else (n - 1)

    pts = []
    for seg in range(seg_count):
        j = (seg + 1) % n
        p0 = v[seg]
        cp1 = [v[seg][0] + o_tan[seg][0], v[seg][1] + o_tan[seg][1]]
        cp2 = [v[j][0] + i_tan[j][0],     v[j][1] + i_tan[j][1]]
        p1 = v[j]

        if seg == 0:
            pts.append(p0)
        pts.append(cp1)
        pts.append(cp2)
        pts.append(p1)

    return pts, closed


def get_shape_at_frame(ks, frame):
    """Evaluate an animated shape property at a specific frame."""
    if not ks:
        return None
    if not ks.get("a", 0):
        k = ks["k"]
        return k if isinstance(k, dict) else (k[0] if isinstance(k, list) and k and isinstance(k[0], dict) else k)

    k_list = ks.get("k", [])
    if not k_list or not isinstance(k_list[0], dict):
        return k_list[0] if isinstance(k_list, list) and k_list else None

    for idx in range(len(k_list) - 1, -1, -1):
        if k_list[idx]["t"] <= frame:
            s = k_list[idx].get("s")
            return s[0] if isinstance(s, list) else s
    s = k_list[0].get("s")
    return s[0] if isinstance(s, list) else s


def find_shape_path_ks(layer):
    """Find the first shape path ('sh') keyframes in a shape layer."""
    for sh in layer.get("shapes", []):
        if sh.get("ty") == "sh":
            return sh.get("ks")
        if sh.get("ty") == "gr":
            for item in sh.get("it", []):
                if item.get("ty") == "sh":
                    return item.get("ks")
    return None


def detect_kind(layer, closed):
    """Auto-detect segment kind from shape styles (fill/stroke)."""
    has_fill = has_stroke = False
    for sh in layer.get("shapes", []):
        if sh.get("ty") == "fl":
            has_fill = True
        elif sh.get("ty") == "st":
            has_stroke = True
        elif sh.get("ty") == "gr":
            for item in sh.get("it", []):
                if item.get("ty") == "fl":
                    has_fill = True
                elif item.get("ty") == "st":
                    has_stroke = True
    if closed:
        return "BEZIER_FILL" if has_fill and not has_stroke else "BEZIER_LOOP"
    return "BEZIER_STRIP"


def layer_stroke_width(layer):
    """Extract stroke width from a shape layer."""
    for sh in layer.get("shapes", []):
        if sh.get("ty") == "st":
            w = sh.get("w", {})
            return int(w.get("k", 0)) if isinstance(w, dict) else int(w)
        if sh.get("ty") == "gr":
            for item in sh.get("it", []):
                if item.get("ty") == "st":
                    w = item.get("w", {})
                    return int(w.get("k", 0)) if isinstance(w, dict) else int(w)
    return 0

# ── Main ────────────────────────────────────────────────────────────

def convert(lot, prefix, asset_name):
    gfx = lot.get("__gfx_sm", {})
    prefix = gfx.get("prefix", prefix)
    asset_name = gfx.get("name", asset_name)
    P = prefix.upper()

    # Viewbox
    if "meta" in gfx:
        vb = gfx["meta"]["viewbox"]
        vx, vy, vw, vh = vb
    else:
        vx, vy, vw, vh = 0, 0, lot.get("w", 200), lot.get("h", 200)

    # Layout
    lay = gfx.get("layout", {})
    sw_def   = lay.get("stroke_width", 4)
    mirror_x = lay.get("mirror_x", 0)
    ground_y = lay.get("ground_y", vy + vh)
    period   = lay.get("timer_period_ms", 33)
    damp     = lay.get("damping_div", 4)

    # Segment config from __gfx_sm
    seg_cfg = {s["layer"]: s for s in gfx.get("segments", [])}

    # Ordered shape layers
    shape_layers = [l for l in lot.get("layers", []) if l.get("ty") == 4]
    if seg_cfg:
        order = [s["layer"] for s in gfx["segments"]]
        by_name = {l["nm"]: l for l in shape_layers}
        shape_layers = [by_name[n] for n in order if n in by_name]

    # Markers → frame list
    markers = sorted(lot.get("markers", []), key=lambda m: m["tm"])
    if not markers:
        times = set()
        for la in shape_layers:
            ks = find_shape_path_ks(la)
            if ks and ks.get("a"):
                for kf in ks.get("k", []):
                    if isinstance(kf, dict) and "t" in kf:
                        times.add(kf["t"])
        markers = [{"tm": t, "cm": "pose_%d" % i, "dr": 30}
                    for i, t in enumerate(sorted(times))]

    # ── Build segments & joint table ──
    segments = []
    joint_names = []
    j_off = 0
    for la in shape_layers:
        nm = la["nm"]
        ks = find_shape_path_ks(la)
        if not ks:
            continue
        shape0 = get_shape_at_frame(ks, markers[0]["tm"])
        if not shape0:
            continue
        pts, closed = lottie_shape_to_ctrl_pts(shape0)
        n_pts = len(pts)

        cfg = seg_cfg.get(nm, {})
        kind = cfg.get("kind", detect_kind(la, closed))
        sw   = cfg.get("stroke_width", layer_stroke_width(la))

        for i in range(n_pts):
            joint_names.append("%s_%d" % (nm, i))

        segments.append(dict(name=nm, kind=kind, joint_a=j_off,
                             joint_count=n_pts, stroke_width=sw, ks=ks))
        j_off += n_pts

    total_j = j_off

    # ── Build poses ──
    poses = []
    for mk in markers:
        fr = mk["tm"]
        coords = []
        for seg in segments:
            shape = get_shape_at_frame(seg["ks"], fr)
            if not shape:
                coords.extend([0, 0] * seg["joint_count"])
                continue
            pts, _ = lottie_shape_to_ctrl_pts(shape)
            for p in pts:
                coords.append(int(round(p[0])))
                coords.append(int(round(p[1])))
        poses.append(coords)

    # ── Parse marker names ──
    clip_info = []
    for mk in markers:
        parts = mk["cm"].split("|", 1)
        en = parts[0].strip()
        cn = parts[1].strip() if len(parts) > 1 else en
        clip_info.append((en, cn, int(mk.get("dr", 30))))

    # ── Generate .inc text ──
    L = []

    # Header
    L.append("/*")
    L.append(" * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD")
    L.append(" *")
    L.append(" * SPDX-License-Identifier: Apache-2.0")
    L.append(" *")
    L.append(" * %s.inc — auto-generated from Lottie JSON by lottie2inc.py" % asset_name)
    L.append(" *")
    L.append(" * Format version: GFX_SM_SCENE_SCHEMA_VERSION (2)")
    L.append(" *")
    L.append(" * Segments:")
    L.append(" *   %-12s %-16s %-16s %s" % ("Segment", "Joints", "Kind", "Notes"))
    L.append(" *   %s  %s %s %s" % ("─"*12, "─"*16, "─"*16, "─"*12))
    for s in segments:
        ja, jc = s["joint_a"], s["joint_count"]
        jr = "%d-%d (%d pts)" % (ja, ja+jc-1, jc)
        notes = "sw=%d" % s["stroke_width"] if s["stroke_width"] else "solid colour"
        L.append(" *   %-12s %-16s %-16s %s" % (s["name"], jr, s["kind"], notes))
    L.append(" */")
    L.append("")
    L.append("#pragma once")
    L.append("")
    L.append('#include "widget/gfx_sm_scene.h"')
    L.append("")

    # ── Metadata ──
    sec = lambda title: (
        L.append("/* %s */" % ("─"*66)),
        L.append("/*  %-65s*/" % title),
        L.append("/* %s */" % ("─"*66)),
        L.append(""),
    )
    sec("Metadata")
    L.append("static const gfx_sm_meta_t %s_meta = {" % prefix)
    L.append("    .version   = GFX_SM_SCENE_SCHEMA_VERSION,")
    L.append("    .viewbox_x = %d," % vx)
    L.append("    .viewbox_y = %d," % vy)
    L.append("    .viewbox_w = %4d," % vw)
    L.append("    .viewbox_h = %4d," % vh)
    L.append("};")
    L.append("")

    # ── Layout ──
    sec("Layout / rendering hints")
    L.append("static const gfx_sm_layout_t %s_layout = {" % prefix)
    L.append("    .stroke_width    = %3d," % sw_def)
    L.append("    .mirror_x        = %3d," % mirror_x)
    L.append("    .ground_y        = %3d," % ground_y)
    L.append("    .timer_period_ms = %3d," % period)
    L.append("    .damping_div     = %3d," % damp)
    L.append("};")
    L.append("")

    # ── Joint names ──
    sec("Joint names")
    L.append("static const char *const %s_joint_names[] = {" % prefix)
    for idx, jn in enumerate(joint_names):
        L.append('    "%s",%s/* %d */' % (jn, " "*(20-len(jn)), idx))
    L.append("};")
    L.append("")

    # ── Poses ──
    sec("Poses — flat coord arrays [x0,y0, x1,y1, …] for %d joints" % total_j)
    for i, coords in enumerate(poses):
        en = clip_info[i][0]
        cs = ", ".join(str(c) for c in coords)
        L.append("static const int16_t %s_p%02d[] = { %s };  /* %s */" % (prefix, i, cs, en))
    L.append("")
    L.append("#define %s_POSE(arr) { .coords = (arr) }" % P)
    L.append("")
    L.append("static const gfx_sm_pose_t %s_poses[] = {" % prefix)
    row = []
    for i in range(len(poses)):
        row.append("%s_POSE(%s_p%02d)" % (P, prefix, i))
        if len(row) == 3 or i == len(poses) - 1:
            L.append("    " + ",  ".join(row) + ",")
            row = []
    L.append("};")
    L.append("")

    # ── Segments ──
    sec("Segment wiring")
    L.append("static const gfx_sm_segment_t %s_segments[] = {" % prefix)
    for s in segments:
        fields = [".kind = %s" % KIND_C[s["kind"]],
                  ".joint_a = %2d" % s["joint_a"]]
        if s["kind"] in ("BEZIER_STRIP", "BEZIER_LOOP", "BEZIER_FILL"):
            fields.append(".joint_count = %d" % s["joint_count"])
        if s["stroke_width"]:
            fields.append(".stroke_width = %2d" % s["stroke_width"])
        L.append("    /* %-8s */ { %s }," % (s["name"], ", ".join(fields)))
    L.append("};")
    L.append("")

    # ── Steps ──
    sec("Clip step arrays")
    L.append("#define STEP(p, h, f) { .pose_index = (p), .hold_ticks = (h), "
             ".interp = GFX_SM_INTERP_DAMPED, .facing = (f) }")
    L.append("")
    L.append("static const gfx_sm_clip_step_t %s_steps[] = {" % prefix)
    for i, (en, cn, hold) in enumerate(clip_info):
        L.append("    STEP(%2d, %2d,  1),  /* %2d: %s */" % (i, hold, i, en))
    L.append("};")
    L.append("")

    # ── Clips ──
    sec("Clips")
    L.append("#define CLIP(idx, lp) \\")
    L.append("    { .steps = &%s_steps[(idx)], \\" % prefix)
    L.append("      .step_count = 1, .loop = (lp) }")
    L.append("")
    L.append("static const gfx_sm_clip_t %s_clips[] = {" % prefix)
    for i, (en, cn, _) in enumerate(clip_info):
        L.append("    CLIP(%2d, true),  /* %s */" % (i, en))
    L.append("};")
    L.append("")

    # ── Sequence ──
    sec("Default playback sequence — one entry per clip")
    seq_nums = ["%2d" % i for i in range(len(clip_info))]
    L.append("static const uint16_t %s_sequence[] = {" % prefix)
    for i in range(0, len(seq_nums), 10):
        chunk = ", ".join(seq_nums[i:i+10])
        L.append("    %s," % chunk)
    L.append("};")
    L.append("")

    # ── Top-level asset ──
    sec("Top-level asset")
    L.append("static const gfx_sm_asset_t %s_scene_asset = {" % prefix)
    L.append("    .meta           = &%s_meta," % prefix)
    L.append("    .joint_names    = %s_joint_names," % prefix)
    L.append("    .joint_count    = (uint8_t)(sizeof(%s_joint_names) / sizeof(%s_joint_names[0]))," % (prefix, prefix))
    L.append("    .segments       = %s_segments," % prefix)
    L.append("    .segment_count  = (uint8_t)(sizeof(%s_segments) / sizeof(%s_segments[0]))," % (prefix, prefix))
    L.append("    .poses          = %s_poses," % prefix)
    L.append("    .pose_count     = (uint16_t)(sizeof(%s_poses) / sizeof(%s_poses[0]))," % (prefix, prefix))
    L.append("    .clips          = %s_clips," % prefix)
    L.append("    .clip_count     = (uint16_t)(sizeof(%s_clips) / sizeof(%s_clips[0]))," % (prefix, prefix))
    L.append("    .sequence       = %s_sequence," % prefix)
    L.append("    .sequence_count = (uint16_t)(sizeof(%s_sequence) / sizeof(%s_sequence[0]))," % (prefix, prefix))
    L.append("    .layout         = &%s_layout," % prefix)
    L.append("};")
    L.append("")

    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="Input Lottie JSON file")
    ap.add_argument("-o", "--output", help="Output .inc file (default: stdout)")
    ap.add_argument("--prefix", default="s_out",
                    help="Variable name prefix (default: s_out, overridden by __gfx_sm.prefix)")
    ap.add_argument("--name", default=None,
                    help="Asset name for header comment (default: input filename stem)")
    args = ap.parse_args()

    with open(args.input, encoding="utf-8") as f:
        lot = json.load(f)

    name = args.name or Path(args.input).stem
    text = convert(lot, args.prefix, name)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
        print("Wrote %s" % args.output, file=sys.stderr)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
