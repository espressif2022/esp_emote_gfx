# esp_emote_gfx test_apps

`test_apps` 已从单一 Unity 固件拆成 **三层工程**，按测试性质分离，避免逻辑单测、视觉演示和硬件集成混在同一 bin 里。

## 测试金字塔

```
L1  unit        — 无 BSP，mock/memory backend，可自动断言，适合 CI
L2  integration — gfx_core + widget 逻辑（仍在 unit 工程内）
L3  hw          — 真板 esp_lcd backend、多 display、emote_gen 分区
L4  visual      — 人工肉眼观察，wait_for_observe，不进 CI gate
```

| 工程 | Project 名 | 入口 | BSP | 典型用途 |
|------|------------|------|-----|----------|
| `unit/` | `gfx_test_unit` | `unity_run_all_tests()` | 否 | backend / fs / anim 像素断言 / timer |
| `visual/` | `gfx_test_visual` | `unity_run_menu()` | 是 | label / anim matrix 等演示 |
| `./` (hw) | `gfx_test_hw` | `unity_run_menu()` | 是 | multi_disp / emote_gen 集成 |

## 目录结构

```
test_apps/
├── README.md                 ← 本文件
├── shared/                   # hw + visual 共用
│   ├── gfx_test_util.c/h     # wait / log / mem monitor
│   ├── gfx_hw_runtime.c/h    # runtime / display / touch / lock
│   └── test_board.c/h        # BSP display + touch init
│
├── unit/                     # L1/L2 单元与集成断言
│   ├── CMakeLists.txt
│   ├── partitions.csv        # factory + assets_test（无 emote_gen）
│   ├── sdkconfig.defaults
│   └── main/
│       ├── harness/
│       │   ├── gfx_test_core_fixture.c/h    # gfx_core init，无 BSP
│       │   └── gfx_test_assets_fixture.c/h  # mmap assets，无 display
│       ├── test_backend.c      # [unit][backend]
│       ├── test_fs.c           # [unit][fs]
│       ├── test_anim_decode.c  # [unit][integration][anim][format]
│       ├── test_timer_logic.c  # [unit][timer]
│       └── app_main.c          # unity_run_all_tests()
│
├── visual/                   # L4 视觉演示
│   ├── CMakeLists.txt
│   ├── partitions.csv        # factory + assets_test
│   ├── sdkconfig.bsp.*
│   └── main/
│       ├── test_label.c
│       ├── test_image.c
│       ├── test_qrcode.c
│       ├── test_button.c
│       ├── test_motion.c
│       ├── test_coverflow.c
│       ├── test_multi_obj.c
│       ├── test_anim.c         # decoder matrix demo
│       └── app_main.c          # unity_run_menu()
│
├── (hw 根目录)               # L3 硬件集成
│   ├── CMakeLists.txt        # project(gfx_test_hw)
│   ├── partitions.csv        # 含 emote_gen 分区
│   ├── sdkconfig.bsp.*
│   ├── prebuilt/             # emote_assets.bin 缓存（configure 时下载）
│   ├── assets_gen/           # emote_gen 分区占位目录
│   └── main/
│       ├── common.h          # 薄封装 → shared + mmap 头
│       ├── test_multi_disp.c # [hw][display][multi]
│       ├── test_anim_emote_gen.c
│       └── app_main.c        # unity_run_menu()
```

## 本地构建

```bash
# 单元测试（无 BSP，推荐 PR 必过）
cd test_apps/unit
idf.py build
idf.py -p PORT flash monitor    # 自动跑完全部 [unit] 用例

# 视觉演示（需 ESP-BOX / P4 等 BSP 板）
cd test_apps/visual
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.bsp.esp-box build
idf.py -p PORT flash monitor    # menu 选手动项

# 硬件集成（含 emote_gen 分区）
cd test_apps
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.bsp.esp-box build
idf.py -p PORT flash monitor
```

首次构建 `hw` 若报 `emote_assets.bin` 缺失，CMake 会从
`https://dl.espressif.com/AE/emote_assets.bin` 下载到 `test_apps/prebuilt/`。

P4 板使用 `sdkconfig.bsp.esp32_p4_function_ev_board`。

## Unity 标签规范

格式：`TEST_CASE("module: behavior", "[layer][module][...]")`

| 层标签 | 工程 | 说明 |
|--------|------|------|
| `[unit]` | unit | 纯逻辑，无 display |
| `[integration]` | unit | memory backend + gfx_core 断言 |
| `[hw]` | hw | 真板 esp_lcd / 多 display |
| `[visual]` | visual | 肉眼观察，含 `wait_for_observe` |

示例：

```c
TEST_CASE("backend: RGB565 flush uses screen stride", "[unit][backend][format]")
TEST_CASE("anim: jpeg palette rgb565 contract", "[unit][integration][anim][format]")
TEST_CASE("display: multi route map", "[hw][display][multi]")
TEST_CASE("label: bitmap font scene", "[visual][widget][label]")
```

## 新增用例放哪里

| 你要测什么 | 放哪里 | Fixture |
|------------|--------|---------|
| backend / refresh / format 像素 | `unit/main/test_backend.c` 或新文件 | `gfx_test_core_fixture` + memory backend |
| gfx_fs API | `unit/main/test_fs.c` | 无 display |
| widget 解码输出 / 像素 spot-check | `unit/main/` 新文件 | `gfx_test_core_fixture` + `gfx_test_assets_fixture` |
| timer / core API 精度 | `unit/main/test_timer_logic.c` | `gfx_test_core_fixture`（无 disp） |
| 真屏 + touch + 多 display | `main/` (hw) | `test_app_runtime_open()` |
| emote_gen 分区集成 | `main/test_anim_emote_gen.c` | hw runtime |
| 肉眼看的 widget 场景 | `visual/main/` | `test_app_runtime_open()` |

**原则：** 能用 `memory backend` + `TEST_ASSERT` 解决的，不要进 hw/visual。

## shared 模块说明

### `gfx_test_util`

与硬件无关的测试辅助：

- `test_app_wait_ms` / `test_app_wait_for_observe`
- `test_app_log_case` / `test_app_log_step`
- `test_app_mem_log_snapshot` / `test_app_mem_monitor_*`

### `gfx_hw_runtime`

真板 runtime（hw + visual 共用）：

- 全局：`emote_handle`, `disp_default`, `touch_default`
- `test_app_runtime_open/close` — mmap + board + gfx + esp_lcd backend + touch
- `test_app_lock/unlock`
- `load_image`, `display_and_graphics_init/clean`

### `test_board`

BSP 板级初始化，与 gfx 无关：

- `test_board_init/deinit`
- `panel_handle`, `io_handle`
- `test_board_lcd_interface()` — S3: `PANEL_IO`，P4: `MIPI_DPI`

## unit harness 说明

### `gfx_test_core_fixture`

```c
gfx_test_core_ctx_t core;
gfx_test_core_open(&core);
gfx_test_core_lock(&core);
// ... gfx_display_add with memory backend ...
gfx_test_core_unlock(&core);
gfx_test_core_close(&core);
```

### `gfx_test_assets_fixture`

```c
gfx_test_assets_ctx_t assets;
gfx_test_assets_open(&assets, "assets_test", MMAP_ASSETS_TEST_FILES, MMAP_ASSETS_TEST_CHECKSUM);
// mmap_assets_get_mem(assets.handle, id)
gfx_test_assets_close(&assets);
```

## 分区表差异

| 分区 | unit | visual | hw |
|------|------|--------|-----|
| `factory` | ✓ | ✓ | ✓ |
| `assets_test` | ✓ | ✓ | ✓ |
| `emote_gen` | ✗ | ✗ | ✓ |

## CI 自动化测试

完整部署说明见 **[CI.md](CI.md)**。

### PR gate（GitHub Actions，无需真板）

| 步骤 | 命令/Job | 作用 |
|------|----------|------|
| Host 测试 | `ctest` in `build-host-sdl` | 8 项 SDL smoke |
| 编译 | `idf-build-apps` × unit / visual / hw | 链接与配置 gate |

### 真板执行 unit（HIL，可选）

```bash
export ESP_PORT=/dev/ttyACM0
./test_apps/ci/run_unit_on_target.sh
```

自建 runner 启用 `.github/workflows/unit-hil.yml`（默认 `workflow_dispatch` + nightly）。

### 待办

- 将更多 backend 逻辑迁到 host ctest
- HIL 支持 Unity tag 过滤（`-t [unit][backend]`）

## 迁移记录（2026-06）

原 `test_anim_player` 单固件已拆分：

| 原文件 | 现位置 |
|--------|--------|
| `test_backend.c` | `unit/main/` |
| `test_fs_store.c` | `unit/main/test_fs.c` |
| `test_anim.c` format 断言 | `unit/main/test_anim_decode.c` |
| `test_timer.c` | `unit/main/test_timer_logic.c` |
| `test_label/image/...` | `visual/main/` |
| `test_multi_disp.c` | `main/` (hw) |
| `common.c` | `shared/gfx_hw_runtime.c` + `shared/gfx_test_util.c` |
| `test_board.c` | `shared/` |
