# arena_model — 相对偏移 arena 验证

验证「想拿到 ITE 的同构 / 快 / 原地，但不使用绝对指针」这条模型。

**升级计划（档位 C）**：[`ARENA_UPGRADE_PLAN_C.md`](ARENA_UPGRADE_PLAN_C.md)  
**ABI v1**：[`ARENA_ABI_v1.md`](ARENA_ABI_v1.md)

实现已升入系统层：

| 库代码 | 路径 |
|---|---|
| 模型 / pack / load | `include/gfx/scene/arena.h` + `src/scene/arena_model.c` |
| 直画 | `include/gfx/scene/arena_draw.h` + `src/scene/arena_draw.c` |
| 场景 / dirty / touch | `include/gfx/scene/arena_scene.h` + `src/scene/arena_scene.c` |
| GSP→ARN | `include/gfx/scene/gsp_to_arena.h` + `src/scene/gsp_to_arena.c` |

本目录只保留 **demo / 测试入口**。

## Targets（保留）

| Target | 作用 |
|---|---|
| `gfx_arena_model_smoke` | 地址模型（pack/load/原地改） |
| `gfx_arena_draw_demo` | 裸 FB `arena_draw`（无 object） |
| `gfx_arena_dirty_demo` | **正式路径**：dirty + render 分支 + touch/action |
| `gfx_arena_sdl_demo` | **可视仿真**：`ARENA_SDL=1` 开窗点 OK |
| `gfx_gsp_to_arena_demo` | **GSP→ARN**：`home.inc` 全量物化 |
| `gfx_arena_compare_demo` | **A/B**：`arena_draw_clipped` vs object bridge（wall/render/flush） |

辅助（非独立 target）：`arena_demo_scene.*`（SDL/ESP 场景）、`arena_compare_run.*`、`arena_gfx_bridge.*`（仅 compare）。

已删除的临时入口：`arena_render_demo`（hybrid）、`arena_bench`（已并入 compare）。

## 构建 / 运行

```bash
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON

cmake --build build-host-sdl --target \
  gfx_arena_model_smoke gfx_arena_draw_demo gfx_arena_dirty_demo \
  gfx_arena_sdl_demo gfx_gsp_to_arena_demo gfx_arena_compare_demo

./build-host-sdl/gfx_arena_model_smoke
./build-host-sdl/gfx_arena_draw_demo
./build-host-sdl/gfx_arena_dirty_demo
./build-host-sdl/gfx_gsp_to_arena_demo
./build-host-sdl/gfx_arena_compare_demo
ARENA_COMPARE_ITERS=400 ./build-host-sdl/gfx_arena_compare_demo

# 可视仿真
ARENA_SDL=1 ./build-host-sdl/gfx_arena_sdl_demo
ARENA_SDL=1 ./build-host-sdl/gfx_gsp_to_arena_demo

# ESP：见 examples/esp/arena_demo/README.md
# 设备 A/B：idf.py -B build -DARENA_DEMO_COMPARE=1 reconfigure build flash monitor
```

## 正式路径（推荐）

```text
arena_pack  或  gsp_to_arena(GSP bytes)
  → arena_load (RAM)
  → arena_scene_attach(disp)
  → arena_scene_set_font(scene, font)
  → 改节点 → arena_scene_mark_dirty
  → gfx_core_refresh_now   # arena_draw_clipped
  → gfx_touch_inject
```

控件能力见 [`ARENA_PARITY.md`](ARENA_PARITY.md)。  
工具链见 [`ARENA_TOOLCHAIN.md`](ARENA_TOOLCHAIN.md)。  
上位机 ARN：`python3 ../gsp_export/gsp_to_arn.py ../gsp_export/home.gsp --out ../gsp_export/home.arn`
