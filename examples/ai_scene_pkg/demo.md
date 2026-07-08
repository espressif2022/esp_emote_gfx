# ai_scene_pkg — u32-offset 场景包 Demo（交接说明）

一个「位置无关二进制场景包」的概念验证：host 侧把场景描述编译成一段
**只含 u32 偏移 / u16 索引、无任何原生指针**的字节包；host（64 位）与设备
（32 位）用**同一份字节**解析一致，加载走「工厂建树」（`gfx_*_create` + setter），
不做 ITU 式指针重定位。图片像素烘焙进包并压缩。

---

## 1. 立身之本（改代码别破坏这两条）

1. **包内永远只有 u32 偏移 / u16 索引，禁止原生指针** —— 保证 32/64 位解析一致，
   无需把 host 降成 32 位、无需冻结任何 widget struct 布局。
2. **loader 必须全程边界校验，坏包只返回错误码、绝不崩** —— 见 `gsp_load.c`
   里对 header/offset/count/parent/blob/crc 的逐项校验。

---

## 2. 当前状态（交接基线）

| 文件 | 职责 |
|---|---|
| `gsp_format.h` | 格式定义 v3：magic/version、header、ObjEntry(64B)、BlobEntry(20B)、codec、小端定宽读写、CRC32、`gsp_bpp`、静态断言 |
| `gsp_pack.c`   | host「编译器」：`gsp_scene_desc_t` → 位置无关字节包（字符串去重 + 图片 blob 压缩 + params 区 + crc 回填） |
| `gsp_load.c`   | host/device 共用 loader：解析+校验+crc → 工厂建树 → blob 解压 → 安全销毁；含 `gsp_dump` |
| `ai_scene_pkg_demo.c` | 硬编码场景（容器/文本/按钮 + logo/photo/自定义星形 shape）→ 打包 → dump → 加载 → 自检 → SDL 渲染 |

已支持：
- 控件：container / label / button / image
- 图片烘焙进包 + 压缩：`STORE`（原样）与 `RLE16`（16bpp 游程；平色/单向渐变很有效，
  否则自动回退 STORE）
- 自定义 shape（路 C）：host 侧光栅化成 RGB565A8 位图（透明背景），走 image blob 链路
- 属性：bg/fg/border/radius、text、callback（按名绑定）、name、hidden、text_align、
  opacity（保留位）、per-widget `params`（扩展点）
- header CRC32 校验

验证：`ctest -R ai_scene_pkg` 通过；headless 自检 `SELF-CHECK: PASS (8 objects)`。

---

## 3. 构建与运行

从仓库根目录 `esp_emote_gfx/`：

```bash
# 配置（首次 / 改过 CMakeLists 才需要）
cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON

# 编译
cmake --build build-host-sdl --target gfx_ai_scene_pkg_demo

# 带窗口运行（SDL 出画）
./build-host-sdl/gfx_ai_scene_pkg_demo

# 无窗口自检（CI / 无显示器）
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy GSP_HEADLESS=1 ./build-host-sdl/gfx_ai_scene_pkg_demo

# 走 ctest（已注册为 headless 用例）
ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure
```

---

## 4. 包格式速览（v3，全小端、定宽、按字节偏移）

```text
[Header 48B][Obj 表 n*64B][Blob 表 b*20B][Params 区][String 区][Blob 数据]
```

Header（48B）：`magic / version / screen_w,h / screen_bg / obj_count /
obj_table_off / str_table_off / blob_count / blob_table_off / total_size /
crc32(整包，计算时本字段视为 0) / reserved`。

ObjEntry（64B）：`type / parent_idx(u16, 0xFFFF=根) / x,y,w,h / flags /
fg,bg,border 色 + border_width + radius / text_off / callback_off / name_off /
blob_idx / params_off + params_len / opacity / text_align / font_id / bind_id`。
先序排列、`parent_idx < 自身索引`。所有 `*_off` 为相对 buffer 起点的**绝对字节偏移**。

BlobEntry（20B）：`w / h / cf / codec / stride / raw_size / comp_size / data_off`。
图片像素烘焙进包，带压缩头（对齐 ITE：存 comp/raw size + codec，加载期解压）。

字段/flag 全集以 `gsp_format.h` 注释为准。

---

## 5. 数据流

```text
打包（host，编译期）：gsp_pack.c
  gsp_pack() → blob_compress()（选 codec）→ rle16_encode()（算法）→ crc32 回填
加载（host/device，加载期）：gsp_load.c
  gsp_load() → 校验(含 crc) → switch(type) 工厂建树 → blob_get()（按 codec 解压）
渲染（沿用引擎既有路径）：
  gfx_core_tick → gfx_render_handler → gfx_render_draw_object_tree(obj->vfunc.draw) → SDL flush
```

---

## 6. 继续推进路线（每步一个闭环 + 验收标准）

按顺序推进；每步做完必须能独立验证。

### S1 拆成两个可执行 + 落盘文件 I/O（最优先）
- 现状 pack/load 在同进程内存传 buffer。改为 `gsp_packc scene → out.gsp`（写文件），
  demo 读 `out.gsp` 再渲染。
- 验收：`out.gsp` 可复现（两次打包 byte 相同）；源码删掉硬编码场景也能从文件跑起来。

### S2 坏包鲁棒性测试（安全地基，紧跟 S1）
- ctest：对 `out.gsp` 逐字节翻转/截断（magic/version/crc/各 offset/count），
  断言返回对应 `GSP_ERR_*` 且进程不崩。
- 验收：所有变异用例正常返回错误；crc 变异必被 `GSP_ERR_CRC` 抓到。

### S3 JSON → scene 描述（作者格式）
- 加 host 侧 `scene.json → gsp_scene_desc_t` 编译器（接 cJSON）。
- 验收：手写 `scene.json` 打包出的 `gsp_dump` == 当前硬编码版本（golden 对比）。

### S4 工厂注册表替换 switch
- `gsp_load.c` 的 `switch(type)` 换成 `type → {create, apply}` 注册表。
- 验收：新增控件类型只加一条注册项、不碰 loader 内核；旧用例全过。

### S5 更多控件 + params 驱动私有属性
- 用 `params` 块接 progress_bar / list / arc 等（每控件私有参数走 params，各自解析）。
- 验收：每加一个控件，dump 可见、渲染正确、自检加一条。

### S6 上设备（ESP 目标）
- `.gsp` 放进 flash 分区 / assets，设备端 `gsp_load` 渲染真机。
- 验收：同一份 `.gsp`，真机画面 == host 预览。

### S7 压缩增强
- RLE 扩到 RGB565A8/planar（对透明 shape 立竿见影），或接通用 LZ
  （heatshrink / UCL nrv2e），照片走 JPEG。
- 验收：dump 压缩比下降，且解压后像素与源 100% 一致（round-trip）。

### S8 live 预览闭环
- watch `scene.json` mtime → 自动重打包 → `gsp_scene_free` + `gsp_load` 重载。
- 验收：改 json 文本 → 屏幕即刷新；连续重载无泄漏。

### 推进顺序

```text
S1 落盘 → S2 坏包测试            （地基：可复现 + 不崩）
  → S3 JSON 编译器 → S4 注册表    （数据驱动 + 可扩展）
  → S5 更多控件   → S6 上设备     （功能面 + 真机闭环）
  → S7 压缩 / S8 live            （增强，随时可插）
```

---

## 7. 自定义 shape 的三条路（现选路 C）

- **路 C（已实现）**：host 光栅化成图片 blob。零引擎改动、任意复杂形状；代价是像素固定、
  运行期不能改色/缩放。见 `ai_scene_pkg_demo.c` 的 `build_shape()`。
- **路 B**：引擎加一个通用 `shape` 控件，几何走 `params` 里的 display-list（矢量指令），
  一个通用 draw 解释。数据小、可动态；需改 `src/`。
- **路 A**：开放自定义 widget class 注册 API + 专用控件。改动最大，适合可复用复杂组件。

格式侧对三条路都已就绪（`type` + `params` 块 + `blob`）。
