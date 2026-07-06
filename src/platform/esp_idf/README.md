# ESP-IDF Platform Port

Target-specific implementations for ESP-IDF builds. Filenames use an `esp_idf_`
prefix so they stay searchable outside the directory path.

| File | Role |
|------|------|
| `esp_idf_platform.c` | Core `gfx_platform.h` contract (FreeRTOS, heap, events) |
| `esp_idf_fs.c` | VFS / asset file access |
| `esp_idf_accel.c` | PPA draw acceleration |
| `esp_idf_jpeg.c` | Hardware JPEG decode port |
| `esp_idf_flush_staging.c` / `esp_idf_flush_staging.h` | DMA2D / PPA flush staging patch |
| `esp_idf_subsystem.c` | Subsystem init wiring (font, jpeg, accel) |
| `esp_idf_touch.c` | `gfx_touch_port.h` driver adapter |
| `gfx_err_bridge.h` | `esp_err_t` ↔ `gfx_err_t` (local only) |

Public cross-target headers live in `src/platform/` (`gfx_platform.h`, `*_priv.h`).
