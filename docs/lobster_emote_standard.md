# Lobster Emote Standard

## Goal

This document defines the standardized lobster emote asset model used by:

- `test_apps/UI/lobster_face_animator.html`
- `test_apps/main/lobster_emote_export.inc`
- `src/widget/lobster/gfx_lobster_emote*.c`

The goal is to reduce per-scene tuning by making web preview and device runtime
follow the same semantic model.

## Version

Current export version:

- `GFX_LOBSTER_EMOTE_EXPORT_VERSION = 2`

## Asset Sections

A valid standardized export contains these sections:

1. `s_lobster_export_meta`
2. `s_lobster_export_layout`
3. `s_lobster_export_semantics` (required in v2)
4. shape bases
5. `s_lobster_expr_sequence`
6. optional embedded background image

## Export Meta

`gfx_lobster_emote_export_meta_t` defines the design-space contract:

- `version`
- `design_viewbox_x`
- `design_viewbox_y`
- `design_viewbox_w`
- `design_viewbox_h`
- `export_width`
- `export_height`
- `export_scale`
- `export_offset_x`
- `export_offset_y`

Rules:

- `export_width/export_height` describe the intended output canvas
- `design_viewbox_*` describes the source design coordinate space
- runtime scale should be derived from this metadata before applying local part transforms

## Layout

`gfx_lobster_emote_layout_t` contains anchor points in exported pixel space:

- eyes
- pupils
- mouth
- antennas

Rules:

- `layout` is anchor-only
- `layout` does not contain export metadata
- if `layout` exists, `export_meta` must also exist

## Semantics

`gfx_lobster_emote_semantics_t` contains the exported runtime semantic contract for parity-sensitive behavior.

It covers:

- emotion axis coefficients for `smile / happy / sad / surprise / angry`
- pose derivation coefficients for eye / pupil / mouth / antenna transforms
- look and pupil clamp ranges
- render multipliers such as eye scale multiplier and antenna thickness base
- widget timing defaults such as `timer_period_ms` and `damping_div`
- preferred mesh segment counts for eye / pupil / antenna rendering

Rules:

- v2 exports must provide `s_lobster_export_semantics`
- runtime requires exported semantics and does not keep legacy fallback behavior


The following local curves are defined relative to their local anchor:

- `s_lobster_eye_white_base`
- `s_lobster_pupil_base`
- `s_lobster_mouth_base`
- `s_lobster_antenna_left_base`
- `s_lobster_antenna_right_base`

Rules:

- these are local coordinates, not global pixel coordinates
- runtime applies anchor position first, then local shape scaling / translation rules

## Sequence

`s_lobster_expr_sequence` defines:

- blend weights
- look offsets
- pupil shape
- hold time

Recommended parity states:

- `00 neutral`
- `07 happy`
- `16 surprise`
- `21 angry`

These should be used as the default reference set for web/device comparison.

## Rendering Semantics

The semantic target is:

1. derive runtime scale from `export_meta`
2. place each part using `layout`
3. apply local motion offsets
4. apply local curve data
5. render background separately from emote curves

Current known limitation:

- browser SVG rendering and device mesh rendering are not identical rasterizers
- parity work should focus first on anchor position, scale, and motion semantics
- small edge/AA differences are expected

## Validation

Use:

```bash
python3 scripts/validate_lobster_export.py test_apps/main/lobster_emote_export.inc
```

Checks:

- required sections exist
- required layout/meta fields exist
- embedded image size matches export size
- version is supported
