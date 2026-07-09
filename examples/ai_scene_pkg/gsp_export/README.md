# gsp_export — 场景包导出工具

把作者侧场景编译成原始 `.gsp`，再生成 demo 用的 `.inc`、可读 manifest 和预览图。

## 产物

| 文件 | 说明 |
|---|---|
| `gsp_export_home.c` | 内置 home 场景 → `home.gsp` |
| `gsp_package_tools.py` | `home.gsp` → `../inc/home.inc` + `home.manifest.md` + `home_preview.bmp` |
| `home.gsp` | 原始二进制包 |
| `home.manifest.md` | 包内容说明（随导出更新） |
| `home_preview.bmp` | 从 blob 抽出的预览图 |

`.inc` 默认写到上级目录的 `../inc/`，供 `gfx_ai_scene_pkg_demo` 编译期 `#include`。

## 用法

在仓库根目录：

```bash
cmake --build build-host-sdl --target gfx_ai_scene_pkg_export_home
./build-host-sdl/gfx_ai_scene_pkg_export_home examples/ai_scene_pkg/gsp_export

python3 examples/ai_scene_pkg/gsp_export/gsp_package_tools.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out-dir examples/ai_scene_pkg/gsp_export \
  --inc-dir examples/ai_scene_pkg/inc \
  --prefix home
```

完整说明见上级 [`../README.md`](../README.md) 第 5 节。
