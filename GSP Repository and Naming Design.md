# GSP 新仓库结构与命名设计

> 状态：设计提案
> 日期：2026-07-10
> 前置文档：`GSP High-Performance Rendering Framework Plan.md`
> 决策范围：新仓库名称、目录结构、命名体系、二进制格式命名、与旧仓库关系

## 1. 结论

- 新开一个 monorepo，上位机工具链与设备端组件同仓库，品牌统一为 **GSP**，
  全称升级为 **Graphics Scene Pipeline**（缩写不变，Package → Pipeline
  更贴合"上位机编译 + 设备重放"的定位）。
- 仓库名 `esp-gsp`，设备端组件名 `esp_gsp`，C API 前缀 `gsp_` 保持不变。
- 上位机编译器由 `uic` 更名为 **`gspc`**（GSP Compiler）。
- 二进制格式统一为 GSP 家族：GSB / GRB / GFB / GSPB，替换 ARN2 / GRES2
  占位名。
- `graphics_scene_package` 迁移完成后归档；`esp_emote_gfx` 冻结维护
  （见主计划 §15）。

## 2. 命名体系

### 2.1 方案 A（推荐）：GSP 品牌统一

| 对象 | 命名 | 说明 |
|---|---|---|
| 仓库 | `esp-gsp` | GitHub 仓库用连字符，Espressif 惯例 |
| 设备端组件 | `esp_gsp` | Component Registry 发布单元 |
| C API 前缀 | `gsp_` / `GSP_` | 与主计划全文一致，零改动 |
| Kconfig 前缀 | `CONFIG_GSP_` | |
| 上位机编译器 CLI | `gspc` | 子命令：`build` / `pack` / `font` / `preview` / `inspect` |
| Python 包 | `gsp-tools`（import 名 `gspc`） | |
| Web 编辑器 | GSP Editor | |
| 作者格式 | GSP1 JSON | 保持现状 |
| 场景二进制 | **GSB**（GSP Scene Binary），magic `GSB1`，`.gsb` | 替换 ARN2 |
| 资源二进制 | **GRB**（GSP Resource Binary），magic `GRB1`，`.grb` | 替换 GRES2 |
| 字体二进制 | **GFB**（GSP Font Binary），magic `GFB1`，`.gfb` | 新增独立命名 |
| 多场景总包 | **GSPB**，`.gspb` | 保持现状 |

说明：

- GFB 独立命名的原因：字体包按 locale/字号独立分发和 OTA（主计划 §7.5），
  生命周期与图片资源不同，不混入 GRB。
- `.gsb`/`.grb` 在无关领域（大地测量、气象）存在同名扩展，嵌入式场景无
  实际冲突风险。
- ARN1 作为 `esp_emote_gfx` 遗留格式仅保留只读兼容 loader，命名不变。

### 2.2 方案 B（备选）：全新品牌

若希望更高辨识度的对外品牌，备选 `esp-litho`（lithography 制版印刷隐喻：
上位机制版一次，设备高速重放）或 `esp-tessera`（马赛克瓷砖，贴合 tile
架构）。代价：主计划全文、API 前缀、格式 magic、工具链名称全部改名，
且 GSP1 作者格式名与新品牌脱节。除非有对外品牌运营需求，不推荐。

本文档其余部分按方案 A 展开；若选方案 B，仅需按上表做前缀替换。

## 3. 仓库划分决策

采用 monorepo（host + device 同仓库）：

- ABI 是 host 编译器与 device runtime 的强耦合契约，同仓库保证一次
  commit 内原子修改格式定义、emitter 和 loader，不存在跨仓库版本漂移窗口；
- golden image 测试链（gspc 输出 → host preview → device 渲染）需要
  三方在同一 CI 中对齐；
- Component Registry 支持从子目录发布组件，仓库中的 tools/web_editor
  不会进入组件分发包。

## 4. 目录结构

```text
esp-gsp/
├── components/
│   └── esp_gsp/                      # 设备端组件（Registry 发布单元）
│       ├── include/gsp/
│       │   ├── gsp.h                 # umbrella header
│       │   ├── gsp_runtime.h         # scene load / state / bind / dirty
│       │   ├── gsp_update.h          # typed update transaction
│       │   ├── gsp_instance.h        # template instance pool
│       │   ├── gsp_widget.h          # 内置标准控件模板包 API
│       │   ├── gsp_repeater.h        # virtual list
│       │   ├── gsp_overlay.h         # Level 3 runtime draw-list
│       │   ├── gsp_renderer.h        # frame begin/execute/end
│       │   ├── gsp_accel.h           # SW/PPA/DMA2D backend 接口
│       │   ├── gsp_codec.h           # decoder plugin 接口
│       │   ├── gsp_present.h         # esp_display_present adapter
│       │   ├── gsp_input.h           # touch / hit-test / action
│       │   ├── gsp_test.h            # public test hooks（Phase 0 起提供）
│       │   └── format/               # ABI 单一事实源（生成物，见 §5）
│       │       ├── gsp_gsb_format.h
│       │       ├── gsp_grb_format.h
│       │       ├── gsp_gfb_format.h
│       │       └── gsp_gspb_format.h
│       ├── src/
│       │   ├── runtime/              # state / instance / overlay / repeater
│       │   ├── renderer/             # command replay + per-format kernels
│       │   │   ├── kernel_rgb565/
│       │   │   ├── kernel_rgb888/
│       │   │   ├── kernel_argb8888/
│       │   │   └── reference/        # reference rasterizer（正确性 oracle）
│       │   ├── accel/                # gsp_accel_sw / _ppa / _dma2d
│       │   ├── codec/                # eaf / jpeg（decoder plugin 实现）
│       │   ├── widget/               # 内置模板包生成物 + widget API
│       │   ├── font/                 # GFB loader / glyph run / atlas cache
│       │   ├── present/              # gsp_present_esp_lcd.c
│       │   └── port/                 # esp_idf / host(SDL/memory) 平台层
│       ├── test/                     # host 可跑单测（memory backend + golden）
│       ├── Kconfig
│       ├── CMakeLists.txt
│       └── idf_component.yml
├── tools/
│   ├── gspc/                         # 上位机编译器（原 uic 迁移改名）
│   │   ├── frontend/                 # GSP1 parse / schema / validate
│   │   ├── ir/                       # layout resolve / static-dynamic split
│   │   ├── opt/                      # merge / cull / dedup / tile index
│   │   ├── backend/                  # gsb / grb / gfb / gspb emitters
│   │   ├── fontc/                    # font.py 升级：atlas + glyph run
│   │   └── widgets/                  # 标准控件模板源（GSP1 JSON，见 §6）
│   ├── preview/                      # host 渲染预览（像素一致性 oracle）
│   └── profiles/                     # target profile 库（esp32p4_rgb565_ppa.yml 等）
├── formats/                          # 格式规范单一事实源（YAML/spec + codegen）
├── web_editor/                       # 从 graphics_scene_package 迁移
├── docs/
│   ├── plan.md                       # 主计划书迁入
│   └── format/                       # GSB / GRB / GFB spec 文档
├── examples/
│   ├── hello_scene/
│   ├── dashboard/                    # bind 密集
│   ├── drag_pages/                   # 全屏拖拽 fast path
│   ├── dynamic_widgets/              # widget API + template instance
│   └── eaf_player/                   # EAF/AAF 动画
├── test_apps/                        # 设备目标测试
│   ├── benchmark/                    # vs LVGL 对照（主计划 §16）
│   └── golden/                       # framebuffer CRC / golden image
└── .gitlab-ci.yml / .github/
```

要点：

- `components/esp_gsp` 是唯一发布单元；`tools/`、`web_editor/` 不进入
  组件包。
- `src/renderer/kernel_*` 按格式独立目录，落实主计划 §2.5"RGB565 用
  独立 16-bit kernel，不经过 888 通用实现"。
- `src/port/` 隔离 esp_idf 与 host（SDL/memory）平台差异，保证组件单测
  可在 host 上跑。
- 首期不建 `components/esp_gsp_extra` 之类的拆分；单组件直到体积或依赖
  证明需要拆分。

## 5. ABI 单一事实源与防漂移

host emitter（Python）与 device loader（C）消费同一份格式定义：

- `formats/` 存放格式规范（字段、偏移、枚举、magic、版本）；
- codegen 同时生成 `include/gsp/format/*.h`（C）和 `gspc` 的
  `format_*.py`（Python），两者均为 checked-in 生成物；
- CI 执行 codegen 并 diff，生成物与规范不一致即失败；
- GSB/GRB/GFB header 均含 format version + CRC，loader 拒绝不匹配版本
  （主计划 §17 Phase 6 rollback 依赖此机制）。

在 Phase 1 格式冻结前，`formats/` 允许自由修改；冻结后任何字段变更必须
递增 version 并保留旧版本 loader 测试向量。

## 6. 内置标准控件模板包的构建方式

- 模板源为 GSP1 JSON，存放于 `tools/gspc/widgets/`（label / image / rect /
  button / progress 等）；
- 构建期由 `gspc` 编译为 GSB template section，生成 C 数组进入
  `src/widget/`（checked-in 生成物，CI 校验再生成一致）；
- `gsp_widget_create()` 即实例化内置模板，与业务模板同一 instance 路径
  （主计划 §11.1）。

## 7. 与旧仓库关系

| 仓库 | 处置 |
|---|---|
| `graphics_scene_package` | schema、gspc（原 uic）、web editor、docs 迁入 `esp-gsp` 后归档，只读保留历史 |
| `esp_emote_gfx` | 冻结维护，服务存量 emote 产品；primitive 与 `gfx_eaf_dec` 按主计划 §15 清单源码移植进 `esp-gsp` |
| `esp_display_present` | 保持独立仓库，`esp_gsp` 以组件依赖引用 |

迁移顺序建议：

1. 建 `esp-gsp` 骨架（目录 + CI + 空组件可编译）；
2. 迁入 docs 与 `formats/` codegen 框架；
3. 迁移 gspc frontend（GSP1 parse/validate 现有代码改名迁入）；
4. 按主计划 §17 Phase 0 起步（benchmark 与可观测性），随后 Phase 1
   在新仓库内实现 GSB emitter 与 host reference renderer；
5. web editor 最后迁移，不阻塞 Phase 0–2。

## 8. CI 门槛（建仓库即启用）

- host：gspc 单测、format codegen diff、reference renderer golden vectors；
- 组件：host 构建（SDL/memory backend）单测 + golden image；
- 目标板：`test_apps/golden` framebuffer CRC、`test_apps/benchmark`
  性能门槛（主计划 §16.4，Phase 2 起强制）；
- 无 LVGL 符号依赖检查（主计划 §2.5 字体一节的链接测试）。
