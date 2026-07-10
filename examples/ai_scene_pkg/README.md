# ai_scene_pkg

Host/ESP examples for binary scene packages.

This directory has two related tracks:

- **GSP**: the original binary scene package format loaded through `gsp_load`.
- **ARN / arena**: the newer arena runtime model used to run package scenes directly.

The production library code lives under `include/gfx/scene/` and `src/scene/`.
This directory keeps examples, exporters, generated demo packages, and validation
entry points.

## Directory Map

| Path | Role |
|---|---|
| `ai_scene_pkg_demo.c` | Host SDL demo for GSP `.inc` packages. |
| `gsp_pack.c` | Host-side GSP pack helper used by exporter examples. |
| `inc/` | Generated `.inc` packages compiled into host/ESP demos. |
| `gsp_export/` | Export tools plus generated `home.*` artifacts. |
| `gsp_protocol/` | GSP binary protocol documentation. |
| `arena_model/` | Host arena smoke/demo/compare entry points（设计文档见 `docs/scene/`）. |
| `gsp_scene_package_skill.md` | Skill instructions for generating/updating GSP packages. |

## Generated Files

These files are generated artifacts and should be regenerated with the tools
rather than edited by hand:

| File | Generator |
|---|---|
| `inc/home.inc` | `gsp_export/gsp_package_tools.py` |
| `inc/home_v4.inc` | external/uic v4 exporter |
| `inc/home_control_v4.inc` | external/uic v4 exporter |
| `gsp_export/home.gsp` | `gfx_ai_scene_pkg_export_home` |
| `gsp_export/home.manifest.md` | `gsp_package_tools.py` |
| `gsp_export/home_preview.bmp` | `gsp_package_tools.py` |
| `gsp_export/home.arn` / `home.arn.md` | `gsp_package_tools.py --arn` or `gsp_to_arn.py` |

Do not keep `__pycache__/` or other local build/cache files in this tree.

## Build And Run

From the repository root:

```bash
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo

./build-host-sdl/gfx_ai_scene_pkg_demo

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy GSP_HEADLESS=1 \
  ./build-host-sdl/gfx_ai_scene_pkg_demo

ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure
```

Expected self-check output contains `SELF-CHECK: PASS`.

## Select A Scene

`gfx_ai_scene_pkg_demo` includes the selected scene at compile time. The path is
relative to `examples/ai_scene_pkg/`.

| Scene | Symbols | Size | Notes |
|---|---|---:|---|
| `inc/home.inc` | `HOME_*` | 480x480 | Default generated demo package. |
| `inc/home_v4.inc` | `HOME_*` | 480x480 | Compact uic v4 package. |
| `inc/home_control_v4.inc` | `HOME_CONTROL_*` | 800x480 | Larger home-control package. |

```bash
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home_control_v4.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
./build-host-sdl/gfx_ai_scene_pkg_demo
```

## Regenerate `home`

```bash
cmake --build build-host-sdl --target gfx_ai_scene_pkg_export_home
./build-host-sdl/gfx_ai_scene_pkg_export_home examples/ai_scene_pkg/gsp_export

python3 examples/ai_scene_pkg/gsp_export/gsp_package_tools.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out-dir examples/ai_scene_pkg/gsp_export \
  --inc-dir examples/ai_scene_pkg/inc \
  --prefix home \
  --arn
```

Then rebuild the demo because `.inc` files are compile-time inputs:

```bash
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
GSP_HEADLESS=1 SDL_VIDEODRIVER=dummy ./build-host-sdl/gfx_ai_scene_pkg_demo
```

More details: [`gsp_export/README.md`](gsp_export/README.md).

## Arena / ARN

Arena is the direct-runtime path for binary scenes:

```text
GSP bytes --gsp_to_arena()--> ARN bytes
ARN bytes --arena_load()--> gfx_arena_scene_t
```

Useful entry points:

```bash
cmake --build build-host-sdl --target \
  gfx_arena_model_smoke gfx_arena_draw_demo gfx_arena_dirty_demo \
  gfx_arena_sdl_demo gfx_gsp_to_arena_demo gfx_arena_compare_demo

./build-host-sdl/gfx_arena_model_smoke
./build-host-sdl/gfx_arena_draw_demo
./build-host-sdl/gfx_arena_dirty_demo
./build-host-sdl/gfx_gsp_to_arena_demo
./build-host-sdl/gfx_arena_compare_demo
```

More details: [`arena_model/README.md`](arena_model/README.md) and
[`docs/scene/`](../../docs/scene/README.md).

## ESP Demos

| Demo | Path | Purpose |
|---|---|---|
| Arena path | `examples/esp/arena_demo` | Recommended package-scene path: ARN/formal arena, GSP->ARN, compare, sweep, playground. |
| Legacy GSP object path | `examples/esp/ai_scene_pkg` | Legacy/debug only: loads GSP through object factory into `gfx_object_t`. |

The ESP arena demo keeps its own runtime helper files in
`examples/esp/arena_demo/main/`; host arena demos may reuse those files.
New package-scene work should target arena, not the legacy GSP object-loader demo.

## Docs

| Topic | Path |
|---|---|
| Scene index / plan / ABI / parity | [`docs/scene/`](../../docs/scene/README.md) |
| GSP protocol (zh) | [`gsp_protocol/gsp_binary_protocol_zh.md`](gsp_protocol/gsp_binary_protocol_zh.md) |
| GSP protocol (en) | [`gsp_protocol/gsp_binary_protocol_en.md`](gsp_protocol/gsp_binary_protocol_en.md) |
| Public headers | [`include/gfx/scene/`](../../include/gfx/scene/README.md) |

## Maintenance Rules

- Keep package bytes pointer-free: package references are offsets or indexes.
- Keep loader validation strict: bad packages return errors instead of crashing.
- Keep generated artifacts paired with their manifest.
- Keep demo-specific helper code next to the demo that owns it.
- Keep design/plan docs in `docs/scene/`; keep `arena_model/` for host validation entry points only.
