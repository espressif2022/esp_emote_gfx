# `home.inc` GSP Scene Package Manifest

This file is generated from `home.gsp` by `gsp_package_tools.py`. It explains the binary bytes that are wrapped by the companion `.inc`.

## Export Summary

- Source package: `home.gsp`
- Package include: `home.inc`
- Runtime package symbol: `home_scene_pkg[]`
- Package length symbol: `home_scene_pkg_len`
- Runtime font binding symbol: `home_fonts[]`
- Source preview image: `home_preview.bmp`
- Format: GSP1 v4, little-endian, pointer-free runtime package
- Loader: `gsp_load_with_fonts(home_scene_pkg, home_scene_pkg_len, ...)`

## Header

| Field | Value |
|---|---:|
| magic | `GSP1` |
| version | `4` |
| screen | `480 x 480` |
| screen_bg | `0x0E1116` |
| obj_count | `12` |
| obj_table_off | `56` |
| str_table_off | `1029` |
| blob_count | `1` |
| blob_table_off | `824` |
| action_count | `3` |
| action_table_off | `844` |
| total_size | `3085` |
| crc32 | `0xD6A5DCD0` |

## Binary Layout

| Region | Byte Range | Size | Notes |
|---|---:|---:|---|
| Header | `0..55` | `56` | Fixed GSP header (v4) |
| Object table | `56..823` | `768` | `12 * 64B` object entries |
| Blob table | `824..843` | `20` | `1 * 20B` blob entries |
| Action table | `844..915` | `72` | `3 * 24B` action entries |
| Params area | `916..1028` | `113` | Widget-private params blocks |
| String table | `1029..1236` | `208` | NUL-terminated UTF-8 strings |
| Blob data | `1237..3084` | `1848` | Compressed image payloads |

## Component Rules Used

| ID | Type | Runtime create path |
|---:|---|---|
| `1` | container | `gfx_container_create()` |
| `2` | label | `gfx_label_create()` |
| `3` | button | `gfx_button_create()` |
| `4` | image | `gfx_image_create()` + package blob source |
| `5` | list | `gfx_list_create()` + params-v1 items |
| `6` | wheel | `gfx_wheel_create()` + params-v1 items |
| `7` | layer | `gfx_container_create()` + layer switching |

Parent rule: entries are preorder; every non-root object must reference an earlier object index. `0xFFFF` means root.

## Object Table

| Idx | Role | Type | Parent | Rect | Flags | Name / Text / Callback / Blob / Params | Font | Bind | Notes |
|---:|---|---|---:|---|---:|---|---:|---:|---|
| `0` | container | container | root | `(0,0 480x480)` | `0x004` | none | `0` | `0` | screen background 0x0E1116; flags: BG_COLOR |
| `1` | homeLayer | layer | `0` | `(36,68 408x344)` | `0x09C` | name@`1029` `homeLayer` | `0` | `0` | layer; flags: BG_COLOR|BORDER|RADIUS|NAME |
| `2` | title | label | `1` | `(30,30 340x32)` | `0x083` | name@`1066` `title`, text@`1039` `AI Scene · u32-offset pkg` | `0` | `0` | -; flags: TEXT|FG_COLOR|NAME |
| `3` | subtitle | label | `1` | `(30,72 340x26)` | `0x083` | name@`1106` `subtitle`, text@`1072` `page 1: image + callback + action` | `1` | `0` | -; flags: TEXT|FG_COLOR|NAME |
| `4` | 温度 23.5°C 数据绑定 | label | `1` | `(30,106 340x30)` | `0x003` | text@`1115` `温度 23.5°C 数据绑定` | `2` | `1` | bind_id 1; flags: TEXT|FG_COLOR |
| `5` | image | image | `1` | `(30,150 96x72)` | `0x040` | blob `0` | `0` | `0` | uses baked image blob; flags: IMAGE |
| `6` | homeNext | button | `1` | `(238,246 140x60)` | `0x0BF` | name@`1154` `homeNext`, text@`1143` `Next`, cb@`1148` `on_ok` | `3` | `0` | C callback `on_ok`; flags: TEXT|FG_COLOR|BG_COLOR|BORDER|RADIUS|CALLBACK|NAME |
| `7` | selectorLayer | layer | `0` | `(36,68 408x344)` | `0x19C` | name@`1163` `selectorLayer` | `0` | `0` | layer; initially hidden; flags: BG_COLOR|BORDER|RADIUS|NAME|HIDDEN |
| `8` | Page 2 · List + Wheel | label | `7` | `(30,28 340x32)` | `0x003` | text@`1177` `Page 2 · List + Wheel` | `0` | `0` | -; flags: TEXT|FG_COLOR |
| `9` | featureList | list | `7` | `(30,78 168x170)` | `0x88E` | name@`1200` `featureList`, params@`916` `67B` | `1` | `0` | items=4, selected=1, item_height=34, rows/page=4, flags=0x0001, values=[`Layer model`, `Wheel widget`, `List widget`, `Binary params`]; flags: FG_COLOR|BG_COLOR|BORDER|NAME|PARAMS |
| `10` | formatWheel | wheel | `7` | `(220,78 158x170)` | `0x88E` | name@`1212` `formatWheel`, params@`983` `46B` | `1` | `0` | items=4, selected=0, item_height=30, rows/page=5, flags=0x0001, values=[`RGB565`, `RGB888`, `BGR888`, `ARGB8888`]; flags: FG_COLOR|BG_COLOR|BORDER|NAME|PARAMS |
| `11` | selectorNext | button | `7` | `(238,246 140x60)` | `0x09F` | name@`1224` `selectorNext`, text@`1143` `Next` | `3` | `0` | -; flags: TEXT|FG_COLOR|BG_COLOR|BORDER|RADIUS|NAME |

## Action Table (v4)

Position-independent `event -> action` records (24B each). No inline structs, no pointers: targets are u16 object indexes or name offsets, params are u32 string offsets, colors are scalar `arg`.

| Idx | Src Obj | Event | Action | Target | Param / Arg |
|---:|---:|---|---|---|---|
| `0` | `6` | click | set_bg_color | obj `1` | arg `0x1E3A5F` |
| `1` | `6` | click | goto | name@`1163` `selectorLayer` | none |
| `2` | `11` | click | goto | name@`1029` `homeLayer` | none |

## String Table

| Offset | String |
|---:|---|
| `1029` | `homeLayer` |
| `1039` | `AI Scene · u32-offset pkg` |
| `1066` | `title` |
| `1072` | `page 1: image + callback + action` |
| `1106` | `subtitle` |
| `1115` | `温度 23.5°C 数据绑定` |
| `1143` | `Next` |
| `1148` | `on_ok` |
| `1154` | `homeNext` |
| `1163` | `selectorLayer` |
| `1177` | `Page 2 · List + Wheel` |
| `1200` | `featureList` |
| `1212` | `formatWheel` |
| `1224` | `selectorNext` |

## Resource Bindings

Fonts are currently outside the binary package as a C binding table in the generated `.inc`. Objects refer to these by `font_id`.

| Font Id | Family | Path | Size | Weight | Style | Used By |
|---:|---|---|---:|---:|---:|---|
| `0` | `NotoSansCJK` | `fonts/NotoSansCJK-Regular.ttc` | `22` | `400` | `0` | object `2`, object `8` |
| `1` | `NotoSansCJK` | `fonts/NotoSansCJK-Regular.ttc` | `17` | `400` | `0` | object `3`, object `9`, object `10` |
| `2` | `NotoSansCJK` | `fonts/NotoSansCJK-Regular.ttc` | `20` | `400` | `0` | object `4` |
| `3` | `NotoSansCJK` | `fonts/NotoSansCJK-Regular.ttc` | `21` | `700` | `0` | object `6`, object `11` |

Images/blobs:

| Blob Id | Size | Format | Codec | Raw | Compressed | Data Offset | Used By |
|---:|---|---:|---|---:|---:|---:|---|
| `0` | `96x72` | `0x04` | `rle16` | `13824` | `1848` | `1237` | object `5` |

## Runtime Path

```text
home.inc
  home_fonts[]      -> host/device font creation or binding
  home_scene_pkg[]  -> gsp_load_with_fonts()
      header/CRC validation
      object table scan
      string offset resolution
      runtime refs: object name / bind_id -> gfx_object_t
      list/wheel params-v1 decode -> widget items
      blob table resolution -> image blob decode/cache
      gfx_*_create + setter
      callback name binding: "on_ok" -> on_ok_cb
      action table dispatch: Next buttons -> layer GOTO
      render through normal GFX object tree
```

## Validation

Run after export:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy GSP_HEADLESS=1 ./build-host-sdl/gfx_ai_scene_pkg_demo
ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure
```

Latest result:

- Package size: `3085` bytes
- CRC: `0xD6A5DCD0`
- Loaded objects: `12`
- Image blobs: `1`
- Self-check: expected `PASS (12 objects from home.inc)`

## Known Limits

- Font descriptions are still C-side binding metadata, not binary package records.
- Image preview is extracted from the first RGB565 blob in the package.
- List/wheel params-v1 is currently a compact demo format; future widgets should define their own params profile.
