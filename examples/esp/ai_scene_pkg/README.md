# ESP ai_scene_pkg

This ESP-IDF project is the legacy GSP object-loader demo. It loads a generated
GSP `.inc` package and materializes it as a `gfx_object_t` tree through
`gfx_gsp_load_with_fonts()`.

It is intentionally separate from `examples/esp/arena_demo`:

| Demo | Path | Purpose |
| --- | --- | --- |
| `ai_scene_pkg` | `GSP package -> gfx_object_t tree` | Legacy/debug path for the old GSP loader. |
| `arena_demo` | `GSP/ARN package -> arena_scene` | Recommended formal arena path, compare/sweep/playground. |

`ai_scene_pkg` does not compile or include files from `arena_demo`.

## Build

```bash
cd examples/esp/ai_scene_pkg
idf.py -B build set-target esp32s31
idf.py -B build reconfigure build flash monitor
```

Default scene:

```text
inc/home_control_v4.inc
```

Override it at configure time:

```bash
idf.py -B build -DGSP_SCENE_INC=\"inc/home.inc\" reconfigure build flash monitor
```

Expected startup log:

```text
gsp_board: start ESP ai_scene_pkg legacy GSP object-loader
gsp_board: scene include: inc/home_control_v4.inc
gsp_board: load inc/home_control_v4.inc: screen=800x480 ...
gsp_board: scene loaded: objects=69 blobs=0 actions=9
```

The `fonts` partition is flashed with the app and is used for runtime FreeType
font binding. If it is unavailable, the demo falls back to `font_puhui_16_4`.
