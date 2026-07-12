# arena_model — Host 验证入口

本目录**只放 host demo / 验证入口**。场景资产格式见
[`docs/asset_spec.rst`](../../../docs/asset_spec.rst)，整体架构见
[`docs/architecture.md`](../../../docs/architecture.md)。

库代码：`include/gfx/scene/` + `src/scene/`。  
ESP 辅助源码：`examples/esp/arena_demo/main/`（host demo 会复用）。

## Targets

| Target | 作用 |
|---|---|
| `gfx_arena_model_smoke` | pack/load/原地改 |
| `gfx_arena_draw_demo` | 裸 FB `gfx_arena_draw` |
| `gfx_arena_dirty_demo` | dirty + render + touch/action |
| `gfx_arena_sdl_demo` | 可视：`ARENA_SDL=1` |
| `gfx_arena_playground_sdl_demo` | playground 壳：`ARENA_SDL=1` |
| `gfx_gsp_to_arena_demo` | GSP→ARN 闭环 |
| `gfx_arena_compare_demo` | A/B wall/render/flush |

## 构建 / 运行

```bash
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON

cmake --build build-host-sdl --target \
  gfx_arena_model_smoke gfx_arena_draw_demo gfx_arena_dirty_demo \
  gfx_arena_sdl_demo gfx_arena_playground_sdl_demo \
  gfx_gsp_to_arena_demo gfx_arena_compare_demo

./build-host-sdl/gfx_arena_model_smoke
./build-host-sdl/gfx_arena_dirty_demo
./build-host-sdl/gfx_arena_playground_sdl_demo
ARENA_SDL=1 ./build-host-sdl/gfx_arena_playground_sdl_demo

# FPS：只比较 List / Wheel / Progress / Button / Image
```

ESP：[`examples/esp/arena_demo/README.md`](../../esp/arena_demo/README.md)
