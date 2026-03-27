# ESP Emote GFX Code Review

## Scope

Reviewed `src/` and `include/` for:

1. Function placement and `.c`/`.h` organization
2. Naming consistency
3. Log style consistency
4. Error handling and null-pointer checks
5. Rendering chain architecture and extensibility
6. Swap/color-byte-order handling
7. `gfx_color_t` / `uint16_t` / `void *` usage consistency

The review is based on current repository state and recent `face_emote` refactors in the worktree.

## High-Level Assessment

The codebase has a solid layered shape:

- `core/` owns runtime, object, display, draw, and render primitives
- `widget/` owns user-facing components built on top of core
- `lib/` wraps codec or algorithm libraries
- `include/` exposes public API
- `src/*_priv.h` files expose internal contracts

The strongest areas are:

- Clear separation between public APIs and private internals in many modules
- Broad use of `ESP_RETURN_ON_FALSE`/`ESP_RETURN_ON_ERROR`
- Consistent `gfx_` namespace on public APIs
- Rendering context carries `swap` through most draw interfaces cleanly

The weakest areas are:

- Mixed log families (`ESP_LOGx` and `GFX_LOGx`) inside the same subsystem style
- Mixed null-check styles (`if (...) return`, `GFX_RETURN_IF_NULL`, `ESP_RETURN_ON_FALSE`)
- Public APIs frequently use `void *src` for image-like inputs without stronger typing
- Swap policy is mostly centralized but still leaks into many leaf modules manually
- Some modules are very disciplined about private headers; others still inline internal details

## 1. Function Placement and File Layout

### Good patterns

- Public declarations are generally in `include/`
- Internal contracts are often isolated to `src/.../*_priv.h`
- Rendering primitives are centralized in:
  - `src/core/draw/gfx_blend.c`
  - `src/core/draw/gfx_sw_draw.c`
- Display orchestration is sensibly grouped in:
  - `src/core/display/gfx_disp.c`
  - `src/core/display/gfx_refr.c`
  - `src/core/display/gfx_render.c`

### Strong examples

- `img` stack:
  - public: `include/widget/gfx_img.h`
  - private decoder interface: `src/widget/img/gfx_img_dec_priv.h`
  - implementation split: `gfx_img.c`, `gfx_img_dec.c`, `gfx_mesh_img.c`
- `anim` stack:
  - public: `include/widget/gfx_anim.h`
  - private decoder abstraction: `src/widget/anim/gfx_anim_decoder_priv.h`

### Weak / mixed areas

- `face_emote` improved after refactor, but still has a private header in widget space instead of a more shared internal convention:
  - `src/widget/face/gfx_face_emote_priv.h`
- `src/common/gfx_comm.h` is really an internal utility header, but acts like a global macro bucket. This tends to spread policy decisions silently across modules.
- `src/lib/qrcode/README.md` sits under `src/`, which is unusual for production source layout.

### Placement conclusions

- Public API in `include/` is reasonable
- Internal contracts should always live under `src/<domain>/..._priv.h`
- Avoid broad, shared internal macro headers unless the project deliberately treats them as a policy layer
- Keep non-code docs out of `src/`

## 2. Naming Consistency

### Mostly consistent

- Public functions use `gfx_<module>_<verb>` style:
  - `gfx_img_set_src`
  - `gfx_mesh_img_set_grid`
  - `gfx_face_emote_set_color`
- Struct names use `_t`
- Private statics usually keep the same prefix

### Inconsistencies found

- Log tags and wording vary between noun phrases and imperative phrases
- Some modules use `create <thing>` wording while others use `set <thing>` wording
- Internal helper naming mixes:
  - `get_*`
  - `calc_*`
  - `make_*`
  - `prepare_*`
  without a strict semantic distinction documented
- `gfx_face_emote` still has names inherited from geometry representation rather than behavior intent:
  - `shape14`
  - `shape8`
  These are practical but opaque to newcomers

### Naming guidance

- Public API:
  - `gfx_<module>_create`
  - `gfx_<module>_set_*`
  - `gfx_<module>_get_*`
  - `gfx_<module>_reset_*`
- Internal helpers:
  - `*_init_*` for first-time setup
  - `*_prepare_*` for pre-draw staging
  - `*_eval_*` for pure transform/solve logic
  - `*_apply_*` for state mutation or draw upload
- Avoid representation-only names in public types where behavior names exist

## 3. Log Style Consistency

### Current state

There is no single enforced style.

Examples:

- Lowercase fragments:
  - `draw mesh image: invalid object or source`
  - `set mesh image src: source is NULL`
- Sentence style:
  - `Failed to allocate memory`
  - `Invalid parameters`
- Mixed logger families:
  - `GFX_LOGE/W/I/D/V`
  - `ESP_LOGW` appears in `face_emote`

### Main issues

- Mixed `ESP_LOGx` and `GFX_LOGx`
- Mixed capitalization:
  - `Source is NULL`
  - `fmt is NULL`
  - `Invalid parameters`
- Mixed phrasing:
  - Some logs describe operation context first
  - Some logs only describe the error

### Recommended standard

- Use `GFX_LOGx` in all component code under `src/`
- Reserve `ESP_LOGx` for top-level app/test code or external integration boundaries
- Format:
  - lower-case
  - operation first
  - reason second
  - no trailing punctuation
- Examples:
  - `create image: display is NULL`
  - `set label text: fmt is NULL`
  - `draw mesh image: unsupported color format %u`

## 4. Error Handling and Null Pointer Checks

### Strengths

- Widespread use of:
  - `ESP_RETURN_ON_FALSE`
  - `ESP_RETURN_ON_ERROR`
- Many modules distinguish:
  - `ESP_ERR_INVALID_ARG`
  - `ESP_ERR_INVALID_STATE`

### Inconsistencies

- Some functions use direct `if (...) return ...`
- Some use `GFX_RETURN_IF_NULL`
- Some use `ESP_RETURN_ON_FALSE`
- Some constructors return `NULL` with logs; some return `NULL` quietly
- Not all animation or draw callbacks propagate downstream errors consistently

### Recommended policy

- Public API:
  - validate arguments with `ESP_RETURN_ON_FALSE`
  - validate object state with `ESP_RETURN_ON_FALSE`
- Constructors:
  - return `NULL`
  - log one operation-scoped failure reason
- Static internal pure helpers:
  - use direct `if` only when no logging is needed
- Draw/update paths:
  - propagate `esp_err_t` up until the scheduler boundary
  - scheduler/timer boundary may downgrade to warning log

## 5. Rendering Chain and Interface Extensibility

### Current chain

The rendering chain is well layered:

1. widget prepares geometry/state
2. object system dispatches widget draw
3. display/render builds draw context
4. draw layer performs blending and triangle rasterization

Relevant modules:

- object/class registration:
  - `src/core/object/gfx_widget_class.c`
- display refresh:
  - `src/core/display/gfx_refr.c`
  - `src/core/display/gfx_render.c`
- draw backends:
  - `src/core/draw/gfx_blend.c`
  - `src/core/draw/gfx_sw_draw.c`

### Extensibility assessment

Good:

- Widget classes are registered via class structs
- Image decoders already use an interface-like registration model
- Animation decoders also use an abstraction layer

Less good:

- Shape/raster backends are not yet abstracted as registrable backends
- Mesh/image drawing is specialized inside `gfx_mesh_img.c`
- Swap behavior and color format assumptions are still deeply coupled to RGB565

### Recommended principle

- Make registration explicit where variation is expected:
  - image decoders
  - animation decoders
  - future vector/raster backends
- Keep widget-level modules unaware of byte order and frame-buffer packing
- Constrain color-format-specific decisions to draw/backend layers

## 6. Swap Handling and Color Byte Order

### What is good

- `swap` is carried in draw context and display flags
- low-level primitives generally accept a `bool swap`
- many blend/draw helpers centralize byte-swap behavior

### What is inconsistent

- Some modules pre-swap colors before calling lower-level functions
- Some pass unswapped `gfx_color_t` and rely on callee swap
- Some APIs expose `gfx_color_t`; some immediately store `uint16_t`
- EAF decoder has separate concepts:
  - `swap_bytes`
  - `swap_color`

### Risk

The same conceptual color may be interpreted in different stages as:

- logical `gfx_color_t`
- raw RGB565 host-endian `uint16_t`
- panel-order swapped `uint16_t`

That is the biggest source of subtle regressions.

### Recommended swap policy

- Rule 1:
  `gfx_color_t` always means logical unswapped color in framework space
- Rule 2:
  only draw/backend code may convert to swapped/raw `uint16_t`
- Rule 3:
  public/widget APIs never manually byte-swap stored style colors
- Rule 4:
  names must distinguish:
  - `color` -> logical `gfx_color_t`
  - `color_raw` -> packed `uint16_t`
  - `swap` -> output-framebuffer byte order

## 7. `gfx_color_t` / `uint16_t` / `void *` Mixing

### Confirmed mixed usage

- `gfx_color_t` is used for styles and logical colors
- `uint16_t *` is used for raw buffers and packed pixel writes
- `void *src` is used for image-like inputs and generic handles

Examples:

- public image APIs:
  - `gfx_img_set_src(gfx_obj_t *obj, void *src)`
  - `gfx_mesh_img_set_src(gfx_obj_t *obj, void *src)`
- raw color storage:
  - button/list background fill paths use `uint16_t fill_color_raw`
- logical color storage:
  - widget style structs use `gfx_color_t`

### Assessment

This is acceptable in principle, but only if boundaries are explicit.

Current problems:

- `void *src` is too weakly typed for public interfaces
- some modules cast between `gfx_color_t *` and `uint16_t *` aggressively
- logical color and packed raw color naming is not always explicit enough

### Recommended type policy

- Public color/style state:
  - `gfx_color_t`
- Internal raw frame-buffer fill:
  - `uint16_t color_raw`
- Generic payload handle:
  - `const void *src` unless mutation is required
- Decoder/runtime user handles:
  - `void *handle` only for opaque ownership objects

## Findings Summary

### Priority P1

- Unify swap/color policy around logical `gfx_color_t` and backend-only raw conversion
- Standardize logging family to `GFX_LOGx` in framework code
- Standardize argument/state validation style

### Priority P2

- Tighten `void *src` public interfaces where possible
- Continue splitting large modules like `face_emote` by responsibility
- Define helper naming semantics in one shared guideline

### Priority P3

- Move stray docs out of `src/`
- Reduce ad hoc macro usage from broad shared internal headers

## Proposed Code Standard

### Module layout

- `include/` contains only public API
- `src/<domain>/<module>.c` contains implementation
- `src/<domain>/<module>_priv.h` contains internal types and private cross-file contracts
- split files by responsibility once a module mixes:
  - public API/lifecycle
  - pure solve logic
  - draw upload or raster logic

### Naming

- public API: `gfx_<module>_<verb>`
- internal static helpers: `gfx_<module>_<verb>_<subject>`
- logical colors: `gfx_color_t color`
- packed colors: `uint16_t color_raw`
- opaque handles: `void *handle`
- source payloads: `const void *src`

### Logs

- framework code uses `GFX_LOGx`
- app/test code may use `ESP_LOGx`
- format:
  - lower-case
  - operation first
  - no trailing punctuation

### Error handling

- public API validates args and state with `ESP_RETURN_ON_FALSE`
- constructors return `NULL` on failure
- static helpers may omit logs only if they are pure and local
- timer/render boundaries may convert propagated errors into warnings

### Color and swap

- widget/style state stores logical `gfx_color_t`
- backend/draw layer computes `color_raw`
- byte swap happens once at the lowest possible layer
- never mix logical and raw naming in one variable

### Extensibility

- register backends where format/codec/raster behavior varies
- keep widgets declarative and backend-agnostic
- avoid hard-coding color format assumptions outside draw/backends

## Recommended Follow-Up Refactors

1. Convert remaining `ESP_LOGx` in framework code to `GFX_LOGx`
2. Normalize validation macros across widgets
3. Audit public `void *src` APIs and convert to `const void *` where mutation is not needed
4. Document swap policy near draw context definitions
5. Add a small contributor guide referencing the design skill
