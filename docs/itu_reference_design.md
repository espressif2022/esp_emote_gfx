# ITU Display Design Notes For GFX

This document records the parts of the ITU display framework that are worth
borrowing, and how GFX should adapt them without becoming a heavy scene engine.

## Goals

- Keep GFX lightweight and explicit.
- Keep assets, decoded pixels, render targets, and display backends separated.
- Leave room for ESP hardware acceleration such as PPA and future display
  engines.
- Make widget resource lifetime and input dispatch predictable.

## 1. Backend Capability Table

### ITU Reference

ITU exposes a direct display operation table around surface creation, blit,
stretch blit, alpha blend, transform, glyph drawing, and flip/present. Software
and hardware ports fill the same operation slots.

### GFX Direction

GFX should keep `flush()` as the minimum backend contract, but add an optional
capability table for accelerated paths:

```c
typedef struct {
    uint32_t caps;
    gfx_err_t (*fill)(...);
    gfx_err_t (*blit)(...);
    gfx_err_t (*blend)(...);
    gfx_err_t (*scale)(...);
    gfx_err_t (*transform)(...);
    gfx_err_t (*draw_glyph)(...);
    gfx_err_t (*present)(...);
} gfx_draw_ops_t;
```

Current implementation status:

- `gfx_backend_cap_t` defines the initial backend capability bits.
- `gfx_draw_ops_t` is an internal backend acceleration contract in
  `src/core/display/gfx_backend_priv.h`.
- callback and memory backends currently advertise `FLUSH`.
- SDL currently advertises `FLUSH | PRESENT`.
- The renderer still uses software drawing first and flushes rendered chunks;
  background fill now tries backend `fill()` first and falls back to software.
  Routing image/blit/blend/scale operations through backend ops is the next
  step.

The renderer chooses the fastest supported path and falls back to software
drawing when an operation is missing. This lets an ESP PPA backend accelerate
fill, blend, scale, and rotate without changing widget code.

### Landing Shape

The ITU-style operation table should land behind renderer-owned helpers, not
inside widgets. A widget still says "draw this image" or "draw this label";
the renderer decides whether that becomes a backend operation or a software
routine.

The intended routing is:

```text
widget draw callback
  -> renderer draw helper
     -> backend draw op when caps, format, area, and alignment match
     -> software fallback otherwise
  -> final flush/present boundary
```

This keeps hardware acceleration optional and avoids coupling a widget to one
specific backend such as SDL, ESP LCD, or PPA.

### Alignment And Roundup Contract

Some render targets and accelerators require dimensions, row pitch, or memory
addresses to be aligned to 4 bytes, 8 bytes, cache-line size, DMA burst size, or
hardware block size. GFX should not hide this inside each widget.

Add a renderer/backend alignment query before enabling more hardware paths:

```c
typedef struct {
    uint16_t width_px;
    uint16_t height_px;
    uint16_t stride_bytes;
    uint16_t addr_bytes;
} gfx_render_alignment_t;
```

The renderer should use one central roundup helper when it chooses chunk size,
temporary surface pitch, or backend operation rectangles. A future helper can be
named like:

```c
gfx_area_t gfx_render_roundup_area(const gfx_area_t *area,
                                   const gfx_render_alignment_t *align,
                                   const gfx_area_t *limit);
```

Rules for the roundup helper:

- Round outwards, then clip to the display or surface limit.
- Never let widget geometry depend on hardware alignment.
- If roundup makes an operation invalid or too expensive, use software fallback.
- Keep image descriptor `stride` as source payload metadata; render roundup is
  a destination/backend constraint.

Current implementation status:

- `gfx_render_alignment_t` is internal backend/render metadata.
- Callback, memory, and SDL backends default every alignment field to 1.
- `gfx_backend_get_alignment()` normalizes missing alignment fields to 1.
- `gfx_render_roundup_area()` rounds half-open render areas outwards and clips
  them to a display/surface limit.
- `gfx_render_roundup_stride_bytes()` centralizes byte-stride alignment.
- Dirty render chunk areas now pass through the area roundup helper before
  drawing and flushing.
- Non full-frame render chunks now use aligned physical row stride, and chunk
  height is calculated from that physical stride so the configured buffer
  capacity is not exceeded.
- Internal backend flush receives source stride; memory and SDL backends can
  consume aligned chunk pitch. The legacy public `flush_cb` remains compact-row
  or full-frame only.
- `gfx_render_is_addr_aligned()` centralizes address-alignment checks for future
  backend ops.
- Internal render buffer allocation uses `gfx_platform_aligned_alloc()` with the
  backend `addr_bytes` requirement.
- `gfx_render_backend_op_can_use_dst()` centralizes capability, destination
  address, stride, and area-alignment checks. The current background fill path
  uses it before trying backend `fill()`, and falls back to software otherwise.
- Image widgets now try backend `blit` for opaque sources and backend `blend`
  for alpha/opacity sources. Missing operations, unsupported alignment, or
  backend rejection fall back to software image drawing.
- Host alignment smoke covers internal buffer address alignment, aligned fill
  op usage, aligned image blit/blend usage, and external misaligned-buffer
  fallback.

### Rules

- Capability bits describe what an operation supports, not just that the
  function pointer exists.
- Unsupported formats must fall back to software conversion.
- Backends must not own widget state or asset decoding policy.
- `flush()` remains valid for simple panels and memory backends.
- Alignment requirements are declared by backend/render capability metadata and
  consumed centrally by the renderer.

## 2. Surface And Resource Cache

### ITU Reference

ITU uses surfaces as central objects with width, height, pitch, format, flags,
address, clipping, static/compressed state, and cache/release behavior.

### GFX Direction

GFX should not merge asset bytes, decoded image descriptors, and framebuffers
into one large object. The current separation is cleaner:

- `gfx_asset_view_t`: read-only bytes and lifetime.
- image/font/animation descriptors: parsed format metadata.
- render buffers or framebuffer surfaces: writable pixel targets.

The part worth borrowing is the decoded-resource cache lifecycle:

```text
asset bytes -> decoder -> decoded surface/cache entry -> ref/release
```

### Memory Policy

Decoded cache can easily consume more RAM than the original asset, especially
for RGB888/RGB888A8 or decompressed animation frames. To avoid hidden memory
growth:

- Cache must be opt-in or budgeted by a configured byte limit.
- Each cache entry tracks decoded size, source key, format, refcount, and last
  use.
- If a widget holds a cache entry, it must release it from the resource release
  lifecycle.
- The cache should support LRU eviction for unused entries.
- Large animations should prefer streaming/decode-on-demand unless explicitly
  pinned.
- Host SDL can expose cache stats first; embedded builds should keep the
  default budget conservative.

### Landing Shape

Start with metadata and budget plumbing before caching actual decoded pixels:

1. Add an internal decoded-resource entry type with source key, format,
   width/height, stride, decoded byte size, refcount, and last-use tick.
2. Add a small cache manager with a byte budget and explicit acquire/release.
3. Wire image resources first because their decode result is easiest to bound.
4. Add animation frame caching only after streaming behavior is clear.

This avoids the ITU-style trap where every surface quietly becomes a cacheable,
decompressible, drawable object and memory ownership becomes hard to audit.

## 3. Widget Resource Lifecycle

### ITU Reference

ITU widgets have clear lifecycle hooks and resource events such as load, load
external, release, update, draw, and exit.

### GFX Direction

GFX already has widget classes and vtables. The next step is to add resource
lifecycle hooks so image, label, animation, motion, and future widgets do not
each invent their own loading/release behavior.

Target class hooks:

```c
typedef struct {
    gfx_err_t (*load)(gfx_object_t *obj);
    gfx_err_t (*release)(gfx_object_t *obj);
    gfx_err_t (*update)(gfx_object_t *obj);
    void (*draw)(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
    void (*delete)(gfx_object_t *obj);
    void (*touch_event)(gfx_object_t *obj, const gfx_touch_event_t *event);
} gfx_widget_class_t;
```

Target resource events:

- `GFX_WIDGET_EVENT_LOAD`: resolve asset views, fonts, decoded image metadata,
  or cache entries.
- `GFX_WIDGET_EVENT_RELEASE`: release decoded/cache resources but keep widget
  configuration.
- `GFX_WIDGET_EVENT_DELETE`: release all runtime state and object memory.

### Rules

- Load/release must be idempotent.
- A widget setter only records source configuration and invalidates the object.
- Actual cache acquisition happens in load/update/draw boundary.
- Display delete must release child widget resources before freeing objects.
- Resource lifecycle hooks should be introduced per widget family; do not add
  compatibility wrappers while the public API is still moving.

## 4. Scene-Level Input Dispatch

### ITU Reference

ITU dispatches mouse/touch events from the top-level scene and walks children
from the last drawn object backward. The first hit object receives the event.

### GFX Direction

GFX should centralize hit-testing in the display/object layer:

- Hit-test walks the display child list by z-order from top to bottom.
- Invisible or disabled objects are skipped.
- Press captures the hit object and move/release continue to that object while
  it remains alive.
- Widget-specific code receives already-normalized events and does not perform
  global hit-test.

Current GFX touch dispatch already captures pressed objects. The cleanup is to
make z-order traversal explicit instead of depending on a forward scan where
the last match wins.

Target helper:

```c
gfx_object_t *gfx_display_hit_test(gfx_display_t *display, uint16_t x, uint16_t y);
```

Future extension can add event bubbling/capture phases, but the first step
should stay simple: topmost hit target only.

Current implementation status:

- `gfx_display_hit_test()` is the single display-level hit-test helper.
- Touch press captures the hit object; move/release continue to use the captured
  object.
- SDL input goes through the same GFX touch injection path as other backends.

Remaining work is mostly robustness: object deletion while pressed, disabled
state filtering, and host/unit tests for topmost hit behavior.

## 5. Keyframe / Tween Layer

### ITU Reference

ITU separates sprite frame switching from keyframe animation that changes child
geometry, color, alpha, and transform properties.

### GFX Direction

GFX already has:

- `gfx_anim`: frame/asset playback.
- `gfx_motion`: domain-specific motion scene playback.

Do not turn `gfx_motion` into a general animation engine. Add a small generic
tween/keyframe module instead.

Target module:

```text
include/gfx/tween.h
src/core/tween/
```

Target concepts:

- `gfx_tween_t`: one timeline bound to an object or callback.
- `gfx_tween_track_t`: one animated property.
- `gfx_tween_keyframe_t`: time/value/easing point.
- `gfx_tween_group_t`: optional group for parallel or sequence playback.

Initial properties:

- position: x, y
- size: width, height
- opacity
- color
- custom numeric callback

Deferred properties:

- angle / transform, after transform support is stable in the renderer.

### Runtime Model

- Tween uses the existing timer/tick system.
- The tween update writes object properties through normal setters.
- Setters invalidate objects, so tween does not call render directly.
- A finished tween can stop, loop, or call a completion callback.

### Why This Shape

This keeps sprite playback, motion assets, and generic UI animation separate:

- `gfx_anim`: image/animation frames.
- `gfx_motion`: structured character/motion scenes.
- `gfx_tween`: small property interpolation layer for UI and simple effects.
