# ai_scene_pkg — GSP 场景包 Demo

Host 侧「位置无关二进制场景包」概念验证：把 UI 描述编译成一段**只含 u32 偏移 / u16 索引、无任何原生指针**的字节包；host（64 位）与设备（32 位）用**同一份字节**解析一致，加载走「工厂建树」（`gfx_*_create` + setter），不做 ITU 式指针重定位。

协议细节见 [`gsp_protocol/gsp_binary_protocol_zh.md`](gsp_protocol/gsp_binary_protocol_zh.md)（英文 [`gsp_protocol/gsp_binary_protocol_en.md`](gsp_protocol/gsp_binary_protocol_en.md)）。

---

## 1. 立身之本（改代码别破坏这两条）

1. **包内永远只有 u32 偏移 / u16 索引，禁止原生指针** —— 保证 32/64 位解析一致，无需把 host 降成 32 位、无需冻结 widget struct 布局。
2. **loader 必须全程边界校验，坏包只返回错误码、绝不崩** —— 见库实现 `src/scene/gsp_load.c` 对 header / offset / count / parent / blob / action / crc 的逐项校验。

---

## 2. 依赖

- Linux + CMake ≥ 3.16
- SDL2、FreeType、libjpeg（host SDL 仿真）
- 字体：demo 按 `.inc` 里 `gsp_font_desc_t.path` 加载；也可 `export GSP_FONT_PATH=/path/to/font.ttc`
- 首次 configure 若缺 heatshrink，需先构建一次 ESP 示例拉取 `managed_components`（见根目录 `CMakeLists.txt`）

---

## 3. 构建与运行

在仓库根目录 `esp_emote_gfx/`：

```bash
# 配置（首次或改过 CMakeLists 后）
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON

# 编译 demo（默认加载 inc/home.inc）
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo

# 带窗口运行
./build-host-sdl/gfx_ai_scene_pkg_demo

# 无窗口自检（CI / 无显示器）
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy GSP_HEADLESS=1 \
  ./build-host-sdl/gfx_ai_scene_pkg_demo

# CTest
ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure
```

期望输出含：`SELF-CHECK: PASS (N objects from inc/....inc)`

---

## 4. 切换不同 `.inc`

Demo 在**编译期** `#include` 场景包（字节数组嵌在 `.inc` 里）。切换场景 = 改 CMake 变量 **`GFX_AI_SCENE_INC`** 后**重新编译**（路径相对 `examples/ai_scene_pkg/`）。

| `.inc` | 宏前缀 | 分辨率 | 说明 |
|---|---|---:|---|
| `inc/home.inc` | `HOME_*` | 480×480 | **默认**；含 list / wheel / layer / 动作表 |
| `inc/home_v4.inc` | `HOME_*` | 480×480 | 精简 7 对象 |
| `inc/home_control_v4.inc` | `HOME_CONTROL_*` | 800×480 | 大场景（69 对象） |

```bash
# 示例：切到 home_control
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home_control_v4.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
./build-host-sdl/gfx_ai_scene_pkg_demo
```

`ai_scene_pkg_demo.c` 按 `.inc` 内宏自动映射：

- `HOME_CONTROL_SCREEN_W` → `home_control_*`
- `HOME_SCREEN_W` → `home_*`
- 都没有 → 编译 `#error`

新增 `.inc` 需遵循上述命名，或在 demo 里加 `#elif` 分支。

---

## 5. 导出 / 更新场景包

工具与中间产物在 `gsp_export/`；demo 消费的 `.inc` 仍在 `inc/`。

```text
作者场景 (C)  ──gsp_export/gsp_export_home──►  gsp_export/home.gsp
                                                  │
                                     gsp_export/gsp_package_tools.py
                                                  │
                    ┌─────────────────────────────┼─────────────────────────────┐
                    ▼                             ▼                             ▼
              inc/home.inc            gsp_export/home.manifest.md   gsp_export/home_preview.bmp
```

```bash
# 1) 编译导出器
cmake --build build-host-sdl --target gfx_ai_scene_pkg_export_home

# 2) 改 gsp_export/gsp_export_home.c 后生成原始包
./build-host-sdl/gfx_ai_scene_pkg_export_home examples/ai_scene_pkg/gsp_export
# → examples/ai_scene_pkg/gsp_export/home.gsp

# 3) 生成 .inc + manifest + 预览
python3 examples/ai_scene_pkg/gsp_export/gsp_package_tools.py \
  examples/ai_scene_pkg/gsp_export/home.gsp \
  --out-dir examples/ai_scene_pkg/gsp_export \
  --inc-dir examples/ai_scene_pkg/inc \
  --prefix home

# 4) 用新包跑 demo
cmake -B build-host-sdl -DGFX_AI_SCENE_INC=inc/home.inc
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo
GSP_HEADLESS=1 SDL_VIDEODRIVER=dummy ./build-host-sdl/gfx_ai_scene_pkg_demo
```

`--prefix home_v4` 可生成 `inc/home_v4.inc` 等同名前缀产物。

---

## 6. 当前状态（交接基线）

| 文件 | 职责 |
|---|---|
| `../../include/gfx/scene/gsp.h` | 公共 GSP API：v4 格式常量、错误码、字体/回调绑定、loader scene handle |
| `../../src/scene/gsp_load.c` | host/device loader：校验 → 工厂建树 → blob 解压 → 动作表 trampoline → `gsp_dump` |
| `gsp_pack.c` | host 打包：`gsp_scene_desc_t` → 二进制（字符串去重、blob 压缩、动作表、crc）；暂留示例/host 工具侧 |
| `gsp_export/gsp_export_home.c` | 内置 demo 场景 → `gsp_export/home.gsp` |
| `gsp_export/gsp_package_tools.py` | `.gsp` → `inc/*.inc` / manifest / 预览 BMP |
| `ai_scene_pkg_demo.c` | 加载选定 `.inc` → dump → load → 自检 → SDL 渲染 |
| `inc/*.inc` | 编译进 demo 的字体表 + 二进制包 |
| `gsp_protocol/` | 二进制协议文档（中/英） |

已支持：

- 控件：container / label / button / image / **list / wheel / layer**
- v4 **动作表**：SHOW / HIDE / SET_TEXT / SET_BG_COLOR / CALL / **GOTO**（layer 切换）
- 图片 blob：STORE + RLE16 压缩；params-v1（list/wheel 条目）
- 属性：bg/fg/border/radius、text、callback、name、hidden、align、opacity（预留）
- CRC32 全包校验

验证：`ctest -R ai_scene_pkg` 通过；headless 含 layer GOTO / list-wheel 交互自检（`home.inc`）。

---

## 7. 包格式速览（v4，小端、定宽、按字节偏移）

```text
[Header 56B]
[Object table: obj_count × 64B]
[Blob table: blob_count × 20B]
[Action table: action_count × 24B]
[Params area]
[String table]
[Blob data]
```

- Header 56B：含 `action_count`(off44)、`action_table_off`(off48)、`crc32`(off40)
- ObjEntry 64B：先序、`parent_idx < 自身索引`；所有 `*_off` 为包内绝对字节偏移
- ActionEntry 24B：`src_idx / event / action / target_idx|name / param / arg`
- BlobEntry 20B：codec + raw/comp size，加载期解压

字段全集以公共头 `include/gfx/scene/gsp.h` 与 [`gsp_protocol/gsp_binary_protocol_zh.md`](gsp_protocol/gsp_binary_protocol_zh.md) 为准。

---

## 8. 数据流

```text
打包：gsp_export/gsp_export_home → gsp_pack() → gsp_export/home.gsp
      gsp_export/gsp_package_tools.py → inc/*.inc + gsp_export/manifest

加载：#include .inc → gsp_load_with_fonts()
      校验(crc/bounds/parent/action) → gfx_*_create + setter
      blob_get() 解压 → 动作表 trampoline 挂 touch cb

渲染：gfx_core_tick → gfx_render_draw_object_tree → SDL flush
```

---

## 9. 目录速查

| 路径 | 作用 |
|---|---|
| `ai_scene_pkg_demo.c` | SDL demo |
| `../../include/gfx/scene/gsp.h` / `../../src/scene/gsp_load.c` | 库 API / 加载实现 |
| `gsp_pack.c` | host 打包器（不进 ESP 固件） |
| `gsp_export/` | 导出工具 + `.gsp` / manifest / 预览 |
| `gsp_export/gsp_export_home.c` | C 侧作者场景 → `.gsp` |
| `gsp_export/gsp_package_tools.py` | `.gsp` → `.inc` 等衍生物 |
| `inc/*.inc` | 编译期嵌入的场景包 |
| `gsp_protocol/` | 二进制协议文档 |
| `gsp_scene_package_skill.md` | AI 改包时的 skill 规范 |
| `arena_model/` | arena 同构/直画验证（smoke / draw / bridge / bench） |
| `arena_model/ARENA_UPGRADE_PLAN_C.md` | **档位 C 升级计划**（Todo 含 [√] 进度） |
| `arena_model/ARENA_ABI_v1.md` | Arena 包 ABI v1 冻结说明 |

---

## 10. 常见问题

| 现象 | 处理 |
|---|---|
| 字体加载失败 | 检查 `.inc` 内 font path；或设 `GSP_FONT_PATH` |
| 切 `.inc` 无变化 | 必须改 `GFX_AI_SCENE_INC` 后 **rebuild** |
| `GSP_ERR_VERSION` / CRC | 用当前分支工具链重新导出 `.inc` |
| 窗口尺寸不对 | 由所选 `.inc` 的 `SCREEN_W/H` 决定 |

---

## 11. 继续推进路线（每步可独立验收）

### S1 运行时加载 `.gsp` 文件（不必 `#include`）

- demo 读 `gsp_export/home.gsp` 或 CLI 指定路径，同一 loader 渲染。
- 验收：换包不重新编译 demo。

### S2 坏包鲁棒性测试

- ctest 对包逐字节变异，断言 `GSP_ERR_*` 且不崩。

### S3 JSON → scene 描述

- `scene.json → gsp_scene_desc_t → gsp_pack`；golden 对比 dump。

### S4 工厂注册表替换 `switch(type)`

- 新增控件只加注册项。

### S5 更多控件 + params profile

- progress_bar / arc 等，每控件独立 params 子协议。

### S6 上设备（ESP）

- flash 分区放 `.gsp` / `.inc`，真机 `gsp_load`。

### S7 压缩增强

- RGB565A8 RLE、heatshrink、JPEG 照片。

### S8 live 预览

- watch 源文件 → 重打包 → `gsp_scene_free` + reload。

```text
S1 运行时读包 → S2 坏包测试
  → S3 JSON → S4 注册表
  → S5 控件 → S6 设备
  → S7 压缩 / S8 live
```

---

## 12. 自定义 shape 的三条路（现选路 C）

- **路 C（已实现）**：host 光栅化成 image blob。零引擎改动；像素固定。
- **路 B**：通用 `shape` + display-list params。数据小、可动态；需改引擎。
- **路 A**：自定义 widget class 注册。适合复杂可复用组件。

格式侧对三条路已就绪（`type` + `params` + `blob`）。

---

## 13. 相关文档

- [GSP 二进制协议（中文）](gsp_protocol/gsp_binary_protocol_zh.md)
- [GSP 二进制协议（English）](gsp_protocol/gsp_binary_protocol_en.md)
- [协议目录索引](gsp_protocol/README.md)
- [Scene package skill 说明](gsp_scene_package_skill.md)
- [arena 验证说明](arena_model/README.md)
- [Arena 升级计划 C（渲染/输入吃 arena）](arena_model/ARENA_UPGRADE_PLAN_C.md)
- [Host 可视仿真](arena_model/README.md)（`ARENA_SDL=1 ./build-host-sdl/gfx_arena_sdl_demo`）
- [ESP 设备 arena demo](../esp/arena_demo/README.md)
