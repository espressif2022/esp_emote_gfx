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
  - [ ] 评估是否公开 `gfx_display_get_render_format()` 调试 API；当前测试先通过 private header 断言内部状态，正式 public API 暂不暴露。
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
- [ ] Surface / decoded resource cache。
  - [ ] 定义 decoded cache entry：source key、format、width/height/stride、decoded bytes、refcount、last_use。
  - [ ] 增加 cache budget 配置，默认保守，避免图片/动画解压后隐藏占用过多内存。
  - [ ] 支持按需 decode、pin、release 和 LRU eviction。
  - [ ] Host SDL 暴露 cache stats；ESP 端保留可关闭/小预算模式。
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
