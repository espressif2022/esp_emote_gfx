# Code Review — `feat/sdl_support`

Reviewed branch: `feat/sdl_support`
Base: `47f9fd0` (`Merge pull request #32 from espressif2022/feat/rig_widget`, == `main` / `origin/main`)
Scope: 9 commits, 307 files (`+87215 / -10779`). Generated/embedded blobs excluded from
review (`sim/host/gfx_host_demo_images.inc` ~50k lines, `test_apps/main/claw_motion.inc`, docs, vendored `managed_components`).

The review was run as **10 focused passes**. Each finding is tagged
`[FIXED]` (changed on this branch in the review) or `[DOC]` (documented for follow-up).
A consolidated change log and validation results are at the end.

Severity legend: **CRITICAL** (crash / memory corruption / build break), **HIGH**
(security / correctness regression), **MEDIUM** (latent defect / divergence), **LOW** (hygiene).

---

## Pass 1 — Module classification (`src/` vs `include/`, `.h`/`priv.h`)

Overall the layout is sound and consistent: `src/` is grouped by responsibility
(`backend/`, `codecs/`, `core/{base,display,object,runtime,tween}`, `fonts/`, `lib/`,
`platform/{esp_idf,host,linux}`, `render/`, `widgets/`), and each module keeps its
`*_priv.h` next to its `.c`. Public headers live only under `include/` and no public
header pulls a `src/` private header (verified).

- **[FIXED] MEDIUM** — `src/core/gfx_types_priv.h` was the only cross-module shared
  internal header living alone at the `src/core/` root, while every other shared
  internal header lives in `src/common/` (`gfx_comm.h`, `gfx_config_internal.h`,
  `gfx_mesh_frac.h`, `gfx_log_priv.h`). Moved to `src/common/gfx_types_priv.h`
  (`git mv`, 10 includers + `test_apps/main/test_anim.c` updated). Internal-only,
  no public API impact.
- **[DOC] MEDIUM** — `include/` carries a dual public namespace: the `include/gfx/*.h`
  facade (thin shims) over the real definitions in `include/core/gfx_*.h`, plus two
  legacy top-level umbrellas (`include/gfx.h`, `include/gfx_base.h`). This is an
  intentional in-progress migration, but since the only exported include dir is
  `include/`, **both namespaces are shipped public API** (v3.0.6). Relocating them is a
  breaking change; recommend consolidating later behind a deprecation window rather
  than now.

## Pass 2 — Public API / header hygiene

- **[DOC] HIGH** — `include/gfx/gfx.h` umbrella always includes
  `gfx/widgets/font_lvgl.h`, which pulls `lvgl.h`. Every `#include "gfx.h"` TU then
  needs LVGL on the include path and pays its compile cost. Recommend making the LVGL
  font module opt-in.
- **[DOC] MEDIUM** — Public widget headers declare `gfx_err_t` while several
  implementations still return `esp_err_t` (`gfx_anim.c`, `gfx_img.c`, `gfx_label.c`,
  widget create paths). Links today (same width) but is an API/type inconsistency.
- **[DOC] MEDIUM** — `gfx_asset.h` exposes internal ownership state in public structs
  (`gfx_asset_view_t.priv`, `gfx_asset_blob_t.view/.owned`) and carries two overlapping
  enums (`gfx_asset_backend_t` vs `gfx_asset_store_type_t`). Facade headers
  (`gfx/object.h`, `gfx/display.h`) redeclare a subset of the core API, creating a
  split/incomplete source of truth.
- **[DOC] LOW** — `include/gfx/widgets/motion.h` `@file` tag says `gfx_motion_scene.h`;
  thin facade headers omit `extern "C"`; `gfx/backends/memory.h` has an unused typedef.

## Pass 3 — Build system / CMake

- **[DOC] HIGH** — The ESP-IDF build globs `src/*.c`; the host build hardcodes
  `GFX_HOST_CORE_SRCS`. The two lists drift: the host list omits `gfx_qrcode.c`,
  `lib/qrcode/*.c`, `fonts/gfx_font_freetype.c`, `fonts/gfx_font_lvgl.c`, and
  `platform/host/gfx_touch_host_stub.c`. Because `gfx/gfx.h` pulls `qrcode.h`, a host
  consumer that uses QR code hits undefined symbols. Recommend deriving both lists from
  one source set (glob + platform filters) and adding a CI diff.
- **[DOC] MEDIUM** — Host build hardcodes heatshrink at
  `test_apps/managed_components/laride__heatshrink/...`, coupling the library build to
  test-app managed components.
- **[DOC] MEDIUM** — `sim/port/include` is `PUBLIC` on `gfx_host_core`, exporting ESP
  shim headers (`esp_log.h`, `esp_err.h`, ...) that can clash in mixed builds; should be
  `PRIVATE`/`INTERFACE`.
- **[DOC] LOW** — `CHANGELOG.md`/`idf_component.yml` are both `3.0.6` but the changelog
  entry omits the SDL backend, asset-store API, and header refactor.

## Pass 4 — Memory safety & ownership

- **[FIXED] HIGH** — `eaf_dec_get_frame_data()` / `eaf_dec_get_frame_size()` resident
  paths used `total_frames > index`, accepting negative indices → out-of-bounds access
  before the array base. Now bounded `index >= 0 && index < total_frames`
  (matching the streaming path).
- **[DOC] MEDIUM** — ESP-IDF `gfx_asset_esp_idf.c` allocates copy-backed views with
  `heap_caps_malloc(..., alloc_caps)` and frees with `free()`. Benign on ESP-IDF
  (`free()` handles heap-caps memory) but an abstraction inconsistency; prefer
  `heap_caps_free()`/`gfx_platform_free()`.
- **[DOC] MEDIUM** — Streaming `frame_buf` and store-backed `gfx_asset_view_t` return
  borrowed pointers; closing the store/stream while a view is open is a UAF. Lifetime
  rules should be documented (close views/blobs/streams before the store).
- **[DOC] MEDIUM** — `gfx_image_decoder_close()` calls every registered decoder's
  `close_cb`, not only the opener.

## Pass 5 — Bounds / integer overflow

- **[FIXED] CRITICAL** — `huffman_decode_data()` wrote `out_data[out_pos++]` with no
  capacity bound; a crafted bitstream/dictionary could heap-overflow the Huffman temp
  or block buffer. The destination capacity is already passed in via `*out_size` on
  entry in every call path; now enforced as a hard bound (`out_pos >= out_cap` → fail).
- **[FIXED] HIGH** — `huffman_decode_data()` `padding_bits` (from dictionary byte 0) was
  unchecked; `padding_bits >= in_size*8` underflows `total_bits` (`size_t`) and reads
  past the input. Now rejected.
- **[FIXED] HIGH** — `eaf_dec_decode_block()` did not validate `block_len`; `block_len-1`
  passed as `size_t` to decoders becomes `SIZE_MAX` on `block_len == 0`. Now validates
  `header/block_data/out_data != NULL && block_len >= 1`.
- **[DOC] HIGH** — Render/blend (`gfx_render.c`, `gfx_blend.c`) and the
  `GFX_BUFFER_OFFSET_*` macros (`gfx_types_priv.h`) compute destination offsets from
  caller geometry with no buffer-size guard; correctness relies on the scene graph
  providing in-bounds areas. Attacker-controlled input is confined to the codecs.
- **[DOC] MEDIUM** — `eaf_dec_decode_rle()` fast path does unaligned `uint32_t` stores
  (`*(uint32_t*)(out+pos)`); UB on strict-alignment targets (works on Xtensa today).
- **[DOC] MEDIUM** — `eaf_dec_calculate_offsets()` / `block_len` table are not validated
  against the frame payload size; recommend bounds-checking offsets ≤ payload.

## Pass 6 — EAF / codec correctness & security

- **[FIXED] HIGH** — Huffman/RLE short decodes (output shorter than the block) left an
  uninitialized tail in the per-block buffer → non-deterministic pixels. `eaf_dec_decode_block()`
  now zero-fills the tail (`out_size .. width*block_height`) after a successful decode.
- **[FIXED] HIGH** — `eaf_dec_decode_huffman_ctx()` single-color path only initialized
  the output when exactly one symbol was found; for 0 / >1 symbols it reported a full
  output while leaving the buffer uninitialized. Now reports a zero-length result so
  the caller zero-fills.
- **[FIXED] MEDIUM (defensive)** — `eaf_dec_decode_frame()` per-frame block buffer is now
  `calloc`'d, so any decoder that returns success without fully writing the block leaves
  a deterministic buffer.
- **[DOC] LOW** — A broken Huffman bitstream (NULL traversal mid-decode) logs and
  returns `ESP_OK` with partial output; left lenient on purpose to avoid rejecting
  real-world assets, now made deterministic by the capacity/tail fixes.

## Pass 7 — Widget lifecycle & correctness

- **[FIXED] HIGH** — `gfx_qrcode.c` allocated `qr_modules` with
  `gfx_platform_malloc(..., GFX_PLATFORM_HEAP_SPIRAM)` but freed it with libc `free()`
  (regen path + delete). Now uses `gfx_platform_free()` to match the allocator.
- **[DOC] HIGH** — `gfx_coverflow.c` card-mode children are orphaned on clear / mode
  switch (`free_items()` frees the descriptor array but never detaches the card objects
  from the parent `child_list`), so stale children can still be drawn.
- **[DOC] HIGH** — `gfx_pageflow.c` commits `page_index` and fires `changed_cb` at the
  start of the slide tween (not on completion), so neighbor pages render against the new
  index mid-transition.
- **[DOC] HIGH** — `gfx_widget_class.c` create-failure path frees the shell object but
  leaks the widget state blob (`obj->src`). Pre-existing on `main`.
- **[DOC] MEDIUM** — `gfx_object.c` `align()` skips the post-layout invalidate that
  `align_to()` performs; `gfx_anim.c` timer callback dereferences `obj` without a NULL
  guard; several setters mutate state without invalidating.

## Pass 8 — Platform parity & error handling

- **[FIXED] CRITICAL** — On ESP-IDF the glob compiled **both** `src/common/gfx_subsystem_init.c`
  and `src/platform/esp_idf/gfx_subsystem_esp_idf.c`, which define the same
  `gfx_subsystem_image_decoder_init/deinit` and `gfx_subsystem_font_init/deinit`
  → duplicate-symbol link error. `gfx_subsystem_init.c` is leftover from before the
  platform split (host uses `gfx_subsystem_host_stub.c`, ESP uses the `esp_idf` file).
  Deleted it.
- **[FIXED] CRITICAL** — `gfx_core_init()` error path dereferenced `disp_ctx->sync.*`
  before checking `disp_ctx != NULL`; a `cfg == NULL` or context-`malloc` failure
  NULL-derefs in cleanup. Now guarded.
- **[DOC] HIGH** — Host JPEG platform is a stub (`is_available()` always false), so the
  JPEG image path and EAF JPEG blocks have no host/sim parity even though libjpeg is
  available to the SDL demo.
- **[DOC] MEDIUM** — `gfx_backend_sdl.c` calls `SDL_QuitSubSystem(SDL_INIT_VIDEO)` per
  backend instance (no refcount); presentation API failures (`SDL_RenderPresent`, ...)
  are ignored.
- **[DOC] MEDIUM** — Linux `gfx_platform_task_create()` ignores `affinity`/`stack_caps`;
  `pthread_mutex/cond` return values are unchecked.

## Pass 9 — Concurrency / thread-safety

- **[DOC] MEDIUM** — `s_default_asset_store` (`gfx_asset.c`) is an unsynchronized global;
  `set/get/close` can race with render-thread `gfx_asset_source_load()`. Document a
  single-threaded-setup contract or add a mutex.
- **[DOC] LOW** — EAF decoder static registry (`s_eaf_decoders`) is initialized lazily
  with no guard; safe under single-threaded init only.

## Pass 10 — Determinism / reproducibility (deep dive)

A standalone harness decoded every EAF/AAF asset twice with independent resident
handles and compared pixel output. Initial state: the Huffman AAF showed 41 differing
frames (reported in a prior session as a suspected decoder bug).

Root cause isolated this pass: **`eaf_dec_get_palette_color()` returned early (`true`)
for an all-zero / transparent palette entry without setting `result->full`**, and the
caller in `eaf_dec_decode_frame()` ignores the return value and reads the
**uninitialized stack `gfx_color_t`**. Index 0 (transparent) therefore produced
heap/stack-layout-dependent garbage — deterministic within a process but differing
between independent decodes.

- **[FIXED] HIGH** — `eaf_dec_get_palette_color()` now sets `result->full = 0` on the
  transparent early-return.

Result after the Pass 5/6/10 fixes: **all 12 assets decode bit-identically across
independent handles (0 mismatches)**, including `transparent.eaf`.

---

## Change log (this review)

All changes are on `feat/sdl_support`, host-built and tested.

| Area | File | Change |
|------|------|--------|
| Module layout | `src/core/gfx_types_priv.h` → `src/common/gfx_types_priv.h` | Relocated shared internal header; updated 11 includers |
| EAF security | `src/lib/eaf/gfx_eaf_dec.c` | Huffman output-capacity bound; padding-bits underflow guard |
| EAF security | `src/lib/eaf/gfx_eaf_dec.c` | `eaf_dec_decode_block()` arg/`block_len` validation |
| EAF safety | `src/lib/eaf/gfx_eaf_dec.c` | Resident `get_frame_data`/`get_frame_size` negative-index guards |
| EAF determinism | `src/lib/eaf/gfx_eaf_dec.c` | Palette transparent entry sets `result->full = 0` (root cause) |
| EAF determinism | `src/lib/eaf/gfx_eaf_dec.c` | Block short-decode tail zero-fill; single-color huffman zero-length; `calloc` block buffer |
| Core crash | `src/core/runtime/gfx_core.c` | Guard `disp_ctx != NULL` in `gfx_core_init` error path |
| Build break | `src/common/gfx_subsystem_init.c` | Deleted (duplicate symbols vs `gfx_subsystem_esp_idf.c` on ESP-IDF) |
| Widget allocator | `src/widgets/basic/gfx_qrcode.c` | `qr_modules` freed with `gfx_platform_free` to match `gfx_platform_malloc` |

## Validation

- Host build (`build_host`): clean, no warnings.
- `ctest` (`build_host`): 8/8 passing.
- Decode determinism harness across all 12 EAF/AAF assets: 0 mismatches
  (independent resident handles produce byte-identical output).
- ESP-IDF build was not run in this environment; the ESP-only fixes
  (duplicate-symbol deletion, `heap_caps` note, QR allocator) are mechanical and
  reasoned, and should be confirmed with `idf.py build` on a target board config.

## Recommended follow-ups (not done here)

1. Reconcile host vs IDF source lists (QR + font stacks) and add a CI diff (Pass 3).
2. Coverflow card child lifecycle and pageflow page-commit timing (Pass 7).
3. Decide whether `font_lvgl.h` stays in the default umbrella; normalize `gfx_err_t`
   on public widget entry points (Pass 2).
4. Document asset-store / view lifetime and thread-safety contracts (Pass 4, 9).
5. Validate offsets/`block_len` against frame payload size and add overflow-checked
   geometry math in render/blend (Pass 5).
