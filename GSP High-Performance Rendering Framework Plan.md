# GSP 高性能预处理 UI 渲染框架计划书

> 状态：架构设计草案  
> 日期：2026-07-10  
> 适用范围：静态页面、数据仪表盘、动态状态、受控动态实例、虚拟列表、页面切换和触摸交互  
> 设计输入：`graphics_scene_package` GSP1 v4 JSON/GSP1  
> 参考实现：`esp_emote_gfx`、`esp_display_present`、`esp_lvgl_adapter`  
> 关联文档：`GSP Repository and Naming Design.md`（仓库结构与命名体系）

## 1. 结论

新框架不应复制 LVGL 的运行时对象树，也不应直接在现有 ARN1
`first_child/next_sibling` 模型上继续堆叠功能。要在目标场景中稳定优于 LVGL，
核心手段必须是：

1. 上位机完成布局、层级展开、裁剪边界、资源格式选择和静态内容烘焙。
2. 设备端执行连续、只读、可 XIP/mmap 的渲染命令流，不创建每控件堆对象。
3. 将可变状态从静态场景中分离，属性更新通过 `bind_id` 直接定位状态槽。
4. 上位机生成 dirty tile/命令索引，设备不扫描完整对象树。
5. 色深和资源格式按目标 SoC、LCD 接口和硬件加速能力编译，避免运行时通用转换。
6. Renderer 与 Present 完全解耦，直接使用 `esp_display_present` 管理 framebuffer、
   TE、VSYNC、pipeline 和完成事件。
7. DMA2D/PPA 作为带能力探测和 fence 的异步执行后端，软件实现始终作为回退。

新框架只有一套渲染机制：compiled command + instance pool。运行时动态创建控件
是一等需求，但不通过第二套 object 树实现——框架内置一份 gspc 预编译的标准控件
模板包（label/image/rect/button 等），runtime create API 等价于"实例化内置模板 +
typed state"，与业务自定义模板走完全相同的执行路径。设备端不内置
`esp_emote_gfx` object 兼容层，`esp_emote_gfx` 原仓库冻结为维护状态，继续服务
存量 emote 产品；新框架以源码移植方式复用其软件绘制 primitive 和 EAF/AAF
动画解码核心，不以组件形式依赖。

“优于 LVGL”仅针对本文定义的预处理 UI 场景，不宣称在复杂滚动列表、通用控件、
运行时布局和大型动画生态上全面优于 LVGL。

预处理不等于只能显示静态 UI。框架必须同时支持：

- 编译场景的动态属性更新；
- 基于编译模板的运行时实例创建/销毁；
- 数据驱动 repeater/虚拟列表；
- popup、toast、光标等临时 overlay；
- 波形、图表等运行时生成几何（bounded overlay draw-list，不是对象树）；
- 业务状态机和页面导航。

---

## 2. 现状与参考组件取舍

### 2.1 `graphics_scene_package`

保留：

- GSP1 v4 作为设计/交换格式。
- JSON schema、校验、web editor、bundle 和资源打包工具。
- `gsp_res.h` 已定义的 RGB565、ARGB8888、A8、glyph index 资源能力。
- `gsp_bundle.h` 的多场景索引、CRC 和对齐原则。

需要替换或扩展：

- ARN1 只支持 `first_child/next_sibling`，不作为最终高性能 ABI。
- ARN1 图片内嵌解压后的 RGB565，无法跨场景共享和 mmap。
- `bind_id` 只转换成动态文本 flag，没有生成正式的 bind 索引。
- 字体默认运行时路径与可选 atlas 路径没有收敛为量产唯一方案。
- 上位机尚未根据目标硬件生成颜色、对齐和加速 profile。

### 2.2 `esp_emote_gfx`

建议复用：

- 半开区间 area/clip 语义。
- dirty area 收集、合并和超限退化策略。
- draw context：buffer、stride、buffer area、clip area、format。
- 软件 fill、blit、blend、mask 等绘制图元。
- backend capability 和软件回退思想。
- 输入设备、触摸坐标转换和基础事件模型。
- 性能统计框架。

不迁移进新框架（留在 `esp_emote_gfx` 原仓库，冻结维护）：

- `gfx_object_t` 运行时对象树和 `gfx_object_child_t` 单链表。
- 全部 widgets（`src/widgets/` 约 14k 行：label/image/button/list/
  coverflow/pageflow/wheel/anim 等）。
- 每对象虚函数 draw/update。
- ARN1 递归 `first_child/next_sibling` 遍历。
- 每帧逐 glyph 转换 alpha bitmap。
- Renderer 内部同步等待每个 flush 完成。
- `esp_lcd_flush_bridge` 和自有 framebuffer pipeline（新框架直接使用
  `esp_display_present`，不经过该桥接）。

需要抽取移植（源码级 copy-adapt，不做组件依赖）：

- 软件 fill/blit/blend/mask primitive。
- dirty area 收集、合并和超限退化策略。
- `esp_idf_accel.c` 中可复用的 PPA format/capability 判断，抽成独立
  accelerator backend，不与 object renderer 绑定。

### 2.3 `esp_display_present`

直接依赖并复用：

- panel interface 和七种 tear mode。
- framebuffer 数量计算与 panel framebuffer 获取。
- double/triple pipeline。
- frame state、pending frame 和 ISR retirement。
- RGB/MIPI framebuffer present。
- SPI/QSPI/I80 的 TE 同步。
- dirty/unrendered area copy。
- cache msync、DMA 对齐和 area 对齐。
- rotation、PPA/DMA2D helper 和共享硬件资源生命周期。
- managed/external callback ownership。

`esp_display_present` 只负责提交和 buffer 生命周期，不存储 GSP 场景，不执行
渲染命令。

### 2.4 `esp_lvgl_adapter`

只参考其成熟工程模式：

- display profile 与接口相关默认配置。
- display/input manager 生命周期。
- framework bridge 与 present 分层。
- triple partial 的 framebuffer patch、unrendered copy 和 pipeline 发布顺序。
- ISR 中只更新状态、释放资源和通知任务。
- render completion、transfer completion、framebuffer completion 三者分离。

不依赖 LVGL API，不复制 LVGL object、style、layout、timer 和 invalidate 系统。

### 2.5 `esp_emote_gfx` 已知问题与新框架处置

#### Object 关系和布局

当前代码已经提供 `gfx_object_add_child(parent, child)`、same-display 校验和简单 cycle
检查，因此“完全不能挂到任意 parent”并非当前实现的准确状态。但底层仍是
`gfx_object_child_t` 单链节点，align target 又形成独立依赖图；位置解析和 dependent
通知依赖递归，并以 depth>8 强制停止。这只能避免无限递归继续运行，不能从模型上证明
布局依赖无环。

新框架处置：

- compiled scene 在 host 端展开 hierarchy；
- 动态 template instance 使用数组 parent index；
- parent/align/layout 依赖在 transaction commit 时做拓扑排序和显式环检测；
- world transform/bbox 缓存一次，render 不递归解算；
- parent 约束、clip 和 content offset 与 flat z-order rendering 分离；
- 自适应排布首期只实现可证明有界的 grid/stack/repeater recipe。

#### 多颜色格式和灰度

当前 `gfx_color_t` 是 semantic RGB565。RGB888/XRGB8888 等路径后续扩展后，部分颜色
仍从 RGB565 语义转换，既丢失色彩精度，也容易让原有 RGB565 快路径经过通用
format branch/conversion。

新框架处置：

- Host IR 统一保存 RGBA8888 semantic color，不从 RGB565 反推 888；
- target compiler 将常量颜色提前量化为 native command payload；
- RGB565 profile 使用独立 16-bit kernel，不经过 888 通用实现；
- RGB888/ARGB8888 profile 使用独立 kernel，保留原始精度；
- 增加 L8/A8 和单色 I1 profile；I2/I4 仅在明确面板需求后加入；
- render/resource/output format 使用显式转换矩阵，未实现组合在编译期报错；
- SW/PPA/DMA2D 路径必须具有格式一致性测试和性能回归。

#### Image decoder/cache

当前已有 C-array/memory/file source 和 JPEG decoder 雏形，但缺少统一的多格式 cache
契约；PNG/QOI 等不是现有正式路径。

新框架处置：

- decoder plugin 采用 `probe/open/decode/decode_region/close` 能力接口；
- JPEG、PNG、QOI 等属于 codec plugin，不进入 renderer；
- EAF/AAF 动画容器作为 anim codec plugin 接入：移植 `gfx_eaf_dec` 流式解码
  核心（reader 抽象、帧表索引、4/8/24-bit 色深），逐帧输出 native surface，
  帧推进通过 bind resource 更新，不在 render task 内解码；
- 原生 mmap 图片绕过 decoder；
- decode 输出统一 native surface 和 ownership/fence；
- 建立有字节预算的 decoded-image LRU cache 和预取接口；
- decoder/cache fault injection、cache hit/miss、OOM 和损坏输入均有测试接口。

#### 字体对 LVGL 的依赖

当前 font adapter 可消费 LVGL bitmap 格式，因此完整功能路径可能引入 LVGL 仓库。
新框架主路径改为自有 GFB font pack（GSP Font Binary，magic `GFB1`，
扩展名 `.gfb`）：

- `font.py` 输出 A8 atlas、glyph metrics 和 glyph-run 数据；
- device 仅解析 GFB font pack，不包含 LVGL header/decoder；
- 静态 shaping 在 host 完成；
- FreeType/LVGL font adapter 只作为 compatibility plugin；
- 编译和链接测试确保关闭 compatibility 后不存在 LVGL 符号依赖。

#### 描边和填充

描边、填充是 renderer 正确性的基础，不能直接继承现有实现并假设正确。当前矩形
stroke 以逐层四条线实现，round-rect fill/stroke 又有独立 AA 路径；不同格式、clip、
小尺寸、大线宽和透明度组合缺少完整的统一规范。

新框架处置：

- 先冻结像素覆盖规则，再实现优化；
- 建立独立软件 reference rasterizer 作为正确性 oracle；
- 矩形、圆角矩形、line、arc/path 使用统一 half-open 坐标和 coverage；
- 明确定义 stroke inside/center/outside、radius clamp、join/cap 和 premultiplied alpha；
- round-rect stroke 按 outer coverage 减 inner coverage，不通过覆盖背景色伪造空心；
- 大面积 interior 可交给 PPA fill，AA edge/corner 由 reference-compatible mask 处理；
- 所有硬件 fast path 与 reference 输出做逐像素或允许误差范围对比。

#### 测试接口

新框架从 Phase 0 就提供 public test hooks，不在功能完成后补测试：

- memory backend 和 deterministic frame clock；
- command trace、dirty/tile dump、state/instance dump；
- allocator/decoder/accelerator/present fault injection；
- 强制选择 SW/PPA/DMA2D 路径；
- framebuffer snapshot/CRC/golden image；
- frame/render/transfer/present fence 注入；
- operation pixels/bytes/time 和 cache hit/miss 统计；
- ABI parser fuzz 和边界向量。

---

## 3. 总体架构

```text
┌──────────────────────────── Host ────────────────────────────┐
│ JSON / GSP1                                                  │
│    │                                                         │
│    ▼                                                         │
│ GSP Compiler                                                 │
│    ├─ layout flatten / absolute bounds                       │
│    ├─ z-order command generation                             │
│    ├─ static/dynamic split                                   │
│    ├─ dirty tile + hit-test index                            │
│    ├─ target color/asset specialization                      │
│    └─ font atlas / optional static tile bake                 │
│          │                 │                  │               │
│          ▼                 ▼                  ▼               │
│       GSB scene       GRB resources     manifest           │
└──────────┬─────────────────┬──────────────────────────────────┘
           │ mmap/XIP        │ mmap/DMA aligned
┌──────────▼─────────────────▼────────── Device ────────────────┐
│ GSP Runtime                                                  │
│    ├─ immutable command stream                               │
│    ├─ mutable state slots                                    │
│    ├─ bind/action/hit index                                  │
│    └─ dirty region + dirty tile resolver                     │
│                        │                                     │
│                        ▼                                     │
│ Renderer                                                     │
│    ├─ command replay / batching                              │
│    ├─ software executor                                      │
│    ├─ PPA executor                                           │
│    └─ DMA2D executor                                         │
│                        │ render fence                        │
│                        ▼                                     │
│ GSP Present Adapter                                          │
│    └─ esp_display_present → esp_lcd → LCD                    │
└──────────────────────────────────────────────────────────────┘
```

逻辑模块：

- `gsp_runtime`：场景、状态、bind、action、dirty、hit test。
- `gsp_renderer`：命令解析、裁剪、批处理和绘制。
- `gsp_accel`：SW/PPA/DMA2D 能力和任务提交。
- `gsp_present_adapter`：将 renderer frame/dirty 转换为
  `esp_display_present` 操作。
- `gspc`：上位机编译器，生成设备直接执行的数据，不参与设备运行。

---

## 4. 上位机编译架构

### 4.1 编译阶段

```text
GSP1 parse
→ schema/type validation
→ layout resolve
→ absolute geometry
→ visibility/clip propagation
→ draw command lowering
→ static/dynamic analysis
→ resource/font lowering
→ target profile specialization
→ tile/hit/bind index
→ binary packing + CRC
```

设备端不再执行：

- parent 坐标累加；
- flex/约束布局；
- 名称查找；
- 图片格式转换；
- TTF 光栅化；
- 静态对象树创建；
- action 字符串解析。

### 4.2 Target profile

新增 target profile 输入，至少包含：

```yaml
name: esp_rgb565_ppa
screen:
  width: 800
  height: 480
present:
  interface: rgb
  output_format: rgb565
  tear_mode: triple_partial
accelerator:
  ppa: true
  dma2d: true
  supported_formats: [rgb565, rgb888, argb8888]
memory:
  resource_alignment: 64
  stride_alignment: 64
renderer:
  tile_width: 32
  tile_height: 32
```

具体 alignment 不能永久硬编码。BSP/目标工程应根据
`esp_display_present_dma_ext_mem_alignment()` 和 SoC 能力生成 profile，
gspc 只消费该 profile。

### 4.3 静态/动态分析

命令分为：

- `STATIC`：运行期间属性不变化，可直接执行或预烘焙。
- `BOUND`：由 bind state 控制文本、颜色、可见性、值或资源。
- `INTERACTIVE`：拥有 hit/action 状态。
- `ANIMATED`：由轻量 timeline 更新状态；首期可不实现。

静态烘焙粒度优先采用 tile/layer，不默认生成整页未压缩 snapshot。整页
snapshot 只用于滑动页面窗口等明确场景。

### 4.4 编译期优化

- 相邻同色矩形在不改变 z-order 的条件下合并。
- 完全被后续不透明命令覆盖的静态命令删除。
- 相同图片和字符串跨场景去重。
- 不透明图片转换为目标原生格式。
- 透明资源按能力选择 ARGB8888 或 color+A8。
- 固定文本写入静态 tile；动态字符生成最小 glyph atlas。
- 预计算每条命令的绝对 bbox 和 clip。
- 对每个 tile 生成按 z-order 排序的 command range/list。
- 为 bind 和 action 生成直接索引，不保留热路径字符串查找。

---

## 5. GSB 设备运行格式

场景二进制命名为 GSB（GSP Scene Binary），magic `GSB1`，扩展名 `.gsb`；
字段级布局见《GSB Binary Format Specification》（草案 v0.9，结构大小在
原型 benchmark 后冻结）。GSP1 继续作为作者格式，ARN1 仅作为
`esp_emote_gfx` 兼容格式。命名体系详见《GSP Repository and Naming Design》。

### 5.1 Section 设计

```text
[header + CRC]
[scene metadata]
[fixed-size command array]
[default mutable-state array]
[bind index]
[action index]
[hit-test index]
[tile → command index]
[resource reference table]
[optional strings/debug names]
```

### 5.2 连续命令流

命令按最终绘制顺序连续存储，不使用 parent/child/sibling 指针。每条命令至少包含：

- opcode；
- flags；
- 绝对 bbox；
- clip/layer 信息；
- state slot；
- resource/font ID；
- opcode 参数。

首期 opcode：

- `FILL_RECT`
- `FILL_ROUND_RECT`
- `BLIT_OPAQUE`
- `BLIT_ALPHA`
- `DRAW_GLYPH_RUN`
- `DRAW_STATIC_TILE`
- `CLIP_PUSH` / `CLIP_POP`（仅确有需要时）

复杂控件在上位机展开为基础命令，不在设备端保留“button/list/progress”语义。

### 5.3 静态数据与可变状态分离

命令和资源引用保持只读、XIP/mmap。RAM 中只保存状态槽，例如：

```c
typedef struct {
    uint32_t value;
    uint32_t color;
    uint32_t resource_id;
    uint16_t text_offset;
    uint16_t flags;
} gsp_state_slot_t;
```

以上只是概念结构，不应在性能原型前冻结布局。

属性更新流程：

```text
bind_id
→ bind index
→ state slot
→ 保存旧 bbox
→ 更新状态
→ 计算新 bbox（需要时）
→ old/new bbox 加入 dirty
```

### 5.4 Tile 索引

上位机将屏幕划分为可配置 tile。每个 tile 保存与其相交、按 z-order 排序的命令
索引或连续 range。

渲染 dirty 时：

```text
dirty merge
→ dirty 覆盖 tiles
→ 合并候选命令
→ generation stamp 去重
→ 按 z-order replay
```

小场景可以通过 header flag 关闭 tile index，直接顺序扫描命令，避免索引大于收益。
是否启用由 gspc 根据节点数、重叠率和目标 profile 决定。

---

## 6. Renderer

### 6.1 Frame API

建议使用显式 frame 生命周期：

```c
gsp_frame_t *gsp_renderer_begin(gsp_renderer_t *renderer,
                                const gsp_dirty_list_t *dirty);

gsp_err_t gsp_renderer_execute(gsp_frame_t *frame,
                               const gsp_scene_t *scene);

gsp_fence_t gsp_renderer_end(gsp_frame_t *frame);
```

`end()` 表示渲染任务已提交，不等价于物理显示完成。

### 6.2 执行过程

1. 根据 dirty 和 present mode 获取 full/partial surface。
2. 查询 tile index，得到命令候选。
3. 进行 bbox/clip 精确过滤。
4. 将连续兼容命令组成 batch。
5. 通过 accelerator capability/cost model 选择 SW、PPA 或 DMA2D。
6. 等待 render fence，执行 cache clean。
7. 将 frame 提交给 present adapter。

### 6.3 不透明覆盖

上位机和 runtime 共同处理：

- 编译期删除永久被覆盖的静态命令。
- runtime 对 dirty tile 从后向前寻找完全不透明覆盖点。
- 找到覆盖点后不重放更早的背景命令。

该优化对卡片、网格按钮和不透明页面收益可能较大，但必须用 golden image 验证。

### 6.4 Primitive contract

所有 executor 共用一份 primitive contract：

- 区域统一为 half-open `[x1, x2) × [y1, y2)`；
- clip 在 coverage 计算前定义，不能由各 backend 自行猜测 inclusive/exclusive；
- opacity、A8 mask 和 source alpha 的组合顺序固定；
- alpha blend 使用明确的 straight/premultiplied 输入约定和整数舍入规则；
- stroke 明确 inside/center/outside、line cap、line join 和 miter limit；
- radius 必须 clamp 到几何尺寸允许范围；
- dirty bbox 包含 AA 和 outside stroke 扩张像素。

实现顺序：

1. 在 host/memory backend 完成低速、确定性的 reference rasterizer；
2. 建立 rect/round-rect/line/path 的参数化 golden vectors；
3. 软件快路径按 scanline/span/coverage mask 优化；
4. PPA/DMA2D 只接管满足 capability 的 interior/fill/blit；
5. 混合路径对同一 primitive 共享 coverage，不允许视觉语义分叉。

重点边界向量包括 0/1px 尺寸、radius=0/最大/越界、stroke 大于半尺寸、负坐标、
跨 clip 边缘、opacity 0/1/254/255，以及 RGB565/RGB888/ARGB8888/L8 output。

---

## 7. 色深与颜色格式

### 7.1 原则

- render format、resource format、output format 分开描述。
- 每个目标 profile 选择一条主路径，避免帧内通用逐像素转换。
- byte-swapped/BGR 等不利于 PPA 的格式不进入内部热路径；只在必要边界转换。
- 不为少量透明资源将整个 framebuffer 升级为 ARGB8888。
- packed RGB888 与 XRGB8888 的选择必须按 SoC/PPA/带宽实测，不凭字节数决定。

### 7.2 推荐 profile

RGB565 HMI：

```text
framebuffer: RGB565
opaque image: RGB565
transparent icon: RGB565 + A8 或预合成
font: A8 atlas
present: RGB565
```

RGB888 输出：

```text
候选 A: RGB565 render → PPA/DMA2D convert → RGB888 present
候选 B: RGB888 render → RGB888 present
候选 C: XRGB8888 render → hardware convert/present
```

三种候选必须比较：

- full/dirty render 时间；
- format conversion 时间；
- PSRAM bytes；
- cache miss；
- PPA/DMA2D 支持程度；
- 最终颜色误差。

### 7.3 资源格式

GRB（GSP Resource Binary，magic `GRB1`，扩展名 `.grb`）至少支持：

- RGB565；
- RGB888；
- ARGB8888；
- A8；
- RGB565+A8 双平面；
- glyph index；
- 可选静态 tile 压缩。

资源 entry 记录 stride、alignment、format、compression 和原始尺寸。Renderer 不从文件
名或控件类型猜测格式。

### 7.4 图片最优路径

图片不能统一采用一种存储格式，也不能在 render task 中临时解码。gspc 根据用途和
target profile 为每张图片生成确定的运行策略。

#### 图片分类

高频 UI 图标：

- 上位机转换为 framebuffer/PPA 原生格式；
- 存入 mmap 资源分区；
- payload、每行 stride 和起始地址满足 DMA/PPA alignment；
- 设备直接以 resource view blit，不复制到 scene RAM；
- 跨场景按内容 hash 去重。

透明图标：

- 背景固定时优先在上位机预合成；
- RGB565 目标可使用 RGB565+A8 双平面，由专用 SW/SIMD mask 路径处理；
- RGB888 目标可在硬件支持时使用 ARGB8888 blend；
- 不允许因为少量透明图标将整个 framebuffer 改为 ARGB8888；
- 当前 PPA helper 不代表支持任意 A8/per-pixel-alpha 组合，必须通过 capability
  matrix 选路。

大背景和照片：

- 静态背景优先在上位机缩放、裁剪并转换为目标原生格式或 baked tile；
- Flash 容量敏感时可保留 JPEG 等压缩格式；
- JPEG 只允许在页面预加载或后台 decode task 解码，不在帧内重复解码；
- 有硬件 JPEG decoder 时输出目标 framebuffer 原生格式；
- 无硬件 decoder 时使用软件/流式解码，并缓存最终像素；
- PNG 不作为高频 runtime 热路径格式，透明 PNG 应在上位机转为目标资源格式。

动态/远程图片：

- decoder 与 renderer 解耦；
- decode task 输出带 format、stride、ownership 和 fence 的 image surface；
- 使用有字节预算的 LRU cache；
- render task 只能消费 READY surface，不能等待网络、文件系统或长时间解码；
- cache miss 时显示预编译 placeholder，完成后标记对应 bbox dirty。

Emote 动画（EAF/AAF）：

- 保留 Flash 中的 EAF/AAF 容器格式，不要求上位机重打包；
- decode task 按帧表流式解码，输出转换为 target profile native surface；
- 帧 surface 双缓冲：解码下一帧时 render 消费当前帧；
- 帧推进 = 一次 bind resource 更新 + 对应 bbox dirty，走 Level 1 状态路径；
- 播放节拍由 timer/VSYNC 驱动，解码跟不上时丢帧而不阻塞 render；
- 4/8-bit 调色板帧在解码时展开为 native format，不在 blit 路径做逐像素查表。

#### 图片资源描述

GRB image entry 至少包含：

```text
resource_id
codec
pixel_format
width / height
stride
data_offset / data_size
alignment
alpha_mode
cache_policy
content_hash
```

`codec` 与 `pixel_format` 分离。例如 JPEG 是存储 codec，decode 后可以得到 RGB565
或 RGB888 surface。

#### 图片绘制 fast path

```text
resource lookup O(1)
→ 检查 decoded/native surface
→ clip source/destination
→ capability + area threshold
→ DMA2D copy / PPA blit / PPA blend / SW fallback
→ 返回 render fence
```

禁止：

- 每帧打开和关闭 decoder；
- 每帧 malloc 解码缓冲；
- 将图片复制进每个 scene package；
- 已是目标格式仍经过通用像素转换；
- 在全屏拖拽过程中解码页面图片。

### 7.5 字体最优路径

量产主路径不在设备上运行 FreeType。`font.py` 已具备 TTF→A8 atlas+glyph index
能力，目标框架应将其升级为唯一高性能字体管线。

#### 静态文本

- 上位机完成字体 fallback、glyph 选择、kerning/shaping 和定位；
- GSB 保存 glyph ID/位置组成的 glyph run，而不是 UTF-8 文本；
- 完全静态文本可直接进入 baked tile；
- 设备不测量字符串宽度，不执行 Unicode shaping。

复杂文字 shaping 的具体 host 库属于实现选择，当前尚未确认；无论使用何种库，
host preview 与设备必须消费同一 glyph run 结果。

#### 动态数字和短文本

- 数字、单位、时间、温度和状态字符生成独立小 atlas；
- 常用 ASCII 可使用直接索引，避免逐 glyph 二分查找；
- 文本改变时只重新生成 glyph run，未改变时复用 cached run；
- state slot 保存 glyph run handle 和 bbox；
- 更新时同时 dirty 旧 bbox 与新 bbox。

#### 多语言与 CJK

- 按 locale 和字号生成独立 font pack；
- 优先收集场景静态字符和业务允许的动态字符集；
- 大字符集按 Unicode range/page 分片，按需 mmap，不复制整个 atlas；
- locale 切换时切换 font resource set；
- 缺字使用确定的 fallback glyph，并在 gspc manifest 中报告；
- 任意动态 Unicode/任意字号如必须支持，可保留 FreeType compatibility path，
  但该路径不参与“优于 LVGL”的性能承诺。

#### Atlas 和 mask

- Flash 中可使用压缩 A4/A8 page，但进入热路径前应准备为 renderer 原生 mask；
- 热路径优先直接消费 A8 atlas，避免逐 glyph 重新展开 alpha；
- glyph metadata 和 atlas page 使用独立 cache；
- glyph run 按 clip 过滤后调用专用 mask blender；
- 当前 PPA 对 A8 mask 支持有限，首选优化 SW/SIMD mask；静态文字通过 bake
  避开每帧 mask blend；
- 文本参与页面拖拽前必须已经合成到 page snapshot。

#### 字体一致性

必须锁定：

- ascender、descender、line height；
- bearing 和 advance；
- rounding 规则；
- baseline；
- UTF-8/codepoint 处理；
- host 与 device 的像素输出。

使用 glyph-run golden data 和 framebuffer golden image 双重验证，避免只比较视觉近似。

---

## 8. DMA2D/PPA 加速架构

### 8.1 独立 accelerator 层

不要在每个 opcode 中直接写 `#if SOC_*`。统一接口：

```c
typedef struct {
    uint32_t caps;
    gsp_err_t (*submit_fill)(...);
    gsp_err_t (*submit_blit)(...);
    gsp_err_t (*submit_blend)(...);
    gsp_err_t (*submit_copy)(...);
    gsp_err_t (*submit_convert)(...);
    gsp_err_t (*submit_transform)(...);
    gsp_fence_t (*flush)(...);
} gsp_accel_ops_t;
```

后端：

- `gsp_accel_sw`
- `gsp_accel_ppa`
- `gsp_accel_dma2d`
- 可选 `gsp_accel_simd`

### 8.2 能力矩阵

初始化时记录：

- 支持的 src/dst format 组合；
- fill、copy、blend、scale、rotation、format conversion；
- 固定 alpha/per-pixel alpha；
- 最大 pending transaction；
- 地址、stride、width/height alignment；
- cache maintenance 要求；
- 是否支持异步完成。

当前参考代码已经表明 PPA 对 RGB565、RGB888、ARGB8888 的部分 fill/SRM/blend
组合可用，但 swapped、BGR、XRGB8888、A8 plane 等路径存在限制。框架不得假定
所有 format 组合都可由 PPA 执行。

### 8.3 Cost model

硬件加速不是面积越小越好。每次操作按以下条件选路：

```text
format supported
AND alignment valid
AND pixels >= operation_threshold
AND accelerator queue available
AND no unresolved dst hazard
→ hardware
ELSE
→ software
```

threshold 按 SoC 和操作类型由 benchmark 生成，不硬编码一个全局值。

### 8.4 PPA 使用范围

优先：

- 大矩形 fill；
- RGB565/RGB888/ARGB8888 图片 blit；
- 支持格式下的 alpha blend；
- scale/rotate/mirror；
- framebuffer format conversion（目标支持时）。

保留软件路径：

- A8 glyph mask；
- 很小的矩形；
- 不支持的 swapped/BGR/双平面格式；
- 复杂圆角边缘；
- PPA queue 繁忙时的低成本操作。

### 8.5 DMA2D 使用范围

优先：

- framebuffer/区域 copy；
- triple-partial 的 unrendered area preservation；
- static tile copy；
- 大块 stride copy；
- 目标 API 支持时的格式转换。

DMA2D 和 PPA 可能共享硬件或内存带宽，必须由统一 scheduler 管理，不能由 renderer
和 present 各自无协调地提交。

### 8.6 异步与 fence

现有 `esp_emote_gfx` PPA 路径以 blocking operation 为主。新框架分两阶段接入：

1. blocking PPA/DMA2D，先验证格式和图像正确性；
2. async queue + fence，与 CPU 命令解析和 LCD present 流水化。

同一 destination surface 的 z-order 操作默认串行。只有不相交区域或明确无依赖的
操作才允许并行。Present 前必须等待所有写 framebuffer 的 render fence 完成。

### 8.7 Cache 与 buffer ownership

每个 buffer 维护状态：

```text
CPU_DIRTY
ACCEL_READING
ACCEL_WRITING
READY_TO_PRESENT
PRESENT_PENDING
DISPLAY_SCANNING
FREE
```

状态转换时集中执行 cache clean/invalidate。禁止每个 draw primitive 对整帧
`cache_msync`。对齐和 window 检查复用 `esp_display_present_dma_*` API。

共享 PPA/DMA2D 资源优先通过 `esp_display_present_hw_resource_acquire/release`
协调；若 renderer 需要 present 未提供的 operation，应增加独立 accelerator client，
但生命周期仍由统一硬件资源管理器控制。

---

## 9. Present Adapter

### 9.1 Frame 生命周期

```text
acquire draw buffer
→ render commands
→ wait render fence
→ cache clean
→ present_async(frame_id)
→ ISR frame/transfer completion
→ retire buffer
→ notify render task
```

必须暴露三种完成语义：

- `render_fence`：像素已写完；
- `transfer_fence`：总线不再读取 source；
- `present_fence`：该 frame 已完成 framebuffer switch/显示事件。

同步截图和端到端 benchmark 等待 `present_fence`；正常渲染只排队并继续处理下一批
状态更新。

### 9.2 接口策略

RGB/MIPI-DSI：

- 默认评估 `TRIPLE_PARTIAL`；
- full-screen 动画可评估 `TRIPLE_FULL`/direct；
- dirty patch 后使用 present pipeline 发布；
- unrendered copy 优先 DMA2D；
- 不在 renderer 中重复实现 framebuffer 状态机。

SPI/QSPI/I80：

- 按 dirty area 提交；
- 支持时使用 TE_SYNC；
- 多 dirty 区按总线事务成本合并；
- source buffer 直到 transfer completion 前不可复用。

### 9.3 Render task

单一高优先级 render task：

- 等待 state-update queue、frame available 和 VSYNC/TE 事件；
- 合并同一周期内的 bind updates；
- 获取 frame 后生成 dirty；
- 提交 renderer 和 present；
- 不 busy-wait。

应用任务只发送状态更新，不直接写 command/state 内存。触摸任务发送 action event，
action 回调在应用上下文执行，避免在 ISR/render 热路径运行业务逻辑。

---

## 10. 全屏拖拽 Fast Path

全屏水平拖拽不能沿用普通 dirty command replay。拖拽时几乎每个像素都变化，如果
每帧重新执行两个页面的命令流，节点结构优化带来的优势会被全屏绘制和总线带宽抵消。

### 10.1 核心策略

```text
拖拽开始前：
  current/previous/next page → native-format full snapshot

每个拖拽帧：
  获取最新 offset
  → 丢弃未提交的旧 offset
  → 从 current snapshot copy strip A
  → 从 neighbor snapshot copy strip B
  → render fence
  → present_async

拖拽结束：
  settle animation
  → 切换 live scene
  → 异步预取新的 neighbor
```

拖拽帧只允许执行两次矩形 copy 和必要的边缘填充，不允许：

- 重放页面命令树；
- 重新绘制文字；
- 解码图片；
- 生成页面 snapshot；
- 等待业务任务；
- 为每个触摸采样排队一帧。

### 10.2 Snapshot window

- 默认维护 `previous/current/next` 三槽滑动窗口；
- snapshot format 与最终 compose framebuffer format 相同；
- 页面动态文本、图片和状态在进入拖拽前合成完成；
- 当前页面内容变化时只标记 snapshot stale，不在手指移动的帧内重建；
- stale snapshot 在 idle、手指按下后的准备窗口或 settle 后异步刷新；
- 内存不足时降为 current+neighbor 两槽；
- 更低内存目标使用 strip/tile cache，但不得在每帧重新解码整页 RLE。

压缩 snapshot 只用于 Flash/换页预取。参与拖拽的 resident snapshot 必须已经解码为
native pixel surface。

### 10.3 RGB 与 MIPI-DSI Video

GRAM 位于 ESP framebuffer 时：

- 使用 double/triple framebuffer，优先评估 triple pipeline；
- draw framebuffer 由两次 DMA2D region copy 合成；
- DMA2D 不可用时使用 PPA SRM copy；两者都不可用时才使用优化 memcpy；
- 两个 strip 不重叠，可组成同一 frame 的顺序 accelerator jobs；
- present 前只等待本 frame 的 compose fence；
- framebuffer 在 VSYNC/frame completion 前不归还；
- 不执行普通 triple-partial 的“复制全部 unrendered area”，因为拖拽帧本身覆盖全屏。

若硬件支持直接 layer/offset compose，可增加专用 present backend；这不是
`esp_display_present` 当前通用能力，必须作为目标特性探测，不能成为基础依赖。

### 10.4 SPI/QSPI/I80 与带 GRAM 的面板

全屏水平拖拽每帧原则上需要传输全屏像素，性能上限由总线带宽和 TE 窗口决定。
框架只能保证采用最优提交路径，不能承诺所有低带宽 SPI 面板达到 60 FPS。

最佳通用路径：

- current/neighbor snapshot 保持 native panel format；
- DMA2D/PPA 将两个 source strip 合成为双 line/stripe DMA buffer；
- compose 与 panel transfer 使用 ping-pong stripe 流水线；
- 同一帧只进入一次 TE window，减少命令事务；
- stripe 高度根据 DMA buffer、总线吞吐和 cache alignment 选择；
- 写入方向必须与 panel MADCTL/TE 扫描方向一致；
- 如果整帧传输无法在安全窗口内完成，降低拖拽 present rate，但保持触摸采样率；
- 若 panel 提供硬件 scroll/window offset，可增加 panel-specific fast path。

这里“兼容所有接口”表示 API 和语义一致，不表示不同物理带宽的接口具有相同 FPS。

### 10.5 调度与低延迟

- touch/input 可以高频采样，但 render queue 只保留最新 offset；
- 最多允许一个 pending drag frame，禁止积压；
- frame available 后使用最新 offset 开始 compose；
- 若上一帧未 present，旧的未开始任务直接取消；
- settle animation 由 VSYNC/present 节拍驱动，不由固定 delay 驱动；
- 记录 input timestamp、render start/end、present frame ID，测量 motion-to-photon。

### 10.6 拖拽专用指标

每个接口记录：

- compose time；
- compose bytes read/write；
- DMA2D/PPA/SW 路径比例；
- present/transfer time；
- displayed FPS，不使用提交 FPS；
- dropped/coalesced frame 数；
- motion-to-photon P50/P95；
- snapshot prepare/prefetch 时间；
- snapshot RAM 峰值。

验收目标：

- renderer 侧 compose 仅与屏幕像素和色深相关，不随页面节点数增长；
- RGB/MIPI video 在相同 panel 配置下 displayed FPS 和 P95 latency 明显优于
  LVGL 双页面对象重绘；
- SPI/QSPI/I80 在总线受限时 displayed FPS 不低于 LVGL，同时 CPU 占用更低；
- 拖拽期间不发生图片解码、字体光栅化或 heap allocation；
- 连续往返拖拽无 tearing、无 snapshot 错页和 buffer reuse race。

---

## 11. 动态生成与动态控制

动态能力不能退化为“重新实现一套 LVGL 对象树”。框架采用三级模型，让常见动态需求
继续走连续命令和固定内存。三级的共同边界：**运行时可以实例化和更新编译期定义的
结构，但不能在设备端从零定义新结构**。设备端没有 object 兼容层和兜底控件树。

### 11.1 三级动态模型

Level 1：编译场景状态更新，性能最高：

- 文本、数值、颜色、可见性、opacity、resource ID、选中状态；
- 所有属性在 GSB 中已有 state slot；
- `bind_id → state slot` O(1)；
- 不创建/销毁渲染命令。

Level 2：编译模板实例，作为唯一动态生成机制：

- card、list row、device item、notification item 等在上位机编译为 template；
- 框架同时内置标准控件模板包（label/image/rect/button/progress 等，由 gspc
  在框架构建期预编译），提供 `gsp_widget_create(type, ...)` 风格 API，满足
  业务运行时动态生成控件的场景，无需业务自备模板；
- 内置模板与业务模板走完全相同的 instance/state/render 路径，无特殊分支；
- 标准控件模板需要参数化 size（state slot 驱动 bbox），超出"instance 只平移"
  的首期约束，列为 Phase 2 明确工作项；
- 设备运行时只创建轻量 instance；
- 多个 instance 共享同一份只读 command fragment 和资源；
- 每个 instance 只有 transform、clip、state range、z-order 和 generation；
- 使用固定 pool/slab，不进行每节点 malloc。

Level 3：受限 runtime overlay draw-list，唯一保留的运行时“手绘”出口：

- selection rectangle、debug overlay、输入光标、波形/图表等运行时生成几何；
- 使用有容量上限的 runtime command buffer；
- 命令直接来自基础 opcode（fill/blit/glyph-run），不创建 widget object；
- 每帧或生命周期结束后批量回收；
- 护栏：无 parent 关系、无 retained 属性更新、无事件回调挂载，
  超出容量返回错误，防止其逐步演化成第二套对象树。

**明确不支持（有意的产品边界）：**

- 第二套 retained object 树：每对象堆节点、指针链、虚函数 draw/update；
- `esp_emote_gfx` `gfx_obj_*` API 兼容层或 adapter；
- 设备端通用布局引擎和超出模板参数化能力的任意结构定义。

需求归属对照：

| 需求 | 归属 |
|---|---|
| 文本/数值/颜色/可见性变化 | Level 1 bind |
| 动态卡片、列表行、通知项 | Level 2 业务 template instance |
| 运行时动态创建 label/image/button 等标准控件 | Level 2 内置标准模板 + widget API |
| popup / toast | Level 2 template instance |
| EAF/AAF 表情动画播放 | anim codec plugin + Level 1 resource bind |
| 光标、调试覆盖、选择框 | Level 3 overlay |
| 波形、图表、手绘内容 | Level 3 overlay 或 canvas surface |
| 超出模板参数化能力的任意结构 | 扩展模板包或回上位机编译，设备端不兜底 |

### 11.2 Template 格式

GSB 新增 template section：

```text
[template table]
  template_id
  local command range
  local bbox
  state schema
  bind schema
  hit-test entries
  resource references
```

template command 使用局部坐标。Instance 在执行时提供轻量 translation/clip；
首期只支持平移和可见性，scale/rotation 只有 target accelerator 支持且需求明确时才加入。

建议 instance 概念结构：

```c
typedef struct {
    uint16_t template_id;
    uint16_t state_base;
    uint16_t parent_index;
    int16_t x;
    int16_t y;
    uint16_t clip_id;
    uint16_t z_order;
    uint16_t generation;
    uint16_t flags;
} gsp_instance_t;
```

结构尺寸和字段在 prototype benchmark 后冻结。

### 11.3 Instance pool

- scene manifest 声明各 template 最大实例数；
- 初始化时一次性分配 instance/state/text pool；
- free list 使用数组索引，不使用堆链表节点；
- create/destroy 为 O(1)；
- handle 使用 `index+generation`，防止释放后旧 handle 误用；
- 任意 active instance 可作为 parent，但必须同 scene 且 commit 时通过 cycle 检查；
- pool 满时返回明确错误，不隐式 malloc；
- development mode 可选扩容，但 release fast path 固定容量。

实例创建不复制 template commands 和图片/font resources。

### 11.4 Repeater 与虚拟列表

动态列表采用 data provider + slot recycling：

```text
total data count
→ viewport range
→ visible slots + overscan
→ template instances
→ bind visible item data
```

- 1000 条数据不创建 1000 个实例；
- 实例数约等于可见行数加少量 overscan；
- 滚动时复用 slot，只更新 transform 和 state；
- 行高固定时使用 O(1) index 计算；
- 可变行高需要上位机/业务提供 height index，不首期实现通用自动布局；
- list viewport 拥有独立 clip 和 tile/hit index；
- scroll offset 更新合并为每帧一次，旧 offset 不排队。

这类虚拟列表属于性能保证范围；任意嵌套、自动测量的通用列表不属于首期范围。

### 11.5 Runtime command buffer

runtime overlay 使用双 command buffer：

```text
application builds next buffer
→ atomic publish
→ render task reads current buffer
→ previous buffer returns to pool
```

要求：

- 固定容量；
- 命令和 payload 分离；
- 字符串复制到 bounded text arena；
- resource 只能通过 resource ID 引用；
- publish 后 immutable；
- 按 layer/z-order 排序；
- overflow 可诊断，不能越界或隐式 heap fallback。

### 11.6 Typed state 与事务提交

建议公开 API：

```c
gsp_update_t *gsp_update_begin(gsp_scene_t *scene);

gsp_err_t gsp_update_set_i32(gsp_update_t *u, gsp_bind_id_t id, int32_t value);
gsp_err_t gsp_update_set_color(gsp_update_t *u, gsp_bind_id_t id, uint32_t rgb);
gsp_err_t gsp_update_set_text(gsp_update_t *u, gsp_bind_id_t id,
                              const char *text, size_t len);
gsp_err_t gsp_update_set_resource(gsp_update_t *u, gsp_bind_id_t id,
                                  gsp_resource_id_t resource);
gsp_err_t gsp_update_set_visible(gsp_update_t *u, gsp_bind_id_t id, bool visible);

gsp_instance_handle_t gsp_instance_create(gsp_update_t *u,
                                          gsp_template_id_t template_id);
gsp_err_t gsp_instance_destroy(gsp_update_t *u, gsp_instance_handle_t instance);
gsp_err_t gsp_instance_set_position(gsp_update_t *u,
                                    gsp_instance_handle_t instance,
                                    int16_t x, int16_t y);
gsp_err_t gsp_instance_set_parent(gsp_update_t *u,
                                  gsp_instance_handle_t instance,
                                  gsp_instance_handle_t parent);

gsp_err_t gsp_update_commit(gsp_update_t *u);
```

事务语义：

- 同一 bind/instance 多次更新只保留最终值；
- commit 时统一计算 old/new bbox；
- dirty 合并一次；
- text/resource 生命周期在 commit 时确定；
- render task 只看到完整前态或完整后态；
- 跨任务调用通过 update queue，不直接修改 active state。

### 11.7 受控动态布局

首期不加入通用 flex/layout engine。允许的动态布局 recipe：

- anchor 到 viewport/parent 边；
- fixed grid；
- fixed/known row height repeater；
- stack with fixed gap；
- template instance translation；
- 文本 bbox 根据预计算 glyph metrics 更新。

gspc 将 recipe 编译为简单参数，设备只执行整数计算。超出该能力的布局在上位机完成；
设备端没有兜底布局引擎。

Parent 关系只用于 ownership、transform、clip 和受控 layout，不用于渲染遍历。
事务 commit 时使用数组 visit-state/topological pass 迭代计算 world transform 和 bbox：

- `UNVISITED/VISITING/DONE` 检测环；
- 检测到 cycle 时整个事务失败，不依赖递归深度强制退出；
- 渲染仍按独立的 flat z-order command/instance list 顺序执行；
- reparent 只更新 parent index、受影响 subtree world transform 和 old/new dirty；
- scroll 如需支持，只实现 parent content offset + clip，不引入完整 LVGL scroll 系统。

### 11.8 动态 hit-test

- 静态命中继续使用编译期 hit index；
- template instance 共享 local hit entries，加 instance translation；
- active instance 按 tile 维护紧凑 handle list；
- instance 移动时只更新 old/new tiles；
- runtime overlay 发布时同时发布 hit buffer；
- hit result 返回 action ID、instance handle 和可选 data index。

业务可据此识别“哪个动态卡片被点击”，不需要为每个实例生成不同回调函数。

### 11.9 动态内容与全屏拖拽

动态内容分两类：

- `PAGE_BOUND`：属于页面，在拖拽开始时冻结进 snapshot；拖拽期间更新进入事务队列，
  settle 后应用并刷新 live scene/snapshot。
- `VIEWPORT_OVERLAY`：状态栏、全局 toast、手势提示等固定在视口；在两页面
  snapshot compose 后作为小 overlay 绘制。

禁止拖拽过程中因 PAGE_BOUND 更新重建整页 snapshot。若业务要求实时变化，更新应
合并并在可用 frame 上限频，不能阻塞触摸/compose。

### 11.10 动态能力性能门槛

- 创建/销毁 template instance 无每节点 heap allocation；
- 100 个同模板实例的创建/销毁明显快于 LVGL 创建/删除等价 object；
- 1000 条数据虚拟列表的 RAM 与可见 slot 数相关，而不是与总数据量线性相关；
- 32 个 bind 的同事务更新只触发一次 dirty merge/refresh；
- 动态实例 render 仍使用同一 SW/PPA/DMA2D executor；
- Level 3 overlay 命令数、字节数和 overflow 次数可统计，
  用于监控其是否被滥用为通用 UI 路径。

---

## 12. 输入与事件

上位机生成独立 hit-test 表：

- bbox；
- z-order；
- action ID；
- flags；
- 可选 tile hit index。

触摸查询不遍历渲染命令和对象树。命中后发送整数 action ID。C 侧 action table 在
链接/初始化阶段验证，设备运行时不按字符串查找函数。

首期事件：

- press；
- release；
- click；
- value change；
- goto/back。

复杂 gesture、惯性滚动和通用 focus 系统不作为首期性能目标。

---

## 13. 内存模型

Flash/mmap：

- GSB immutable command；
- tile/bind/hit index；
- strings；
- GRB resources；
- glyph atlas；
- static baked tiles。

Internal RAM：

- mutable state slots；
- template instance pool；
- runtime command/text double buffer；
- dirty list；
- update queue；
- accelerator descriptors/fences；
- 高频 glyph/cache metadata。

PSRAM/panel framebuffer：

- full framebuffer；
- optional draw buffer；
- page transition snapshot；
- 大型 decoded resource cache。

禁止：

- 每节点 malloc；
- child-list malloc；
- 每帧字符串 malloc；
- 每 glyph alpha malloc；
- 场景加载时复制整个图片资源；
- 为格式转换无条件增加一个完整 framebuffer。

---

## 14. 代码组织

仓库结构以《GSP Repository and Naming Design》为准：monorepo `esp-gsp`，
上位机与设备端组件同仓库，`components/esp_gsp` 为唯一组件发布单元。

上位机（`tools/gspc`）：

```text
esp-gsp/tools/gspc/
├── frontend/            # GSP1 parse / schema / validate
├── ir/                  # layout resolve / static-dynamic split
├── opt/                 # merge / cull / dedup / tile index
├── backend/             # gsb / grb / gfb / gspb emitters + target profiles
├── fontc/               # font.py 升级：atlas + glyph run
└── widgets/             # 内置标准控件模板源（GSP1 JSON）
```

设备端组件（`components/esp_gsp`）：

```text
esp-gsp/components/esp_gsp/
├── include/gsp/
│   ├── gsp_runtime.h / gsp_update.h / gsp_instance.h / gsp_widget.h
│   ├── gsp_repeater.h / gsp_overlay.h / gsp_renderer.h
│   ├── gsp_accel.h / gsp_codec.h / gsp_present.h / gsp_input.h / gsp_test.h
│   └── format/          # gsp_gsb/grb/gfb/gspb_format.h（codegen 生成）
└── src/
    ├── runtime/         # state / instance / overlay / repeater
    ├── renderer/        # command replay + kernel_rgb565/rgb888/argb8888
    │                    #   + reference/（正确性 oracle）
    ├── accel/           # gsp_accel_sw.c / gsp_accel_ppa.c / gsp_accel_dma2d.c
    ├── codec/           # eaf / jpeg（decoder plugin）
    ├── widget/          # 内置模板包生成物 + widget API
    ├── font/            # GFB loader / glyph run / atlas cache
    ├── present/         # gsp_present_esp_lcd.c
    └── port/            # esp_idf / host(SDL/memory) 平台层
```

格式定义以仓库 `formats/` 目录为单一事实源，codegen 同时生成
`include/gsp/format/*.h` 与 gspc 的 Python 常量，CI diff 防止 host/device
漂移。

该组件直接在新仓库起步，不在 `esp_emote_gfx` 内孵化：孵化会天然诱惑复用 object
路径，且新框架已决定不携带 object 层。软件 primitive 以源码移植进入新仓库，用
reference rasterizer golden vectors 验证移植前后像素一致；`esp_emote_gfx` 原仓库
不因新框架发生接口变更。

---

## 15. 与 `esp_emote_gfx` 的关系

### 原仓库处置

- `esp_emote_gfx` 冻结为维护状态：只修 bug，不加新控件，不迁移进新仓库；
- 继续服务存量 emote 产品（表情/动画类），生命周期独立于新框架；
- 新框架不提供 `gfx_obj_*` API 兼容层，存量 UI 迁移 = 用 GSP JSON
  重新描述并经 gspc 编译，属于一次性成本；
- ARN1 在新框架中保留只读兼容 loader，不继续扩展。

### 源码移植清单（copy-adapt，非组件依赖）

- 软件 fill/blit/blend/mask primitive；
- 半开区间 area/clip 语义和 dirty 收集/合并策略；
- input driver 和触摸坐标转换；
- host SDL/memory backend 测试思路；
- image decoder 中可复用部分；
- `gfx_eaf_dec` EAF/AAF 流式解码核心（reader 抽象、无 object 层依赖，
  直接移植为 anim codec plugin）；
- `esp_idf_accel.c` 的 PPA capability/format 判断。

### 新仓库全新实现

- GSB loader 和 contiguous command executor；
- bind/state/tile/hit runtime；
- template instance pool、repeater 和 runtime overlay；
- accelerator scheduler；
- `esp_display_present` adapter。

---

## 16. 公平性能基准

### 16.1 三种独立指标

纯 renderer：

```text
invalidate/update → pixels ready in memory
```

排除 LCD、VSYNC 和异步 present。

当前帧端到端延迟：

```text
state update → render → submit → 对应 frame_id present completion ISR
```

两边都必须等待同一语义的当前帧完成。

稳态吞吐：

```text
连续 N 帧提交 → drain pipeline → 总时间 / 实际 present 帧数
```

同时记录 CPU 占用，不能只记录 `refresh_now()` 返回时间。

### 16.2 对照条件

GSP 与 LVGL 必须使用：

- 相同像素输出；
- 相同字体位图和字号；
- 相同图片资源；
- 相同分辨率和旋转；
- 相同 tear mode；
- 相同 framebuffer 数量；
- 相同 PPA/DMA2D 开关；
- 相同 dirty 区域；
- 相同 panel PCLK/总线配置。

使用 golden image 或 framebuffer CRC 验证场景等价。

### 16.3 场景集

- 静态 100 节点页面；
- 200/500/1000 节点网格；
- 文本密集仪表盘；
- 动态数字/时间连续变化；
- 静态 CJK、多语言 locale 切换和缺字 fallback；
- 图片密集页面；
- alpha 图标页面；
- mmap 原生图片、JPEG 首次 decode 和 decode-cache hit；
- EAF/AAF 动画连续播放（对照 `esp_emote_gfx` 现有播放路径的 FPS/CPU）；
- widget API 运行时创建/更新/销毁标准控件（对照 LVGL `lv_obj_create` 等价操作）；
- 单 bind 更新；
- 10/32 个离散 bind 同帧更新；
- 100 个动态 card template create/update/destroy；
- 1000 条数据、可见 10/20 行的虚拟列表；
- popup/toast runtime overlay 高频创建和回收；
- 全屏切换；
- 2/4 页全屏连续往返拖动；
- 拖拽期间快速反向、松手 settle 和 neighbor prefetch；
- RGB565/RGB888 profile；
- RGB、SPI/QSPI、MIPI-DSI（硬件可用时）。

### 16.4 “明显优势”验收门槛

以下均为项目目标，不是当前事实：

- scene activation 比 LVGL 运行时建树快至少 10 倍；
- scene/runtime 非 framebuffer RAM 比 LVGL 低至少 40%；
- renderer-only dirty update 比 LVGL 快至少 30%；
- renderer-only full frame 比 LVGL 快至少 20%；
- 相同实际 present FPS 下，render CPU 占用低至少 30%；
- renderer-bound 场景稳态 FPS 高至少 20%；
- bus/VSYNC-bound 场景 FPS 不低于 LVGL，同时 CPU 和内存明显更低；
- 高频原生图片 draw 无 scene-RAM copy、无帧内 decode；
- 静态文本无设备端 shaping/光栅化，动态 glyph-run 更新无帧内 malloc；
- 全屏拖拽 compose 时间不随页面节点数增长；
- 全屏拖拽期间无图片 decode、字体 raster 和 frame queue 积压；
- template instance 创建/销毁无 per-node heap，批量操作明显快于 LVGL object；
- 虚拟列表 RAM 由 visible+overscan 决定，不随总数据数线性增长；
- 动态 update transaction 只发布一次完整状态和一次 dirty merge；
- 当前帧端到端延迟不劣于 LVGL；
- 视觉输出通过 golden image。

只有满足 renderer、内存、CPU 三类门槛后，才能对外宣称“明显优于 LVGL”。

---

## 17. 实施阶段

### Phase 0：基准与可观测性

- 修正 LVGL 与当前 ARN benchmark 的完成语义。
- 增加 frame ID、render/transfer/present fence 计时。
- 增加 CPU cycles、dirty pixels、copy bytes、PPA/DMA2D pixels、峰值 RAM。
- 建立 golden image。

退出条件：可以分别比较 renderer、present latency 和 steady throughput。

### Phase 1：GSB 与纯软件参考渲染器

- 定义未冻结的 GSB prototype。
- gspc 输出连续命令、state/bind/hit/tile index。
- gspc 输出 template section、state schema 和 instance pool manifest。
- 实现 memory/SDL software renderer。
- 与 GSP preview 做像素一致性验证。

退出条件：所有首期 opcode 正确，GSB 不创建节点对象。

### Phase 2：设备 RGB565 fast path

- mmap/XIP load。
- SW fill/blit/mask。
- bind update 和 dirty tile replay。
- typed update transaction、template instance pool 和 runtime overlay buffer。
- fixed-row virtual repeater。
- GRB 原生图片、RGB565+A8、stride/alignment 和 O(1) resource view。
- 图片 decode task、native surface cache 和 placeholder 更新流程。
- A8 glyph atlas、glyph index、静态 glyph run 和动态小字符集。
- 禁用量产热路径 FreeType，静态文本允许 baked tile。
- 与 ARN1/LVGL 做 renderer-only 对比。

退出条件：达到 dirty/full renderer 目标；原生图片和 cached glyph 无帧内 malloc/copy；
图片、字体和 framebuffer golden image 一致。未达标时必须给出热点证据。

### Phase 3：`esp_display_present` 集成

- RGB/MIPI double/triple pipeline。
- SPI/QSPI/I80 TE_SYNC。
- frame/transfer/present fence。
- dirty/unrendered copy。
- rotation 和 callback ownership。
- full snapshot acquire/release 和 previous/current/next window。
- drag frame latest-offset coalescing 和 frame ID。

退出条件：无撕裂、无 buffer reuse race，端到端统计口径一致。

### Phase 4：PPA blocking fast path

- capability matrix。
- fill、blit、blend、scale/rotate。
- format/alignment validation。
- 每操作 SW/PPA crossover threshold benchmark。

退出条件：所有硬件路径有软件 fallback 和 golden image。

### Phase 5：DMA2D 与异步 accelerator

- DMA2D framebuffer/region copy。
- unrendered copy。
- async PPA/DMA2D queue。
- buffer state、hazard 和 fence。
- CPU parse、accelerator 和 present 流水化。
- 全屏拖拽两 strip DMA2D/PPA compose。
- RGB/MIPI full-frame compose 与 SPI/QSPI stripe compose。
- drag motion-to-photon、drop/coalesce 和 snapshot RAM 统计。

退出条件：压力测试无 cache/buffer race，CPU 占用达到目标；全屏拖拽满足对应接口
的 displayed FPS/latency 门槛，且帧内无资源 decode 和 heap allocation。

### Phase 6：多场景、OTA 和产品化

- GSPB/资源包设备 loader。
- 跨场景资源去重和 mmap。
- scene preload/switch。
- ABI version/CRC/rollback。
- CI 性能门槛。

退出条件：量产场景可升级、回滚并通过多接口回归。

---

## 18. 风险与控制

### Tile index 体积

高重叠场景可能产生较大索引。gspc 必须比较 index size 与顺序扫描成本，允许按场景
关闭或改用 row-band/range index。

### 静态烘焙占用

整页 RGB565 snapshot 占用大。默认使用压缩静态 tile，只为页面拖动保留有限数量
解码后的 full snapshot。

### PPA/DMA2D 启动开销

小操作硬件加速可能更慢。必须使用 operation-specific threshold，不能强制全部 PPA。

### 图片 decode 与 cache 峰值

压缩图片节省 Flash 但会增加 decode 时间和 native surface RAM。manifest 必须输出
最坏 cache 峰值；大图采用后台预取和字节预算 LRU，render task 不承担 decode。

### 字体包体与动态字符范围

CJK 全字符多字号 atlas 不可行。按 locale/range 分片并报告缺字；任意动态 Unicode
只能进入兼容路径，不纳入高性能保证。

### 全屏拖拽物理带宽

低带宽 SPI 即使 renderer compose 很快，也可能无法在 TE window 内传完整帧。必须
区分 renderer 瓶颈和总线瓶颈，并允许降低 displayed FPS、使用 stripe pipeline 或
panel-specific scroll；不得用提交 FPS 掩盖实际显示能力。

### 动态能力重新演化成对象树

若 template instance 不断增加独立属性、指针和虚函数，最终会重复 LVGL 的成本。
必须保持 template immutable、instance POD、state typed、pool bounded。设备端没有
兜底对象层是有意约束：复杂需求回到上位机编译，或明确拒绝。

### 无 object 兼容层的覆盖缺口

放弃 object 兼容层后，运行时动态控件由内置标准模板包承接，剩余无法预编译的
需求只剩 Level 3 overlay 一个出口。若产品出现三级模型都接不住的场景，正确响应
是扩展标准模板包/gspc 能力，而不是在设备端补一套控件树。需在早期产品接入时验证
三级模型覆盖率，Level 3 使用统计超过阈值即是模型不足的信号。

### 标准模板包演化成隐性控件库

内置 widget API 若不断追加样式属性和行为开关，模板参数会膨胀成 LVGL style
系统。约束：标准模板只暴露 typed state schema 中的字段；新需求优先加新模板
而不是加参数；模板包版本随 GSB ABI 一起管理。

### Pool 容量不足

固定 pool 可预测但可能耗尽。gspc manifest 输出容量和内存预算，运行时返回可诊断错误；
禁止 release 构建静默 malloc。容量应根据产品场景和压力测试确定。

### 动态文本生命周期

业务传入的字符串不能以裸指针跨帧使用。commit 时复制到 bounded text arena 或引用
有明确生命周期的 resource，buffer rollover 前等待对应 render fence。

### 格式组合爆炸

首期只产品化少数 profile，例如 RGB565-native 和一个 RGB888 profile。其他组合走
软件 fallback，验证后再加入。

### Async 数据竞争

命令只读、状态单写者、framebuffer 明确 ownership。所有 accelerator/present 提交
使用 frame ID 和 fence，禁止依赖隐式 `flush_ready`。

### 与 LVGL 功能范围不等价

首期不追求通用控件和复杂动画。Benchmark 只比较两边都能等价表达的预处理 UI。

---

## 19. 首个可执行决策

1. 不再扩展 ARN1 ABI。
2. `esp_emote_gfx` 冻结维护，object/widget 系统不迁移、不重构、不做兼容层。
3. 新仓库直接起步，软件 primitive 源码移植并用 golden vectors 验证一致，
   不在 `esp_emote_gfx` 内孵化。
4. 设备 present 直接接入 `esp_display_present`，不通过 `esp_lvgl_adapter`。
5. gspc 首先实现连续 command、独立 state 和 RGB565 target profile。
6. 在引入 PPA/DMA2D 前先建立软件基线和正确 fence 统计。
7. PPA 先 blocking 验证，DMA2D 用于 copy；随后统一演进到 async scheduler。
8. Phase 2 benchmark 未达到 renderer 优势时，不继续扩大控件功能，先解决热点。
9. 图片首先完成 mmap 原生格式和跨场景共享，decoder 不进入 render task。
10. 字体首先完成 A8 atlas、glyph run 和动态小字符集，FreeType 只作兼容路径。
11. 全屏拖拽建立独立 snapshot-compose fast path，不经过普通 dirty command replay。
12. 动态生成只采用 immutable template+instance pool，不新增运行时控件树。
13. 所有动态控制通过 typed transaction 批量提交，禁止业务直接修改 active state。
14. 运行时动态控件由内置标准控件模板包 + widget API 承接，与业务模板同路径；
    不实现 object 兼容层。
15. EAF/AAF 移植 `gfx_eaf_dec` 作为 anim codec plugin 接入，解码在 decode task，
    帧推进走 bind resource 更新。

该路径将优势建立在“上位机预处理 + 设备连续执行 + 原生格式 + 异步 present”上，
而不是仅用另一套控件 API 重复 LVGL 的运行时工作。
