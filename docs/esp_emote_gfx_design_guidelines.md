# ESP Emote GFX Design Guidelines

## Scope

This review covers the code-bearing files under `include/` and `src/`, with `test_apps/main/` sampled to understand actual integration and API usage.

Primary review dimensions:

1. Function placement and `.c` / `.h` organization
2. Naming consistency for functions, variables, and data structures
3. Log style consistency
4. Error handling and null-pointer checks
5. Rendering chain extensibility and registration design
6. `swap` color handling and flag consistency
7. `gfx_color_t` / `uint16_t` / `void *` usage consistency

## Render And Call Chain

The current render chain is layered and mostly coherent:

1. `gfx_obj_create_class_instance()` binds widget state and class metadata to a `gfx_obj_t`
2. `gfx_render_update_child_objects()` and `gfx_render_draw_child_objects()` iterate object children
3. `gfx_render_part_area()` builds `gfx_draw_ctx_t`
4. widget draw callbacks render into the draw buffer
5. `gfx_blend.c` and `gfx_sw_draw.c` perform final pixel operations

Representative files:

- `src/core/object/gfx_widget_class.c`
- `src/core/object/gfx_obj_priv.h`
- `src/core/display/gfx_render.c`
- `src/core/draw/gfx_blend.c`
- `src/core/draw/gfx_sw_draw.c`

The animation path already uses a registry-based abstraction:

- `src/widget/anim/gfx_anim_decoder.c`
- `src/widget/anim/gfx_anim_decoder_priv.h`
- `src/widget/anim/gfx_anim_decoder_eaf.c`

The image path is less abstract:

- `include/widget/gfx_img.h`
- `include/widget/gfx_mesh_img.h`
- `src/widget/img/gfx_img.c`
- `src/widget/img/gfx_mesh_img.c`

The `face_emote` path is a composite widget built on top of `mesh_img`, but its internal implementation is currently spread across `gfx_face_emote.c`, `gfx_face_emote_pose.c`, and `gfx_face_emote_mesh.c`.

## Findings

### 1. Function Placement And `.c` / `.h` Organization

Good:

- Public APIs are mostly isolated in `include/`
- Internal draw contracts are kept in `src/.../*_priv.h`
- Widget class registration is centralized in `src/core/object/gfx_widget_class.c`
- Render orchestration is cleanly separated from draw primitives

Needs improvement:

- `gfx_obj_t` stores all widget-private state in a generic `void *src`, which keeps the core generic but weakens type boundaries across all widgets
- `gfx_face_emote` private contracts are reasonable in `src/widget/face/gfx_face_emote_priv.h`; public aliases now exist for eye/brow/mouth shapes, but the legacy numeric names are still present and should be treated as compatibility names rather than preferred API vocabulary
- `src/common/gfx_comm.h` acts like a broad internal policy bucket rather than a narrowly scoped helper header

Evidence:

- `src/core/object/gfx_obj_priv.h:53`
- `src/widget/face/gfx_face_emote_priv.h:1`
- `include/widget/gfx_face_emote.h:16`

Conclusion:

- The overall placement is reasonable, but the core object model is still too untyped at widget state boundaries
- Public headers should describe domain intent, while representation detail should stay private

### 2. Naming Consistency

Mostly consistent:

- Public APIs follow `gfx_<module>_<verb>` naming
- Widget class names and tags are fairly predictable
- Setter APIs are broadly aligned across widgets

Inconsistencies:

- `gfx_face_emote` now has public semantic aliases for eye/brow/mouth shapes, but the legacy `shape14` / `shape8` names still exist and remain easy to reach first when reading the header
- `obj->src` means very different things across modules: image source, widget state object, animation state, QR buffer owner
- Variable naming around draw buffers alternates between `dest_pixels`, `dst`, `dest_buf`, `pixel_buf`, `src_pixels_16`, `color_palette`, which is understandable locally but not standardized project-wide

Evidence:

- `include/widget/gfx_face_emote.h:16`
- `src/core/object/gfx_obj_priv.h:54`
- `src/widget/anim/gfx_anim.c:556`
- `src/widget/basic/gfx_qrcode.c:129`

Conclusion:

- Public API naming is acceptable
- Internal naming needs a documented semantic rule set: `*_cfg`, `*_state`, `*_ops`, `*_ctx`, `*_desc`, `*_points_q8`, `*_pixels_16`

### 3. Log Style Consistency

Current state:

- There is a project logging layer via `GFX_LOGx`
- Many modules still include `esp_log.h`
- Message style is mixed: sentence case, title case, lower-case fragments
- Operation scoping is inconsistent

Concrete examples:

- `src/widget/img/gfx_img.c:59` logs `Invalid object or source`
- `src/widget/img/gfx_img.c:186` logs `Source is NULL`
- `src/widget/basic/gfx_qrcode.c:114` logs `Generating QR: ...`
- `src/widget/face/gfx_face_emote.c:68` uses `ESP_LOGW` instead of `GFX_LOGW`

Conclusion:

- Component code under `src/` should use `GFX_LOGx` only
- Format should be: `operation: reason`
- Messages should be lower-case and avoid punctuation

Recommended format:

- `draw image: object or source is NULL`
- `set image src: source is NULL`
- `generate qrcode: failed to allocate buffer`
- `apply face pose: left eye mesh update failed`

### 4. Error Handling And Null Checks

Good:

- Many public APIs already use `ESP_RETURN_ON_FALSE` / `ESP_RETURN_ON_ERROR`
- The animation decoder registry validates callback completeness before registration

Evidence:

- `src/widget/anim/gfx_anim_decoder.c:21`
- `src/widget/face/gfx_face_emote_pose.c:119`
- `src/core/object/gfx_widget_class.c:28`

Problems:

- Some modules use direct `if (...) return ...` without aligned logging semantics
- Some constructors return `NULL` silently
- Some runtime callbacks early-return on invalid state and some log, without a documented boundary policy
- Some leaf geometry functions return `ESP_ERR_INVALID_ARG` without context logging

Evidence:

- `src/widget/face/gfx_face_emote.c:210`
- `src/widget/face/gfx_face_emote_mesh.c:73`
- `src/widget/basic/gfx_qrcode.c:175`

Conclusion:

- Public API: validate with `ESP_RETURN_ON_FALSE`
- Internal hot path: direct return is fine when the caller owns logging
- Constructor: one error log, one exit path when practical
- Timer/render callback: warn on recoverable issues, avoid repeated error spam

### 5. Rendering Chain Extensibility And Registration

Strongest part:

- Widget class registration is explicit
- Animation decoders are explicitly registrable and source-probed

Evidence:

- `src/core/object/gfx_widget_class.c:22`
- `src/widget/anim/gfx_anim_decoder.c:19`

Less strong:

- Image widgets now expose a typed source descriptor entry point, but source types are still narrow and not yet expanded beyond in-memory image descriptors
- `face_emote` is composable in practice but not extensible by registration; it is hardcoded to a specific mesh-backed implementation
- The render backend is implicitly software-only from the widget perspective; there is no draw backend ops table

Evidence:

- `include/widget/gfx_img.h:78`
- `include/widget/gfx_mesh_img.h:37`
- `src/widget/face/gfx_face_emote.c:205`

Conclusion:

- Wherever variability is expected, use an ops or descriptor layer
- Current priority candidates:
  - expand typed image source types beyond in-memory descriptors
  - optional draw backend ops
  - optional face renderer ops if future implementations are expected

### 6. `swap` Color Handling And Flag Consistency

Current chain:

- `gfx_disp_t.flags.swap` is copied into `gfx_draw_ctx_t.swap`
- Widgets pass `ctx->swap` into blend routines
- Some widgets pre-swap raw colors manually before fill
- Decoder paths also accept `swap_color`

Evidence:

- `src/core/display/gfx_render.c:219`
- `src/core/object/gfx_obj_priv.h:31`
- `src/widget/basic/gfx_qrcode.c:131`
- `src/widget/anim/gfx_anim.c:753`
- `src/lib/eaf/gfx_eaf_dec.c:613`

Main issue:

- There are two swap policies in flight:
  - policy A: caller pre-swaps `uint16_t` raw color before calling fill
  - policy B: callee receives `gfx_color_t` plus `swap` flag and performs conversion internally

Examples:

- `gfx_render.c` pre-swaps background before `gfx_sw_blend_fill_area()`
- `gfx_qrcode.c` pre-swaps QR buffer colors before later calling `gfx_sw_blend_img_draw(..., ctx->swap)`
- draw primitives like `gfx_sw_blend_draw()` and `gfx_blend_color_mix()` also consume `swap`

Conclusion:

- Swap must be normalized at one layer only
- Recommended rule:
  - structured color APIs consume `gfx_color_t` plus `swap`
  - raw `uint16_t` APIs consume already-normalized native buffer order and must be named `*_raw`
  - decoders output native pixel order for the target draw path, not mixed behavior per caller

### 7. `gfx_color_t` / `uint16_t` / `void *` Mixing

Current state:

- `gfx_color_t` is the semantic color type
- `uint16_t *` is used heavily in hot loops and buffer code
- `void *src` is used for both widget-owned state and external data sources

Evidence:

- `include/core/gfx_types.h:73`
- `src/widget/anim/gfx_anim.c:556`
- `src/widget/basic/gfx_qrcode.c:129`
- `include/widget/gfx_img.h:70`
- `include/widget/gfx_mesh_img.h:36`
- `src/core/object/gfx_obj_priv.h:54`

Assessment:

- `gfx_color_t` and `uint16_t` mixing is acceptable in hot loops when the ownership is clear
- `void *src` mixing is the weakest part because it collapses:
  - widget private state
  - external image descriptor source
  - animation source memory
  - child-list payloads

Conclusion:

- Keep `gfx_color_t` for APIs and semantic values
- Allow `uint16_t *` only in low-level draw and codec internals
- Replace public `void *src` image APIs with typed descriptors
- Consider renaming `gfx_obj_t.src` to `impl` or `instance` internally to reduce semantic confusion

## Recommended Code Standard

### Header And Source Placement

- Public API only in `include/`
- Internal contracts only in `src/<domain>/*_priv.h`
- Do not expose representation-specific types publicly unless they are intentional public data models
- Avoid shared catch-all private headers; prefer narrow domain headers

### Naming

- Public functions: `gfx_<module>_<verb>`
- Public constructors: `gfx_<module>_create`
- Public configuration structs: `gfx_<module>_cfg_t`
- Runtime mutable state: `gfx_<module>_state_t`
- Operation tables: `gfx_<module>_ops_t`
- Descriptors for external payloads: `gfx_<module>_desc_t`
- Fixed-point coordinate arrays: `*_points_q8`
- Prefer semantic public aliases such as `gfx_face_emote_eye_shape_t` over legacy numeric names like `shape14`

### Logging

- Use `GFX_LOGx` in all library code
- Reserve `ESP_LOGx` for app/test integration only
- Message format: `operation: reason`
- Lower-case only
- No trailing punctuation
- Use one stable tag per file

### Error Handling

- Public API:
  - `ESP_RETURN_ON_FALSE` for input validation
  - `ESP_RETURN_ON_ERROR` for dependency propagation
- Constructors:
  - return `NULL`
  - emit one scoped log on failure
- Internal helpers:
  - direct return allowed in hot path if caller owns logging
- Timer/render callbacks:
  - avoid repeated error spam
  - downgrade transient failures to warning when the system can continue

### Rendering And Registration

- Keep widget logic above draw backend details
- Register variation points explicitly
- Prefer:
  - class registry for widgets
  - decoder registry for content formats
  - backend ops for renderer variants
- Composite widgets should own orchestration only, not low-level pixel policy

### Color And Swap

- Semantic color type: `gfx_color_t`
- Raw buffer pixel type: `uint16_t`
- Single swap handoff point per pipeline stage
- Raw fill functions should be clearly named as raw/native-order
- Structured draw functions should accept `gfx_color_t` plus `swap`

### `void *` Usage

- Allowed for opaque internal handles only
- Public API should use typed descriptors, not naked `void *`
- `gfx_obj_t` may keep a generic implementation pointer internally, but widget public setters should be typed

## Module Design Principles

1. Keep domain intent above storage representation.
2. Keep byte order policy below widget level.
3. Keep public APIs typed, even if internal runtime stays generic.
4. Put extensibility behind explicit ops tables or registries.
5. Separate semantic color values from raw buffer words.
6. Make composite widgets orchestration-only; reuse existing primitives underneath.
7. Make hot-path performance exceptions explicit and local, not project-wide style drift.

## Priority Refactors

1. Introduce typed image source descriptors and deprecate public `void *src` APIs.
2. Unify all `src/` logs on `GFX_LOGx` with lower-case `operation: reason` wording.
3. Normalize swap policy so only one layer performs byte-order conversion for each path.
4. Rename future public `face_emote` representation types away from numeric shape names.
5. Document when `gfx_color_t` may be cast to `uint16_t *` and restrict that to draw/codec internals.
