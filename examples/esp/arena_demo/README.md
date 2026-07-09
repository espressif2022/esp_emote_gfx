# ESP Arena Demo

包场景正式路径：`load → attach → draw / dirty / touch`，**不建** `gfx_object_t` 树。

板级与 `ai_scene_pkg` 相同：RGB565 HMI（800×480）+ GT1151。

## 模式一览

| 宏 | 作用 |
|---|---|
| （默认） | 手写 ARN demo：顶栏 + OK / Idle，可点 |
| `ARENA_DEMO_SWEEP=1` | 双板目视：16×12 网格逐行扫变色（尽力刷） |
| `ARENA_DEMO_COMPARE=1` | 单板 A/B 计时（UART 打 wall/render/flush） |
| `ARENA_DEMO_FROM_GSP=1` | `home.inc` → ARN，点 Next 变色 |

优先级：`SWEEP` > `COMPARE` > `FROM_GSP` > 默认。

---

## 默认 demo

```bash
cd examples/esp/arena_demo
idf.py -B build set-target esp32s31   # 或你的芯片
idf.py -B build build flash monitor
```

预期：深色底 + 标题「Arena Demo」；右下角蓝 **OK** / 灰 **Idle**；点 OK 变橙，标题 `Arena OK #N`。

---

## 双板目视：逐行扫变色（比效率）

同一 16×12 带文案 button 网格。每帧只改 **一行** 颜色并局部 `refresh_now`，从上往下循环。  
**无人为延时**，两边都尽力刷；并排看谁扫得更快。

| 板 | 路径 | 宏 |
|---|---|---|
| 板 1 | A formal arena（`arena_draw_clipped`） | `-DARENA_DEMO_PATH=A` |
| 板 2 | B object bridge（`draw_child_objects`） | `-DARENA_DEMO_PATH=B` |

```bash
cd examples/esp/arena_demo

# 板 1 — arena（建议独立 build 目录）
idf.py -B build-a -DARENA_DEMO_SWEEP=1 -DARENA_DEMO_PATH=A reconfigure build flash

# 板 2 — object（换板 / 串口后再烧）
idf.py -B build-b -DARENA_DEMO_SWEEP=1 -DARENA_DEMO_PATH=B reconfigure build flash
```

串口应看到：

- `path=A formal arena` 或
- `path=B object bridge`
- 以及 `(max rate)`

怎么看：扫行越快 → 该路径局部刷新越高效（含 render + flush）。

---

## 单板 A/B 计时

同压力场景，先跑 A 再跑 B，UART 打印 load 与 full/dirty 的 **wall / render / flush** 及比值（`object/arena`，>1 表示 arena 更快）。

```bash
idf.py -B build -DARENA_DEMO_COMPARE=1 reconfigure build flash monitor

# 可选：迭代次数（默认 30）
idf.py -B build -DARENA_DEMO_COMPARE=1 -DARENA_COMPARE_ITERS=50 reconfigure build flash monitor
```

- 看画路径差异 → **render**
- 看一帧体感 → **wall**（设备上 flush 常占大头）
- 结束后屏可能停在 B 最后一帧，以日志为准

若平时用 `build-qspi`：

```bash
idf.py -B build-qspi -DARENA_DEMO_COMPARE=1 reconfigure build flash monitor
```

---

## GSP → ARN

```bash
idf.py -B build -DARENA_DEMO_FROM_GSP=1 reconfigure build flash monitor
```

显示 `home.inc` 物化结果；点 **Next**（`on_ok`）变色。  
与 `SWEEP` / `COMPARE` 互斥（后两者优先）。

---

## Host 仿真

```bash
# 仓库根目录
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl --target gfx_arena_sdl_demo gfx_arena_compare_demo
ARENA_SDL=1 ./build-host-sdl/gfx_arena_sdl_demo
./build-host-sdl/gfx_arena_compare_demo
```

更多工具链说明见 `examples/ai_scene_pkg/arena_model/ARENA_TOOLCHAIN.md`。
