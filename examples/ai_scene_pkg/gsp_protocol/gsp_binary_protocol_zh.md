# GSP 二进制场景包协议

> 状态：草案 v0.1  
> 范围：`examples/ai_scene_pkg` 概念验证  
> 目的：统一上位机导出器、生成的 `.inc`、manifest 和设备端 loader 的二进制约束。

本文档是 GSP scene package 的二进制协议。导出器必须按此协议生成字节；loader 必须在创建任何 GFX object 之前校验此协议。

当前 demo 包是 `home.inc`：

```text
home_fonts[]      运行时字体绑定元数据
home_scene_pkg[]  无指针 GSP1 二进制包
home_scene_pkg_len
```

`home_scene_pkg[]` 是设备端/host 端真正消费的运行时 payload，里面禁止出现原生 C 指针。

---

## 1. 核心规则

1. 包内多字节字段统一使用 little-endian。
2. 包内字节必须位置无关。
3. 运行时 payload 禁止包含原生 C 指针。
4. object 之间的引用使用 `u16` object index。
5. string、params、blob table、blob data 的引用使用 `u32` byte offset，offset 相对 package base。
6. 所有表项大小固定：

| 表项 | 大小 |
|---|---:|
| Header | `56` bytes |
| Object entry | `64` bytes |
| Blob entry | `20` bytes |
| Action entry | `24` bytes |

7. `header.total_size` 必须等于导出的运行时 package 字节长度。
8. `header.crc32` 必须匹配 `gsp_crc32_scene()`；计算 CRC 时，CRC 字段本身按 0 处理。
9. loader 必须用错误码拒绝坏包；遇到非法 offset、非法 count、非法 parent、坏字符串、坏 blob、CRC 不匹配时不能崩溃。

---

## 2. 运行时包布局

当前布局：

```text
[Header 56B]
[Object table: obj_count * 64B]
[Blob table: blob_count * 20B]
[Action table: action_count * 24B]
[Params area]
[String table]
[Blob data]
```

所有区域都通过相对 `home_scene_pkg[]` 起始地址的绝对 byte offset 访问。

通用 offset 关系：

```text
header_off     = 0
obj_table_off  = GSP_HEADER_SIZE
blob_table_off = obj_table_off + obj_count * GSP_OBJ_SIZE
action_table_off = blob_table_off + blob_count * GSP_BLOB_SIZE
params_off     = action_table_off + action_count * GSP_ACTION_SIZE
str_table_off  = params_off + params_total_size
blob_data_off  = str_table_off + string_table_size
total_size     = blob_data_off + blob_data_size
```

---

## 3. Header

Header 大小：`GSP_HEADER_SIZE = 56`。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u32` | `magic` | 必须是 `GSP_MAGIC`，字节为 `GSP1` |
| `4` | `u32` | `version` | 必须匹配 `GSP_VERSION` |
| `8` | `u16` | `screen_w` | scene 逻辑宽度 |
| `10` | `u16` | `screen_h` | scene 逻辑高度 |
| `12` | `u32` | `screen_bg` | RGB888 背景色 |
| `16` | `u32` | `obj_count` | object entry 数量 |
| `20` | `u32` | `obj_table_off` | object table 的绝对 offset |
| `24` | `u32` | `str_table_off` | string table 的绝对 offset |
| `28` | `u32` | `blob_count` | blob entry 数量 |
| `32` | `u32` | `blob_table_off` | blob table 的绝对 offset |
| `36` | `u32` | `total_size` | package 总字节数 |
| `40` | `u32` | `crc32` | 整包 CRC32，计算时此字段视为 0 |
| `44` | `u32` | `action_count` | action entry 数量；`0` 表示无动作表 |
| `48` | `u32` | `action_table_off` | action table 的绝对 offset；无动作表时为 `0` |
| `52` | `u32` | `reserved` | v4 必须为 `0` |

校验要求：

- `magic == GSP_MAGIC`
- `version == GSP_VERSION`
- `total_size <= supplied_buffer_size`
- `total_size >= GSP_HEADER_SIZE`
- `gsp_crc32_scene(buf, total_size) == crc32`
- `obj_count > 0`
- `obj_count <= 0xFFFF`

---

## 4. Object Entry

Object entry 大小：`GSP_OBJ_SIZE = 64`。

object 使用先序排列。子节点必须出现在父节点之后。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `type` | `GSP_OBJ_*` |
| `2` | `u16` | `parent_idx` | 父 object index，或 `GSP_NO_PARENT` |
| `4` | `i16` | `x` | 相对父节点的 local x |
| `6` | `i16` | `y` | 相对父节点的 local y |
| `8` | `u16` | `w` | 宽度 |
| `10` | `u16` | `h` | 高度 |
| `12` | `u32` | `flags` | `GSP_F_*` 位 |
| `16` | `u32` | `fg_color` | RGB888 前景色 |
| `20` | `u32` | `bg_color` | RGB888 背景色 |
| `24` | `u32` | `border_color` | RGB888 边框色 |
| `28` | `u16` | `border_width` | 边框宽度 |
| `30` | `u16` | `radius` | 圆角半径 |
| `32` | `u32` | `text_off` | 指向 NUL 结尾字符串的绝对 offset |
| `36` | `u32` | `callback_off` | 指向 callback 名称字符串的绝对 offset |
| `40` | `u32` | `name_off` | 指向 object 名称字符串的绝对 offset |
| `44` | `u32` | `blob_idx` | blob table index |
| `48` | `u32` | `params_off` | 私有 params 的绝对 offset |
| `52` | `u16` | `params_len` | 私有 params 长度 |
| `54` | `u8` | `opacity` | 设置 `GSP_F_OPACITY` 时有效，范围 `0..255` |
| `55` | `u8` | `text_align` | 设置 `GSP_F_ALIGN` 时有效，取 `GSP_ALIGN_*` |
| `56` | `u16` | `font_id` | 运行时字体绑定 id |
| `58` | `u16` | `bind_id` | 数据绑定 id |
| `60` | `u32` | `reserved0` | v4 必须为 `0` |

parent 校验：

- root object 使用 `parent_idx = GSP_NO_PARENT`。
- 非 root object 必须满足 `parent_idx < current_object_index`。
- loader 必须拒绝 `parent_idx >= current_object_index`。

字符串校验：

- 如果设置了字符串相关 flag，对应 offset 必须落在 `[0, total_size)` 内。
- 字符串必须在 `total_size` 之前遇到 NUL 结束符。

params 校验：

- 如果设置 `GSP_F_PARAMS`，必须满足 `params_off + params_len <= total_size`。
- 通用 loader 先做边界校验；具体 widget 可以按 Component Profile 解析自己的 params。

---

## 5. Object 类型：基础组件身份

`type` 只回答一个问题：这个 entry 要创建哪一种基础组件。它不定义属性集合，也不表示某个字段一定会被该组件使用。

当前 v4 object type 注册表：

| Type ID | 符号 | 基础组件 | Loader 创建 | 资源依赖 |
|---:|---|---|---|---|
| `1` | `GSP_OBJ_CONTAINER` | container | `gfx_container_create()` | 无 |
| `2` | `GSP_OBJ_LABEL` | label | `gfx_label_create()` | `font_id` 可选 |
| `3` | `GSP_OBJ_BUTTON` | button | `gfx_button_create()` | `font_id` 可选，`callback_off` 可选 |
| `4` | `GSP_OBJ_IMAGE` | image | `gfx_image_create()` | `blob_idx` 指向 Blob table |
| `5` | `GSP_OBJ_LIST` | list | `gfx_list_create()` | `font_id` 可选，`GSP_F_PARAMS` 可选 |
| `6` | `GSP_OBJ_WHEEL` | wheel | `gfx_wheel_create()` | `font_id` 可选，`GSP_F_PARAMS` 可选 |
| `7` | `GSP_OBJ_LAYER` | layer | `gfx_container_create()` | 可作为 `GSP_ACT_GOTO` 目标 |

未知 object type 必须返回 `GSP_ERR_TYPE`。

---

## 6. Object 属性字段：通用有效位

flags 是“Object Entry 里哪些字段有效”的通用 bitset。它不是控件类型定义。某个属性对某个控件是否有意义，由第 7 节 Component Profile 决定。

当前 v4 属性字段注册表：

| 属性 | Flag | Bit / Hex | Object Entry 字段 | 值类型 | 通用校验 |
|---|---|---:|---|---|---|
| text | `GSP_F_TEXT` | `0 / 0x001` | `text_off` | UTF-8 NUL string offset | offset 必须在包内且 NUL 结尾 |
| fg color | `GSP_F_FG_COLOR` | `1 / 0x002` | `fg_color` | RGB888 | 无额外 offset 校验 |
| bg color | `GSP_F_BG_COLOR` | `2 / 0x004` | `bg_color` | RGB888 | 无额外 offset 校验 |
| border | `GSP_F_BORDER` | `3 / 0x008` | `border_color`, `border_width` | RGB888 + u16 | 无额外 offset 校验 |
| radius | `GSP_F_RADIUS` | `4 / 0x010` | `radius` | u16 | 无额外 offset 校验 |
| callback | `GSP_F_CALLBACK` | `5 / 0x020` | `callback_off` | UTF-8 NUL string offset | offset 必须在包内且 NUL 结尾 |
| image resource | `GSP_F_IMAGE` | `6 / 0x040` | `blob_idx` | u32 blob index | `blob_idx < blob_count` |
| name | `GSP_F_NAME` | `7 / 0x080` | `name_off` | UTF-8 NUL string offset | offset 必须在包内且 NUL 结尾 |
| hidden | `GSP_F_HIDDEN` | `8 / 0x100` | flags only | boolean | 无额外字段 |
| opacity | `GSP_F_OPACITY` | `9 / 0x200` | `opacity` | u8, `0..255` | 预留，当前 loader 未应用 |
| text align | `GSP_F_ALIGN` | `10 / 0x400` | `text_align` | `GSP_ALIGN_*` | 值应在已定义枚举内 |
| private params | `GSP_F_PARAMS` | `11 / 0x800` | `params_off`, `params_len` | byte range | `params_off + params_len <= total_size` |

导出器不应设置无对应 flag 的字段。loader 可以忽略未启用字段，但必须校验已启用字段的 offset/index。

---

## 7. Component Profile：基础组件属性矩阵

Component Profile 定义每类基础组件如何解释第 6 节的通用属性字段。导出器和设备端 loader 应以这里作为“控件属性能力”的协商面。

### 7.1 通用字段

所有 object 都必须支持这些结构字段：

| 字段 | 是否需要 flag | 含义 |
|---|---|---|
| `type` | 否 | 基础组件类型 |
| `parent_idx` | 否 | 父子关系 |
| `x/y/w/h` | 否 | 相对父节点的局部布局矩形 |
| `flags` | 否 | 有效字段位 |
| `GSP_F_HIDDEN` | 是 | 初始可见性 |
| `GSP_F_NAME` | 是 | object 名称 offset，loader 校验后建立 `name -> gfx_object_t` 运行期查找表 |
| `GSP_F_PARAMS` | 是 | 组件私有扩展参数块；list/wheel 已定义 params-v1 |

### 7.2 v4 属性支持矩阵

`Y` 表示 v4 loader 会消费该属性；空白表示该组件不应导出该属性。

| 属性 | 字段 / 资源 | container | label | button | image | list | wheel | layer | 说明 |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|
| layout | `x/y/w/h` | Y | Y | Y | Y | Y | Y | Y | 所有组件都设置位置和尺寸 |
| hidden | `GSP_F_HIDDEN` | Y | Y | Y | Y | Y | Y | Y | 初始隐藏 |
| name | `GSP_F_NAME` | Y | Y | Y | Y | Y | Y | Y | 建立运行期 name lookup |
| params | `GSP_F_PARAMS` |  |  |  |  | Y | Y |  | list/wheel params-v1 items |
| bg color | `GSP_F_BG_COLOR`, `bg_color` | Y |  | Y |  | Y | Y | Y | 背景色 |
| fg color | `GSP_F_FG_COLOR`, `fg_color` |  | Y | Y |  | Y | Y |  | 文本色 |
| border | `GSP_F_BORDER`, `border_color/border_width` | Y |  | Y |  | Y | Y | Y | 边框 |
| radius | `GSP_F_RADIUS`, `radius` | Y |  | Y |  |  |  | Y | 容器/按钮/layer 圆角 |
| text | `GSP_F_TEXT`, `text_off` |  | Y | Y |  |  |  |  | 文本内容 |
| font | `font_id` |  | Y | Y |  | Y | Y |  | runtime font binding |
| text align | `GSP_F_ALIGN`, `text_align` |  | Y |  |  |  |  |  | 当前 button/list/wheel 未接 align |
| callback | `GSP_F_CALLBACK`, `callback_off` |  |  | Y |  |  |  |  | callback 名称绑定到运行时函数 |
| image | `GSP_F_IMAGE`, `blob_idx` |  |  |  | Y |  |  |  | blob 解码后绑定为 image source |
| opacity | `GSP_F_OPACITY`, `opacity` | reserved | reserved | reserved | reserved | reserved | reserved | reserved | 字段预留，当前 loader 未应用 |

### 7.3 导出器约束

| 规则 | 要求 |
|---|---|
| 属性范围 | 只为目标组件设置矩阵中支持的属性 flag |
| profile 外属性 | 必须先扩展协议，或通过 `GSP_F_PARAMS` 定义私有参数格式 |
| `font_id` | 只给 text-like 组件使用；非文本组件应置 0 |
| `callback_off` | 只给可交互组件使用；当前 v4 只有 button 消费它 |
| `blob_idx` | 只在设置 `GSP_F_IMAGE` 时使用；当前 v4 只有 image 消费它 |
| `opacity` | 当前只允许作为预留字段，不应依赖运行时效果 |

### 7.4 设备端约束

| 阶段 | 要求 |
|---|---|
| 通用校验 | 先按 Header/Object/Blob/String 的通用规则校验 offset/index |
| 属性应用 | 再按 Component Profile 应用属性 |
| 不支持 flag | bring-up 阶段建议拒绝；产品阶段可按版本协商选择忽略或拒绝 |
| scene 拓扑 | 不能根据 demo 的 object 顺序或控件名称硬编码逻辑 |

---

## 8. Blob Entry

Blob entry 大小：`GSP_BLOB_SIZE = 20`。

blob entry 描述烘焙进包里的二进制资源，目前主要是图片像素。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `w` | 图片宽度 |
| `2` | `u16` | `h` | 图片高度 |
| `4` | `u8` | `cf` | `gfx_color_format_t` |
| `5` | `u8` | `codec` | `GSP_CODEC_*` |
| `6` | `u16` | `stride` | 每行字节数，或 `0` 表示默认 |
| `8` | `u32` | `raw_size` | 解码后字节数 |
| `12` | `u32` | `comp_size` | 存储字节数 |
| `16` | `u32` | `data_off` | 指向压缩字节的绝对 offset |

当前 codec：

| ID | 名称 | 含义 |
|---:|---|---|
| `0` | `GSP_CODEC_STORE` | 原始字节直接存储 |
| `1` | `GSP_CODEC_RLE16` | RGB565 游程编码，token = `u16 count + u16 pixel` |

blob 校验：

- `blob_idx < blob_count`
- `data_off + comp_size <= total_size`
- `raw_size > 0`
- 对 `STORE`，要求 `comp_size == raw_size`
- 对 `RLE16`，解码字节数必须恰好等于 `raw_size`

当前 image 流程：

```text
BlobEntry
  -> blob_get()
      -> 解码 STORE/RLE16 到 scene 生命周期内 RAM
      -> 填充 gfx_image_dsc_t
      -> gfx_image_set_source_desc()
```

---

## 9. String Table

字符串使用 UTF-8，并以 NUL 结束。

offset 是相对 package base 的绝对 byte offset。

string table 可以包含：

- label 文本
- button 文本
- callback 名称
- object 名称

string table 内字符串不要求排序。导出器可以对相同字符串做去重。

---

## 10. 字体绑定协议

当前 v4 把字体描述放在 `home_scene_pkg[]` 外部：

```c
static const gsp_font_desc_t home_fonts[] = { ... };
```

这是 `.inc` 里的 C 元数据，不属于无指针二进制运行时 payload。

object entry 通过 `font_id` 引用字体。运行时映射关系：

```text
ObjEntry.font_id -> gsp_font_binding_t.id -> gfx_font_t
```

导出器约束：

- 每个 label/button 的 `font_id` 都应该能在 `home_fonts[].id` 中找到。
- font path 是逻辑作者信息。host 和 device 可以有不同解析方式。

loader 约束：

- 如果找不到 `font_id`，loader 可以使用 `default_font`。
- 设备端集成时需要决定字体来自文件、flash asset 还是预创建 font handle。

未来方向：

- 将 font table record 移入二进制 package。
- `gfx_font_t` handle 仍然只存在于运行时。

---

## 11. `.inc` 元数据协议

生成的 `.inc` 可以带 C 预处理宏，方便编译期使用：

```c
#define HOME_SCREEN_W
#define HOME_SCREEN_H
#define HOME_COLOR_FMT
#define HOME_OBJ_COUNT
#define HOME_FONT_COUNT
```

重要边界：

- `home_scene_pkg[]` header 是运行时加载的事实来源。
- `HOME_SCREEN_W/H` 应与 header `screen_w/screen_h` 一致。
- `HOME_OBJ_COUNT` 应与 header `obj_count` 一致。
- `HOME_COLOR_FMT` 目前还不是 v4 binary header 字段。

当前 `HOME_COLOR_FMT` 含义：

```text
0 = 未指定 / 由 display 或 backend 决定输出格式
```

图片颜色格式存储在每个 blob 的 `BlobEntry.cf` 中。

如果后续设备端/导出器需要 scene-level output format，应把 `HOME_COLOR_FMT` 升级为真实 header 字段或 reserved header 扩展。当前不要从这个宏静默推断运行时 framebuffer format。

---

## 12. 导出器要求

生成 `.inc` 的导出器必须同步生成匹配的 manifest。

必须输出：

```text
home.inc
gsp_export/home.manifest.md
可选源图预览，例如 gsp_export/home_preview.bmp
```

导出器必须：

1. 使用固定宽度 little-endian 写入所有字段。
2. 运行时 payload 只使用 index 和 offset。
3. 在最终布局确定后计算所有 table offset。
4. 在整包组装完成后计算 CRC。
5. 输出 `home_scene_pkg_len = sizeof(home_scene_pkg)`。
6. 输出或更新 `gsp_export/home.manifest.md`。
7. 导出后运行 loader 校验。

manifest 必须包含：

- header 值
- 二进制区域布局
- object table
- string table
- font bindings
- blob table
- validation result

---

## 13. 设备端 Loader 要求

设备端 loader 在创建 object 前必须校验：

1. header size
2. magic
3. version
4. total size
5. CRC
6. object table bounds
7. blob table bounds
8. object count limits
9. parent indexes
10. enabled string offsets
11. enabled params offsets
12. enabled blob indexes

设备端 loader 应尽量在足够校验之后再创建 object，避免坏包造成半棵树。

如果中途创建失败：

- 销毁已创建 object
- 释放已解码 blob buffer
- 返回错误码

---

## 14. 版本管理

当前版本：

```text
GSP_VERSION = 4
```

任何二进制字段布局变化都必须变更 version。

通常不需要 bump version 的兼容变化：

- 新增 object type ID，旧 loader 会拒绝未知 type
- 新增 flag bit，旧 exporter 不输出这些 bit
- 新增 codec ID，旧 loader 会拒绝未知 codec

必须 bump version 的不兼容变化：

- 改变 header/object/blob entry 大小
- 复用已有字段 offset
- 改变 endian
- 改变 CRC 覆盖范围
- 改变 string/offset 解释方式

当前 v4 已引入 Action Table（事件→动作），header 为 `56` 字节，见 §16。

---

## 15. 非协议内容：具体 Scene 数据

本协议不定义任何固定 scene object 列表。

以下内容都属于具体 package 的 scene 数据，必须写在每个 package 自己的 manifest 中，而不是写进通用协议：

- object 数量
- object 顺序
- widget 名称和语义角色
- 父子树结构
- 文本内容
- callback 名称
- blob 数量和资源含义
- 实际颜色、尺寸和位置

例如，当前 `home.inc` 有自己的 object table 和 blob table，但这些细节不属于通用二进制协议。它们应该只出现在 `gsp_export/home.manifest.md` 中。

协议只定义任意 package 如何描述这些信息：

```text
ObjectEntry[N]  -> type, parent_idx, rect, flags, string offsets, blob index, font id
BlobEntry[M]    -> dimensions, format, codec, raw/comp sizes, data offset
String table    -> UTF-8 strings referenced by offset
```

设备端 loader 必须实现上面的通用规则，不能硬编码任何 demo scene 的拓扑结构。

---

## 16. 动作表 Action Table（v4）

> 状态：v4 已实现。本节定义“可交互场景”所需的
> **位置无关 事件→动作 表**。对标 ITE 的 `ITUAction[]`（事件触发、按名/索引找目标、
> 带参数），但运行时 payload 里**只有 u16 索引 + u32 偏移，禁止任何原生指针**，因此
> 同一份字节在 host(64) / device(32) 解析一致，且每个引用都能做边界校验。

### 16.0 目的

把“行为”变成数据：谁（源 object）在什么事件下，对谁（目标 object）做什么动作、带什么参数。
这样交互无需改固件——导出器/大模型直接生成动作表，loader 统一派发。

### 16.1 v4 Header 变化

v4 把 Header 由 `48` 扩到 `GSP_HEADER_SIZE = 56` 字节，新增两个字段（属于必须 bump version 的变化，见 §14）：

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `44` | `u32` | `action_count` | action entry 数量；`0` = 无动作表（等价 v3 行为）|
| `48` | `u32` | `action_table_off` | action table 的绝对 offset；`action_count==0` 时为 `0` |
| `52` | `u32` | `reserved` | v4 必须为 `0` |

- v3 的 off 44 `reserved` 在 v4 被重新定义为 `action_count`。因 v3 要求该字段为 `0`，v4 包在旧 v3 loader 上会因 `version` 不符被拒（不会误读），兼容边界安全。

### 16.2 运行时包布局（v4）

```text
[Header 56B]
[Object table: obj_count * 64B]
[Blob table: blob_count * 20B]
[Action table: action_count * 24B]     ← v4 新增
[Params area]
[String table]
[Blob data]
```

offset 关系（v4）：

```text
obj_table_off    = GSP_HEADER_SIZE                          (= 56)
blob_table_off   = obj_table_off   + obj_count   * GSP_OBJ_SIZE
action_table_off = blob_table_off  + blob_count  * GSP_BLOB_SIZE
params_off       = action_table_off + action_count * GSP_ACTION_SIZE
str_table_off    = params_off + params_total_size
blob_data_off    = str_table_off + string_table_size
total_size       = blob_data_off + blob_data_size
```

### 16.3 Action Entry

Action entry 大小：`GSP_ACTION_SIZE = 24`。

| Offset | 类型 | 字段 | 含义 |
|---:|---|---|---|
| `0` | `u16` | `src_idx` | 触发事件的源 object index；当前必须 `< obj_count` |
| `2` | `u16` | `event` | `GSP_EV_*`，触发条件 |
| `4` | `u16` | `action` | `GSP_ACT_*`，要执行的动作 |
| `6` | `u16` | `target_idx` | 目标 object index；`GSP_ACT_NO_TARGET = 0xFFFF` = 无 index 目标（用名字或对 src 自身）|
| `8` | `u32` | `target_name_off` | 可选：目标名 / 回调函数名字符串的绝对 offset；`0` = 用 `target_idx` |
| `12` | `u32` | `param_off` | 可选：参数块绝对 offset（如 SET_TEXT 的字符串）；`0` = 无 |
| `16` | `u16` | `param_len` | 参数块字节数 |
| `18` | `u16` | `flags` | `GSP_AF_*`（预留，v4 必须为 `0`）|
| `20` | `u32` | `arg` | 内联小整数参数（如 GOTO 的 scene id、SET_BG_COLOR 的 RGB888、SET_VAR 的槽位/值、delay 帧数）|

### 16.4 事件枚举 `GSP_EV_*`（append-only，值冻结）

| 值 | 符号 | 含义 |
|---:|---|---|
| `0` | `GSP_EV_NONE` | 无事件 |
| `1` | `GSP_EV_CLICK` | 按下并在控件内抬起（click 语义）|
| `2` | `GSP_EV_PRESS` | 按下 |
| `3` | `GSP_EV_RELEASE` | 抬起 |
| `4` | `GSP_EV_LONG` | 长按（预留）|
| `5` | `GSP_EV_VALUE` | 值变化（预留）|

### 16.5 动作枚举 `GSP_ACT_*`（append-only，值冻结）

| 值 | 符号 | 用到的字段 | 含义 |
|---:|---|---|---|
| `0` | `GSP_ACT_NONE` | — | 空动作 |
| `1` | `GSP_ACT_SHOW` | `target_idx` | 显示目标 |
| `2` | `GSP_ACT_HIDE` | `target_idx` | 隐藏目标 |
| `3` | `GSP_ACT_TOGGLE` | `target_idx` | 切换可见 |
| `4` | `GSP_ACT_SET_TEXT` | `target_idx`, `param_off/len` | 设置目标文本 |
| `5` | `GSP_ACT_SET_BG_COLOR` | `target_idx`, `arg`(RGB888) | 设置背景色 |
| `6` | `GSP_ACT_SET_OPACITY` | `target_idx`, `arg`(0..255) | 设置不透明度 |
| `7` | `GSP_ACT_CALL` | `target_name_off` | 调用宿主注册函数（按名绑定，等价现 callback）|
| `8` | `GSP_ACT_GOTO` | `target_idx` 或 `target_name_off` | 显示目标 layer，并隐藏同 parent 下其它 layer |
| `9` | `GSP_ACT_BACK` | — | 返回上一层（预留） |

未知 `event` / `action`：bring-up 阶段建议拒绝（`GSP_ERR_ACTION`），产品阶段可按版本协商忽略。

### 16.6 校验规则

- `src_idx < obj_count`。
- `target_idx == GSP_ACT_NO_TARGET` 或 `target_idx < obj_count`。
- 若 `target_name_off != 0`：offset 落在 `[0, total_size)` 且在 `total_size` 前 NUL 结尾。
- 若 `param_off != 0`：`param_off + param_len <= total_size`。
- `flags` 必须为 `0`（v4）。
- `event` / `action` 在已定义枚举内（见 16.5 未知值处理）。
- 全部引用为 index/offset，loader 建树后把 `src_idx/target_idx` 解析为 `gfx_object_t*` 缓存，**运行时才有指针**。

### 16.7 运行时派发模型（loader）

```text
1. 先按 §13 建好 object 树。
2. 遍历 action table，逐条校验（16.6）；把 src_idx/target_idx 解析成 gfx_object_t*，
   把 target_name_off（若有）解析成回调名，建成运行期 action 列表 rt_actions[]。
3. 对每个“有入边事件”的源 object，按其 event 类型挂底层监听：
     - 触摸类(CLICK/PRESS/RELEASE) -> gfx_object_set_touch_cb 合成
     - LONG/VALUE 当前预留
4. 事件到达时：扫描 rt_actions[] 中 (src, event) 匹配项，依次执行 action：
     SHOW/HIDE/TOGGLE/SET_* -> 调对应 gfx setter 到 target
     GOTO                   -> 显示目标 layer，隐藏同 parent 下其它 layer
     CALL                   -> 用 cb 绑定表按 target_name 找宿主函数并调用
     SET_VAR                -> 写运行时变量表（触发绑定刷新）
5. 任一步校验失败：销毁已建对象/已解码 blob，返回错误码，不崩（同 §13）。
```

- 派发只依赖运行期解析出的指针缓存；**包内永远是 index/offset**，符合 §1 核心规则。

### 16.8 与 ITU 的对比（为什么这样才位置无关）

| 维度 | ITU `ITUAction[]` | GSP Action Table |
|---|---|---|
| 目标引用 | inline `char target[N]` + 运行时 `void* cachedTarget` | `target_idx`(u16) / `target_name_off`(u32)，指针只在运行期缓存 |
| 参数 | inline `char param[64]` 定长 | `param_off/len` 变长 + `arg` 内联小整数 |
| 挂载 | 每 widget/layer inline `actions[N]` | 独立 action table，按 `src_idx` 关联，去膨胀 |
| 平台 | 32 位内存镜像 + 指针回写 | 位置无关，host/device 同解析 |
| 坏包 | 野指针难校验 | index/offset 全边界校验 |

一句话：**把 ITU 的“行为即数据”思想搬过来，但用“表 + 索引/偏移”替换“inline 结构 + 指针”**，两全其美。

### 16.9 导出器约束

- 只在 `action_count > 0` 时写 action table，并同步写 header 的 `action_count` / `action_table_off`。
- `src_idx` 必须指向真实存在的 object；`target_idx` 若非 `GSP_ACT_NO_TARGET` 也必须指向真实 object。
- `GSP_ACT_CALL` 的 `target_name_off` 必须能在宿主 cb 绑定表中找到（否则运行期忽略）。
- action table 参与 `total_size` 与 CRC 计算（同其它区域）。
- manifest 必须列出 action table（源/事件/动作/目标/参数），供审阅与大模型对齐。

### 16.10 版本

- 引入 action table = **v4**（header 尺寸变化，按 §14 必须 bump）。
- `action_count == 0` 的 v4 包语义等价 v3（纯静态场景）。
