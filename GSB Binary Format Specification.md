# GSB/GRB/GFB 二进制格式规范

> 状态：草案 v0.9（字段布局在 Phase 1 原型 benchmark 后冻结为 v1.0）
> 前身：`esp_emote_gfx/examples/ai_scene_pkg/gsp_protocol/gsp_binary_protocol_zh.md`（v0.1，GSP1 magic / version 4，下称"旧协议"）
> 关联文档：`GSP High-Performance Rendering Framework Plan.md`（§5/§7/§11/§12）、`GSP Repository and Naming Design.md`
> 适用：gspc emitter、设备端 loader、host preview、格式 codegen（`formats/`）

## 1. 与旧协议的关系

### 1.1 继承的纪律（全部保留）

1. 包内多字节字段统一 little-endian。
2. 字节位置无关；运行时 payload 禁止原生 C 指针。
3. 引用只用定宽 index 和相对包基址的 `u32` byte offset。
4. 表项固定大小；整包 CRC32（计算时 CRC 字段视为 0）。
5. loader 以错误码拒绝坏包，任何非法 offset/index/count/CRC 不允许崩溃。
6. 枚举注册表 append-only，值一经发布即冻结。
7. 导出器必须同步生成 manifest；导出后必须回跑 loader 校验。

### 1.2 替换的模型

| 旧协议（v0.1 / GSP1 v4） | GSB | 原因（主计划章节） |
|---|---|---|
| Object entry → loader 调 `gfx_*_create()` 建树 | 连续命令流，设备零对象创建 | §1/§5 |
| `parent_idx` 树 + local x/y | 上位机展开为绝对 bbox + z-order 顺序命令 | §4.1 |
| 图片 blob 内嵌场景包、加载时解码进 RAM | 资源独立 GRB，mmap/XIP 直接消费，跨场景共享 | §7.4 |
| `callback_off` 字符串按名查找 | 整数 `action_id`，init 时一次性绑定校验 | §12 |
| 文本 = UTF-8 字符串 + 设备端排版 | 静态文本 = glyph run；动态文本 = state slot | §7.5 |
| `font_id` → `.inc` 外部 C 元数据 | GFB 字体包，独立分发/OTA | §7.5 |
| 固定顺序区域 + 手算 offset（header 扩一次 bump 一次版本） | section directory，可选 section 免 bump | 本文 §3 |
| list/wheel 等控件语义进包 | 控件在上位机降级为基础命令；动态控件走 template | §5.2/§11 |

### 1.3 三种文件与总包

| 文件 | magic | 扩展名 | 内容 |
|---|---|---|---|
| GSB | `GSB1` | `.gsb` | 单场景：命令流、状态、bind/action/hit/tile 索引、模板 |
| GRB | `GRB1` | `.grb` | 资源：图片、baked tile、EAF 等，跨场景共享 |
| GFB | `GFB1` | `.gfb` | 字体：A8 atlas、glyph metrics、cmap |
| GSPB | 见 `gsp_bundle.h` | `.gspb` | 多文件总包：索引 + 对齐 + CRC，OTA/分区单元 |

---

## 2. 通用容器

GSB/GRB/GFB 共用同一容器结构。

### 2.1 文件头（32 bytes）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u32` | `magic` | `GSB1` / `GRB1` / `GFB1` |
| `4` | `u16` | `version_major` | 破坏性变化 bump；loader 不等则拒绝 |
| `6` | `u16` | `version_minor` | append-only 变化 bump；loader 允许更高 minor |
| `8` | `u32` | `total_size` | 文件总字节数 |
| `12` | `u32` | `crc32` | 整文件 CRC32，计算时本字段为 0 |
| `16` | `u16` | `section_count` | section directory 条目数 |
| `18` | `u16` | `header_flags` | bit0：debug 构建；其余为 0 |
| `20` | `u32` | `target_profile_id` | 编译时 target profile 的 hash；loader 不匹配时按策略警告/拒绝 |
| `24` | `u32` | `content_id` | 构建内容 hash，OTA 去重/回滚识别 |
| `28` | `u32` | `reserved` | 必须为 0 |

### 2.2 Section directory（每项 16 bytes，紧随文件头）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `type` | section 类型注册表（§4.1/§6.1/§7.1） |
| `2` | `u16` | `flags` | bit0 `REQUIRED`：loader 不识别该 type 时必须拒绝整包；未置位则跳过 |
| `4` | `u32` | `offset` | 相对文件基址，≥4 字节对齐（数据类 section 按 §2.4） |
| `8` | `u32` | `size` | section 字节数 |
| `12` | `u32` | `count` | 表类 section 的条目数；非表类为 0 |

`REQUIRED` 位是前向兼容机制：minor 版本新增可选 section 不影响旧 loader；
新增旧 loader 必须理解的语义才 bump major。这替代了旧协议"header 扩 8 字节
就必须 v3→v4"的演化方式。

### 2.3 通用校验（loader 必做，任一失败即拒绝）

1. `magic`、`version_major` 匹配；
2. `total_size >= 32 + section_count*16` 且 `<= 提供的 buffer 大小`；
3. CRC32 匹配；
4. 每个 section：`offset + size <= total_size`，offset 对齐，section 互不重叠；
5. 所有 `REQUIRED` 且未知的 type → 拒绝；
6. 各 section 内部 index/offset/count 按各自规则校验（后述各节）。

### 2.4 对齐

- 表类 section：4 字节对齐；
- GRB 中 `cache_policy = MMAP_DIRECT` 的资源 payload：`data_off` 与 stride
  按 target profile 的 `resource_alignment` / `stride_alignment`（如 64）；
- GFB atlas page：按 profile `resource_alignment`；
- GSPB 内各文件起始：按成员文件最大对齐要求。

对齐值来源于 target profile（主计划 §4.2），emitter 写入 manifest，
loader 按 profile 校验，不硬编码。

---

## 3. GSB：场景二进制

一个 GSB = 一个场景。多场景经 GSPB 打包。

### 3.1 Section 类型注册表（append-only）

| Type | 名称 | REQUIRED | 内容 |
|---:|---|:---:|---|
| `0x0001` | `SCENE_META` | 是 | 场景元数据 |
| `0x0002` | `COMMANDS` | 是 | 定长命令数组 |
| `0x0003` | `STATE_DEFAULTS` | 否 | 可变状态槽默认值 |
| `0x0004` | `BIND_INDEX` | 否 | bind_id → state slot + 受影响命令 |
| `0x0005` | `CMD_REF_LISTS` | 否 | 命令索引列表池（bind/tile 共用） |
| `0x0006` | `ACTION_TABLE` | 否 | 事件 → 动作 |
| `0x0007` | `HIT_INDEX` | 否 | 命中测试表 |
| `0x0008` | `TILE_INDEX` | 否 | tile → 命令索引 |
| `0x0009` | `RES_REFS` | 否 | 局部资源引用 → GRB/GFB resource_id |
| `0x000A` | `TEMPLATES` | 否 | 模板表（动态实例化） |
| `0x000B` | `GLYPH_RUNS` | 否 | 静态文本 glyph run 数据 |
| `0x000C` | `TEXT_DEFAULTS` | 否 | BOUND 文本默认 UTF-8 内容 |
| `0x000D` | `CLIP_TABLE` | 否 | 共享 clip 矩形表 |
| `0x000E` | `ACTION_PARAMS` | 否 | action 变长参数池 |
| `0x000F` | `DEBUG_NAMES` | 否 | bind/action/command 名称（仅调试构建） |
| `0x0010` | `VIS_GROUPS` | 否 | 可见性组（layer/hidden/多态控件） |

### 3.2 `SCENE_META`（32 bytes）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `scene_id` | GSPB 内唯一 |
| `2` | `u16` | `screen_w` | 逻辑宽 |
| `4` | `u16` | `screen_h` | 逻辑高 |
| `6` | `u8` | `color_profile` | 本包特化的 native 渲染格式（§8.1 枚举） |
| `7` | `u8` | `flags` | bit0：启用 tile index；bit1：含模板；其余 0 |
| `8` | `u32` | `bg_color` | native 格式背景色 |
| `12` | `u32` | `cmd_count` | 命令数 |
| `16` | `u16` | `state_slot_count` | 状态槽数（scene 级，不含 instance） |
| `18` | `u16` | `bind_count` | bind 数 |
| `20` | `u16` | `tile_w` | tile 宽（未启用为 0） |
| `22` | `u16` | `tile_h` | tile 高 |
| `24` | `u16` | `tiles_x` | 水平 tile 数 |
| `26` | `u16` | `tiles_y` | 垂直 tile 数 |
| `28` | `u32` | `reserved` | 0 |

### 3.3 `COMMANDS`：命令条目（32 bytes，冻结前可调）

命令按最终绘制顺序（z-order）连续存储。坐标为屏幕绝对坐标、half-open
`[x1,x2)×[y1,y2)`（主计划 §6.4）。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u8` | `opcode` | §3.4 注册表 |
| `1` | `u8` | `flags` | bit0 `STATE_BOUND`；bit1 `OPAQUE`（不透明覆盖提示）；bit2 `TEMPLATE_LOCAL`（坐标为模板局部） |
| `2` | `u16` | `state_slot` | 绑定状态槽；`0xFFFF` = 静态 |
| `4` | `i16` | `x1` | |
| `6` | `i16` | `y1` | |
| `8` | `i16` | `x2` | half-open |
| `10` | `i16` | `y2` | half-open |
| `12` | `u16` | `clip_id` | `CLIP_TABLE` 索引；`0xFFFF` = 无附加 clip |
| `14` | `u16` | `layer` | 0 = 页面；1 = viewport overlay（主计划 §11.9） |
| `16` | `12B` | `params` | 按 opcode 解释（§3.4） |
| `28` | `u32` | `reserved` | 0 |

设计说明：dirty bbox 必须已含 AA/outside-stroke 扩张（emitter 责任，
主计划 §6.4）；`STATE_BOUND` 命令的 bbox 是编译期最大 bbox，运行期实际
bbox 由状态槽决定但不得超出。

### 3.4 Opcode 注册表与 params 布局（append-only）

| ID | 符号 | params（offset 16 起，12B） |
|---:|---|---|
| `0x01` | `FILL_RECT` | `u32 color`（native）、`u8 opacity`、`u8 value_mode`（0 无 / 1 宽度随 slot.value 0..100 缩放 / 2 高度缩放）、6B 填 0 |
| `0x02` | `FILL_ROUND_RECT` | `u32 color`、`u16 radius`、`u16 stroke_width`（0=纯填充）、`u8 opacity`、`u8 stroke_mode`（0 inside/1 center/2 outside）、`u8 value_mode`（同上）、1B 填 0 |
| `0x03` | `BLIT_OPAQUE` | `u16 res_ref`、`i16 src_x`、`i16 src_y`、6B 填 0 |
| `0x04` | `BLIT_ALPHA` | `u16 res_ref`、`i16 src_x`、`i16 src_y`、`u8 opacity`、`u8 alpha_mode`（§8.2）、4B 填 0 |
| `0x05` | `DRAW_GLYPH_RUN` | `u32 run_off`（`GLYPH_RUNS` 内）、`u16 font_ref`（`RES_REFS`）、`u32 color`、2B 填 0 |
| `0x06` | `DRAW_STATIC_TILE` | `u16 res_ref`、`i16 src_x`、`i16 src_y`、6B 填 0 |
| `0x07` | `CLIP_PUSH` | 无 params；clip 矩形取本命令 bbox |
| `0x08` | `CLIP_POP` | 无 params |

未知 opcode：loader 校验阶段拒绝整包（命令是 REQUIRED section）。
新增 opcode = minor bump；旧 loader 因拒绝未知 opcode 而安全。

radius/stroke 的像素语义由《GSP Primitive Coverage Specification》定义，
本文只定义编码。GSP1 属性到 opcode 的降级规则见
《GSP1 to GSB Lowering Specification》。

### 3.5 `STATE_DEFAULTS`：状态槽（16 bytes）

场景激活时整段复制进 RAM，此后包内数据只读。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u32` | `value` | 整数值（进度、枚举等） |
| `4` | `u32` | `color` | native 格式颜色 |
| `8` | `u16` | `res_ref` | `RES_REFS` 索引 |
| `10` | `u16` | `text_off` | `TEXT_DEFAULTS` 内偏移（16B 单位）；`0xFFFF` 无 |
| `12` | `u16` | `glyph_run` | 运行期 glyph run 缓存句柄；包内固定 `0xFFFF` |
| `14` | `u16` | `flags` | bit0 visible；其余 0 |

### 3.6 `BIND_INDEX`（12 bytes，按 `bind_id` 升序）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `bind_id` | 应用侧标识 |
| `2` | `u8` | `type` | 0 I32 / 1 COLOR / 2 TEXT / 3 RESOURCE / 4 VISIBLE |
| `3` | `u8` | `flags` | 0 |
| `4` | `u16` | `state_slot` | 目标槽 |
| `6` | `u16` | `cmd_count` | 受影响命令数 |
| `8` | `u32` | `cmd_list_off` | `CMD_REF_LISTS` 内偏移，`cmd_count` 个 `u32` 命令索引 |

update commit 时由此直接取 old/new bbox 合并 dirty，不扫描命令流
（主计划 §5.3）。校验：`state_slot < state_slot_count`，type 与槽用法一致，
命令索引 `< cmd_count`。

### 3.7 `ACTION_TABLE`（16 bytes）与 `HIT_INDEX`（16 bytes）

Hit entry（按 z 降序）：

| Offset | 类型 | 字段 |
|---:|---|---|
| `0` | `i16×4` | `x1,y1,x2,y2`（绝对，half-open） |
| `8` | `u16` | `z` |
| `10` | `u16` | `action_first`（`ACTION_TABLE` 起始索引） |
| `12` | `u16` | `action_count`（同一触发依次执行的动作数，≥1） |
| `14` | `u16` | `flags`（bit0：template-local，供实例平移） |

Action entry：

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `action_id` | 发布给应用的整数 ID |
| `2` | `u8` | `event` | 继承旧协议 §16.4 枚举（值冻结）：0 NONE / 1 CLICK / 2 PRESS / 3 RELEASE / 4 LONG / 5 VALUE |
| `3` | `u8` | `action` | §3.8 注册表 |
| `4` | `u16` | `target_bind` | 目标 bind_id（SET_*/SHOW/HIDE）或 scene_id（GOTO）；`0xFFFF` 无 |
| `6` | `u16` | `param_len` | 变长参数字节数 |
| `8` | `u32` | `arg` | 内联小整数（颜色、值、延时帧数等） |
| `12` | `u32` | `param_off` | `ACTION_PARAMS` 内偏移；0 = 无 |

### 3.8 Action 枚举（继承旧协议 §16.5，值冻结，append-only）

| 值 | 符号 | 变化 |
|---:|---|---|
| `0` | `ACT_NONE` | 不变 |
| `1` | `ACT_SHOW` / `2` `ACT_HIDE` / `3` `ACT_TOGGLE` | 目标由 object index 改为 `target_bind`（VISIBLE bind） |
| `4` | `ACT_SET_TEXT` | 目标为 TEXT bind，参数在 `ACTION_PARAMS` |
| `5` | `ACT_SET_BG_COLOR` → `ACT_SET_COLOR` | 目标为 COLOR bind |
| `6` | `ACT_SET_OPACITY` | 目标为 I32 bind |
| `7` | `ACT_CALL` | **不再按名字符串查找**：`action_id` 即绑定键，应用 init 时注册 handler，loader 校验所有 CALL 的 id 已注册 |
| `8` | `ACT_GOTO` | `target_bind` 为 scene_id；层间切换由 gspc 展开为 SHOW/HIDE 动作链，不新增枚举 |
| `9` | `ACT_BACK` | 不变 |
| `10` | `ACT_SET_VALUE` | 新增：I32 bind 赋值 `arg`，触发 VALUE 事件 |
| `11` | `ACT_TOGGLE_VALUE` | 新增：I32 bind 在 0/1 间翻转（toggle 控件），触发 VALUE 事件 |

`DEBUG_NAMES` 可携带 `action_id → 名称` 供工具链与 manifest 对照；
量产包可剥离。

### 3.9 `TILE_INDEX`

头部（8B）后接 per-tile 表（每 tile 8B，行优先）：

```text
u32 list_off   → CMD_REF_LISTS 内偏移（u16 命令索引 × count，z-order）
u16 count
u16 flags
```

单 GSB 命令数 ≤ 65535 时 tile 列表用 `u16` 索引（`CMD_REF_LISTS` 中
bind 列表用 `u32`，两者以偏移区分用途）。是否生成本 section 由 gspc
按节点数/重叠率决定（主计划 §5.4）。

### 3.10 `RES_REFS`（8 bytes）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u32` | `resource_id` | GRB/GFB 内全局 ID |
| `4` | `u8` | `kind` | 0 image / 1 baked tile / 2 font / 3 anim |
| `5` | `u8` | `flags` | 0 |
| `6` | `u16` | `reserved` | 0 |

命令中的 `res_ref`/`font_ref` 是本表索引。场景激活时一次性解析为
resource view（O(1)，主计划 §7.4），缺失资源在激活期报错，不在渲染期。

### 3.11 `TEMPLATES`（32 bytes）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `template_id` | |
| `2` | `u16` | `flags` | 0 |
| `4` | `u32` | `cmd_first` | 模板命令区起始（命令带 `TEMPLATE_LOCAL`，局部坐标） |
| `8` | `u16` | `cmd_count` | |
| `10` | `u16` | `state_count` | 每实例状态槽数 |
| `12` | `u32` | `state_defaults_off` | 每实例默认状态（`STATE_DEFAULTS` 布局） |
| `16` | `u16` | `hit_first` | 局部 hit entry 起始 |
| `18` | `u16` | `hit_count` | |
| `20` | `u16` | `max_instances` | pool 容量 manifest（主计划 §11.3） |
| `22` | `i16×4` | `x1,y1,x2,y2` | 模板局部 bbox |
| `30` | `u16` | `reserved` | 0 |

内置标准控件模板包（主计划 §11.1）使用同一布局，`template_id`
高位段（≥ `0xF000`）保留给框架内置模板。

### 3.12 `GLYPH_RUNS`

run 头（8B）+ glyph 记录（6B × count）：

```text
run:   u16 font_ref | u16 glyph_count | u16 flags | u16 reserved
glyph: u16 glyph_id | i16 x | i16 y      （相对命令 bbox 左上角，baseline 已折算）
```

设备不做测量、shaping、fallback；缺字在 gspc 阶段报告（主计划 §7.5）。

### 3.13 `VIS_GROUPS`：可见性组（24 bytes）

解决"一个状态控制一段命令区间的可见性"：layer 显隐、对象 `hidden`、
toggle 等多态控件。子树在命令流中被 gspc 展平为连续区间，组以 O(1)
区间判断跳过整段命令，命令条目本身不携带组信息。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `group_id` | |
| `2` | `u16` | `state_slot` | 控制槽 |
| `4` | `u32` | `cmd_first` | 命令区间起始 |
| `8` | `u16` | `cmd_count` | 命令区间长度 |
| `10` | `u16` | `flags` | bit0 `MATCH_VALUE`：可见条件附加 `slot.value == match_value` |
| `12` | `i16×4` | `x1,y1,x2,y2` | 组 bbox（显隐切换时的 dirty 区域） |
| `20` | `u32` | `match_value` | `MATCH_VALUE` 置位时比较值 |

可见条件：`slot.flags.visible && (!MATCH_VALUE || slot.value == match_value)`。
组可嵌套（区间包含），renderer 以栈式区间扫描处理；组区间不得部分重叠。
校验：区间落界、bbox 合法、`state_slot < state_slot_count`、嵌套关系合法。

---

## 4. GRB：资源二进制

Sections：`0x0101 RES_TABLE`（REQUIRED）、`0x0102 RES_DATA`（REQUIRED）。

### 4.1 Resource entry（40 bytes）

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u32` | `resource_id` | 全局唯一（gspc 分配，跨场景稳定） |
| `4` | `u8` | `codec` | §8.3 注册表 |
| `5` | `u8` | `pixel_format` | §8.1；对压缩 codec 表示解码目标格式 |
| `6` | `u8` | `alpha_mode` | §8.2 |
| `7` | `u8` | `cache_policy` | 0 `MMAP_DIRECT` / 1 `DECODE_LRU` / 2 `PRELOAD` |
| `8` | `u16` | `width` | |
| `10` | `u16` | `height` | |
| `12` | `u32` | `stride` | 字节；MMAP_DIRECT 必须满足 profile 对齐 |
| `16` | `u32` | `data_off` | `RES_DATA` 内偏移；MMAP_DIRECT 必须满足 profile 对齐 |
| `20` | `u32` | `data_size` | 存储字节数 |
| `24` | `u32` | `raw_size` | 解码后字节数（MMAP_DIRECT 时 == data_size） |
| `28` | `u32` | `content_hash` | 内容 hash，跨场景/跨包去重 |
| `32` | `u32` | `aux` | 双平面 A8 的 plane 偏移；EAF 的帧数；其余 0 |
| `36` | `u32` | `reserved` | 0 |

校验继承旧协议 §8：offset/size 落界、`raw_size > 0`、STORE 类
`data_size == raw_size`、解码输出长度必须精确等于 `raw_size`。

### 4.2 codec 与 pixel_format 分离

`codec` 描述存储编码，`pixel_format` 描述像素含义（主计划 §7.4）。
`MMAP_DIRECT` 资源 renderer 以 resource view 直接 blit；压缩资源经
decode task 输出 native surface 进 LRU cache，render task 只消费
READY surface。

---

## 5. GFB：字体二进制

Sections：`0x0201 FONT_META`（REQUIRED）、`0x0202 GLYPH_TABLE`（REQUIRED）、
`0x0203 CMAP`、`0x0204 ATLAS_PAGES`（REQUIRED）、`0x0205 KERN`（可选）。

### 5.1 `FONT_META`（24 bytes）

```text
u16 font_size_px | i16 ascent | i16 descent | u16 line_height
u16 baseline | u8 atlas_format (A8/A4) | u8 flags
u16 page_count | u16 glyph_count | u32 default_glyph | u32 reserved
```

rounding 规则、baseline、bearing 语义与 host 完全一致，用 glyph-run
golden data 双端验证（主计划 §7.5）。

### 5.2 `GLYPH_TABLE`（每 glyph 16 bytes，glyph_id 即数组下标）

```text
u16 atlas_page | u16 atlas_x | u16 atlas_y
u8 w | u8 h | i8 bearing_x | i8 bearing_y | u8 advance
3B reserved
```

### 5.3 `CMAP`

两段式：ASCII `0x20..0x7E` 直接索引表（95 × u16），其后为按 codepoint
升序的 `{u32 codepoint, u16 glyph_id}` 二分表。大字符集按 Unicode range
分片为多个 GFB，按需 mmap（主计划 §7.5）。

### 5.4 `ATLAS_PAGES`

page 表 `{u32 off, u16 w, u16 h, u16 flags}` + 对齐的 A8/A4 位图数据。
A4 page 进入热路径前展开为 A8（准备阶段，非渲染帧内）。

---

## 6. 导出器（gspc）要求

继承旧协议 §12 并扩展：

1. 固定宽度 little-endian 写入；payload 只含 index/offset；
2. 布局确定后统一计算 section offset；整包组装后计算 CRC；
3. 每包同步输出 manifest：header 值、section 布局、命令统计、
   bind/action/hit/template 表、资源与字体引用、pool 容量与 RAM 预算、
   缺字报告、validation result；
4. 导出后必须以 reference loader 回跑校验 + host preview 渲染 golden；
5. `res_ref` 引用的 `resource_id` 必须存在于同 bundle 的 GRB/GFB；
6. 字段常量一律来自 `formats/` codegen 产物，禁止手写魔数。

## 7. 设备端 loader 要求

继承旧协议 §13 的"先校验、后使用、失败可回退"：

1. §2.3 容器校验 → 各 section 内部校验，全部通过后才建立运行时视图；
2. 场景激活：复制 `STATE_DEFAULTS` 进 RAM、解析 `RES_REFS` 为 resource
   view、按 manifest 分配 instance pool——此外零拷贝、零对象创建；
3. 激活失败：释放已分配 pool/cache 引用，返回错误码，包数据不产生副作用；
4. ACT_CALL 的 `action_id` 在激活期与注册表比对，缺失按配置警告或拒绝；
5. fuzz 边界：本规范所有校验规则进入 ABI parser fuzz 向量（主计划 §2.5 测试接口）。

## 8. 公共枚举注册表（append-only，值冻结）

### 8.1 `pixel_format` / `color_profile`

| 值 | 含义 |
|---:|---|
| `0` | RGB565 |
| `1` | RGB888（packed） |
| `2` | ARGB8888 |
| `3` | XRGB8888 |
| `4` | A8 |
| `5` | L8 |
| `6` | I1 |
| `7` | RGB565 + A8 双平面（`aux` = A8 plane 偏移） |

### 8.2 `alpha_mode`

| 值 | 含义 |
|---:|---|
| `0` | opaque |
| `1` | straight alpha |
| `2` | premultiplied alpha |
| `3` | 外置 A8 plane |

### 8.3 `codec`

| 值 | 含义 |
|---:|---|
| `0` | STORE（原始像素，MMAP_DIRECT 的唯一合法 codec） |
| `1` | RLE16（旧协议遗留，仅兼容导入） |
| `2` | JPEG |
| `3` | PNG |
| `4` | EAF/AAF（动画容器，`aux` = 帧数） |
| `5` | QOI（预留） |

## 9. 版本管理

继承旧协议 §14 框架，按 §2.1/§2.2 双版本号 + REQUIRED 位细化：

- **不 bump**：新增可选 section、新增 opcode/枚举值（旧 loader 拒绝未知
  REQUIRED 内容或跳过可选内容，均安全）——记 minor；
- **bump major**：改变任何已发布表项大小/字段偏移/endian/CRC 范围/offset
  语义，或新增旧 loader 必须理解的 REQUIRED section；
- v0.9 冻结为 v1.0 的前置：Phase 1 原型 benchmark 确认命令 32B、状态槽
  16B、tile u16 索引三项尺寸决策（主计划 §5）。

## 10. 非协议内容

继承旧协议 §15：具体场景的命令数量与顺序、bind/action 语义角色、资源
含义、颜色尺寸位置均属包数据，写入各包 manifest，不写入本规范。loader
不得硬编码任何具体场景拓扑。
