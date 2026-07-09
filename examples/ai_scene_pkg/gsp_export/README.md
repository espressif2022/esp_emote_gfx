# gsp_export — 场景包导出工具

把作者侧场景编译成原始 `.gsp`，再生成 demo 用的 `.inc`、可读 manifest 和预览图。

## 产物

| 文件 | 说明 |
|---|---|
| `gsp_export_home.c` | 内置 home 场景 → `home.gsp` |
| `gsp_package_tools.py` | `home.gsp` → `../inc/home.inc` + `home.manifest.md` + `home_preview.bmp`（`--arn` 另出 `.arn`） |
| `gsp_to_arn.py` | `home.gsp` → `home.arn`（ARN1，供 `arena_load`） |
| `home.gsp` | 原始二进制包 |
| `home.manifest.md` | 包内容说明（随导出更新） |
| `home_preview.bmp` | 从 blob 抽出的预览图 |
| `home.arn` | 可选 ARN1 包（`--arn` 或 `gsp_to_arn.py`） |

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
  --prefix home \
  --arn

# 或单独 export ARN
python3 examples/ai_scene_pkg/gsp_export/gsp_to_arn.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out examples/ai_scene_pkg/gsp_export/home.arn
```

完整说明见上级 [`../README.md`](../README.md) 第 5 节；ARN 策略见 [`../arena_model/ARENA_TOOLCHAIN.md`](../arena_model/ARENA_TOOLCHAIN.md)。
