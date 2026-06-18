# 本地 TODO

## TODO 记录规则

- 完成项使用 `[x]`，并在任务后标注 `(done: YYYY-MM-DD)`。
- 后续按本文顺序推进；跨模块大任务先落设计/骨架，再接入实现和测试。
- 已完成但早于本规则的历史任务可暂不补时间，后续继续处理时再补齐。
- 与当前主线不再一致但仍保留作历史参考的未完成项，标注 `[LEGACY]`，不进入近期推进队列。

## 顶层 TODO

- [ ] Asset store 文件读取 V1：按 `docs/asset_store_file_loading_design.md` 实现统一 store/open/read/load 合同。
  - [x] 新增 `gfx_asset_store_open()` 统一配置入口，并保留 `gfx_asset_store_open_dir()` / `gfx_asset_store_open_mmap()` 兼容 wrapper。(done: 2026-06-18)
  - [x] 将 `gfx_asset_view_t` 扩展为 flags 语义，区分 direct mapped address、owned copy、mapped view、persistent view。(done: 2026-06-18)
  - [x] 保留 ESP-IDF `esp_mmap_assets` backend 的零拷贝 direct-address 行为，不降级为普通文件读取。(done: 2026-06-18)
  - [x] ESP-IDF 增加 VFS file/dir backend，使用 `fopen` / `fread` 从 FATFS、SPIFFS、SD card 读取到 owned buffer。(done: 2026-06-18)
  - [x] ESP-IDF 增加 raw partition backend，支持 `esp_partition_mmap` direct view 与 `esp_partition_read` owned copy 两种加载方式。(done: 2026-06-18)
  - [x] Linux directory backend 接入新 flags/caps，保持 POSIX `mmap` 优先、read fallback。(done: 2026-06-18)
  - [x] 新增 store capability 查询 API，明确 backend 是否支持 direct address、owned copy、open by name、open region。(done: 2026-06-18)
  - [ ] 将直接依赖 `esp_mmap_assets` 的示例/播放器迁移到 `gfx_asset_store_t`，资源层不解析 `index.json` 或业务内容。
  - [x] 增加 host 与 ESP-IDF smoke，覆盖 open/read/load/view close 生命周期。(done: 2026-06-18)

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
    - [x] image/blit/blend 路径接入 backend ops，未声明能力、未对齐或 backend 拒绝时 fallback 到 software draw。(done: 2026-06-15)
    - [ ] scale/transform/draw_glyph 路径接入 backend ops。
  - [x] memory backend 和 SDL backend 先声明最小能力，ESP PPA backend 后续接入 fill/blend/scale/rotate。(done: 2026-06-12)
  - [x] 文档记录格式限制和 fallback 规则，避免 widget 直接依赖硬件 backend。(done: 2026-06-12)
- [ ] Render roundup / 对齐接口。
  - [x] 设计文档：明确 roundup 属于 renderer/backend 协商，不属于 widget 或 image source descriptor。(done: 2026-06-12)
  - [x] 定义 `gfx_render_alignment_t`，描述 width/height/stride/address 对齐需求，例如 4 字节、8 字节、cache line、DMA burst。(done: 2026-06-12)
  - [x] backend 增加 alignment metadata/getter，SDL/memory/callback 默认 1，ESP PPA/LCD backend 后续按硬件要求填写。(done: 2026-06-12)
  - [x] 新增 central helper：`gfx_render_roundup_area()` / `gfx_render_roundup_stride_bytes()`，统一向外扩展并 clip 到 display/surface limit。(done: 2026-06-12)
  - [x] render chunk 切分使用 roundup helper，避免 PPA/DMA backend 在 flush 或 draw_ops 内部各自偷偷对齐。(done: 2026-06-12)
  - [x] 非 full-frame render chunk 使用 aligned stride，并按物理 stride 计算 chunk 高度，避免超过 `buf_pixels` 容量。(done: 2026-06-15)
  - [x] backend internal flush 增加 source stride 参数，memory/SDL backend 支持读取 aligned chunk pitch；legacy `flush_cb` 保持旧 API，只支持紧凑或 full-frame stride。(done: 2026-06-15)
  - [x] 增加 `gfx_render_is_addr_aligned()`，供后续 backend ops 在调用前统一检查地址对齐。(done: 2026-06-15)
  - [x] 内部 render buffer 分配接入 `addr_bytes` 对齐，使用 `gfx_platform_aligned_alloc()`；外部 buffer 不强制，交给 backend op eligibility fallback。(done: 2026-06-15)
  - [x] 增加 backend op 目标 surface/area eligibility helper，统一检查 cap、addr、stride、width/height alignment；当前已接入 fill fallback。(done: 2026-06-15)
  - [x] image/blit/blend backend ops 在调用前复用 alignment eligibility，不满足时 fallback software。(done: 2026-06-15)
  - [x] 抽出 shared image scale draw：从 coverflow 内部 `draw_image_scaled` 收敛为 `gfx_sw_blend_img_scale_draw()`，供 image/pageflow/coverflow/button image bg 复用。(done: 2026-06-15)
  - [x] 增加 `gfx_render_backend_scale()`：优先走 backend `scale()`，失败或不满足 alignment 时 fallback 到 shared software scale draw；coverflow scale 路径已接入，当前无 scale op 的 backend 自动走软件 fallback。(done: 2026-06-15)
  - [x] 接入内部 ESP-IDF PPA platform accel provider：core init 自动注册 fill/SRM client，built-in callback backend 与 memory backend 自动复用平台 `draw_ops`，初始化失败时回退 software，不阻断 core bring-up。(done: 2026-06-17)
  - [x] PPA 当前能力白名单已收敛：`fill dst={RGB565,RGB888,ARGB8888}`，`blit/scale dst={RGB565,RGB888,ARGB8888}`，`blit/scale src={RGB565,ARGB8888}`；`RGB565_SWAPPED/BGR888/XRGB8888` 与 alpha blend 路径暂不走 PPA，`RGB888 src` 已临时禁用并回退 software 以规避板端图片颜色异常。(done: 2026-06-17)
  - [x] PPA provider 启动日志增加能力摘要与一次性 reject reason，板端可直接看到启用的 ops、alignment、格式白名单与 fallback 原因。(done: 2026-06-17)
  - [ ] 继续完善 PPA provider。
    - [ ] 查清并补上 `RGB888 src -> RGB565/RGB888` 的正确 SRM 输入语义，再恢复 `RGB888 src` 的 blit/scale 加速。
    - [ ] 评估 `XRGB8888` 是否以 `ARGB8888 + alpha=255` 语义接入 PPA，避免 32bpp opaque 图源长期只能走 software。
    - [ ] 评估 `RGB565_SWAPPED` / `BGR888` 是否通过 `byte_swap` / `rgb_swap` 或预处理桥接接入，而不是永久排除。
    - [ ] 在格式白名单稳定后补 host/ESP smoke，显式断言 PPA 命中与 software fallback 的结果一致，避免板端再次出现 image/pageflow 颜色回归。
  - [ ] transform backend ops 在调用前复用 alignment eligibility，不满足时 fallback software。
  - [x] 增加 host alignment smoke：覆盖内部 buffer addr 对齐、aligned fill op 命中、外部错位 buffer fallback。(done: 2026-06-15)
  - [x] 扩展 host alignment smoke：覆盖 aligned image blit/blend 命中，以及外部错位 buffer 的 image blit fallback。(done: 2026-06-15)
  - [x] 增加 ESP-IDF/Unity 对齐 smoke：真实 IDF 内部 buffer 分配满足 `addr_bytes`、aligned fill/blit/blend 命中、外部错位 buffer fallback 到 software、legacy `flush_cb` compact stride 边界保持可用。(done: 2026-06-15)
  - [ ] 扩展 ESP-IDF/Unity 对齐测试：4-byte/8-byte 对齐面积、边缘 clip、超出 limit fallback、dirty area 不漏绘。
- [ ] RGB565 / RGB888 输出格式框架。
  - [x] display config 增加输出 `color_format`，默认保持 legacy RGB565 / RGB565_SWAPPED 行为。(done: 2026-06-15)
  - [x] display 内部分离 `render_format` 与 `output_format`，当前软件渲染先保持 RGB565，flush 前按 output format 打包。(done: 2026-06-15)
  - [x] render ctx / backend surface 携带目标 format 和 pixel size，后续 backend ops 不再靠 `swap` 猜目标格式。(done: 2026-06-15)
  - [x] 修正 RGB565 家族 display format 解析：`render_format` 默认直接跟随 `output_format`，不再被 `flags.swap` 单独改写成相反字节序。(done: 2026-06-16)
  - [x] RGB888 / XRGB8888 display 默认直接使用同格式 `render_format`，不再退回 RGB565 render + flush 前转换。(done: 2026-06-16)
  - [x] 增加 `GFX_COLOR_FORMAT_BGR888`，用于对接 ESP RGB LCD / LVGL 风格 `B,G,R` 24bpp payload；`gfx_888` 板级测试改为直接输出 BGR888，不再在 flush callback 中临时换色。(done: 2026-06-16)
  - [x] host / ESP smoke 增加 render-format 断言，防止 RGB888 / XRGB8888 路径回退成 RGB565 中转。(done: 2026-06-16)
  - [x] host / ESP smoke 增加 `RGB565_SWAPPED` display byte-order 回归，验证背景填充后输出字节序与 render/output format 一致。(done: 2026-06-16)
  - [x] 修正旧 swap 配置与 `RGB565_SWAPPED` 的语义混淆：未显式设置 `color_format` 时默认仍为 `RGB565`，SPI 565 测试工程显式配置 `GFX_COLOR_FORMAT_RGB565`。(done: 2026-06-16)
  - [x] 修正 EAF/JPEG 24bit 动画 decode 双重 swap：decoder 固定输出内部语义 RGB565，render 按 `ctx->format` 写目标 surface，不再由 decoder 按 display swap 生成 native 数据。(done: 2026-06-16)
  - [x] memory backend 增加 RGB888 framebuffer 格式，支持 RGB565/RGB888 flush 输入转换。(done: 2026-06-15)
  - [x] SDL backend 支持 RGB888 flush 输入，继续展示到 XRGB8888 texture。(done: 2026-06-15)
  - [x] 增加 host 与 ESP-IDF memory backend smoke，验证 RGB888 display output flush 为 `R,G,B` 字节。(done: 2026-06-15)
  - [x] RGB888 image source 直接保留 24-bit 精度，不再强制落到 RGB565 semantic 再输出。(done: 2026-06-15)
  - [x] 增加 host smoke：覆盖 RGB888 / XRGB8888 / ARGB8888 源到 RGB888 输出的精度与 alpha 行为。(done: 2026-06-15)
  - [x] 增加 host / ESP smoke：覆盖 XRGB8888 display output 的 `0xFFRRGGBB` 像素布局，以及 RGB888/XRGB8888 image source -> XRGB8888 render/output 精度。(done: 2026-06-17)
  - [x] 将普通 UI 写入路径从 `gfx_color_t *` 16bpp 强转迁移到 format-aware surface writer：label mask、container/button/list/wheel/pageflow/coverflow/qrcode 背景、文本、边框按实际输出格式写入。(done: 2026-06-15)
  - [x] mesh triangle/polygon rasterizer 增加目标 format 参数，RGB888/ARGB/XRGB 源在变形绘制时不再先压到 RGB565 semantic。(done: 2026-06-15)
  - [x] anim block render 从 `GFX_DRAW_CTX_DEST_PTR()` + `gfx_color_t *` 写入迁移到按 `ctx->format` 写目标 surface，4bit/8bit/24bit 路径均按实际输出格式落像素。(done: 2026-06-15)
  - [x] RGB888 image source -> RGB888 render buffer 保留 24-bit 精度，不再经过 RGB565 semantic 中转。(done: 2026-06-15)
  - [ ] [LEGACY] 评估是否公开 `gfx_display_get_render_format()` 调试 API；当前主线不暴露 render_format public debug API，测试继续通过 private/internal 入口断言。
  - [x] 文档补充 swap 配置规则：推荐显式设置 `color_format`；低字节优先面板显式使用 `GFX_COLOR_FORMAT_RGB565_SWAPPED`。(done: 2026-06-16)
  - [x] 设计文档补充 LVGL/ITU 借鉴后的老模型清理方案：以 `color_format` / surface contract 为唯一边界，`swap` 不再进入 decoder/widget/render 热路径。(done: 2026-06-17)
  - [x] 参考 LVGL v9 收敛 RGB565 swap：保留 `RGB565` / `RGB565_SWAPPED` format 作为唯一字节序来源，移除 decoder/widget 内部的 `swap_color` / `swap_bytes` / `ctx->swap` 传递，并删除旧 `gfx_color_t * + bool swap` 渲染 helper。(done: 2026-06-17)
    - [x] Stage A：冻结新合同，新代码只使用 `ctx->format` 或 surface descriptor，禁止新增 `bool swap` 渲染参数；已移除 `gfx_draw_ctx_t.swap`、backend surface/image `swap` 字段和 `GFX_DRAW_CTX_DEST_PTR` 宏。(done: 2026-06-17)
    - [x] Stage B：删除 legacy raw 16bpp helper，旧 `gfx_color_t * + bool swap` API 不再保留，统一收敛到 format-aware helper。(done: 2026-06-17)
    - [x] Stage C：清理 decoder/widget swap 参数，移除 EAF/anim call chain 的 `swap_color` / `swap_bytes`，移除 image/mesh/anim/render backend 路径中的 `ctx->swap`。(done: 2026-06-17)
    - [x] Stage D：按 destination format 恢复性能，补上 RGB565 text/mask、RGB565 image scale、RGB565_SWAPPED 对称快路径，并增加 host smoke 回归覆盖。(done: 2026-06-17)
    - [x] Stage E：删除 `gfx_draw_ctx_t.swap`、display config `flags.swap` 与 memory backend `swap`，renderer contract 只保留显式 `color_format`。(done: 2026-06-17)
  - [ ] 增加 host/ESP 动画 JPEG RGB565 回归：同一帧覆盖 JPEG decode、palette frame、UI style color，验证 RGB565 与 RGB565_SWAPPED output 下均无双重 swap。
    - [x] ESP-IDF/Unity 已补同帧 JPEG + palette + UI style color 回归，覆盖 RGB565 与 RGB565_SWAPPED 输出无双重 swap。(done: 2026-06-17)
    - [ ] host 侧补等价回归，避免只靠板端覆盖动画/JPEG 混合链路。
  - [ ] 检查 motion 后续如新增专用 framebuffer raster path，必须直接接 format-aware surface writer，禁止 widget 内部 `GFX_DRAW_CTX_DEST_PTR()` + `gfx_color_t *` 写输出。
  - [x] 删除旧 RGB565-only `gfx_sw_blend_*` / `gfx_sw_draw_*` raw API，避免新 widget 误用。(done: 2026-06-17)
  - [x] 恢复 RGB565 opaque surface fill 快路径：普通 UI 背景/卡片/list/button 纯色填充继续走 16bpp 批量写，同时按 `dest_format` 保持正确字节序。(done: 2026-06-16)
  - [x] 恢复 RGB565 text/mask 快路径：`gfx_sw_blend_mask_draw_fmt()` / `mask_color_draw_fmt()` 在 565 目标上避免逐像素 RGBA dispatch。(done: 2026-06-17)
  - [x] 恢复 RGB565 image scale 快路径：565/565-swapped 目标走专用 nearest-neighbor surface path，并补 host smoke 验证字节序。(done: 2026-06-17)
  - [ ] 扩展 host/ESP 测试：验证 RGB565、RGB565_SWAPPED、RGB888 flush 数据字节序、stride、full-frame double-buffer sync。
    - [x] ESP-IDF/Unity 已覆盖 RGB565、RGB565_SWAPPED、RGB888 的 flush 字节序、screen stride 和 full-frame double-buffer sync。(done: 2026-06-17)
    - [ ] host 侧补等价 full-frame double-buffer sync 覆盖，和 memory backend smoke 对齐。
- [ ] Surface / decoded resource cache(参考 `docs/itu_reference_design.md` 的 decoded-resource cache 落地形态，不抄 LVGL/ITU“万物皆可缓存 surface”模型)。
  设计约束：保持 `gfx_asset_view_t`(只读字节) / image-font-anim descriptor(格式元数据) / render buffer(可写像素) 三者分离；缓存只接管“decoded surface entry”这一层，按 `asset bytes -> decoder -> decoded entry -> ref/release` 生命周期；先做 metadata + budget plumbing，再缓存真正的 decoded pixels。
  - [ ] 定义 decoded cache entry：source key、format、width/height/stride、decoded bytes、refcount、last_use tick。
  - [ ] image decoder 接入 decoded cache：当前 `GFX_IMAGE_SRC_TYPE_FILE` 每个 `gfx_image_resource` 实例都会独立 open/decode/alloc，同一 JPG 在 Image/Pageflow/Coverflow 间会重复解码；后续按 file name/asset id 做 key 共享 decoded buffer，并在 resource close 时 refcount release。
  - [ ] 增加 cache budget 配置(按字节预算)，默认保守且可关闭，避免 RGB888/RGB888A8 或解压动画帧隐藏占用过多内存。
  - [ ] 支持按需 decode、pin、release 和 LRU eviction；widget 持有 entry 时必须在 resource release 生命周期释放。
  - [ ] 接入顺序：image resource 先接(decode 结果易于 bound)；anim 帧缓存等 streaming 行为明确后再接，大动画默认 streaming/decode-on-demand，除非显式 pin。
  - [ ] Host SDL 暴露 cache stats；ESP 端保留可关闭/小预算模式。
- [ ] 组件收敛 / Widget convergence。
  说明：以下为 widget 层主线地基，优先级高于继续扩 PPA 格式白名单；按 P1->P4 顺序推进，P5/P6 为 demo 层清理，与主线解耦可随时做。image resource helper(P2) 完成后再回头扩 PPA / decoded cache，避免加速路径反复被 widget 资源/坐标细节打断。
  - [x] P1 收敛 label 文本绘制复用，去掉伪装成 label object 的模式。(done: 2026-06-18)
    - [x] 抽出内部 `gfx_label_text_box_draw()` / `gfx_label_text_box_update()` helper，输入 label state、area、clip、draw ctx，不再要求调用方临时改写 `obj->type/src/geometry/align`。(done: 2026-06-18)
    - [x] 将 `gfx_button.c`、`gfx_list.c`、`gfx_wheel.c`、`gfx_pageflow.c`、`gfx_coverflow.c` 的 `call_label_draw/update` 伪装调用迁移到新 helper，删除各自保存/还原 object 现场的代码。(done: 2026-06-18)
  - [ ] P2 统一 image / mesh image 资源入口，对齐 widget load/release 生命周期。
    - [x] 抽出 `gfx_image_resource` 内部 helper，统一 image source 校验、header 查询、decoder open/close、data/stride/format 输出。(done: 2026-06-18)
    - [x] 将 `gfx_mesh_img` 从 draw 阶段自行 prepare/open/close decoder 迁移到 load/release + 共享 helper，和 `gfx_img` 对齐。(done: 2026-06-18)
    - [x] pageflow image、coverflow image 路径接入共享 image resource helper，为后续 decoded cache 预留统一入口；coverflow card image 当前经 child `gfx_mesh_img` 已共享同一 helper 生命周期。(done: 2026-06-18)
  - [x] P3 收敛 flow 类组件图片数组所有权。(done: 2026-06-18)
    - [x] `gfx_coverflow_set_image_items()` / `gfx_pageflow_set_image_pages()` 改为内部 copy 指针数组(对齐 `set_card_descriptors()` 的 copy 语义)，避免调用方额外保活外部数组。(done: 2026-06-18)
    - [x] 当前不保留 borrowed API；如后续确有零拷贝数组需求，再单独提供命名明确的 `*_borrowed()` API 并在文档写清生命周期要求。(done: 2026-06-18)
  - [ ] P4 推进 widget draw 坐标合同收口(依赖已有 resolved abs_area 地基)。
    - [x] Anim 前置地基：decoder 增加 `get_info()`，EAF/AAF 在 set source 时 probe natural size/frame_count，anim object 在 draw 前已有稳定尺寸。(done: 2026-06-18)
    - [x] 第一步(低风险,纯读取)：widget draw 统一改用 `gfx_object_get_abs_area_exclusive()` 取绝对区域，删除各 widget draw 内 `gfx_object_calc_pos_in_parent()` + 手动从 `obj->geometry` 拼 `obj_area` 的写法；该 helper 已内部惰性解析对齐、命中 `resolved.abs_area` 缓存并叠加 parent `clip_children` 裁剪。(done: 2026-06-18)
      - [x] 已收 `gfx_button.c` / `gfx_list.c` / `gfx_wheel.c` / `gfx_pageflow.c` / `gfx_coverflow.c`，并顺手收敛 `label` / `qrcode` / `anim` / `mesh_img` / `img` draw 入口。(done: 2026-06-18)
      - [x] `gfx_img.c` / `gfx_qrcode.c` 特例处理：`obj_area` 宽高来自 image header / scaled_size，取 `abs_area` 左上角 + 内容宽高拼，不直接用返回的 x2/y2。(done: 2026-06-18)
      - [ ] ESP smoke：板上验证 parent `clip_children` 下 child draw 区域与 hit-test(`gfx_object_get_abs_area`)一致，迁移前后无回归。
      - [x] Host smoke：`gfx_host_list_smoke` / `gfx_host_wheel_smoke` / `gfx_host_coverflow_smoke` / `gfx_host_container_smoke` 通过。(done: 2026-06-18)
    - [x] 第二步(大改)：让 `geometry` 退成 local 兼容存储，`gfx_object_calc_pos_in_parent()` 不再写 `geometry.x/y` 而是只写 `resolved.abs_area`；`gfx_object_get_abs_area()` 从 `local_geometry` + parent 链纯计算，draw 路径彻底不读写 screen-space `geometry`。(done: 2026-06-18)
      - [x] 为兼容现有 public API，`gfx_object_get_pos()` 仍返回 resolved absolute pos；后续如需要暴露 local pos，再新增命名明确的 getter。(done: 2026-06-18)
    - [x] coverflow card child layout 从 draw/update 内改 child screen-space 坐标迁移到 parent-local slot/layout helper，和 container clipping/local geometry 主线对齐。(done: 2026-06-18)
  - [ ] P5 [demo] format demo 资源模块拆分为 catalog 与 asset_loader 两层。
    - [ ] catalog：widget/action/list/wheel/card 等静态元数据。
    - [ ] asset_loader：flow jpg、anim/eaf decode、后续 font/image 资源加载与 anim asset 全局状态。
  - [ ] P6 [demo] format demo motion 资源边界收敛：scene shell 不直接 include motion `.inc`，action count/name 与 motion asset 由 motion demo/catalog 模块提供，scene 只做 focus/show/hide 调度。
- [ ] Widget 资源生命周期。
  - [x] 扩展 widget class hooks：`load` / `release` / `update` / `draw` / `delete` / `touch_event`。(done: 2026-06-15)
  - [x] 增加 object resource 状态和 helper：`gfx_object_load_resource()` / `gfx_object_release_resource()` / `gfx_object_mark_resource_dirty()`。(done: 2026-06-15)
  - [x] render 在 draw 前统一调用 resource load；object/display delete 统一 release child resource，再 delete object。(done: 2026-06-15)
  - [x] image widget 首批迁移到 load/release：setter 记录 source/header 并标记 resource dirty，draw 复用已打开 decoder。(done: 2026-06-15)
  - [x] label/font 迁移到 load/release：label/button/list 的 font setter 只记录 source，load 创建 font adapter，release 释放 glyph cache 和 adapter。(done: 2026-06-15)
  - [ ] 继续迁移 anim、motion 两类资源型 widget。
- [ ] Scene-level input dispatch。
  - [x] touch hit-test 显式按 z-order 从 top 到 bottom 命中，命中即停。
  - [x] touch press 捕获命中对象，move/release 继续派发给 captured object。
  - [x] 跳过 invisible 对象，widget 不再做全局 hit-test；disabled 状态待对象模型补充后接入。(done: 2026-06-12)
  - [x] 增加 `gfx_display_hit_test()` internal helper，复用当前 touch hit-test 逻辑。(done: 2026-06-12)
  - [x] hit-test 支持对象树递归，child 优先于 parent 命中；touch capture 活跃检查同步支持 child object。(done: 2026-06-15)
  - [ ] 对象数量较多时，将递归 top-down walk 改为非递归栈或双向 z-list。
  - [ ] 为 topmost hit、capture release、object delete during press 增加 host/unit 测试。
- [ ] Container / composite object model。
  - [x] `gfx_object` 增加 `parent/child_list`，支持对象树而不是只有 display 一维 child list。(done: 2026-06-15)
  - [x] 新增 `gfx_object_add_child()` / `gfx_object_remove_child()` / `gfx_object_get_parent()` public API。(done: 2026-06-15)
  - [x] render/update/layout/hit-test/delete 支持递归对象树，删除 parent 时级联释放 child。(done: 2026-06-15)
  - [x] 新增轻量 `gfx_container` widget：背景、边框、可作为组合 parent。(done: 2026-06-15)
  - [x] 增加 host container smoke：覆盖父子挂载、child touch 命中、remove/re-add、parent delete cascade。(done: 2026-06-15)
  - [x] 小内存扫描安全地基：新增 `gfx_object_get_abs_area()`，统一 object -> dirty area 转换入口；当前仍保持 screen-space geometry。(done: 2026-06-15)
  - [x] 小内存扫描安全地基：新增 `gfx_object_invalidate_tree()`，parent move/resize/visible/tree/delete/layout 更新时递归 invalidate 子树，避免 child 超出 parent 时漏擦。(done: 2026-06-15)
  - [x] host container smoke 增加 parent hide/move dirty 覆盖验证，确保 child 区域进入 dirty list。(done: 2026-06-15)
  - [x] `gfx_object_align()` 默认基准从 display 调整为 parent area；root object 继续相对 display，`align_to()` 继续显式相对 target。(done: 2026-06-15)
  - [x] host container smoke 增加 parent-relative align 覆盖，验证 child 无 target 对齐时使用 parent 而不是 screen。(done: 2026-06-15)
  - [x] 参考 LVGL resolved coords：为 object 增加 `local_geometry` 与 `resolved.abs_area` cache；dirty/hit-test/render 入口统一读取 resolved area，当前 `geometry` 仍保留 screen-space 兼容语义。(done: 2026-06-15)
  - [ ] 参考 ITU draw offset：评估是否在 render ctx 中加入 `origin_x/y`，用于过渡期支持局部 child 绘制，避免 widget draw 内部手动累加 parent。
  - [ ] 定义 child 局部坐标语义：保留 local geometry，渲染/命中通过 layout context 计算 absolute rect，避免 draw 过程反复改写 `geometry.x/y`。
  - [ ] 局部坐标落地前先拆 `geometry`：区分 local rect 与 resolved/absolute rect，禁止 widget draw 直接累加 parent offset。
  - [ ] 将 invalidate/render/hit-test 全部改为依赖 resolved absolute area，再打开 local child coordinate。
  - [x] 增加 container clipping：`gfx_container_set_clip_children()` 支持 child 绘制和 hit-test 限制在 parent bounds 内。(done: 2026-06-15)
  - [x] container clipping 接入后补小内存扫描测试：host container smoke 覆盖 child 越出 parent 时 dirty 只保留可见交集，外侧不可命中。(done: 2026-06-15)
  - [ ] 增加 container layout helpers：padding、row/column、overlay/slot，先服务 card/list item/page slot，不做复杂 flex。
  - [x] coverflow card 从单 image/text 内绘制迁移为 container 组合模式：`gfx_coverflow_set_card_items()` 接管 card container，SDL demo 使用 container + mesh image + label 展示风景图和标题。(done: 2026-06-15)
  - [ ] pageflow card 从单 image/text 内绘制迁移为 container + image + label 组合，支持风景图下方标题/状态。
  - [x] coverflow card slot 正式化：新增 `gfx_coverflow_card_dsc_t` / `gfx_coverflow_set_card_descriptors()`，不再依赖“第一个 child 是 image、第二个 child 是 label”的约定；旧 `set_card_items()` 保持兼容并自动 fallback。(done: 2026-06-15)
  - [x] mesh image 增加普通图片矩形快捷设置：`gfx_mesh_img_set_source_rect()` / `gfx_mesh_img_set_image_rect()`，避免用户为非变形图片手动串 `set_src_desc + set_grid + set_rect`。(done: 2026-06-15)
  - [ ] coverflow card 组合在 local geometry/clip 完成后，从 screen-space 绝对坐标更新迁移到 parent-local layout。
- [ ] 交互组件增强，参考 ITU listbox/scrolllistbox/pageflow/coverflow/wheel 的交互模型，不复制复杂继承层级。
  - [x] SDL demo 左侧增加 widget catalog list，focus 后右侧只显示对应 widget preview，作为后续 list/wheel/pageflow/coverflow 交互验收入口。(done: 2026-06-15)
  - [x] `gfx_list` v2：从 item-step 滚动改为 pixel-level `scroll_offset`，保留 `top_index` 作为派生/兼容状态。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加 click/drag 判定阈值，避免轻微移动误触发滚动或误选中。(done: 2026-06-15)
  - [x] `gfx_list` v2：完善 selected/focused 状态，区分当前选中项、键盘/旋钮焦点项、触摸 pressed 项。(done: 2026-06-15)
  - [x] `gfx_list` v2：touch move 支持连续拖拽，release 后按位移阈值决定点击选中或滚动结算。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加惯性滑动，记录 move velocity，release 后用 timer/tick 衰减滚动。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加边界 overscroll 和 bounce-back 回弹，参数可配置但默认保守。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加 snap-to-item 选项，release 后对齐到最近 item 边界。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加分页模型 `page_index/page_count/items_per_page`，支持 prev/next/set page。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加 page load callback，用于外部按页填充数据，参考 ITU ListBox `OnLoadPage`。(done: 2026-06-15)
  - [x] `gfx_list` v2：增加 selection callback 的 confirm 参数，区分 focus 改变和用户确认选择。(done: 2026-06-15)
  - [ ] `gfx_list` v2：补 host/Unity 测试，覆盖点击、拖拽阈值、惯性停止、边界回弹、分页切换。
    - [x] host list smoke 覆盖分页切换、page load callback、programmatic select、touch confirm。(done: 2026-06-15)
    - [x] host list smoke 覆盖 touch drag 滚动和 release snap。(done: 2026-06-15)
    - [x] host list smoke 覆盖惯性滚动和顶部 overscroll 回弹。(done: 2026-06-15)
  - [x] 新增 `gfx_wheel`：中心选中、上下滚动、snap 到中心 item、可选循环滚动。(done: 2026-06-15)
  - [ ] `gfx_wheel`：支持 center item 独立字体/颜色/缩放样式，普通 item 使用衰减样式。
    - [x] center item 独立背景/文字颜色已支持；缩放样式等待 text scale/transform 能力。(done: 2026-06-15)
  - [x] `gfx_wheel`：支持 value changed callback 和 confirm callback。(done: 2026-06-15)
    - [x] value changed callback 已支持；confirm callback 已增加 `gfx_wheel_confirm()` / `gfx_wheel_set_confirm_cb()`，host smoke 覆盖 tap confirm 与 drag release 不 confirm。(done: 2026-06-15)
  - [x] 新增 `gfx_pageflow`：水平/垂直分页拖拽、阈值翻页、回弹到当前页。(done: 2026-06-15)
  - [x] `gfx_pageflow`：增加 image page 数据源接口，SDL demo 使用 RGB888 风景图验证图片分页切换。(done: 2026-06-15)
  - [ ] `gfx_pageflow`：支持 page changed callback，页面内容先用 object 列表/slot 管理，不引入复杂继承。
    - [x] page changed callback 已支持；当前页面内容先用轻量文本页，object slot 待 container 能力补齐。(done: 2026-06-15)
  - [x] 新增 `gfx_coverflow`：在 `gfx_pageflow` 基础上增加 scale/alpha/offset 过渡；等待 transform/scale backend ops 或 software transform 稳定后实现。(done: 2026-06-15)
    - [x] 第一版支持中心/侧边卡片 offset、尺寸差异和拖拽翻页；真实 scale/alpha transition 待 transform/opacity 能力补齐。(done: 2026-06-15)
    - [x] 增加 coverflow zoom/spacing/dim 效果管线：拖动时按 progress 连续插值位置和尺寸，图片按卡片区域缩放绘制，按 zoom 排序绘制层级。(done: 2026-06-15)
    - [x] 抽出 `gfx_flow_effect` 草案：输入 selected index、drag offset、spacing、center/side zoom，输出 pos/zoom/dim/z-order，先服务 coverflow，后续复用到 pageflow/wheel。(done: 2026-06-15)
    - [x] 将 coverflow 的 visual state sort 抽成小型 helper，仅在第二个组件复用时再公开到 common/render 层。(done: 2026-06-15)
    - [x] coverflow scale 路径接入 backend `scale()`，软件路径保留 fallback。(done: 2026-06-15)
    - [x] coverflow release 后增加 tween 回弹/滑入动画，避免只在 release 瞬间跳到目标 index。(done: 2026-06-15)
  - [x] `gfx_coverflow`：增加 image item 数据源接口，SDL demo 使用 RGB888 风景图验证图片卡片切换。(done: 2026-06-15)
  - [x] `gfx_coverflow`：增加 card item 数据源接口，支持由外部 container card 承载图片和文字；host smoke 覆盖 card mode 拖拽选择。(done: 2026-06-15)
  - [x] test app 增加 coverflow card scene 预览项，板端 Unity 菜单可直接观察 container card、RGB888 图片和标题组合效果。(done: 2026-06-15)
  - [x] SDL demo 拆分 Button preview 与 Motion control，Motion action button/list 只在 Motion 页展示。(done: 2026-06-15)
  - [x] SDL demo 调整 720x720 布局、导航行高和预览控件尺寸，改善字体偏小和界面拥挤问题。(done: 2026-06-15)
- [ ] 通用 keyframe/tween 层设计。
  - [x] 新增 `include/gfx/tween.h` 和 `src/core/tween/` V1，实现轻量 `gfx_tween` handle、全局 core tick 更新、`i32` 插值和 done callback。(done: 2026-06-15)
  - [x] 使用现有 tick/timer，core tick 驱动 active tween；value callback 更新 widget 私有字段并触发 invalidate。(done: 2026-06-15)
  - [x] coverflow release tween 迁移到 `gfx_tween_start_i32()`，删除 widget 内部 start_ms/duration/ease 小状态机。(done: 2026-06-15)
  - [x] 增加 host tween smoke，覆盖 start/midway/complete/done callback，并纳入 `gfx_host_smoke`。(done: 2026-06-15)
  - [ ] 支持 position、size、opacity、color typed property helper。
  - [x] pageflow release 回弹迁移到 `gfx_tween`，release 后切换目标页并 tween `drag_offset -> 0`。(done: 2026-06-15)
  - [x] wheel snap 迁移到 `gfx_tween`，release 后立即更新 selected，再 tween `scroll_y` 到中心行。(done: 2026-06-15)
  - [ ] list bounce-back 迁移到 `gfx_tween`；惯性滑动仍保留 physics 逻辑，先只收越界回弹和 snap settle。
  - [ ] 明确与 `gfx_anim`、`gfx_motion` 的边界：frame 播放、角色 motion、通用 UI tween 三者分开。

## Host SDL 仿真输入

- [x] 将 SDL 鼠标分发收口到统一的 GFX touch 注入路径。
- [x] SDL backend 只负责窗口事件/坐标转换，不直接做 widget hit-test。
- [x] 重新构建 host SDL 和 host core target。
- [x] 在本机 RDP display 上 smoke test SDL demo。

## 板级测试 App

- [x] `gfx_888` 本地 `hmi_rgb_board` BSP 增加 GT1151 触摸初始化，TP IO 使用 `RST=51 / SDA=49 / SCL=50 / INT=48`，并接入 `gfx_touch_add()`。(done: 2026-06-16)
- [x] `gfx_565` 去掉外部 `bsp_display_new/bsp_touch_new` 依赖，改为本地 `components/hmi_rgb_board` 简化 BSP，结构与 `gfx_888` 对齐。(done: 2026-06-16)
- [x] `gfx_565` / `gfx_888` 触摸接入后取消 coverflow 自动切页，交互改为用户手动触摸切换。(done: 2026-06-16)

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
  - [x] dummy video backend 下跑 CI，不要求真实窗口。(done: 2026-06-15)
  - [x] 增加 `gfx_host_smoke` / CTest 入口，统一运行 asset、alignment、SDL dummy smoke。(done: 2026-06-15)
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
  - [ ] [LEGACY] 支持按 ID 打开，兼容 `mmap_generate_assets_test.h` 里的 asset id；当前主线优先按文件名打开资源，ESP mmap id 仅保留兼容路径。
  - [x] 明确 `open/close` 生命周期：如果后端是 mmap，`close` 只释放 view；如果后端是 fread buffer，`close` 释放内存。(done: 2026-06-12)

- [x] ESP-IDF 后端：封装现有 `esp_mmap_assets`。(done: 2026-06-18)
  - [x] 在 `src/platform/esp_idf/gfx_asset_esp_idf.c` 中接入 mmap-assets backend，并保留 direct-address/mapped view 语义。(done: 2026-06-18)
  - [x] `gfx_asset_store_open_mmap()` / `gfx_asset_store_open()` 内部调用 `mmap_assets_new()`。(done: 2026-06-18)
  - [x] `gfx_asset_open_by_id()` 内部调用 `mmap_assets_get_mem()` / `mmap_assets_get_size()`。(done: 2026-06-18)
  - [x] `gfx_asset_open_by_name()` 内部遍历 `mmap_assets_get_name()` 并复用 id open 路径。(done: 2026-06-18)
  - [x] 新增 ESP-IDF Unity asset store smoke，覆盖 mmap-assets id/name 打开路径。(done: 2026-06-18)

- [ ] Host/Linux 后端：从普通目录加载资源。
  - [x] 新增 `src/platform/linux/gfx_asset_fs_linux.c`。(done: 2026-06-12)
  - [x] `gfx_asset_store_open_dir(root_dir)` 指向一个 host 资源根目录。(done: 2026-06-12)
  - [x] `gfx_asset_open_by_name()` 使用 `open/stat/mmap`，失败时 fallback 到 `fread + malloc`。(done: 2026-06-12)
  - [ ] [LEGACY] `gfx_asset_open_by_id()` 通过 manifest 将 ID 映射到文件名；当前 host/SDL 与 format demo 均改为 name-based 资源加载。
  - [x] 支持只读 mmap，避免动画大文件每次复制，并接入 view flags / caps 查询。(done: 2026-06-18)
  - [ ] [LEGACY] Windows 后续可单独加 `_win32` 后端；当前主线只覆盖 Linux SDL 与 ESP-IDF。

- [ ] [LEGACY] 资源 manifest / index 固化；当前主线不要求业务资源层解析 index，优先 `gfx_asset_open_by_name()`。
  - [ ] [LEGACY] 为 host 生成或维护 `assets_manifest.json`，记录 `id -> filename -> size -> optional checksum`。
  - [ ] [LEGACY] 兼容 ESP 生成头里的 ID，例如 `MMAP_ASSETS_TEST_MI_1_EYE_24BIT_AAF`。
  - [ ] [LEGACY] CMake host target 可选择读取 `test_apps/main/mmap_generate_assets_test.h` 或生成一个 host manifest header。
  - [ ] 明确资源根目录来源：优先 `GFX_ASSET_ROOT` 环境变量，其次 CMake 配置路径，最后默认 `test_apps/assets_test` / `test_apps/assets_gen`。

- [ ] anim/eaf 资源入口与 image 对齐(主线优先，先于 streaming/cache)。
  现状诊断：`gfx_image` 侧已收敛——`gfx_image_resource_t`(set_source/open/close/load/release) + `gfx_image_decoder_open_file_source()`(default store `open_by_name` + `fread` fallback，覆盖 mmap / VFS / partition)。`gfx_anim_decoder_eaf.c` 的 `gfx_anim_eaf_open_file_source()` 是更薄且不完整的副本：只走 `gfx_asset_get_default_store()` + `open_by_name`，无 `fread` fallback，且 `gfx_anim_src_get_data_size/peek_data` 与 import/export frame_desc 搬运偏重。
  - [x] P1 固化 anim eaf/aaf “一次性 load” 合同：`open` 时一次性把整个 eaf/aaf 通过 asset view load/map 成常驻内存，运行期所有帧从该常驻 buffer decode；禁止按帧重新打开文件或读盘；streaming/按需读取明确留作后续优化(见下方 decoded cache)。(done: 2026-06-18)
  - [x] P2 anim FILE source 覆盖多后端，不再假设 default store 是 mmap：普通 SPIFFS(VFS `fread` 整文件到 owned buffer)、mmap-fs(direct address view)、raw partition(`esp_partition_mmap` direct / `esp_partition_read` owned)三种模式都要走通，并补 `fread` fallback。(done: 2026-06-18)
    - [x] 抽出公共 `gfx_asset_source` 文件加载 helper(`src/core/base/gfx_asset_source.{c,h}`：default store `open_by_name` + `fread` fallback + 统一 release)，让 `gfx_image_decoder` 与 anim eaf 共用同一份，删除 anim 侧的薄副本。(done: 2026-06-18)
  - [x] P3 裁薄 eaf 过渡封装：移除常驻 buffer 后已无意义的 `gfx_anim_src_get_data_size/peek_data` 重复逻辑(MEMORY 路径直接用 `src->data/data_len`，FILE 路径走 `gfx_asset_source`)。(done: 2026-06-18)
    - [ ] (后续)进一步收敛 `gfx_anim_eaf_import/export_frame_desc` 的双向字段搬运与 `block_len/palette` 所有权转移；`gfx_anim_frame_desc_t` 与 `eaf_dec_header_t` 字段几乎一致，可评估最小化映射，但属于 decoder 热路径，单独小步做。
  - [ ] P4 anim 与 image 接口一致性：
    - [x] src 描述符与 FILE 解析对齐：`gfx_anim_src_t {type,data,data_len}` 已与 `gfx_image_src_t` 对齐，FILE 路径统一走 P2 公共 `gfx_asset_source` loader。(done: 2026-06-18)
    - [ ] (延后)anim widget 的 lazy `load`/`release` 生命周期迁移：当前 anim 在 `set_src_desc` 即 eager open decoder 并常驻，符合 P1“一次性 load 并保持”的取向；image 的 lazy load/release 主要服务 decoded cache 的按需释放。按 decoded cache“image 先接、anim 后接”的顺序，anim 的 load/release 迁移与下方“Widget 资源生命周期 → 继续迁移 anim、motion”合并推进，暂不新增冗余的 `gfx_anim_resource_t` 包一层。
  - [ ] image/font 同样提供 helper：`gfx_image_src_from_asset()`、`gfx_font_cfg_from_asset()`，与 anim 共用资源 view 语义。
  - [ ] 保持 decoder 只依赖内存视图，不让 decoder 直接打开文件，边界更清晰。
  - [ ] 明确资源 view 在 widget 使用期间必须保持有效，或者由 widget 复制必要数据。

- [ ] EAF/AAF 解码层优化(`src/lib/eaf/gfx_eaf_dec.c`)。
  风险提示：以下都在 decode 热路径上，逐项小步改、每步跑 host anim smoke + SDL dummy smoke 回归(同一 `.eaf/.aaf` 解码输出字节应保持一致)；先做低风险性能项，再碰正确性项，流式重构留最后。
  - [x] (低风险，先做)decode session / scratch 复用：handle(`eaf_dec_ctx_t`) 持有可复用 scratch(Huffman tmp buffer + 节点 arena)，按需增长、deinit 释放；anim widget 侧 `block_offsets`/`decode_buffer`/`palette_cache` 改为跨帧复用、仅按需增长。消除逐块 Huffman malloc/free 与逐帧 buffer churn。只加复用、解码输出严格不变。(done: 2026-06-18，host smoke + 8bit/24bit headless 回归通过)
  - [x] (低风险)Huffman 树复用：`huffman_decode_data` 改为从 handle arena bump 分配节点(按 dict `Σcode_len+1` 上界预留)，跨块复用、无逐块 `calloc`/递归 `free`；无 handle 时回退 transient malloc。树结构与输出与原实现一致。(done: 2026-06-18)
  - [x] (中风险，需回归)header/table 非对齐 `*(uint16_t*)`/`*(uint32_t*)` 读改为 `memcpy`(`eaf_rd_u16/u32/i32`)；行为严格等价(小端语义不变)。(done: 2026-06-18)
  - [x] (中风险)`decode_huffman_rle` 中间 buffer：`*out_size`(已等于 `width*block_height`) `*2` 保留为可证明的最坏上界(RLE pair 2 字节→≥1 字节输出)，加注释说明非魔法系数；buffer 改为复用 handle scratch。(done: 2026-06-18)
  - [x] (中风险)`eaf_dec_init` 补长度校验：NULL/最小头/`stored_len` 校验区间/`total_frames` 与 frame table 容量(防溢出)/逐帧 `frame_base+offset+size <= data_len` 与 `frame_size>=EAF_MAGIC_LEN`；`entries` malloc null 检查。截断/损坏文件不越界。(done: 2026-06-18)
  - [x] (小项)4bit 矛盾收口：`eaf_dec_get_frame_info`/`probe` 早拒 4bit(decode 未实现)，仅放行 8/24bit；并补 frame payload 大小 guard。(done: 2026-06-18)
  - [x] (高风险/大改)fread 流式 reader 抽象 —— 按帧 streaming 基座落地：在解码核之上加只读访问层 `eaf_dec_reader_t {io_ctx,size,read}`；新增 `eaf_dec_init_reader()` 只在 open 时读 header + frame table(整数化拷贝到 ctx)，decode 时把每帧 payload 按需 `read` 进复用 `frame_buf`(按 index 缓存，同帧不重读)，把“整文件常驻”降到“单帧常驻”。decode 核(`get_frame_info`/`decode_block`)与 widget 完全不改：streaming 时 `eaf_dec_get_frame_data()` 返回复用帧缓冲，resident `eaf_dec_init()` 路径原样保留(两种并存模式，opt-in)。(done: 2026-06-18)
    - asset 层新增 `gfx_asset_source_open_stream/stream_read/close_stream`(`gfx_asset_stream_t`)：fopen 优先(SPIFFS/FATFS/SD/host-dir 真流式，仅请求字节常驻)，否则回退默认 store 的 mapped/direct view 零拷贝；decoder 不直接开文件，由 `gfx_anim_decoder_eaf.c` 把 stream 桥到 reader，边界清晰。
    - opt-in：`gfx_anim_src_t.flags` 新增 `GFX_ANIM_SRC_FLAG_STREAMING`(默认 0 = resident P1)，仅对 FILE 源生效；mapped/direct 源即使请求 streaming 也走零拷贝 resident(streaming mmap 无收益)。host demo 经 `GFX_DEMO_ANIM_STREAM` 跑 FILE+streaming。
    - 决策落定：granularity 选“按帧”而非“按块”——正常整帧播放时按帧只 1 次顺序 `read`(syscall 最少)，且与 widget `frame_payload + block_offsets[i]` 常驻指针契约零冲突；prefetch task 暂不引入(见下方 follow-up)。
    - 校验：payload 字节 resident≡streaming 完全一致(`payload_diff=0`)；整套帧解码 streaming 与 fresh resident handle 在 8bit.eaf/24bit.aaf 全帧 0 mismatch，huff.aaf 的 41/62 差异为 **streaming 无关**(resident-vs-resident 同样 41 帧、同一帧集)。host ctest 8/8 通过。
  - [ ] (follow-up，需决策)frame-level bounce 双缓冲 + prefetch：当前按帧 streaming 串行(读帧 N → 解帧 N)，要真正“两块交互”需在显示帧 N 时预读帧 N+1 到第二缓冲——同步 fread 下必须借独立 prefetch task 才有真重叠(render 由 timer 驱动，非自由解码循环)，是本项最高风险部分，建议落到目标板按真实 I/O 时序决定 task 形态后再做。
  - [ ] (新发现，streaming 无关，pre-existing)Huffman/RLE 块短解码尾部未初始化：部分 huff 块 decode 返回 OK 但 `out_size < width*block_height`，`eaf_dec_decode_frame`/widget 的 per-frame temp/decode_buffer 尾部保留旧值/未初始化即被渲染(复用 buffer 时为上一块残留)。表现为两 handle 解同一 huff 文件确定性地差 41/62 帧。建议：decode_block 成功后对未覆盖区清零，或对短解码块按错误处理。属解码健壮性，独立小步评估。

- [ ] Host SDL demo / test app 验证。
  - [x] 给 host SDL demo 增加一个 animation panel，从 host 文件系统加载 `.aaf/.eaf`。(done: 2026-06-12)
  - [ ] 将 `test_anim.c` 的 `mmap_assets_handle_t` 访问迁移到 `gfx_asset_store_t`，ESP 与 host 共享主流程。
  - [x] 增加 host 专用 asset smoke：打开 store，加载一个文件并验证 view 生命周期。(done: 2026-06-12)
  - [ ] 保留 ESP-IDF Unity 用例，确保 flash/mmap 分区路径没有回归。

- [ ] CMake / 文档。
  - [x] host target 显式编入 Linux FS 后端，ESP-IDF target 显式编入 mmap/unsupported adapter 后端。(done: 2026-06-12)
  - [ ] 文档补充资源目录布局、环境变量、常见调试命令。
  - [ ] [LEGACY] 文档补充 manifest 格式；当前主线不引入 host manifest。
  - [ ] 记录“资源 API 只提供只读字节视图，格式解释仍归 image/font/anim decoder”的设计约束。
