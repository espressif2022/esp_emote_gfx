# Host Simulation

This directory contains host-side simulation support for `esp_emote_gfx`.
It is intentionally outside the ESP-IDF component build path.

## Layout

| Path | Role |
| --- | --- |
| `host/` | Host executables, smoke tests, and the shared SDL runner |
| `port/include/` | Small host-only compatibility headers such as `sdkconfig.h` and LVGL font structs |

The runnable format playground uses shared UI code from `examples/format_playground/`
and the SDL backend from `src/backend/sdl/`.

## Dependencies

Install CMake, a C compiler, SDL2 or SDL3 development files, and libjpeg
development files.

Ubuntu example:

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev libjpeg-dev
```

SDL3 is also supported when `pkg-config` can find `sdl3`.

## Build

From the `esp_emote_gfx` repository root:

```bash
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl -j
```

Useful targets:

```bash
cmake --build build-host-sdl --target gfx_host_sdl_demo
cmake --build build-host-sdl --target gfx_host_smoke
```

## Run The SDL Demo

```bash
./build-host-sdl/gfx_host_sdl_demo
```

By default, the demo loads assets from `examples/assets/format`.
Use `GFX_FS_ROOT` to point at another loose asset directory:

```bash
GFX_FS_ROOT=/path/to/assets ./build-host-sdl/gfx_host_sdl_demo
```

## Run Smoke Tests

```bash
ctest --test-dir build-host-sdl --output-on-failure
```

For a headless SDL smoke run:

```bash
cmake -D GFX_HOST_SDL_DEMO=build-host-sdl/gfx_host_sdl_demo \
      -P simulation/host/run_sdl_dummy_smoke.cmake
```

## Reuse From Another Component

Application or component repositories can reuse the host simulator by adding
`esp_emote_gfx` as a normal CMake subdirectory in their host-only build:

```cmake
set(ESP_EMOTE_GFX_ROOT "/path/to/esp_emote_gfx" CACHE PATH "esp_emote_gfx root")
add_subdirectory("${ESP_EMOTE_GFX_ROOT}" "${CMAKE_BINARY_DIR}/esp_emote_gfx")

target_link_libraries(my_host_demo
    PRIVATE
        gfx_host_core
        gfx_backend_sdl
        gfx_host_runner
)
```

For ESP-IDF builds, keep using the component dependency path instead.

## Directory Name

`simulation` is used as the full top-level name for host simulation entry
points and small host compatibility shims. If this area grows, split by role
under `simulation/` first, such as `simulation/host/`, `simulation/port/`, and
`simulation/tools/`.
