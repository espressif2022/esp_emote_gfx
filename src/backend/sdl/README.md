# SDL Host Backend

This directory is a host-only adapter for desktop simulation (`sdl_backend.c`). It lives under
`src/backend/` as a real backend, but ESP-IDF component builds explicitly
exclude it so embedded targets do not link SDL.

The adapter implements the same display backend contract used by embedded
panels:

- GFX renders semantic RGB565 into its render buffer.
- The SDL backend receives flush rectangles.
- The adapter converts RGB565 to a desktop texture format at the final boundary.
- SDL input events can later be mapped into the GFX touch pipeline.

## Build

On Linux, install SDL2 or SDL3 development files, then configure the host build
from the repository root:

```sh
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl -j
./build-host-sdl/gfx_host_sdl_demo
```

The host CMake target intentionally compiles `src/platform/linux/` and excludes
`src/platform/esp_idf/`. ESP-IDF builds keep using `idf_component_register()` and do
not link SDL.

For a core-only compile smoke test without SDL:

```sh
cmake -S . -B build-host-core -DGFX_BUILD_HOST_SDL=OFF
cmake --build build-host-core -j
```

Current status: host simulator backend. It verifies the GFX display backend,
POSIX platform boundary, image/animation loading, touch injection, and basic
widget rendering. Text rendering uses a tiny host bitmap font for smoke tests.
A formal host runner and more automated screenshot/golden tests should be added
in later simulator milestones.
