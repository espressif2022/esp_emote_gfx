#!/usr/bin/env python3
"""Generate C keyframe tables from scripts/face_keyframes.json.

Output file is a C include fragment consumed by test_mouth_model.c.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List


def _load_json(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _fmt_eye_row(i: int, row: Dict[str, Any], label: str) -> str:
    return (
        f"    /* [{i}] {label:<7} */ "
        f"{{ {int(row['ry_top']):2d}, {int(row['ry_bottom']):3d}, {int(row['tilt']):3d} }},"
    )


def _fmt_brow_row(i: int, row: Dict[str, Any], label: str) -> str:
    return (
        f"    /* [{i}] {label:<7} */ "
        f"{{ {int(row['arch']):2d}, {int(row['raise']):3d}, {int(row['tilt']):3d} }},"
    )


def _fmt_mouth_row(i: int, row: Dict[str, Any], label: str) -> str:
    return (
        f"    /* [{i}] {label:<7} */ "
        f"{{ {int(row['smile']):3d}, {int(row['open']):2d} }},"
    )


def _fmt_emotion_row(row: Dict[str, Any]) -> str:
    return (
        f'    {{ "{row["name"]}", '
        f"{int(row['w_smile']):4d}, {int(row['w_happy']):4d}, {int(row['w_sad']):4d}, "
        f"{int(row['w_surprise']):4d}, {int(row['w_angry']):4d}, {int(row['hold_ticks']):3d}U }},"
    )


def _emit(config: Dict[str, Any], src_json: Path) -> str:
    refs = config["reference_keyframes"]
    seq: List[Dict[str, Any]] = config["sequence"]

    labels = ["neutral", "smile", "happy", "sad", "surprise", "angry"]
    if len(refs["eye"]) != 6 or len(refs["brow"]) != 6 or len(refs["mouth"]) != 6:
        raise ValueError("reference_keyframes eye/brow/mouth must all contain 6 entries")

    out: List[str] = []
    out.append("/* Auto-generated file. Do not edit manually. */")
    out.append(f"/* Source: {src_json.as_posix()} */")
    out.append("")

    out.append("static const face_eye_kf_t s_ref_eye[] = {")
    for i, row in enumerate(refs["eye"]):
        out.append(_fmt_eye_row(i, row, labels[i]))
    out.append("};")
    out.append("")

    out.append("static const face_brow_kf_t s_ref_brow[] = {")
    for i, row in enumerate(refs["brow"]):
        out.append(_fmt_brow_row(i, row, labels[i]))
    out.append("};")
    out.append("")

    out.append("static const face_mouth_kf_t s_ref_mouth[] = {")
    for i, row in enumerate(refs["mouth"]):
        out.append(_fmt_mouth_row(i, row, labels[i]))
    out.append("};")
    out.append("")

    out.append("static const face_emotion_t s_face_sequence[] = {")
    out.append("/*   name            Sm    Hp    Sd    Su    An   hold */")
    for row in seq:
        out.append(_fmt_emotion_row(row))
    out.append("};")
    out.append("")

    return "\n".join(out)


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    parser = argparse.ArgumentParser(description="Generate C keyframe tables from JSON config")
    parser.add_argument(
        "--config",
        type=Path,
        default=script_dir / "face_keyframes.json",
        help="Input JSON path",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=repo_root / "test_apps" / "main" / "test_mouth_model_keyframes.inc",
        help="Output C include path",
    )
    args = parser.parse_args()

    config = _load_json(args.config)
    text = _emit(config, args.config)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8")
    print(f"Wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
