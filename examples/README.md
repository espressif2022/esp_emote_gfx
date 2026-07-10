# Examples

Runnable demos and shared assets. Host SDL entry points live under `simulation/host/`; this tree holds the shared UI and device projects.

## Layout

| Path | Role |
| --- | --- |
| `format_playground/` | Format / widget playground (host SDL + ESP RGB boards) |
| `assets/format/` | Binary assets for format playground (`GFX_FS_ROOT` default) |
| `assets/fonts/` | Font fixtures used by host smoke tests |
| `motion/` | Generated motion scene data (`claw_motion.inc`) |
| `ai_scene_pkg/` | Host GSP/ARN package demos & export tools |
| `esp/arena_demo/` | ESP-IDF ARN package-scene demo |
| `esp/format_rgb565/` | ESP-IDF RGB565 HMI board demo |
| `esp/format_rgb888/` | ESP-IDF BGR888 HMI board demo |

Scene design / plan / ABI：**[`docs/scene/`](../docs/scene/README.md)**


## Host (SDL)

See `../simulation/README.md` for the full host simulation build notes.

```bash
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl -j
./build-host-sdl/gfx_host_sdl_demo
# optional: GFX_FS_ROOT=/path/to/assets ./build-host-sdl/gfx_host_sdl_demo
```

Expression host simulation is owned by the `esp_emote_expression` repository. From that repository, configure a host build and run `gfx_host_expression_demo`.

## ESP-IDF (board demos)

```bash
cd examples/esp/format_rgb565   # or format_rgb888
idf.py build flash monitor
```

Unity / conformance tests remain under `test_apps/main/`.
