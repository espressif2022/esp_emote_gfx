# Render Format Design

This document defines the long-term RGB565/RGB888/ARGB render path for GFX.
It replaces the temporary "render RGB565, convert before flush" model with a
format-aware render surface model.

Current status as of 2026-06-16:

- RGB565, RGB565 swapped, RGB888, BGR888, and XRGB8888 displays now render
  directly into the same internal render format by default.
- The old "render RGB565, convert before flush" flow remains only as a
  fallback for transitional paths where `render_format != output_format`.
- Widget solid fill, text mask, image blend, mesh raster, and anim block write
  paths have been migrated away from `gfx_color_t *` destination casts.

## Goals

- A display can render directly into RGB565, RGB565 swapped, RGB888, BGR888,
  XRGB8888, or ARGB8888 buffers.
- RGB888 image sources keep 24-bit color precision when the destination is
  RGB888 or XRGB8888.
- ARGB8888 and XRGB8888 are handled explicitly and efficiently.
- Plane-alpha formats such as RGB565A8 and RGB888A8 remain supported.
- Existing RGB565 paths keep their compact fast path and do not pay for generic
  per-pixel dispatch.

## References

ITU `itu_sw.c` is useful as a semantic reference. It branches on destination
format, then source format, and shows the expected ARGB8888 alpha formula when
blending into RGB565.

LVGL v9 is the better architectural reference. Its software renderer first
dispatches by destination format, then calls destination-specific source-format
handlers. RGB888 and XRGB8888 share the same implementation with a destination
pixel size of 3 or 4 bytes.

GFX should follow LVGL's dispatch shape while keeping the implementation much
smaller.

## Format Semantics

Color formats are buffer/image formats, not the same thing as `gfx_color_t`.
`gfx_color_t` remains a semantic RGB565 color for simple public style APIs.

RGB565 source descriptors are byte streams. `GFX_COLOR_FORMAT_RGB565` stores
the low byte first (`00 F8` for red), while `GFX_COLOR_FORMAT_RGB565_SWAPPED`
stores the high byte first (`F8 00`). Destination framebuffers are written as
native memory surfaces through the destination format writer. Do not cast a
destination buffer to `gfx_color_t *` unless the destination is explicitly an
RGB565-family render buffer.

For RGB565-family destination buffers, write bytes by `dest_format` byte-order
rules, not by host-endian `uint16_t` layout. This matters for
`RGB565_SWAPPED` when a target explicitly wants high-byte-first payload bytes.

Display configuration rule:

- Preferred: set `gfx_display_config_t.color_format` explicitly.
- Leaving `color_format` as `UNKNOWN` resolves to `GFX_COLOR_FORMAT_RGB565`.
- Use `GFX_COLOR_FORMAT_RGB565_SWAPPED` only when the target byte stream is
  explicitly high-byte first (`F8 00` for red). RGB framebuffer panels that
  use native little-endian RGB565 memory should use `GFX_COLOR_FORMAT_RGB565`.

LVGL v9 reference point:

- LVGL keeps the display color format as the renderer contract.
- For RGB565 byte-endian changes, LVGL documents `lv_draw_sw_rgb565_swap()` as
  a flush-callback step, not as a decoder/widget concern.
- GFX should follow the same boundary: decoders produce format-local image
  buffers, widgets sample by source format, and display/output byte order is
  handled only by `render_format` / `output_format` or the final flush bridge.

## Cleanup Direction

GFX should borrow LVGL's boundary, not its implementation layout. The useful
idea is that the layer/display format is a contract, and the renderer chooses
the correct path from that contract. GFX should also keep the ITU-style surface
boundary: every render/backend operation receives an explicit surface
description, not loose pointers plus hidden global state.

The old model to remove is:

- `gfx_draw_ctx_t.swap` as a second source of RGB565 byte order.
- decoder parameters such as `swap_color` and `swap_bytes`.
- widget code writing through `gfx_color_t *` or `GFX_DRAW_CTX_DEST_PTR()`.
- legacy helpers where the caller passes a raw 16bpp pointer plus a `bool swap`.

The new model is:

- `gfx_display_config_t.color_format` defines the display/output byte stream.
- `render_format` normally equals `output_format`.
- `gfx_draw_ctx_t.format` is the only render-target format seen by widgets.
- source image descriptors carry their own `header.cf`.
- software blend dispatches by destination format first, then source format.
- backend ops receive `format`, `stride`, `area`, `clip`, and buffer pointers as
  an explicit surface contract.

Board integrations must express byte order only through
`gfx_display_config_t.color_format`. When a board needs high-byte first RGB565
output, configure the display as `GFX_COLOR_FORMAT_RGB565_SWAPPED`.

### Target Shape

The final software renderer should look like this at the conceptual level:

```text
gfx_sw_blend_surface_fill(dst_surface, color, opa, mask)
  -> to_rgb565()
  -> to_rgb565_swapped()
  -> to_rgb888_or_bgr888()
  -> to_xrgb8888()

gfx_sw_blend_image(dst_surface, src_surface, opa, mask)
  -> to_rgb565(src rgb565/rgb565_swapped/rgb888/bgr888/xrgb8888/argb8888)
  -> to_rgb565_swapped(...)
  -> to_rgb888_or_bgr888(...)
  -> to_xrgb8888(...)
```

This is not a request to mirror LVGL's files. GFX can keep fewer functions and
smaller code, but the branch order should be the same: destination format is
selected once at the top, and source format handling stays inside that target
path. This keeps RGB565 performance local to the RGB565 target path and stops
RGB888 support from slowing down common 565 UI rendering.

### Surface Boundary

Borrow the ITU boundary, not its inheritance style:

- A render surface is `{ buf, width/height or area, stride, format, clip }`.
- A source surface/image is `{ data, width/height, stride, format, alpha }`.
- Backend draw ops use these surface descriptors for eligibility and fallback.
- Widgets do not know whether the final target is SPI, RGB LCD, memory, SDL,
  software, or PPA.

This makes hardware acceleration easier later: PPA/future backends can accept
or reject an explicit operation by format, stride, alignment, and area. The
software path remains the reference fallback.

### Migration Plan

Stage A: freeze the new contract.

- New code must use `ctx->format` or a surface descriptor.
- New decoder APIs must not accept target byte-order parameters.
- New widget code must not use `GFX_DRAW_CTX_DEST_PTR()` for direct 16bpp
  writes.
- Tests must assert that RGB565, RGB565_SWAPPED, RGB888, BGR888, and XRGB8888
  display configs produce matching `render_format` and `output_format`.

Stage B: delete old raw 16bpp helpers.

- Remove raw `gfx_color_t * + bool swap` helper entry points.
- Update remaining callers to use format-aware helpers only.
- Keep RGB565 performance through destination-format-specific fast paths, not
  through parallel legacy APIs.

Stage C: remove swap from decoders and widgets.

- Drop `swap_color` / `swap_bytes` from internal decoder call chains after
  compatibility call sites are migrated.
- Remove `ctx->swap` from image, mesh, anim, and render backend paths.
- Replace direct raw writes with `gfx_color_write_rgb565_bytes()` or a
  destination-format writer.

Stage D: restore performance per destination format.

- RGB565 opaque fill uses 16bpp batch fill.
- RGB565 text/mask blend gets a dedicated target path.
- RGB565 image scale gets a dedicated nearest-neighbor fast path or backend
  scale fallback.
- RGB888/BGR888/XRGB8888 keep direct byte writers and avoid RGB565 semantic
  round-trips.

Stage E: delete the old model.

- Remove `gfx_draw_ctx_t.swap`.
- Remove decoder `swap_*` parameters.
- Remove raw `gfx_color_t * + swap` helpers.
- Remove display/backend config `swap` flags so the renderer contract exposes
  only explicit color formats.

24-bit note:

- `GFX_COLOR_FORMAT_RGB888` means payload bytes are `R, G, B`.
- `GFX_COLOR_FORMAT_BGR888` means payload bytes are `B, G, R`.
- Some ESP-IDF RGB LCD pipelines label the panel input as `RGB888` while the
  actual byte payload is `BGR24`. In that case, configure GFX with
  `GFX_COLOR_FORMAT_BGR888` instead of swapping channels in the app flush
  callback.

Alpha must be split into two concepts:

- `pixel alpha`: alpha stored inside each pixel, for example ARGB8888.
- `plane alpha`: a separate A8 plane following the color payload, for example
  RGB565A8 and RGB888A8.

Helpers:

- `gfx_color_format_has_pixel_alpha(format)`
- `gfx_color_format_has_plane_alpha(format)`
- `gfx_color_format_has_alpha(format)` returns either of the two.

Plane-alpha formats use `stride * height` bytes for color payload, followed by
`width * height` bytes of A8 alpha. Packed ARGB/XRGB formats do not have a
separate alpha plane.

## Render Surface Model

`gfx_draw_ctx_t` carries:

- `buf`
- `buf_area`
- `clip_area`
- `stride` in pixels
- `format`
- `pixel_size`

All new software draw helpers should accept destination format explicitly, or
accept a small destination surface descriptor. Old helpers that take
`gfx_color_t *` are RGB565-only compatibility helpers.

The display owns:

- `render_format`: internal render buffer format.
- `output_format`: final backend/flush format.

In the current model, `render_format == output_format` for RGB565,
RGB565_SWAPPED, RGB888, BGR888, and XRGB8888 displays created through
`gfx_display_add()`. Conversion before flush is only a fallback for legacy
transitional paths.

## Dispatch Shape

Use target-format dispatch first:

```text
gfx_sw_blend_image()
  -> to_rgb565()
       -> src rgb565 / rgb888 / rgb888a8 / xrgb8888 / argb8888
  -> to_rgb888(dest_px_size = 3 or 4)
       -> src rgb565 / rgb888 / rgb888a8 / xrgb8888 / argb8888
```

Fast paths:

- RGB565 -> RGB565 with same byte order and no mask: row memcpy.
- RGB888 -> RGB888 and no mask/global opacity: row memcpy.
- XRGB8888 -> XRGB8888 and no mask/global opacity: row memcpy.
- RGB888 -> XRGB8888 and XRGB8888 -> RGB888: copy RGB channels, ignore X.

Alpha paths:

- Final opacity = source pixel alpha or plane alpha.
- If a mask is present, multiply by mask.
- If global opacity is not cover, multiply by global opacity.
- XRGB8888 always has source alpha 255.
- ARGB8888 uses source alpha from the high byte of the logical `0xAARRGGBB`
  value. On little-endian CPUs this means memory order is B, G, R, A.

## Staged Implementation

Stage 1:

- Add ARGB8888 and split alpha helpers.
- Add format-aware image draw and image scale helpers for RGB565/RGB888
  destinations.
- Make RGB888 display render directly to RGB888 buffer for image/fill paths.
- Keep legacy RGB565 helpers only as compatibility entry points while widget
  paths migrate to format-aware writers.

Status:

- Done for RGB888 and XRGB8888 display render buffers.

Stage 2:

- Migrate text fill/mask and solid fill to destination surface helpers.
- Migrate QR/button/list/pageflow/coverflow solid backgrounds to surface fill.
- Migrate shape/mesh rasterizers to destination surface helpers.
- Add tests for RGB888 text and shape paths.

Status:

- Done for text fill/mask, solid fill, QR, button, list, pageflow, coverflow,
  wheel, mesh raster, and anim block render paths.

Stage 3:

- Migrate motion rendering to format-aware writers if it adds a direct
  framebuffer raster path.
- Add XRGB8888 display output and ARGB8888 source image tests.

Status:

- Host and ESP-IDF smoke now assert `RGB888` and `XRGB8888` display
  `render_format`.
- Host and ESP-IDF memory-backend tests cover RGB888 and BGR888 output bytes.
- Host smoke covers XRGB8888 opaque source behavior and ARGB8888 pixel alpha
  blend into RGB888 output.
- XRGB8888 display-output precision tests are partially in place; a dedicated
  source-image to XRGB8888 output precision case is still pending.

Stage 4:

- Add backend/PPA capability mapping using the same source/destination formats.
- Add optional premultiplied ARGB8888 path only when assets or hardware need it.

## Testing

Required smoke coverage:

- RGB888 display with RGB888 memory backend preserves `R,G,B` bytes from a
  RGB888 source image.
- RGB888A8 source blends against a RGB888 background.
- ARGB8888 source blends against RGB565 and RGB888 backgrounds.
- XRGB8888 source copies as opaque.
- RGB565 display behavior stays byte-compatible with previous tests.

Current implemented smoke coverage:

- Host smoke:
  `simulation/host/gfx_host_alignment_smoke.c`
- ESP-IDF / Unity:
  `test_apps/main/test_backend.c`

These tests currently verify:

- RGB888 display output byte layout.
- RGB888 display internal `render_format == RGB888`.
- XRGB8888 display internal `render_format == XRGB8888`.
- RGB888 source preserves 24-bit precision on RGB888 output.
- XRGB8888 source copies as opaque.
- ARGB8888 source pixel alpha blends into RGB888 output.

Still pending:

- Explicit XRGB8888 destination precision test for RGB888/XRGB8888 source data.
- Full flush byte-order and stride checks for RGB565, RGB565_SWAPPED, RGB888
  with full-frame double buffer sync.
