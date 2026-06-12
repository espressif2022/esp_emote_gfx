# 本地 TODO

## TODO 记录规则

- 完成项使用 `[x]`，并在任务后标注 `(done: YYYY-MM-DD)`。
- 后续按本文顺序推进；跨模块大任务先落设计/骨架，再接入实现和测试。
- 已完成但早于本规则的历史任务可暂不补时间，后续继续处理时再补齐。

## 架构和命名整理

- [x] 新增 `docs/architecture.md`，固化目标分层、命名规则和迁移顺序。
- [x] 新增 `docs/itu_reference_design.md`，记录 ITU 显示框架借鉴点和 GFX 落地方案。(done: 2026-06-12)
- [x] 新增 `include/gfx/` public API 门面，新代码优先使用 `gfx/gfx.h`、`gfx/display.h`、`gfx/object.h`、`gfx/widgets/*`。
- [x] 新 public API 统一放到 `include/gfx/`，旧路径入口不再扩展。
- [x] 平台层已迁移到 `src/platform/`。
- [x] widget 实现已迁移到 `src/widgets/`。
- [x] 将 SDL backend 从 demo/sim 目录移到 `src/backend/sdl/`，并从 ESP-IDF component build 中显式排除。
- [x] SDL backend public API 统一为 `gfx_backend_sdl_*`，旧 SDL backend 符号已删除。
- [x] 将 memory backend 从 `src/core/display/` 拆到 `src/backend/memory/`。(done: 2026-06-12)
- [x] 将 render/draw 从 `src/core/display` / `src/core/draw` 收敛到 `src/render/`。(done: 2026-06-12)
- [x] 将 image/font/anim decoder 逐步归入 `src/codecs/` / `src/fonts/`，widget 只保留 UI 状态和 draw/update 逻辑。(done: 2026-06-12)
  - [x] image decoder 从 `src/widgets/img/` 迁到 `src/codecs/image/`。(done: 2026-06-12)
  - [x] anim decoder 从 `src/widgets/anim/` 迁到 `src/codecs/anim/`。(done: 2026-06-12)
  - [x] font adapter/loader 从 `src/widgets/font/` 拆分到 `src/fonts/`。(done: 2026-06-12)
- [x] 将 public API 文档从旧 `gfx_disp/gfx_obj/gfx_img` 名称迁移到新 `gfx_display/gfx_object/gfx_image` 名称。(done: 2026-06-12)
  - [x] 文档生成入口改为扫描 `include/gfx/` 新 public API 门面，不再从 `include/core/` 生成推荐页。(done: 2026-06-12)
  - [x] 重新生成并清理旧 `gfx_disp/gfx_obj/gfx_img` API 页面。(done: 2026-06-12)
- [x] 将剩余 `gfx_disp/gfx_obj/gfx_img` public 命名直接迁移到 `gfx_display/gfx_object/gfx_image`。(done: 2026-06-12)
  - [x] `gfx_img` public API 迁移为 `gfx_image`，删除旧 image wrapper。(done: 2026-06-12)
  - [x] `gfx_disp` public API 迁移为 `gfx_display`，实现入口同步改为 `src/core/display/gfx_display.c`。(done: 2026-06-12)
  - [x] `gfx_obj` public API 迁移为 `gfx_object`，实现入口同步改为 `src/core/object/gfx_object.c`。(done: 2026-06-12)

## ITU 借鉴落地

- [x] 设计文档：将 ITU 的 backend ops、surface/cache、widget lifecycle、scene input dispatch、sprite/keyframe 分层写入 `docs/itu_reference_design.md`。(done: 2026-06-12)
- [x] 补充 ITU 借鉴落地细节：renderer-owned backend ops 路由、fallback 规则、resource cache 落地顺序、input dispatch 当前状态。(done: 2026-06-12)
- [ ] Backend capability table。
  - [x] 定义 `gfx_draw_ops_t` 和 capability bits，覆盖 fill/blit/blend/scale/transform/draw_glyph/present。(done: 2026-06-12)
  - [x] 在 `docs/backend_architecture.rst` 中说明 capability、software fallback 和 backend alignment 边界。(done: 2026-06-12)
  - [ ] renderer 优先走 backend ops，缺失能力自动 fallback 到 software draw。
    - [x] 背景 fill 先尝试 backend `fill()`，未声明能力或返回失败时 fallback 到 software fill。(done: 2026-06-12)
    - [ ] image/blit/blend 路径接入 backend ops。
    - [ ] scale/transform/draw_glyph 路径接入 backend ops。
  - [x] memory backend 和 SDL backend 先声明最小能力，ESP PPA backend 后续接入 fill/blend/scale/rotate。(done: 2026-06-12)
  - [x] 文档记录格式限制和 fallback 规则，避免 widget 直接依赖硬件 backend。(done: 2026-06-12)
- [ ] Render roundup / 对齐接口。
  - [x] 设计文档：明确 roundup 属于 renderer/backend 协商，不属于 widget 或 image source descriptor。(done: 2026-06-12)
  - [x] 定义 `gfx_render_alignment_t`，描述 width/height/stride/address 对齐需求，例如 4 字节、8 字节、cache line、DMA burst。(done: 2026-06-12)
  - [x] backend 增加 alignment metadata/getter，SDL/memory/callback 默认 1，ESP PPA/LCD backend 后续按硬件要求填写。(done: 2026-06-12)
  - [x] 新增 central helper：`gfx_render_roundup_area()` / `gfx_render_roundup_stride_bytes()`，统一向外扩展并 clip 到 display/surface limit。(done: 2026-06-12)
  - [x] render chunk 切分使用 roundup helper，避免 PPA/DMA backend 在 flush 或 draw_ops 内部各自偷偷对齐。(done: 2026-06-12)
  - [ ] render buffer 分配接入 stride/address 对齐，支持非 full-frame 临时 chunk pitch 对齐。
  - [ ] image/blit/blend/scale/transform backend ops 在调用前检查 alignment，不满足时 fallback software。
  - [ ] 增加测试：4-byte/8-byte 对齐面积、边缘 clip、超出 limit fallback、dirty area 不漏绘。
- [ ] Surface / decoded resource cache。
  - [ ] 定义 decoded cache entry：source key、format、width/height/stride、decoded bytes、refcount、last_use。
  - [ ] 增加 cache budget 配置，默认保守，避免图片/动画解压后隐藏占用过多内存。
  - [ ] 支持按需 decode、pin、release 和 LRU eviction。
  - [ ] Host SDL 暴露 cache stats；ESP 端保留可关闭/小预算模式。
- [ ] Widget 资源生命周期。
  - [ ] 扩展 widget class hooks：`load` / `release` / `update` / `draw` / `delete` / `touch_event`。
  - [ ] setter 只记录 source/config 并 invalidate，资源获取统一放到 load/update/draw 边界。
  - [ ] display delete 时按对象树统一 release child resource，再 delete object。
  - [ ] 先迁移 image、anim、label/font、motion 四类资源型 widget。
- [ ] Scene-level input dispatch。
  - [x] touch hit-test 显式按 z-order 从 top 到 bottom 命中，命中即停。
  - [x] touch press 捕获命中对象，move/release 继续派发给 captured object。
  - [x] 跳过 invisible 对象，widget 不再做全局 hit-test；disabled 状态待对象模型补充后接入。(done: 2026-06-12)
  - [x] 增加 `gfx_display_hit_test()` internal helper，复用当前 touch hit-test 逻辑。(done: 2026-06-12)
  - [ ] 对象数量较多时，将递归 top-down walk 改为非递归栈或双向 z-list。
  - [ ] 为 topmost hit、capture release、object delete during press 增加 host/unit 测试。
- [ ] 通用 keyframe/tween 层设计。
  - [ ] 新增 `include/gfx/tween.h` 和 `src/core/tween/` 设计草案。
  - [ ] 支持 position、size、opacity、color、custom numeric callback。
  - [ ] 使用现有 tick/timer，属性更新走 object/widget setter 并触发 invalidate。
  - [ ] 明确与 `gfx_anim`、`gfx_motion` 的边界：frame 播放、角色 motion、通用 UI tween 三者分开。

## Host SDL 仿真输入

- [x] 将 SDL 鼠标分发收口到统一的 GFX touch 注入路径。
- [x] SDL backend 只负责窗口事件/坐标转换，不直接做 widget hit-test。
- [x] 重新构建 host SDL 和 host core target。
- [x] 在本机 RDP display 上 smoke test SDL demo。

## Host SDL 长期架构

目标：让 SDL 仿真成为稳定的 host runner，而不是 demo 里临时拼出来的窗口循环。SDL 的事件处理、timer 推进、render、present 都应有明确线程归属，避免“鼠标点一下才刷新一帧”或后台线程调用 SDL present 的问题。

- [x] 增加 `gfx_core_tick()`，允许 host 主线程同步推进 timer 和刷新。
- [x] 增加 `gfx_core_config_t.manual_tick`，让 SDL demo 可以关闭后台 render task，由主循环驱动。
- [x] SDL poll 在处理事件后调用 `gfx_core_tick()`，保证动画/ motion 不依赖鼠标事件推进。
- [ ] 抽出正式 host runner，例如 `gfx_host_run(gfx, disp, runner_cfg)`。
  - [ ] runner 负责固定帧率 sleep、SDL event pump、`gfx_core_tick()`、退出条件。
  - [ ] demo 只负责创建 UI，不直接维护 while/sleep/poll 细节。
  - [ ] 支持 headless/dummy 模式，用于 CI smoke。
- [ ] 收紧 SDL backend 职责。
  - [ ] backend 只做 framebuffer -> SDL texture/present 和 input translation。
  - [ ] 不在 backend 内部直接依赖某个应用主循环策略。
  - [ ] 明确 `poll` / `present` 必须在 SDL owner thread 调用。
- [ ] 统一 core 调度模型。
  - [ ] 后台 task 模式用于 ESP-IDF/RTOS。
  - [ ] manual tick 模式用于 host/SDL/单线程仿真。
  - [x] 文档说明两种模式不能同时驱动同一个 context。(done: 2026-06-12)
- [ ] 增加自动验证。
  - [ ] host SDL demo 运行 2 秒，检查 anim frame index 或 render frame count 有递增。
  - [ ] dummy video backend 下跑 CI，不要求真实窗口。
  - [ ] RDP/桌面环境下保留人工检查命令。

## Image / Font Spec 固化

- [x] 固化 image color format 边界：RGB565、RGB888、RGB565A8、RGB888A8。
- [x] 明确 plane-style alpha 格式布局、stride、对齐和解码约定。
- [x] 文档补充 `stride == 0` 表示 tight stride，和 decoder/render 行为保持一致。(done: 2026-06-12)
- [x] 为 image asset/spec 文档补充最小示例和调试检查项。
- [x] 固化字体缺字 placeholder 行为，避免缺字直接空白。
- [x] 固化 font size fallback 行为，处理请求字号超过字体实际 size 的场景。
- [x] 在 SDL demo 或 test app 中加入 RGB888/RGB888A8 与字体 fallback 的可视化验证。

## Public/Core API 去 ESP 依赖

- [x] 定义 GFX 自己的错误类型，例如 `gfx_err_t`，逐步替代 public header 里的 `esp_err_t`。
- [x] 定义 GFX 输入点/触摸配置类型，例如 `gfx_input_point_t` / `gfx_touch_source_t`，避免 public API 暴露 `esp_lcd_touch`。
- [x] 将 ESP-IDF touch handle、GPIO、ISR 等细节下沉到 `src/platform/esp_idf`。
- [x] 梳理 public/core 头文件，只保留跨平台 API、类型和语义。
- [ ] ESP-IDF 相关适配只保留在 platform/adapter 层，不在 public API 暴露旧式入口。
- [ ] 同步更新 host build、ESP-IDF build 和 API 文档。

## FS / Asset 兼容层

目标：让 `test_anim.c` 这类依赖 flash/mmap 资源的用例，也能在 host SDL 仿真里直接加载同一批 `.aaf/.eaf/.bin/.ttf` 文件。应用层不直接关心资源来自 ESP flash 分区、mmap assets，还是 Linux 文件系统。

- [x] 定义 public 资源访问抽象，例如 `gfx_fs.h` / `gfx_asset.h`。(done: 2026-06-12)
  - [x] 统一资源句柄：`gfx_asset_store_t` 表示一个资源仓库。(done: 2026-06-12)
  - [x] 统一资源视图：`gfx_asset_view_t` 表示一段只读资源内存，包含 `data`、`size`、`name`、`id`、生命周期信息。(done: 2026-06-12)
  - [x] 支持按路径/名称打开：例如 `"mi_1_eye_24bit.aaf"`。(done: 2026-06-12)
  - [ ] 支持按 ID 打开，兼容 `mmap_generate_assets_test.h` 里的 asset id。
  - [x] 明确 `open/close` 生命周期：如果后端是 mmap，`close` 只释放 view；如果后端是 fread buffer，`close` 释放内存。(done: 2026-06-12)

- [ ] ESP-IDF 后端：封装现有 `esp_mmap_assets`。
  - [ ] 新增 `src/platform/esp_idf/gfx_asset_mmap_esp_idf.c`。
  - [ ] `gfx_asset_store_open_mmap_partition(partition_label, max_files, checksum)` 内部调用 `mmap_assets_new()`。
  - [ ] `gfx_asset_open_by_id()` 内部调用 `mmap_assets_get_mem()` / `mmap_assets_get_size()`。
  - [ ] `gfx_asset_open_by_name()` 内部遍历 `mmap_assets_get_name()`，复用 `test_anim_emote_gen.c` 里按名称查找的逻辑。
  - [ ] 直接迁移现有 test app，从 `mmap_assets_handle_t` 切到 `gfx_asset_store_t`。

- [ ] Host/Linux 后端：从普通目录加载资源。
  - [x] 新增 `src/platform/linux/gfx_asset_fs_linux.c`。(done: 2026-06-12)
  - [x] `gfx_asset_store_open_dir(root_dir)` 指向一个 host 资源根目录。(done: 2026-06-12)
  - [x] `gfx_asset_open_by_name()` 使用 `open/stat/mmap`，失败时 fallback 到 `fread + malloc`。(done: 2026-06-12)
  - [ ] `gfx_asset_open_by_id()` 通过 manifest 将 ID 映射到文件名。
  - [x] 支持只读 mmap，避免动画大文件每次复制。(done: 2026-06-12)
  - [ ] Windows 后续可单独加 `_win32` 后端；当前先服务 Linux SDL。

- [ ] 资源 manifest / index 固化。
  - [ ] 为 host 生成或维护 `assets_manifest.json`，记录 `id -> filename -> size -> optional checksum`。
  - [ ] 兼容 ESP 生成头里的 ID，例如 `MMAP_ASSETS_TEST_MI_1_EYE_24BIT_AAF`。
  - [ ] CMake host target 可选择读取 `test_apps/main/mmap_generate_assets_test.h` 或生成一个 host manifest header。
  - [ ] 明确资源根目录来源：优先 `GFX_ASSET_ROOT` 环境变量，其次 CMake 配置路径，最后默认 `test_apps/assets_test` / `test_apps/assets_gen`。

- [ ] Widget source descriptor 接入资源 view。
  - [ ] `gfx_anim_src_t` 增加资源 view 类型，或新增 helper 将 `gfx_asset_view_t` 转成 `GFX_ANIM_SRC_TYPE_MEMORY`。
  - [ ] image/font 同样提供 helper：`gfx_image_src_from_asset()`、`gfx_font_cfg_from_asset()`。
  - [ ] 保持 decoder 只依赖内存视图，不让 decoder 直接打开文件，边界更清晰。
  - [ ] 明确资源 view 在 widget 使用期间必须保持有效，或者由 widget 复制必要数据。

- [ ] Host SDL demo / test app 验证。
  - [x] 给 host SDL demo 增加一个 animation panel，从 host 文件系统加载 `.aaf/.eaf`。(done: 2026-06-12)
  - [ ] 将 `test_anim.c` 的 `mmap_assets_handle_t` 访问迁移到 `gfx_asset_store_t`，ESP 与 host 共享主流程。
  - [x] 增加 host 专用 asset smoke：打开 store，加载一个文件并验证 view 生命周期。(done: 2026-06-12)
  - [ ] 保留 ESP-IDF Unity 用例，确保 flash/mmap 分区路径没有回归。

- [ ] CMake / 文档。
  - [x] host target 显式编入 Linux FS 后端，ESP-IDF target 显式编入 mmap/unsupported adapter 后端。(done: 2026-06-12)
  - [ ] 文档补充资源目录布局、manifest 格式、环境变量、常见调试命令。
  - [ ] 记录“资源 API 只提供只读字节视图，格式解释仍归 image/font/anim decoder”的设计约束。
