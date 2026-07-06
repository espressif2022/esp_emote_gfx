# Host Simulation Stubs

Used by the Linux/SDL host build together with `src/platform/linux/`. Filenames
use a `host_` prefix so stubs stay searchable in tabs and grep.

| File | Role |
|------|------|
| `host_font.c` / `host_font_priv.h` | Host FreeType font backend |
| `host_accel_stub.c` | No-op PPA acceleration |
| `host_jpeg_stub.c` | No-op hardware JPEG |
| `host_subsystem_stub.c` | Subsystem init stubs |
| `host_touch_stub.c` | `gfx_touch_port.h` stubs |
| `host_touch_core_stub.c` | `gfx_touch_*` core API stubs (optional link) |

Runtime primitives (mutex, tasks, heap) come from `../linux/linux_platform.c`.
