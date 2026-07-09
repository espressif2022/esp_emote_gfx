# Arena 工具链与过渡策略

## 当前状态（2026-07）

| 路径 | 状态 |
|---|---|
| ARN1 pack/load（`arena_pack` / `arena_load`） | [√] 系统层 |
| Host 可视 / ESP demo | [√] `gfx_arena_sdl_demo` / `examples/esp/arena_demo` |
| GSP factory load（`gsp_load`） | [√] **长期并存**（复杂控件 / 旧包） |
| GSP → ARN 物化（运行时） | [√] `gsp_to_arena()`；见下 |
| 上位机直接 export ARN | [√] `gsp_to_arn.py` / `gsp_package_tools.py --arn` |
| skill / manifest 写 ARN | [ ] 未接 |

## GSP → ARN 物化（`gsp_to_arena`）

| 项 | 说明 |
|---|---|
| API | `include/gfx/scene/gsp_to_arena.h` · `src/scene/gsp_to_arena.c` |
| 支持 | container/layer/label/button；image（RGB565 STORE/RLE16）；list/wheel items |
| Host 闭环 | `./build-host-sdl/gfx_gsp_to_arena_demo`；`ARENA_SDL=1` 开窗 |
| ESP | `idf.py -B build -DARENA_DEMO_FROM_GSP=1 build`（`examples/esp/arena_demo`） |

`home.inc` 实测：12 GSP 对象 → 12 arena 节点（含 image/list/wheel），`on_ok` 可点。

## 上位机 export ARN

```bash
# 从已有 .gsp
python3 examples/ai_scene_pkg/gsp_export/gsp_to_arn.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out examples/ai_scene_pkg/gsp_export/home.arn

# 或随 GSP 侧产物一起生成
python3 examples/ai_scene_pkg/gsp_export/gsp_package_tools.py \
  examples/ai_scene_pkg/gsp_export/home.gsp --arn
```

设备侧：`arena_load(home.arn)` → `arena_scene_attach`（无需再跑 `gsp_to_arena`）。

## 过渡期结束条件（`p6-cleanup`）

宣布「包场景默认 ARN」当且仅当：

1. 目标产品场景控件均在 [`ARENA_PARITY.md`](ARENA_PARITY.md) 为 [√]
2. export 能产出 ARN1（或产品场景可经 `gsp_to_arena` 覆盖）
3. ESP 上 load/draw 计时达标

在此之前：**GSP + ARN 双格式长期并存**；手写 UI 继续 `gfx_*_create`。

## 计时接口

Host A/B（wall / render / flush）：

```bash
./build-host-sdl/gfx_arena_compare_demo
ARENA_COMPARE_ITERS=400 ./build-host-sdl/gfx_arena_compare_demo
./build-host-sdl/gfx_gsp_to_arena_demo
```

ESP 可视化：`examples/esp/arena_demo`（pack/bind/refresh µs + OK 回调）。

ESP A/B：

```bash
cd examples/esp/arena_demo
idf.py -B build -DARENA_DEMO_COMPARE=1 reconfigure build flash monitor
# 可选：-DARENA_COMPARE_ITERS=50
```

看 **full/dirty render** 比值判断画路径；wall≈1.0 且 flush≈wall 时说明瓶颈在 LCD。

## ABI

见 [`ARENA_ABI_v1.md`](ARENA_ABI_v1.md)。IMAGE blob 为 v1 兼容扩展（`reserved`=img 偏移），未升 version。
