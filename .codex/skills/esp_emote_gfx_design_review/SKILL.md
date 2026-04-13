---
name: esp-emote-gfx-design-review
description: Use when reviewing or extending the esp_emote_gfx codebase for module boundaries, naming consistency, logging style, error handling, rendering-chain extensibility, color/swap policy, and safe use of gfx_color_t vs uint16_t vs void*.
---

# ESP Emote GFX Design Review

Use this skill when working in `esp_emote_gfx` and the task involves architecture, refactoring, review, or consistency work rather than a single isolated bug.

## Goals

- Preserve the framework layering: `core -> widget -> app/test`
- Keep public API in `include/` and internal contracts in `src/.../*_priv.h`
- Enforce a consistent policy for naming, logs, error handling, color handling, and byte swap handling
- Prefer small, responsibility-based module splits over monolithic files

## Review Checklist

For any touched module, review these areas:

1. Function placement:
   Public declarations belong in `include/`; internal cross-file declarations belong in `src/<domain>/<module>_priv.h`.
2. Naming:
   Public APIs use `gfx_<module>_<verb>`. Internal helpers should keep the same module prefix and use action-oriented names such as `prepare`, `eval`, `apply`, `init`, `reset`.
3. Logs:
   Framework code should use `GFX_LOGx`, not `ESP_LOGx`. Messages should be lower-case and operation-scoped, for example `set image src: source is NULL`.
4. Error handling:
   Public APIs validate args and state with `ESP_RETURN_ON_FALSE` and propagate `esp_err_t` where possible.
5. Rendering chain:
   Widgets should prepare state and geometry, while raster/color-format specifics stay in draw/backend layers.
6. Swap:
   `gfx_color_t` is logical unswapped color. Byte swapping should happen only in draw/backend code.
7. Type safety:
   Use `gfx_color_t` for logical style state, `uint16_t color_raw` for packed framebuffer values, and `const void *src` for immutable payload sources.

## Module Design Rules

### Layout

- `include/`:
  only public API and public types
- `src/<domain>/<module>.c`:
  implementation and private statics
- `src/<domain>/<module>_priv.h`:
  private shared structs, constants, and non-public function declarations

If a module mixes multiple responsibilities, split it by role. Common split patterns:

- `<module>.c`: public API and lifecycle
- `<module>_pose.c` or `<module>_solver.c`: pure transform/solve logic
- `<module>_mesh.c` or `<module>_draw.c`: geometry upload or raster preparation

### Naming

- Public:
  `gfx_<module>_create`, `set_*`, `get_*`, `reset_*`
- Internal:
  `gfx_<module>_<verb>_<subject>`
- Variables:
  `color` for logical `gfx_color_t`
  `color_raw` for packed `uint16_t`
  `handle` for opaque ownership pointers
  `src` for external payload pointers

## Color and Swap Policy

- Store logical style colors as `gfx_color_t`
- Do not pre-swap widget style colors at API boundaries
- Convert to raw packed RGB565 only in low-level draw/backend code
- Use `ctx->swap` / display swap flags only in the last conversion stage
- If both logical and raw forms exist in one function, name them explicitly:
  - `gfx_color_t color`
  - `uint16_t color_raw`

## Logging Policy

- Use `GFX_LOGE/W/I/D/V` in framework code under `src/`
- Reserve `ESP_LOGx` for applications, tests, or external integration wrappers
- Log style:
  - lower-case
  - operation first
  - no punctuation at end

Good:

```c
GFX_LOGE(TAG, "set mesh image src: source is NULL");
```

Avoid:

```c
ESP_LOGE(TAG, "Invalid parameters");
GFX_LOGE(TAG, "Failed to allocate memory.");
```

## Error Handling Policy

- Public APIs:
  validate arguments and object state with `ESP_RETURN_ON_FALSE`
- Constructors:
  return `NULL` on failure and log one operation-scoped reason
- Internal pure helpers:
  may use direct early returns if no log adds value
- Timer/render boundaries:
  may downgrade propagated failures to warnings

## How To Review a Change

When reviewing a patch in `esp_emote_gfx`, prioritize:

1. Wrong ownership boundaries between widget/core/draw
2. Swap/color confusion between logical and raw representations
3. Inconsistent validation or silent invalid-state handling
4. Logging family/style drift
5. New `void *` usage where `const void *` or a stronger type would work

## References

- Review findings and current standards: `docs/esp_emote_gfx_code_review.md`
