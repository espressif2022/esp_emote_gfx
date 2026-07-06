# Core Modules

Runtime code under `src/core/` is split by responsibility:

| Directory | Role |
|-----------|------|
| `runtime/` | `gfx_core`, timer, touch scheduling |
| `display/` | display, refresh, backend registry |
| `object/` | object tree and widget class dispatch |
| `tween/` | animation tween engine |
| `fs/` | asset filesystem (mount, fopen, load) |
| `log/` | module log levels |
| `types/` | small type helpers (`gfx_color_hex`) |

Public declarations live in `include/gfx/`.
