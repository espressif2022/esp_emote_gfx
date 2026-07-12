# gsp_export

Tools and generated artifacts for the `home` GSP demo package.

## Files

| File | Kind | Purpose |
|---|---|---|
| `gsp_export_home.c` | source | Built as `gfx_ai_scene_pkg_export_home`; emits `home.gsp`. |
| `gsp_package_tools.py` | source/tool | Converts `.gsp` to `.inc`, manifest, preview BMP, and optional ARN. |
| `gsp_to_arn.py` | source/tool | Converts an existing `.gsp` to `.arn`. |
| `home.gsp` | generated | Raw GSP package. |
| `home.manifest.md` | generated | Human-readable package map. |
| `home_preview.bmp` | generated | Preview extracted from blobs. |
| `home.arn` | generated | ARN1 package for `arena_load`. |
| `home.arn.md` | generated | Summary for `home.arn`. |

`__pycache__/` is local cache output and should not live in the repo.

## Regenerate

From the repository root:

```bash
cmake --build build-host-sdl --target gfx_ai_scene_pkg_export_home
./build-host-sdl/gfx_ai_scene_pkg_export_home examples/ai_scene_pkg/gsp_export

python3 examples/ai_scene_pkg/gsp_export/gsp_package_tools.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out-dir examples/ai_scene_pkg/gsp_export \
  --inc-dir examples/ai_scene_pkg/inc \
  --prefix home \
  --arn
```

`--inc-dir` writes compile-time demo packages into `../inc/`.

## ARN Only

```bash
python3 examples/ai_scene_pkg/gsp_export/gsp_to_arn.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out examples/ai_scene_pkg/gsp_export/home.arn
```

## Validate

```bash
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo gfx_gsp_to_arena_demo

GSP_HEADLESS=1 SDL_VIDEODRIVER=dummy ./build-host-sdl/gfx_ai_scene_pkg_demo
./build-host-sdl/gfx_gsp_to_arena_demo
```

Top-level workflow: [`examples/README.md`](../../README.md). Package regeneration rules:
[`SKILL.md`](SKILL.md). Asset format: [`docs/asset_spec.rst`](../../../docs/asset_spec.rst).
