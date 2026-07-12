# Examples and validation

This is the canonical entry point for runnable demos and validation projects.

## Choose an entry point

| Goal | Path or target | Environment |
| --- | --- | --- |
| Browse widgets and render formats | `gfx_host_sdl_demo` | Linux/macOS host + SDL |
| Run automated host checks | `gfx_host_smoke` / `ctest` | Host, suitable for CI |
| Validate GSP/ARN scene packages | `gfx_ai_scene_pkg_demo`, `gfx_arena_*` | Host, headless or SDL |
| Run the complete widget playground on a board | `examples/esp/format_rgb565/` or `format_rgb888/` | ESP-IDF + display board |
| Run the ARN package runtime on a board | `examples/esp/arena_demo/` | ESP-IDF + display board |
| Check the legacy GSP object loader | `examples/esp/ai_scene_pkg/` | ESP-IDF + display board |
| Run automatic component assertions | `test_apps/unit/` | ESP-IDF, no BSP |
| Validate display integration | `test_apps/` | ESP-IDF + BSP |
| Inspect visual widget cases | `test_apps/visual/` | ESP-IDF + BSP |

`examples/` contains reusable demo scenes, assets, tools, and ESP-IDF projects.
Host executable entry points and compatibility headers live in `simulation/`.
Assertions and hardware conformance projects live in `test_apps/`.

## Host simulation

Install CMake, SDL2 or SDL3 development files, and libjpeg development files,
then run from the repository root:

```bash
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON -DBUILD_TESTING=ON
cmake --build build-host-sdl --parallel
ctest --test-dir build-host-sdl --output-on-failure
```

Open the widget playground:

```bash
./build-host-sdl/gfx_host_sdl_demo
```

The default asset root is `examples/assets/format`. Override it with
`GFX_FS_ROOT=/path/to/assets`. See [`simulation/README.md`](../simulation/README.md)
for simulator dependencies and embedding details.

## Scene package validation

GSP/ARN Arena is an experimental, opt-in component surface in the current
release. Configure a separate build directory when working on it:

```bash
cmake -S . -B build-host-arena \
  -DGFX_BUILD_HOST_SDL=ON \
  -DBUILD_TESTING=ON \
  -DGFX_BUILD_EXPERIMENTAL_ARENA=ON
cmake --build build-host-arena --parallel
ctest --test-dir build-host-arena --output-on-failure
```

Use `build-host-arena` instead of `build-host-sdl` in the scene-specific
commands below when following this split configuration.

`ai_scene_pkg/` contains two paths:

- GSP object loading, retained for compatibility and package conversion;
- ARN arena loading and direct runtime rendering, used by the current package runtime.

The runtime library is under `include/gfx/scene/` and `src/scene/`. Demo
packages, exporters, protocol notes, and validation entry points are organized
as follows:

The Arena release boundary, rendering contract, and Object/Arena parity gates
are documented in [`docs/arena_stability.rst`](../docs/arena_stability.rst).

| Path | Role |
| --- | --- |
| `ai_scene_pkg/ai_scene_pkg_demo.c` | Host SDL demo for compiled GSP packages |
| `ai_scene_pkg/inc/` | Generated `.inc` package fixtures |
| `ai_scene_pkg/gsp_export/` | Export tools and generated `home.*` artifacts |
| `ai_scene_pkg/gsp_protocol/` | GSP binary protocol documentation |
| `ai_scene_pkg/arena_model/` | Host ARN smoke, render, SDL, and comparison entry points |

After configuring the experimental Arena Host build, build and run the GSP loader demo:

```bash
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
./build-host-sdl/gfx_ai_scene_pkg_demo

GSP_HEADLESS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./build-host-sdl/gfx_ai_scene_pkg_demo

ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure
```

Expected self-check output contains `SELF-CHECK: PASS`.

The selected GSP scene is a compile-time input relative to
`examples/ai_scene_pkg/`:

| Scene | Size | Purpose |
| --- | ---: | --- |
| `inc/home.inc` | 480x480 | Default generated package |
| `inc/home_control_v4.inc` | 800x480 | Larger home-control package |

```bash
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home_control_v4.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
./build-host-sdl/gfx_ai_scene_pkg_demo
```

ARN is the direct runtime path:

```text
GSP bytes --gsp_to_arena()--> ARN bytes
ARN bytes --arena_load()-----> gfx_arena_scene_t
```

Build and run individual ARN validation targets:

```bash
cmake --build build-host-sdl --target \
  gfx_arena_model_smoke \
  gfx_arena_draw_demo \
  gfx_arena_dirty_demo \
  gfx_arena_sdl_demo \
  gfx_arena_playground_sdl_demo \
  gfx_gsp_to_arena_demo \
  gfx_arena_compare_demo

./build-host-sdl/gfx_arena_model_smoke
./build-host-sdl/gfx_arena_draw_demo
./build-host-sdl/gfx_arena_dirty_demo
./build-host-sdl/gfx_gsp_to_arena_demo
./build-host-sdl/gfx_arena_compare_demo
```

### Regenerate the bundled GSP/ARN fixture

```bash
cmake --build build-host-sdl --target gfx_ai_scene_pkg_export_home
./build-host-sdl/gfx_ai_scene_pkg_export_home examples/ai_scene_pkg/gsp_export

python3 examples/ai_scene_pkg/gsp_export/gsp_package_tools.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out-dir examples/ai_scene_pkg/gsp_export \
  --inc-dir examples/ai_scene_pkg/inc \
  --prefix home \
  --arn

cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
GSP_HEADLESS=1 SDL_VIDEODRIVER=dummy \
  ./build-host-sdl/gfx_ai_scene_pkg_demo
```

Generated byte arrays must not be edited by hand. Regeneration and manifest
rules live in
[`ai_scene_pkg/gsp_export/SKILL.md`](ai_scene_pkg/gsp_export/SKILL.md).
Exporter details are in
[`ai_scene_pkg/gsp_export/README.md`](ai_scene_pkg/gsp_export/README.md), and
ARN target details are in
[`ai_scene_pkg/arena_model/README.md`](ai_scene_pkg/arena_model/README.md).

Scene documentation:

| Topic | Path |
| --- | --- |
| Runtime architecture | [`docs/architecture.md`](../docs/architecture.md) |
| Asset format | [`docs/asset_spec.rst`](../docs/asset_spec.rst) |
| GSP protocol (Chinese) | [`gsp_binary_protocol_zh.md`](ai_scene_pkg/gsp_protocol/gsp_binary_protocol_zh.md) |
| GSP protocol (English) | [`gsp_binary_protocol_en.md`](ai_scene_pkg/gsp_protocol/gsp_binary_protocol_en.md) |
| Public scene headers | [`include/gfx/scene/`](../include/gfx/scene/README.md) |

## ESP-IDF examples

Each directory under `examples/esp/` is an independent ESP-IDF project:

```bash
cd examples/esp/format_rgb565
idf.py set-target <target>
idf.py build flash monitor
```

Use `format_rgb565` or `format_rgb888` for the full object/widget playground.
Use `arena_demo` for the ARN package runtime. The legacy `ai_scene_pkg` project
exists only to validate the older GSP-to-object loading path.

New package-scene work should target `arena_demo`. Its build modes, including
playground, GSP conversion, sweep, and A/B comparison, are documented in
[`esp/arena_demo/README.md`](esp/arena_demo/README.md).

## Tests

`test_apps` is not an end-user example. It is split by verification role:

```bash
cd test_apps/unit && idf.py build       # automatic assertions, no BSP
cd test_apps && idf.py build            # hardware integration
cd test_apps/visual && idf.py build     # manual visual cases
```

Board-specific configuration and CI commands are documented in
[`test_apps/README.md`](../test_apps/README.md).

## Directory ownership

| Path | Contents |
| --- | --- |
| `format_playground/` | Shared widget UI used by host and ESP projects |
| `assets/` | Demo and test assets |
| `motion/` | Generated motion scene fixture |
| `ai_scene_pkg/` | Scene package examples, exporters, fixtures, and host validation |
| `esp/` | Standalone ESP-IDF example projects |
| `common/` | Display glue shared by examples |

Generated `build*`, `managed_components`, `sdkconfig`, cache, and Python bytecode
directories are local artifacts and must not be committed.

Scene package fixtures must remain pointer-free, use offsets or indexes for
package references, pass strict loader validation, and keep generated artifacts
synchronized with their manifests.
