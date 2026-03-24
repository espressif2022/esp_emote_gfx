#!/usr/bin/env python3
"""Local keyframe inspector for face controls.

This tool extracts emotion/keyframe definitions from a JSON config and computes
per-widget target keyframes and sampled control points (mouth/eyes/brows).

Usage examples:
  python3 scripts/face_keyframe_debug.py --expr happy
  python3 scripts/face_keyframe_debug.py --index 12 --dump-json /tmp/frame.json
  python3 scripts/face_keyframe_debug.py --all --dump-json /tmp/all_frames.json
  python3 scripts/face_keyframe_debug.py --expr sad --plot /tmp/sad.png
"""

from __future__ import annotations

import argparse
import json
import math
import tempfile
import warnings
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Tuple


@dataclass
class EyeKF:
    ry_top: int
    ry_bottom: int
    tilt: int


@dataclass
class BrowKF:
    arch: int
    raise_: int
    tilt: int


@dataclass
class MouthKF:
    smile: int
    open_: int


@dataclass
class Emotion:
    name: str
    w_smile: int
    w_happy: int
    w_sad: int
    w_surprise: int
    w_angry: int
    hold_ticks: int


def clamp(v: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, v))


def smooth_tent(i: int, n: int) -> int:
    if n == 0:
        return 0
    half_i = 2 * (i if i <= n // 2 else (n - i))
    x = (half_i * 1000) // n
    return (x * x * (3000 - 2 * x)) // 1_000_000


def select_mouth_shape(smile: int, open_: int, solver: Dict[str, Any]) -> str:
    if open_ >= solver["surprise_open"]:
        return "surprise_o"
    if smile >= solver["smile_thresh"]:
        return "smile_u"
    if smile <= -solver["sad_thresh"]:
        return "sad_n"
    return "idle"


def select_mouth_mode(smile: int, open_: int, solver: Dict[str, Any]) -> str:
    if open_ >= solver["mode_thresh"]:
        return "open"
    if smile >= solver["mode_thresh"]:
        return "smile"
    return "flat"


def blend_field(tbl: List[Dict[str, int]], field: str, em: Emotion) -> int:
    n = tbl[0][field]
    return (
        n
        + (tbl[1][field] - n) * em.w_smile // 100
        + (tbl[2][field] - n) * em.w_happy // 100
        + (tbl[3][field] - n) * em.w_sad // 100
        + (tbl[4][field] - n) * em.w_surprise // 100
        + (tbl[5][field] - n) * em.w_angry // 100
    )


def blend_emotion(config: Dict[str, Any], em: Emotion) -> Tuple[EyeKF, BrowKF, MouthKF]:
    refs = config["reference_keyframes"]
    solver = config["solver"]

    eye = EyeKF(
        ry_top=blend_field(refs["eye"], "ry_top", em),
        ry_bottom=blend_field(refs["eye"], "ry_bottom", em),
        tilt=blend_field(refs["eye"], "tilt", em),
    )
    brow = BrowKF(
        arch=blend_field(refs["brow"], "arch", em),
        raise_=blend_field(refs["brow"], "raise", em),
        tilt=blend_field(refs["brow"], "tilt", em),
    )
    mouth = MouthKF(
        smile=clamp(blend_field(refs["mouth"], "smile", em), -solver["smile_limit"], solver["smile_limit"]),
        open_=clamp(blend_field(refs["mouth"], "open", em), 0, solver["open_limit"]),
    )
    return eye, brow, mouth


def build_eye_points(cx: int, cy: int, rx: int, ry_top: int, ry_bottom: int, tilt_left: int, segs: int) -> Dict[str, List[List[int]]]:
    upper: List[List[int]] = []
    lower: List[List[int]] = []
    ry_top = max(2, ry_top)
    ry_bottom = max(2, ry_bottom)

    for i in range(segs + 1):
        px = cx - rx + ((2 * rx * i) // segs)
        # Circular arc weight for rounder eye silhouette than parabola.
        u = (2.0 * i / segs) - 1.0  # -1..1
        edge_w = int(max(0.0, math.sqrt(max(0.0, 1.0 - u * u))) * 1000.0)
        tilt = tilt_left - (tilt_left * 2 * i) // segs
        upper_y = cy - (ry_top * edge_w // 1000) + tilt
        lower_y = cy + (ry_bottom * edge_w // 1000) + tilt
        upper.append([px, upper_y])
        lower.append([px, lower_y])

    return {"upper": upper, "lower": lower}


def build_brow_points(cx: int, cy: int, half_w: int, arch: int, tilt_left: int, thickness: int, segs: int) -> Dict[str, List[List[int]]]:
    top: List[List[int]] = []
    bottom: List[List[int]] = []
    thickness = max(1, thickness)

    for i in range(segs + 1):
        px = cx - half_w + ((2 * half_w * i) // segs)
        edge_w = smooth_tent(i, segs)
        tilt = tilt_left - (tilt_left * 2 * i) // segs
        mid_y = cy - (arch * edge_w // 1000) + tilt
        top.append([px, mid_y - thickness])
        bottom.append([px, mid_y + thickness])

    return {"top": top, "bottom": bottom}


def solve_mouth_curves(geom: Dict[str, Any], solver: Dict[str, Any], smile: int, open_: int) -> Dict[str, Any]:
    mouth_cx = geom["mouth_cx"]
    mouth_cy = geom["mouth_cy"]
    mouth_w = geom["mouth_obj_w"]
    mouth_h = geom["mouth_obj_h"]

    left_x = mouth_cx - mouth_w // 2
    right_x = mouth_cx + mouth_w // 2
    center_y = mouth_cy

    mode = select_mouth_mode(smile, open_, solver)
    mode_smile = smile - solver["mode_thresh"] if mode == "smile" else smile
    mode_open = open_ - solver["mode_thresh"] if mode == "open" else open_

    width_bias = clamp(mode_smile // 2 - (mode_open // 2), -solver["width_limit"], solver["width_limit"])
    smile_corner_lift = (mode_smile * 3) // 4
    open_corner_drop = mode_open // 6
    center_drop = (mode_open * 4) // 5 - (mode_smile // 8)

    left_x += -width_bias
    right_x += width_bias
    center_y += center_drop

    width_trim = max(0, mode_open // 6)
    left_x += width_trim
    right_x -= width_trim

    line_half_thickness = 2 + (mode_open // 24)
    min_aperture = line_half_thickness + 1
    shape = select_mouth_shape(smile, open_, solver)

    segs = solver["dense_segments"]
    x_points: List[int] = []
    upper_curve: List[int] = []
    lower_curve: List[int] = []

    for i in range(segs + 1):
        x = left_x + ((right_x - left_x) * i) // segs
        cw = smooth_tent(i, segs)
        mid_y = center_y
        aperture_upper = min_aperture
        aperture_lower = min_aperture

        if shape == "sad_n":
            sad_mag = abs(clamp(mode_smile, -solver["smile_limit"], 0))
            mid_y = center_y + (sad_mag // 4) - (cw * ((sad_mag // 4) + (sad_mag // 2)) // 1000)
            aperture_upper = line_half_thickness + 1 + (mode_open // 14) + (cw * (1 + mode_open // 12) // 1000)
            aperture_lower = line_half_thickness + 2 + (mode_open // 10) + (cw * (2 + mode_open // 8) // 1000)
        elif shape == "smile_u":
            smile_mag = max(0, mode_smile)
            mid_y = center_y - (smile_mag // 4) + (cw * ((smile_mag // 4) + (smile_mag // 2)) // 1000)
            aperture_upper = line_half_thickness + 1 + (mode_open // 14) + (cw * (1 + mode_open // 12) // 1000)
            aperture_lower = line_half_thickness + 2 + (mode_open // 10) + (cw * (3 + mode_open // 8) // 1000)
        elif shape == "surprise_o":
            mid_y = center_y + (cw * (mode_open // 10) // 1000)
            half_gap = 4 + (mode_open // 3) + (cw * (3 + mode_open // 4) // 1000)
            aperture_upper = (half_gap * 9) // 10
            aperture_lower = (half_gap * 11) // 10
        else:
            mid_y = center_y - (cw // 1000)
            aperture_upper = line_half_thickness + 1
            aperture_lower = line_half_thickness + 1

        aperture_upper = max(aperture_upper, min_aperture)
        aperture_lower = max(aperture_lower, min_aperture)

        x_points.append(x)
        upper_curve.append(mid_y - aperture_upper)
        lower_curve.append(mid_y + aperture_lower)

    return {
        "mode": mode,
        "shape": shape,
        "mode_open": mode_open,
        "line_half_thickness": line_half_thickness,
        "anchors": {
            "left_corner": [x_points[0], (upper_curve[0] + lower_curve[0]) // 2],
            "right_corner": [x_points[-1], (upper_curve[-1] + lower_curve[-1]) // 2],
            "upper_lip_center": [mouth_cx, min(upper_curve)],
            "lower_lip_center": [mouth_cx, max(lower_curve)],
            "mouth_center": [mouth_cx, center_y],
        },
        "upper_curve": [[x_points[i], upper_curve[i]] for i in range(segs + 1)],
        "lower_curve": [[x_points[i], lower_curve[i]] for i in range(segs + 1)],
    }


def build_frame(config: Dict[str, Any], em: Emotion, index: int) -> Dict[str, Any]:
    geom = config["geometry"]
    solver = config["solver"]

    eye, brow, mouth = blend_emotion(config, em)

    mcx = geom["mouth_cx"]
    mcy = geom["mouth_cy"]
    eye_l_cx = mcx - geom["eye_x_half_gap"]
    eye_r_cx = mcx + geom["eye_x_half_gap"]
    eye_cy = mcy + geom["eye_y_ofs"]
    brow_cy = eye_cy + geom["brow_y_ofs_extra"] - brow.raise_

    left_eye = build_eye_points(
        eye_l_cx, eye_cy, solver["eye_rx"], eye.ry_top, eye.ry_bottom, eye.tilt, solver["eye_segments"]
    )
    right_eye = build_eye_points(
        eye_r_cx, eye_cy, solver["eye_rx"], eye.ry_top, eye.ry_bottom, -eye.tilt, solver["eye_segments"]
    )

    left_brow = build_brow_points(
        eye_l_cx,
        brow_cy,
        solver["brow_half_w"],
        brow.arch,
        brow.tilt,
        solver["brow_thickness"],
        solver["brow_segments"],
    )
    right_brow = build_brow_points(
        eye_r_cx,
        brow_cy,
        solver["brow_half_w"],
        brow.arch,
        -brow.tilt,
        solver["brow_thickness"],
        solver["brow_segments"],
    )

    mouth_curves = solve_mouth_curves(geom, solver, mouth.smile, mouth.open_)

    return {
        "index": index,
        "emotion": asdict(em),
        "preview": {
            "display_w": int(geom["display_w"]),
            "display_h": int(geom["display_h"]),
        },
        "solver": {
            "mode_thresh": int(solver["mode_thresh"]),
            "corner_rows": int(solver.get("corner_rows", 6)),
        },
        "keyframes": {
            "eye": asdict(eye),
            "brow": {"arch": brow.arch, "raise": brow.raise_, "tilt": brow.tilt},
            "mouth": {"smile": mouth.smile, "open": mouth.open_},
        },
        "anchors": {
            "mouth_center": [mcx, mcy],
            "left_eye_center": [eye_l_cx, eye_cy],
            "right_eye_center": [eye_r_cx, eye_cy],
            "left_brow_center": [eye_l_cx, brow_cy],
            "right_brow_center": [eye_r_cx, brow_cy],
        },
        "mouth": mouth_curves,
        "left_eye": left_eye,
        "right_eye": right_eye,
        "left_brow": left_brow,
        "right_brow": right_brow,
    }


def _pick_emotion(sequence: List[Emotion], expr: str | None, index: int | None) -> Tuple[int, Emotion]:
    if index is not None:
        if not (0 <= index < len(sequence)):
            raise ValueError(f"index out of range: {index}, valid [0..{len(sequence)-1}]")
        return index, sequence[index]

    if expr is None:
        return 0, sequence[0]

    for i, em in enumerate(sequence):
        if em.name == expr:
            return i, em
    raise ValueError(f"expression not found: {expr!r}")


def _load_config(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _load_sequence(config: Dict[str, Any]) -> List[Emotion]:
    out: List[Emotion] = []
    for it in config["sequence"]:
        out.append(
            Emotion(
                name=it["name"],
                w_smile=int(it["w_smile"]),
                w_happy=int(it["w_happy"]),
                w_sad=int(it["w_sad"]),
                w_surprise=int(it["w_surprise"]),
                w_angry=int(it["w_angry"]),
                hold_ticks=int(it["hold_ticks"]),
            )
        )
    return out


def _maybe_plot(frame: Dict[str, Any], out_png: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except Exception as e:  # pragma: no cover
        raise RuntimeError("matplotlib is required for --plot, try: pip install matplotlib") from e

    fig, ax = plt.subplots(figsize=(6, 6), dpi=160)
    _draw_frame(ax, frame, show_title=True, title_size=12)
    fig.patch.set_facecolor("black")
    plt.tight_layout(pad=0.4)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_png)
    plt.close(fig)


def _plot_all_frames(frames: List[Dict[str, Any]], out_dir: Path) -> List[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    paths: List[Path] = []
    for frame in frames:
        name = frame["emotion"]["name"]
        idx = frame["index"]
        out_png = out_dir / f"{idx:02d}_{name}.png"
        _maybe_plot(frame, out_png)
        paths.append(out_png)
    return paths


def _build_contact_sheet(frames: List[Dict[str, Any]], out_png: Path, columns: int = 6) -> None:
    try:
        import matplotlib.pyplot as plt
    except Exception as e:  # pragma: no cover
        raise RuntimeError("matplotlib is required for --contact-sheet, try: pip install matplotlib") from e

    if len(frames) == 0:
        raise ValueError("no frames to render in contact sheet")

    cols = max(1, columns)
    rows = (len(frames) + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(cols * 2.6, rows * 2.6), dpi=180)
    if rows == 1 and cols == 1:
        axes_list = [axes]
    elif rows == 1:
        axes_list = list(axes)
    elif cols == 1:
        axes_list = [ax for ax in axes]
    else:
        axes_list = [ax for row in axes for ax in row]

    for i, ax in enumerate(axes_list):
        if i >= len(frames):
            ax.axis("off")
            continue
        frame = frames[i]
        _draw_frame(ax, frame, show_title=True, title_size=8)

    fig.patch.set_facecolor("black")
    plt.tight_layout(pad=0.4)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_png)
    plt.close(fig)


def _polygon_from_bands(top: List[List[int]], bottom: List[List[int]]) -> Tuple[List[int], List[int]]:
    poly = top + list(reversed(bottom))
    return [p[0] for p in poly], [p[1] for p in poly]


def _strip_from_centerline(points: List[List[int]], half_t: List[int]) -> Tuple[List[int], List[int]]:
    top: List[List[int]] = []
    bottom: List[List[int]] = []
    for i, p in enumerate(points):
        t = max(1, int(half_t[i]))
        top.append([p[0], p[1] - t])
        bottom.append([p[0], p[1] + t])
    return _polygon_from_bands(top, bottom)


def _inset_polygon(points: List[List[int]], scale: float) -> List[List[int]]:
    cx = sum(p[0] for p in points) / float(len(points))
    cy = sum(p[1] for p in points) / float(len(points))
    out: List[List[int]] = []
    for p in points:
        out.append([int(round(cx + (p[0] - cx) * scale)), int(round(cy + (p[1] - cy) * scale))])
    return out


def _corner_arc_polygon(
    p0: List[int], p1: List[int], p2: List[int], half_t: int, rows: int = 6
) -> Tuple[List[int], List[int]]:
    left_edge: List[List[int]] = []
    right_edge: List[List[int]] = []
    den = max(1, rows)
    for i in range(den + 1):
        t = i / float(den)
        omt = 1.0 - t
        bx = int(round(omt * omt * p0[0] + 2.0 * t * omt * p1[0] + t * t * p2[0]))
        by = int(round(omt * omt * p0[1] + 2.0 * t * omt * p1[1] + t * t * p2[1]))
        left_edge.append([bx - half_t, by])
        right_edge.append([bx + half_t, by])
    poly = left_edge + list(reversed(right_edge))
    return [p[0] for p in poly], [p[1] for p in poly]


def _draw_frame(ax: Any, frame: Dict[str, Any], show_title: bool, title_size: int) -> None:
    display_w = int(frame["preview"]["display_w"])
    display_h = int(frame["preview"]["display_h"])

    ax.set_facecolor("black")
    ax.set_aspect("equal")
    ax.set_xlim(0, display_w)
    ax.set_ylim(display_h, 0)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    upper = frame["mouth"]["upper_curve"]
    lower = frame["mouth"]["lower_curve"]
    half_t = max(1, int(frame["mouth"].get("line_half_thickness", 2)))
    up_t = [half_t for _ in upper]
    lo_t = [half_t for _ in lower]
    ux, uy = _strip_from_centerline(upper, up_t)
    lx, ly = _strip_from_centerline(lower, lo_t)
    ax.fill(ux, uy, color="white", linewidth=0)
    ax.fill(lx, ly, color="white", linewidth=0)

    # Corner arcs to match C runtime's left/right quadratic-Bézier connectors.
    mode_open = int(frame["mouth"].get("mode_open", frame["keyframes"]["mouth"]["open"]))
    corner_dx = 3 + (mode_open // 12)
    corner_rows = int(frame.get("solver", {}).get("corner_rows", 6))
    left_x = upper[0][0]
    right_x = upper[-1][0]
    left_mid_y = (upper[0][1] + lower[0][1]) // 2
    right_mid_y = (upper[-1][1] + lower[-1][1]) // 2

    lpx, lpy = _corner_arc_polygon(
        [left_x, upper[0][1] - half_t],
        [left_x - corner_dx, left_mid_y],
        [left_x, lower[0][1] + half_t],
        half_t,
        rows=corner_rows,
    )
    rpx, rpy = _corner_arc_polygon(
        [right_x, upper[-1][1] - half_t],
        [right_x + corner_dx, right_mid_y],
        [right_x, lower[-1][1] + half_t],
        half_t,
        rows=corner_rows,
    )
    ax.fill(lpx, lpy, color="white", linewidth=0)
    ax.fill(rpx, rpy, color="white", linewidth=0)

    # Eyes: fill closed eye lens polygons.
    for eye in (frame["left_eye"], frame["right_eye"]):
        ex, ey = _polygon_from_bands(eye["upper"], eye["lower"])
        ax.fill(ex, ey, color="white", linewidth=0)

    # Brows: fill brow strip polygons.
    for brow in (frame["left_brow"], frame["right_brow"]):
        bx, by = _polygon_from_bands(brow["top"], brow["bottom"])
        ax.fill(bx, by, color="white", linewidth=0)

    if show_title:
        ax.set_title(
            f"{frame['index']:02d} {frame['emotion']['name']}",
            fontsize=title_size,
            color="white",
            pad=4,
        )


def _build_gif_from_pngs(png_paths: List[Path], out_gif: Path, frame_ms: int, loop: int) -> None:
    try:
        from PIL import Image
    except Exception as e:  # pragma: no cover
        raise RuntimeError("Pillow is required for --gif, try: pip install Pillow") from e

    if len(png_paths) == 0:
        raise ValueError("no PNG frames provided for GIF export")

    images = [Image.open(str(p)).convert("RGBA") for p in png_paths]
    out_gif.parent.mkdir(parents=True, exist_ok=True)
    with warnings.catch_warnings():
        warnings.filterwarnings(
            "ignore",
            message="Palette images with Transparency expressed in bytes should be converted to RGBA images",
            category=UserWarning,
        )
        images[0].save(
            str(out_gif),
            save_all=True,
            append_images=images[1:],
            duration=max(20, frame_ms),
            loop=max(0, loop),
            optimize=False,
            disposal=2,
        )
    for img in images:
        img.close()


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="Inspect face keyframes and sampled coordinates.")
    parser.add_argument("--config", type=Path, default=script_dir / "face_keyframes.json", help="JSON config path")
    parser.add_argument("--expr", type=str, default=None, help="Expression name in sequence")
    parser.add_argument("--index", type=int, default=None, help="Expression index in sequence")
    parser.add_argument("--all", action="store_true", help="Export all sequence entries")
    parser.add_argument("--dump-json", type=Path, default=None, help="Write result JSON to file")
    parser.add_argument("--plot", type=Path, default=None, help="Render one frame preview PNG (matplotlib)")
    parser.add_argument("--plot-all-dir", type=Path, default=None, help="Render all sequence frames into this directory")
    parser.add_argument("--contact-sheet", type=Path, default=None, help="Render all frames as one contact-sheet PNG")
    parser.add_argument("--sheet-cols", type=int, default=6, help="Contact-sheet columns, default 6")
    parser.add_argument("--gif", type=Path, default=None, help="Render all frames into one GIF (requires Pillow)")
    parser.add_argument("--gif-frame-ms", type=int, default=140, help="GIF frame duration in ms, default 140")
    parser.add_argument("--gif-loop", type=int, default=0, help="GIF loop count (0 = infinite)")
    args = parser.parse_args()

    config = _load_config(args.config)
    sequence = _load_sequence(config)

    if args.all:
        frames = [build_frame(config, em, i) for i, em in enumerate(sequence)]
        result: Dict[str, Any] = {"count": len(frames), "frames": frames}
        out_paths: List[Path] = []
        if args.plot_all_dir is not None:
            out_paths = _plot_all_frames(frames, args.plot_all_dir)
            print(f"Wrote {len(out_paths)} frame PNGs to: {args.plot_all_dir}")
        if args.contact_sheet is not None:
            _build_contact_sheet(frames, args.contact_sheet, args.sheet_cols)
            print(f"Wrote contact sheet: {args.contact_sheet}")
        if args.gif is not None:
            if len(out_paths) == 0:
                with tempfile.TemporaryDirectory(prefix="face_preview_") as td:
                    tmp_paths = _plot_all_frames(frames, Path(td))
                    _build_gif_from_pngs(tmp_paths, args.gif, args.gif_frame_ms, args.gif_loop)
            else:
                _build_gif_from_pngs(out_paths, args.gif, args.gif_frame_ms, args.gif_loop)
            print(f"Wrote GIF: {args.gif}")
    else:
        idx, em = _pick_emotion(sequence, args.expr, args.index)
        frame = build_frame(config, em, idx)
        result = frame
        if args.plot is not None:
            _maybe_plot(frame, args.plot)

    output = json.dumps(result, ensure_ascii=False, indent=2)
    if args.dump_json is None:
        print(output)
    else:
        args.dump_json.parent.mkdir(parents=True, exist_ok=True)
        args.dump_json.write_text(output + "\n", encoding="utf-8")
        print(f"Wrote JSON: {args.dump_json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
