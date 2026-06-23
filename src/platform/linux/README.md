# Linux Host Port

The Linux/POSIX platform implementation is in:

```text
src/platform/linux/gfx_platform_linux.c
```

It implements the internal `gfx_platform` contract with:

- `pthread` recursive mutexes
- `pthread` condition-variable event bits
- `pthread_create` render task startup
- `clock_gettime(CLOCK_MONOTONIC)` timing
- standard heap allocation

This is the base needed by the SDL simulator. A complete Linux simulator target
should compile this port instead of `src/platform/esp_idf/gfx_platform_esp_idf.c`,
then link the SDL backend under `src/backend/sdl/`.

Minimal compile smoke test:

```sh
cc -std=c11 -Wall -Wextra \
  -Iinclude -Isrc -Isimulation/port/include \
  -c src/platform/linux/gfx_platform_linux.c \
  -o /tmp/gfx_platform_linux.o \
  -pthread
```

`simulation/port/include` contains small compatibility headers for host builds that do
not include ESP-IDF headers.

