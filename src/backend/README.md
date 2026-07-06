# Display Backends

Each subdirectory is one backend target. Filenames use a `{backend}_` prefix so they
stay searchable in tabs and grep (same rule as `src/platform/`).

| Directory | Main file | Public header |
|-----------|-----------|---------------|
| `custom/` | `custom_backend.c` | `include/gfx/backends/custom.h` |
| `memory/` | `memory_backend.c` | `include/gfx/backends/memory.h` |
| `sdl/` | `sdl_backend.c` | `include/gfx/backends/sdl.h` |
| `esp_lcd/` | `esp_lcd.c` | `include/gfx/backends/esp_lcd.h` |

ESP LCD internals:

| File | Role |
|------|------|
| `esp_lcd.c` | Panel flush, callbacks, notify APIs |
| `esp_lcd_flush_bridge.c` | DIRECT / STAGING flush paths |
| `esp_lcd_priv.h` | Shared private types (not installed) |

## Render buffer vs flush delivery (esp_lcd)

Two independent axes — do not conflate them:

| Axis | Config | Meaning |
|------|--------|---------|
| **Render buffer layout** | `gfx_display_config_t.buffers.buf_pixels` | `>= h_res*v_res` → full-screen stride; smaller → partition render. |
| **Flush delivery** | `gfx_backend_esp_lcd_config_t.tear_mode` (or legacy `flush_mode`) | How chunks reach the panel; see tear mode table below. |

Tear mode routing (`flush_bridge`):

| `tear_mode` | Path | Notes |
|-------------|------|-------|
| `NONE` / `DOUBLE_DIRECT` | DIRECT — each chunk → `draw_bitmap` | SPI default |
| `DOUBLE_PARTIAL` / `TRIPLE_PARTIAL` | STAGING — patch → last full blit | Needs `panel_fb` or `staging_fb` |
| `DOUBLE_FULL` / `TRIPLE_FULL` | FULL — last chunk full blit + pipeline | RGB / MIPI typical |
| `TE_SYNC` | DIRECT (stub) | TE GPIO path not yet implemented |

Legacy `flush_mode` is still accepted; when `tear_mode` is zero it is inferred at backend create.

Typical pairings:

| Scenario | `full_frame` | `flush_mode` | Notes |
|----------|--------------|--------------|-------|
| SPI partial render | `< screen | DIRECT (default) | Each dirty chunk sent over SPI. |
| RGB scanout | `== screen` (driver FB) | DIRECT | Draw into driver FB; flush commits scanout buffer. |
| Triple-partial + rotation / one-shot panel blit | `< screen` | STAGING | App allocates `staging_fb`; not the LCD driver FB. |

Public API symbols keep the `gfx_backend_<name>_` prefix (e.g. `gfx_backend_esp_lcd_create`).

## Planned: Buffer Pipeline Refactor

**Status:** Phase 1–4 complete. See [`docs/buffer_pipeline_design.md`](../../docs/buffer_pipeline_design.md).

Implemented pieces:

- Render no longer toggles `buf_act` or runs `sync_pending` clean copy
- `gfx_buf_pipeline_t` in `esp_lcd_buf_pipeline.c` (commit / acquire / ISR retire)
- `gfx_copy_unrendered_areas()` in `esp_lcd_copy_unrendered.c`
- STAGING + `panel_fb_count >= 2`: patch → draw_fb, last chunk clean copy + pipeline rotate
- Full-frame double buffer without pipeline: `buf_act` swap in `post_flush_buf_update()` after `wait_flush`
- Config: `panel_fb[3]`, `panel_fb_count`, `gfx_tear_mode_t` with auto-inference from `flush_mode`
- `flush_bridge` routes by `tear_mode` (PARTIAL / FULL / DIRECT)
- `gfx_buf_pipeline_init_full()` for DOUBLE/TRIPLE_FULL panel FB layout

- `gfx_display_flush_ready(disp)` — `swap_act_buf` 已移除

**Migration:** 将 `gfx_display_flush_ready(disp, true/false)` 改为 `gfx_display_flush_ready(disp)`；tear-free 请配置 `esp_lcd` 的 `tear_mode` + `panel_fb[]`。
