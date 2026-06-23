# ESP Emote GFX Architecture

This document defines the target structure and naming rules for GFX. The goal
is to keep the framework small while giving it clear module boundaries.

## Design Reference

The ITU display framework is useful as a structural reference, not as a style
to copy directly.

Useful ideas from ITU:

- A display framework benefits from explicit draw operations such as create
  surface, blit, stretch blit, alpha blend, transform, draw glyph, and present.
- Widgets should have a predictable lifecycle: create/init, load resources,
  update state, draw, release resources, delete.
- A scene or root object should own event dispatch, z-order, and dirty state
  propagation.
- Static and compressed resources need a clear cache/release lifecycle.
- Sprite animation and keyframe animation are separate concepts.

Things GFX intentionally avoids:

- A single giant public header for every type and widget.
- String-based action systems inside every widget.
- Mixing asset bytes, decoded images, render targets, and cache ownership into
  one surface type.
- Global draw function pointers as the primary backend interface.

## Target Layers

### Public API

Public headers live under `include/gfx/`.

Applications should normally include:

```c
#include "gfx/gfx.h"
```

Host simulator code can include host-only backend headers such as:

```c
#include "gfx/backends/sdl.h"
```

Public APIs should live under `include/gfx/`. Headers under `include/core/` and
`include/gfx/widgets/` are internal or transitional and should not grow new
application-facing APIs.

### Runtime

Runtime owns timers, locking, task/manual tick policy, and lifecycle.

Target files:

```text
src/core/runtime/
```

Public names:

- `gfx_core_t` or `gfx_handle_t` for the runtime handle
- `gfx_core_init()`
- `gfx_core_deinit()`
- `gfx_core_tick()`
- `gfx_core_refresh_now()`

#### Scheduling Modes

GFX has two runtime scheduling modes:

- Background task mode: `manual_tick = false`. The platform task created by
  `gfx_core_init()` owns timer advancement, dirty display refresh, and regular
  frame pacing. This is the default mode for ESP-IDF / RTOS targets.
- Manual tick mode: `manual_tick = true`. The application owns the event loop
  and must call `gfx_core_tick()` regularly from the same thread that owns the
  host backend. This is the default shape for SDL simulation, where SDL event
  polling and texture presentation need stable thread ownership.

Do not drive the same `gfx_handle_t` with both modes. A context created in
background task mode should not be manually ticked from an application loop,
and a manual-tick context will not create the background render task.

### Display

Display owns resolution, framebuffer buffers, dirty areas, background color,
and the root object list.

Target files:

```text
src/core/display/
```

Preferred public names use `display`.

Target names:

- `gfx_display_t`
- `gfx_display_config_t`
- `gfx_display_add()`
- `gfx_display_delete()`
- `gfx_display_refresh_all()`

Display format model:

- `color_format` in `gfx_display_config_t` describes the output / flush format.
- Internally the display tracks both `render_format` and `output_format`.
- For RGB565, RGB565_SWAPPED, RGB888, and XRGB8888 displays, the current
  default is direct same-format rendering, not RGB565-only intermediate
  rendering.
- `render_format` is currently internal state and is not exposed as a public
  debug API yet.
- Preferred configuration is explicit `color_format`. Leaving `color_format`
  as `UNKNOWN` uses the legacy RGB565 default. Use `RGB565_SWAPPED`
  explicitly for high-byte-first RGB565 buffers.

### Object Tree

Objects own geometry, visibility, alignment, z-order, input callbacks, widget
class dispatch, and invalidation.

Target files:

```text
src/core/object/
```

Preferred public names use `object`.

Target names:

- `gfx_object_t`
- `gfx_object_set_pos()`
- `gfx_object_set_size()`
- `gfx_object_delete()`

### Render

Render converts dirty object regions into pixels. It owns draw context,
clipping, dirty area merge, and software drawing helpers.

Target files:

```text
src/render/
src/render/sw/
```

Render files now live under `src/render/`, with software routines under
`src/render/sw/`.

Target concepts:

- `gfx_renderer_t`
- `gfx_draw_ctx_t`
- `gfx_draw_ops_t`
- `gfx_surface_t`

`gfx_surface_t` should represent pixel memory or a render target. It must not
own asset loading policy.

Render format rule:

- `gfx_color_t` is a semantic RGB565 color value for style APIs.
- Destination pixel memory must be written through destination-format-aware
  helpers.
- New widget code should not cast `ctx->buf` to `gfx_color_t *` unless the
  destination format is explicitly RGB565-family and the helper is a legacy
  compatibility path.

### Backend

Backends present rendered pixels or provide target-specific acceleration. A
backend may expose capabilities, but the renderer should still have a software
fallback.

Target files:

```text
src/backend/memory/
src/backend/sdl/
src/backend/esp_lcd/
```

Public backend headers live under:

```text
include/gfx/backends/
```

Target names:

- `gfx_backend_t`
- `gfx_backend_sdl_create()`
- `gfx_backend_memory_create()`

### Platform

Platform code provides OS services only: mutexes, tasks, events, clocks, heap
allocation, and target input adapters.

Target files:

```text
src/platform/esp_idf/
src/platform/linux/
src/platform/host/
```

Platform code should not implement widget policy or resource format decoding.

### Assets And Codecs

Assets provide read-only bytes. Codecs interpret bytes.

Target files:

```text
src/core/asset/
src/codecs/image/
src/codecs/eaf/
src/codecs/font/
```

Rules:

- `gfx_asset_view_t` owns byte access and lifetime.
- Image, animation, and font decoders consume byte views.
- Widgets can hold a source descriptor, but decoders should not open files by
  themselves.

### Widgets

Widgets live under:

```text
include/gfx/widgets/
src/widgets/
```

Each widget should use this internal shape:

```text
src/widgets/<name>/
  gfx_<name>.c          public setters and create
  gfx_<name>_class.c    update/draw/delete class callbacks
  gfx_<name>_draw.c     complex drawing helpers, if needed
  gfx_<name>_private.h  private state and helper declarations
```

Simple widgets can stay in one `.c` file.

## Naming Rules

### Files

- Public headers: `include/gfx/<module>.h`
- Public widget headers: `include/gfx/widgets/<widget>.h`
- Public backend headers: `include/gfx/backends/<backend>.h`
- Private headers: `*_private.h`
- Internal/transitional headers may live under `include/core/` and
  `include/gfx/widgets/`, but new application APIs should not be added there.

Prefer full words for public module names:

- `display` instead of `disp`
- `image` instead of `img`
- `object` instead of `obj` for new core APIs

Compact names should not be introduced for new public APIs.

### Types

- Public types: `gfx_<module>_<name>_t`
- Opaque handles: `gfx_<module>_t`
- Private types: keep in `.c` files when possible; otherwise use
  `gfx_<module>_<name>_private_t`

Examples:

```c
typedef struct gfx_display gfx_display_t;
typedef struct gfx_object gfx_object_t;
typedef struct gfx_surface gfx_surface_t;
```

### Functions

Public functions use:

```text
gfx_<module>_<verb>[_object]()
```

Examples:

```c
gfx_display_add()
gfx_display_refresh_all()
gfx_object_set_pos()
gfx_image_set_source_desc()
gfx_backend_sdl_create()
```

Static functions inside `.c` files use `s_` and do not repeat the full module
prefix unless it improves clarity.

Examples:

```c
static gfx_err_t s_prepare_frame(...);
static void s_render_8bit_pixels(...);
```

### Return Values

Public GFX APIs return `gfx_err_t`. ESP-IDF error types must stay in ESP-IDF
adapter code or platform code.

### Platform Boundaries

GFX keeps ESP-IDF types and headers out of portable core code:

| Layer | Error / check / log | ESP-IDF headers |
| --- | --- | --- |
| `include/` | `gfx_err_t` only | none |
| `src/` (core, widgets, codecs, render, …) | `gfx_err_t`, `common/gfx_check.h`, `GFX_LOG*` | none |
| `src/platform/esp_idf/` | convert at boundary via `gfx_err_bridge.h` | real `esp_*` allowed |
| Host simulation (`gfx_host_core`) | same as portable `src/` | none |
| Host demos / expression | `simulation/port/include` (`lvgl.h`, `sdkconfig.h`) only; expression host via `emote_port.h` | host-only shims |

CI runs `scripts/check_no_esp_in_src.sh` to enforce: no `#include "esp_*"` under
`src/` except `src/platform/esp_idf/`.

### Host Simulation and Examples Layout

| Path | Purpose |
| --- | --- |
| `simulation/host/` | Host executables and smoke tests (`gfx_host_sdl_demo`, `gfx_host_*_smoke`) |
| `simulation/port/include/` | Minimal host shims (`lvgl.h`, `sdkconfig.h`) |
| `examples/format_playground/` | Shared widget/format playground UI (host + ESP board demos) |
| `examples/assets/format/` | Format playground binary assets |
| `examples/assets/fonts/` | Font fixtures used by host smoke tests |
| `examples/esp/` | ESP-IDF board projects (`format_rgb565`, `format_rgb888`) |
| `test_apps/main/` | Unity conformance only |

Host SDL builds link playground sources from `examples/`; they must not depend on
`test_apps/` demo code paths.

1. Add the new public include tree under `include/gfx/`.
2. Delete old forwarding entry points instead of adding new alternate names.
3. Move host-only backend headers out of `include/core/`.
4. Rename public APIs in documentation and demos first.
5. Move source files to the target directories in small batches.
6. Remove transitional headers once their declarations have moved to the new
   public locations.
