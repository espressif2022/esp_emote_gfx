# ESP Arena Demo

包场景正式路径：`load → attach → draw / dirty / touch`，**不建** `gfx_object_t` 树。

板级：RGB565 HMI（800×480）+ GT1151。

## 模式一览

| 宏 | 作用 |
|---|---|
| （默认） | 手写 ARN demo：顶栏 + OK / Idle，可点 |
| `ARENA_DEMO_PLAYGROUND=1` | **Arena P0 游乐场壳**：左导航 + 静态预览 |
| `ARENA_DEMO_SWEEP=1` | 双板目视：16×12 网格逐行扫变色（尽力刷） |
| `ARENA_DEMO_COMPARE=1` | 单板 A/B 计时（UART 打 wall/render/flush） |
| `ARENA_DEMO_FROM_GSP=1` | GSP `.inc` → ARN（可选复杂家控场景） |

优先级：`SWEEP` > `COMPARE` > `PLAYGROUND` > `FROM_GSP` > 默认。

> **和 `format_rgb565` playground 的关系**  
> - **Arena P0**（本 demo `PLAYGROUND`）：导航 + Button/Label/Image/List/Wheel；Motion/Anim/Coverflow 等为 stub。  
> - **Object 完整版**：`examples/esp/format_rgb565`（含 Motion/Anim/Coverflow/Pageflow…）。  
> - **复杂包场景**：`ARENA_GSP_SCENE=control`（`home_control_v4`）。

---

## 默认 demo

```bash
cd examples/esp/arena_demo
idf.py -B build set-target esp32s31   # 或你的芯片
idf.py -B build build flash monitor
```

预期：深色底 + 标题「Arena Demo」；右下角蓝 **OK** / 灰 **Idle**；点 OK 变橙，标题 `Arena OK #N`。

---

## Arena P0 playground（format_playground 壳）

左导航 list 切换右侧预览。  
已接：Button / Label / Image / List（拖拽+惯性+snap）/ Wheel / Image Button / Progress。  
未接（stub）：Motion / Anim / Coverflow / Pageflow（用 `format_rgb565`）。

```bash
cd examples/esp/arena_demo
idf.py -B build \
  -DARENA_DEMO_COMPARE=0 -DARENA_DEMO_SWEEP=0 -DARENA_DEMO_FROM_GSP=0 \
  -DARENA_DEMO_PLAYGROUND=1 \
  reconfigure build flash monitor
```

操作：点左侧项切换面板；Button 可点变色；List/Wheel 可点选行。

### PC 仿真（SDL）

```bash
# 在仓库根目录（已有 build-host-sdl 时可跳过 configure）
cmake -S . -B build-host-sdl   # 默认 GFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl --target gfx_arena_playground_sdl_demo

# 无头自检
./build-host-sdl/gfx_arena_playground_sdl_demo

# 开窗交互
ARENA_SDL=1 ./build-host-sdl/gfx_arena_playground_sdl_demo
```

---

## GSP → ARN（含复杂家控）

```bash
cd examples/esp/arena_demo

# 简单 home（12 对象）— 务必关掉 COMPARE/SWEEP 缓存
idf.py -B build \
  -DARENA_DEMO_COMPARE=0 -DARENA_DEMO_SWEEP=0 \
  -DARENA_DEMO_FROM_GSP=1 -DARENA_GSP_SCENE=home \
  reconfigure build flash monitor

# 复杂家控 home_control_v4（69 对象，800×480）
idf.py -B build \
  -DARENA_DEMO_COMPARE=0 -DARENA_DEMO_SWEEP=0 \
  -DARENA_DEMO_FROM_GSP=1 -DARENA_GSP_SCENE=control \
  reconfigure build flash monitor
```

| `ARENA_GSP_SCENE` | 文件 | 规模 |
|---|---|---|
| `home`（默认） | `inc/home.inc` | 12 对象；点 Next（`on_ok`） |
| `control` | `inc/home_control_v4.inc` | 69 对象；空调/加湿/净化等按钮可点变色 |

串口应看到 `GSP→ARN (inc/…)`，而不是 `A/B compare`。

---

## 双板目视：逐行扫变色（比效率）

同一 16×12 带文案 button 网格。每帧只改 **一行** 颜色并局部 `refresh_now`，从上往下循环。  
**无人为延时**，两边都尽力刷；并排看谁扫得更快。

```bash
cd examples/esp/arena_demo

# 板 1 — arena
idf.py -B build-a -DARENA_DEMO_SWEEP=1 -DARENA_DEMO_PATH=A reconfigure build flash

# 板 2 — object
idf.py -B build-b -DARENA_DEMO_SWEEP=1 -DARENA_DEMO_PATH=B reconfigure build flash
```

---

## 单板 A/B 计时

```bash
idf.py -B build -DARENA_DEMO_COMPARE=1 reconfigure build flash monitor
# 可选：-DARENA_COMPARE_ITERS=50
```

看 **render** 比画路径；**wall** 看体感（设备上 flush 常占大头）。

---

## Object 路径：RGB565 format_playground

要完整控件游乐场（非 ARN），跑现有工程：

```bash
cd examples/esp/format_rgb565
idf.py -B build set-target esp32s31
idf.py -B build build flash monitor
```

左侧列表切换 Motion / Anim / Coverflow / Image / Button / List / Wheel 等。

---

## Host 仿真

```bash
# 仓库根目录
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON
cmake --build build-host-sdl --target gfx_arena_sdl_demo gfx_arena_compare_demo gfx_gsp_to_arena_demo
ARENA_SDL=1 ./build-host-sdl/gfx_arena_sdl_demo
./build-host-sdl/gfx_arena_compare_demo
./build-host-sdl/gfx_gsp_to_arena_demo
```

更多见 [`docs/scene/`](../../../docs/scene/README.md)（toolchain / parity / plan）。
