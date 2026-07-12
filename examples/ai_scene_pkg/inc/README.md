# inc

Compile-time scene package includes for host and ESP demos.

These files are generated and should not be edited by hand.

| File | Symbols | Size | Producer | Used by |
|---|---|---:|---|---|
| `home.inc` | `HOME_*` | 480x480 | `gsp_export/gsp_package_tools.py` | default host GSP demo; arena GSP conversion demo |
| `home_control_v4.inc` | `HOME_CONTROL_*` | 800x480 | external/uic v4 exporter | legacy ESP GSP demo; ESP arena `control` scene |

Host scene selection is compile-time:

```bash
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home_control_v4.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
```

Regenerate `home.inc` through [`../gsp_export/README.md`](../gsp_export/README.md).
