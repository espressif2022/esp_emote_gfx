# Scene Package 二进制格式 v1（`.gsp`）

> 上一篇：[TODO List](07-todo.md) ｜ 返回：[README / 索引](README.md)
>
> 本页固化 [07-todo.md](07-todo.md) 的 D1（格式定义）、D4（loader）、D5（callback 绑定）、D6（校验）契约。属性字段以 [03-scene-format.md](03-scene-format.md) 的「属性白名单」为唯一数据源；本页只定义**它们如何被编码进二进制，以及如何还原成设备端 `gfx_object_t` object tree**。

## 0. 设计目标与约束

1. **Host 预览（64-bit）与设备（32-bit）共用同一个 loader**（见 [08-risks 1.3](08-risks-and-references.md)）。→ 格式**不得依赖指针宽度、结构体内存布局、编译器对齐**。
2. **设备端不解析 JSON / 字符串属性**，颜色/坐标/索引全部由上位机 compiler 预处理。
3. **坏包不得导致设备端崩溃**（D6）：所有引用用**整数索引 + 显式 count/offset**，可做边界校验。
4. **对象树用 factory 还原**：`gfx_<type>_create()` + setter，而不是内存镜像重定位（理由见 [第 8 节](#8-与-itu-加载方式的对比与取舍)）。
5. 第一版只覆盖 `container` / `label` / `button` 三种 widget，绝对坐标。

## 1. 顶层布局

`.gsp` 由一个定长 header + 若干「表段」组成。所有多字节整数一律 **little-endian**；所有表段起始 **4 字节对齐**。文件内部**不存任何指针**，只存 `u32` 字节偏移（相对文件头）与整数索引。

```text
┌────────────────────────────────────────────┐
│ header            (定长 64 字节)             │
├────────────────────────────────────────────┤
│ object_table      obj_count × ObjEntry(28B)  │
├────────────────────────────────────────────┤
│ style_table       style_count × StyleEntry   │
├────────────────────────────────────────────┤
│ string_table      offset[] + utf8_blob       │
├────────────────────────────────────────────┤
│ callback_table    name_off[] (→string_table) │
├────────────────────────────────────────────┤
│ (预留) resource_table / asset_blob           │
└────────────────────────────────────────────┘
```

> 表的物理顺序不强制，loader 只依赖 header 里的 offset/count 定位；但建议按上表顺序，便于 dump 调试。

## 2. Header（64 字节，定长）

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0x00 | `magic` | `u8[4]` | `'G','S','P','\0'`（0x00505347 LE） |
| 0x04 | `version` | `u16` | 格式版本，v1 = `1` |
| 0x06 | `header_size` | `u16` | = 64；用于向后兼容跳过未知 header 尾部 |
| 0x08 | `file_size` | `u32` | 整包字节数，用于完整性校验 |
| 0x0C | `flags` | `u32` | 预留=0（颜色一律以 RGB888 存储，见 §7） |
| 0x10 | `screen_w` | `u16` | 必须等于目标显示分辨率宽 |
| 0x12 | `screen_h` | `u16` | 必须等于目标显示分辨率高 |
| 0x14 | `screen_bg` | `u32` | 屏幕背景色，RGB888（`0x00RRGGBB`，见 §7） |
| 0x18 | `obj_count` | `u16` | object_table 条目数 |
| 0x1A | `style_count` | `u16` | style_table 条目数 |
| 0x1C | `string_count` | `u16` | string_table 条目数 |
| 0x1E | `callback_count` | `u16` | callback_table 条目数 |
| 0x20 | `object_table_off` | `u32` | object_table 起始偏移 |
| 0x24 | `style_table_off` | `u32` | style_table 起始偏移 |
| 0x28 | `string_table_off` | `u32` | string_table 起始偏移 |
| 0x2C | `callback_table_off` | `u32` | callback_table 起始偏移 |
| 0x30 | `resource_table_off` | `u32` | 预留，v1 = 0 |
| 0x34 | `crc32` | `u32` | 除本字段外全文件 CRC32（0 = 不校验） |
| 0x38 | `target_color_format` | `u8` | 面板像素格式**提示**（`GFX_COLOR_FORMAT_*`）：仅用于预览一致性/校验，渲染时以 display-port 实际配置为准；0=未指定 |
| 0x39 | `reserved` | `u8[7]` | 预留清零 |

## 3. object_table —— 树的骨架

每条 `ObjEntry` 定长 **28 字节**，**按先序（pre-order DFS）排列**：父节点一定排在其所有子孙之前。

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0x00 | `type` | `u8` | widget 类型枚举（见下） |
| 0x01 | `flags` | `u8` | bit0=`visible`，bit1=`bg_enable`，bit2=`clip_children`，bit3=`border_dash` |
| 0x02 | `style_idx` | `u16` | → style_table；`0xFFFF` = 无 style |
| 0x04 | `parent_idx` | `i16` | 父节点在本表中的下标；`-1` = 顶层（挂到 display） |
| 0x06 | `id_str` | `u16` | → string_table，widget 的字符串 id；`0xFFFF` = 无 |
| 0x08 | `text_str` | `u16` | → string_table，text 内容；`0xFFFF` = 无 |
| 0x0A | `callback_idx` | `u16` | → callback_table；`0xFFFF` = 无 |
| 0x0C | `x` | `i16` | 显示像素 |
| 0x0E | `y` | `i16` | |
| 0x10 | `w` | `u16` | |
| 0x12 | `h` | `u16` | |
| 0x14 | `align` | `u8` | 文本对齐枚举（label/button），0=auto |
| 0x15 | `long_mode` | `u8` | label 长文本模式枚举 |
| 0x16 | `font_str` | `u16` | → string_table，字体**注册名**（见 §6.1 / A11）；`0xFFFF` = 默认字体 |
| 0x18 | `reserved` | `u8[4]` | 预留清零，保证 28B 且 4 字节对齐 |

**为什么用 `parent_idx` 而不是 ITU 的 child/sibling 偏移**：先序 + parent_idx 可以**一遍线性扫描建树**，且每个 `parent_idx` 只要校验 `-1 <= parent_idx < 当前下标` 即可保证「无环、父在子前、不越界」，坏包无法构造出野引用（对比见 §8）。

### 3.1 枚举 / flag 数值编码表（compiler 与 loader 共用，值冻结）

所有枚举值一经分配**永不复用/改动**，新增只能往后追加。

**`type`（`ObjEntry.type`, u8）** ← JSON `type`

| 值 | 常量 | JSON |
|----|------|------|
| 0 | `GSP_OBJ_CONTAINER` | `"container"` |
| 1 | `GSP_OBJ_LABEL` | `"label"` |
| 2 | `GSP_OBJ_BUTTON` | `"button"` |

**`align`（`ObjEntry.align`, u8）** ← JSON `align`，对应 `gfx_text_align_t`

| 值 | 常量 | JSON |
|----|------|------|
| 0 | `GFX_TEXT_ALIGN_AUTO` | `"auto"`（缺省） |
| 1 | `GFX_TEXT_ALIGN_LEFT` | `"left"` |
| 2 | `GFX_TEXT_ALIGN_CENTER` | `"center"` |
| 3 | `GFX_TEXT_ALIGN_RIGHT` | `"right"` |

**`long_mode`（`ObjEntry.long_mode`, u8）** ← JSON `long_mode`，对应 `gfx_label_long_mode_t`

| 值 | 常量 | JSON |
|----|------|------|
| 0 | `GFX_LABEL_LONG_WRAP` | `"wrap"`（缺省） |
| 1 | `GFX_LABEL_LONG_SCROLL` | `"scroll"` |
| 2 | `GFX_LABEL_LONG_CLIP` | `"clip"` |
| 3 | `GFX_LABEL_LONG_SCROLL_SNAP` | `"scroll_snap"` |

> 上面 3 张枚举值**刻意与 `include/gfx/widgets/label.h` 的 C 枚举顺序对齐**，loader 可直接 `(gfx_text_align_t)align` 转换，无需查表。

**`ObjEntry.flags`（u8）位定义** ← JSON 对应 bool 字段

| bit | 名称 | JSON 字段 | 缺省 |
|-----|------|-----------|------|
| 0 | `GSP_F_VISIBLE` | `visible` | 1（true） |
| 1 | `GSP_F_BG_ENABLE` | `bg_enable`（label/container） | 见白名单 |
| 2 | `GSP_F_CLIP_CHILDREN` | `clip_children`（container） | 0 |
| 3 | `GSP_F_BORDER_DASH` | `border_dash`（container） | 0 |
| 4-7 | 预留 | — | 0 |

### 3.2 JSON 字段 → package → widget setter 三列映射（compiler + loader 照此实现）

字段定义唯一数据源仍是 [03-scene-format.md](03-scene-format.md) §2 白名单；本表只说明**每个字段落到 package 的哪里、loader 用哪个 setter 还原**。

| JSON 字段 | 适用 | package 落点 | loader setter |
|-----------|------|--------------|---------------|
| `type` | 全部 | `ObjEntry.type` | `factory[type](disp)` |
| `id` | 全部 | `ObjEntry.id_str` → str | 记入 scene id 表（`gfx_scene_find_obj`） |
| `x`,`y` | 全部 | `ObjEntry.x/y` | `gfx_object_set_pos` |
| `w`,`h` | 全部 | `ObjEntry.w/h` | `gfx_object_set_size` |
| `visible` | 全部 | `flags.VISIBLE` | `gfx_object_set_visible` |
| `children` | container | 展开为子 `ObjEntry` + `parent_idx` | `gfx_object_add_child(parent,child)` |
| `text` | label,button | `ObjEntry.text_str` → str | `gfx_label_set_text` / `gfx_button_set_text` |
| `color` | label | `StyleEntry.text_color` | `gfx_label_set_color` |
| `text_color` | button | `StyleEntry.text_color` | `gfx_button_set_text_color` |
| `bg_color` | 全部 | `StyleEntry.bg_color` | `gfx_{label,button,container}_set_bg_color` |
| `bg_color_pressed` | button | `StyleEntry.bg_color_pressed` | `gfx_button_set_bg_color_pressed` |
| `bg_enable` | label,container | `flags.BG_ENABLE` | `gfx_{label,container}_set_bg_enable` |
| `radius` | button,container | `StyleEntry.radius` | `gfx_{button,container}_set_radius` |
| `border_color` | button,container | `StyleEntry.border_color` | `gfx_{button,container}_set_border_color` |
| `border_width` | button,container | `StyleEntry.border_width` | `gfx_{button,container}_set_border_width` |
| `clip_children` | container | `flags.CLIP_CHILDREN` | `gfx_container_set_clip_children` |
| `align` | label,button | `ObjEntry.align` | `gfx_{label,button}_set_text_align` |
| `long_mode` | label | `ObjEntry.long_mode` | `gfx_label_set_long_mode` |
| `font` | label,button | `ObjEntry.font_str` → str | `gfx_{label,button}_set_font`（registry 解析，§6.1） |
| `font_size` | label,button | 归一进 `font_str`（不单独存） | — （见 §6.1） |
| `callback` | button | `ObjEntry.callback_idx` → cb_table | 记入 scene 绑定表（`gfx_scene_bind_callback`，§6） |
| `screen.bg_color` | scene | `header.screen_bg` | `gfx_display_set_bg_color` |

> `label.bg_color` / `container.bg_color` 与 `button.bg_color` 都落到 `StyleEntry.bg_color`；apply 时按 `type` 分派到对应 widget 的 setter。

## 4. style_table —— 样式去重共享

把颜色/边框/圆角等**成组样式**抽出来去重，多个 widget 可共享同一 `style_idx`（灵感来自 ITU/motion 的 palette 复用）。所有颜色统一存 **RGB888（`u32`，`0x00RRGGBB`）**（理由见 §7）。`StyleEntry` 定长 **24 字节**：

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0x00 | `bg_color` | `u32` | 背景色 / button 普通态背景，RGB888 |
| 0x04 | `text_color` | `u32` | 文本色（label.color / button.text_color），RGB888 |
| 0x08 | `border_color` | `u32` | 边框色，RGB888 |
| 0x0C | `bg_color_pressed` | `u32` | button 按下态背景，RGB888；无则 = `bg_color` |
| 0x10 | `radius` | `u16` | 圆角 |
| 0x12 | `border_width` | `u8` | 边框宽 |
| 0x13 | `opa` | `u8` | 不透明度 0-255，默认 255 |
| 0x14 | `reserved` | `u8[4]` | 预留清零，保证 24B 且 4 字节对齐 |

> 颜色一律 888 存储是**无损**的（保留作者 `#RRGGBB` 全精度）；loader 加载时用 `gfx_color_hex()` 转成设备当前的 `gfx_color_t`。因为 style 已去重，`u32` 相对 `u16` 的体积代价可忽略，换取「面板无关 + 未来色深升级不用重编包」。

## 5. string_table —— 字符串池

```text
string_table_off →
  u32 blob_off          // utf8_blob 相对文件头的偏移
  u32 offset[string_count]   // 每条字符串在 blob 内的起始偏移(相对 blob_off)
  ... (对齐)
  utf8_blob:            // 连续存放, 每条以 '\0' 结尾
    "ok_btn\0" "Start\0" "on_ok\0" ...
```

- 所有 id / text / callback 名 / font 注册名统一进这里，**去重**（相同字符串只存一份）。
- 每条**必须以 `\0` 结尾**，且 loader 校验「`offset[i]` 落在 blob 内、且在 blob 尾部前能找到 `\0`」。

### 5.1 字符串生命周期契约（已定）

先厘清现有实现：`gfx_label_set_text()` 内部是 **malloc + memcpy 拷贝**（见 `src/widgets/label/gfx_label.c`），widget 自己持有文本副本。因此：

- **label / button 的 text 在 `set_text` 时已被复制到 widget 堆内存**，加载后不再引用 package blob。
- 加载后仍引用 blob 的只有 scene 句柄需要保留的**小字符串**：widget `id`（供 `gfx_scene_find_obj`）与 callback 名（供 `gfx_scene_bind_callback`）。
- **契约：package buffer（`data`）必须在 `gfx_scene` 存活期间保持有效。** 设备端天然满足（mmap 到 flash，常驻）；host 端由 demo/调用方持有 blob 直到 `gfx_scene_delete`。这样 **host 与 device 同一套契约，无分叉、零额外拷贝**。
- 「ITU 式 label 文本就地零 copy」**不适用于第一版**：label 强制拷贝文本，真正零 copy 需要一个 widget 级 `set_text_static`（引用只读常量、不拷贝）新 API，列为后续 RAM 优化项，不在 v1。

## 6. callback_table —— 只存名字，运行时绑定

```text
callback_table_off →
  u16 name_str[callback_count]   // 每个回调名 → string_table 索引
```

- 包内**只有回调名字符串索引**，绝不存 C 函数指针（ITU 也是加载时才重新 bind 函数指针，见 §8）。
- 设备端：`gfx_scene_load_package()` 建树时把 button 的 `callback_idx` 记到 scene 的绑定表；业务侧调用
  `gfx_scene_bind_callback(scene, "on_ok", cb, user_data)` 完成实际绑定（对应 D5 / A12）。
- **click 语义**统一：底层用 `gfx_object_set_touch_cb`，loader 内部包一层「按下 + 在范围内抬起 = click」再回调用户 cb（见 A12）。

### 6.1 字体解析：按注册名解析，不打包字体（已定）

对应 A11 / D1「只引用资源名」。约定：

- **package 内不含字体二进制**，只在 `obj.font_str` 存一个**字体注册名**（→ string_table），例如 `"puhui_16"`；`0xFFFF` = 默认字体。
- **字号内嵌在字体本身**：注册名即唯一标识（如 `puhui_16` 表示 16px），package **不单独存 `font_size`**。JSON 的 `font_size` 属于「受限」字段（见 [03-scene-format.md](03-scene-format.md)），compiler 负责把 `font + font_size` 归一到一个已注册的字体名或降级为默认。
- **host / device 各自维护一个 font registry**（A11），加载时 `registry_get(font_str)` 解析为 `gfx_font_t`；**查不到或为默认 → 回退默认字体**，绝不失败。
- 第一版只注册**一个默认字体**（host 用编译进二进制的 LVGL blob，如 `font_puhui_16_4`）；后续再扩多字体、磁盘/包内字体加载。

**font registry 具体形态（v1）**：

```c
// 一张全局注册表，key = 注册名(与 JSON font 字段一致), value = gfx_font_t
typedef struct { const char *name; gfx_font_t font; } gfx_font_reg_entry_t;

gfx_err_t   gfx_font_registry_register(const char *name, gfx_font_t font); // 应用启动时登记
gfx_font_t  gfx_font_registry_get(const char *name);   // 查不到返回 default
gfx_font_t  gfx_font_registry_default(void);            // 保证非 NULL
```

约定：

- **命名规范**：`<family>_<pxsize>`，如 `puhui_16`、`puhui_24`。compiler 把 JSON 的 `font` +（受限的）`font_size` 归一成这个名字；若目标名字未注册则降级默认。
- **host 与 device 注册表内容必须一致**（同名 → 视觉一致），由应用在启动时用同一份字体清单调用 `register`。这份清单是「固件/host 侧约定」，**不进 package**（package 只带名字）。
- registry 是**应用级单例**，scene loader 只读它；scene 之间共享，不随 `gfx_scene_delete` 释放。
- v1 只 `register` 一个默认字体即可跑通；`font_str == 0xFFFF` 或未注册名 → `gfx_font_registry_default()`。

## 7. 颜色编码约定（已定：包内统一 888，两类面板都支持）

先厘清两个**互相独立**的概念，避免把它们混成“565 还是 888 二选一”：

1. **面板像素格式**（`GFX_COLOR_FORMAT_RGB565` / `RGB888` / `ARGB8888` …）是 **display-port 的运行时配置，不是 package 的属性**。renderer 会把颜色按面板格式展开输出，所以**同一个 `.gsp` 在 565 板和 888 板上都能跑**——“两类面板都支持”本来就成立，无需在包里选。
2. **widget 颜色 API** 目前是 `gfx_color_t`（16 位，语义 RGB565，见 `include/gfx/types.h`）。`gfx_color_hex(0xRRGGBB)` 会把 888 收敛成 565。也就是说，喂给 widget 的颜色**当前**都会落到 565，与面板是不是 888 无关。

基于此，package 的存储策略定为：

> **包内所有颜色统一存无损 RGB888（`u32`，`0x00RRGGBB`）；loader 加载时用 `gfx_color_hex()` 转成设备当前的 `gfx_color_t`。**

这样同时满足：

- **两类面板都支持**：面板格式由 display-port 决定，一个包通吃 565 / 888 屏。
- **无损、可升级**：包保留作者全精度 888；将来 `gfx_color_t` 若升级到 888/8888，**老包无需重编**，只要 loader 改一行转换即可。
- **零重复格式**：不再定义 565-packed 变体（style 已去重，体积代价可忽略），避免格式分叉与 compiler 双路径。

`header.target_color_format`（§2, 0x38）只是**面板格式提示**，供 host 预览与设备端一致性校验/告警，不参与实际渲染取色。

## 8. 与 ITU 加载方式的对比与取舍

参考 `ite-sdk` 的 ITU 实现（`share/itu/itu_widget.c`、`include/ite/itu.h`）。

### 8.1 ITU 的做法：内存镜像 + 指针重定位

ITU 的 widget 是纯 POD 结构体，树是**侵入式 first-child / next-sibling 链表**：

```1366:1378:/home/xuxin/esp_work/third_party/ite-sdk/include/ite/itu.h
typedef struct ITUWidgetTag
{
    ITCTree tree;                           ///< Tree node
    ITUWidgetType type;                     ///< Widget type
    char name[ITU_WIDGET_NAME_SIZE];        ///< Widget name
    unsigned int flags;
    ...
    ITURectangle rect;                      ///< 位置尺寸
    ITUColor color;
```

`.itu` 文件 ≈ 整棵树的结构体内存 dump，指针以「文件内偏移」存储。加载时整文件读进起始地址 `base`，逐 widget 把偏移 **加上 base 还原成真实指针**，函数指针不落盘、按类型重新绑定：

```418:451:/home/xuxin/esp_work/third_party/ite-sdk/share/itu/itu_widget.c
void ituWidgetLoad(ITUWidget* widget, uint32_t base)
{
    ...
    if (widget->tree.parent)
        widget->tree.parent = (ITCTree*)((uint32_t)widget->tree.parent + base);
    if (widget->tree.sibling)
        widget->tree.sibling = (ITCTree*)((uint32_t)widget->tree.sibling + base);
    if (widget->tree.child)
        widget->tree.child = (ITCTree*)((uint32_t)widget->tree.child + base);

    widget->effect      = NULL;
    widget->Exit        = ituWidgetExitImpl;   // 函数指针按类型重新 bind
    widget->Clone       = ituWidgetCloneImpl;
    ...
}
```

每个类型有自己的 `XxxLoad`（`ituButtonLoad`→`ituBackgroundLoad`→`ituWidgetLoad`），先调父类再重定位自己那部分（surface、text 等）。

### 8.2 为什么 esp_emote_gfx 不照搬

| 维度 | ITU 内存镜像法 | 对 esp_emote_gfx 的问题 |
|------|----------------|--------------------------|
| 指针宽度 | `uint32_t base`，假设 32-bit 指针 | Host 预览 64-bit、设备 32-bit，**需共用 loader**；32 位偏移重定位在 64 位 host 直接崩 |
| 对象模型 | widget 是 POD，可整块 dump | `gfx_object_t` 是不透明句柄：内部分配、带 class vtable、`disp` 反向指针、自动挂 `child_list`，无法 dump/relocate |
| 坏包安全 | 坏 offset = 野指针，难校验 | D6 要求坏包不崩；索引 + count/offset 可做严格边界校验 |
| 版本演进 | 改结构体即破格式（ABI 锁死） | index-table 加字段兼容性好 |
| 字节序/对齐 | 平台锁定 | 需跨平台稳定 → 显式 LE + 定长条目 |

### 8.3 从 ITU 吸收的三点

1. **函数指针/回调绝不序列化，加载时按 type/name 重新绑定** → 本格式的 callback_table 只存名字、运行时 `gfx_scene_bind_callback`（§6 / D5）。
2. **类型分发的 per-type loader** → object factory 按 `type` 分发到 `gfx_container/label/button` 的 apply 函数（§10）。
3. **只读数据就地引用、不 copy** → package buffer 常驻（device=mmap flash），scene 只需保留 id/callback 名的 blob 引用而不复制（§5.1）。注：label 文本因 widget 强制拷贝暂不受益，真正就地零 copy 待未来 `set_text_static`。

### 8.4 一句话取舍

> ITU 的「内存镜像重定位」是为 32 位单平台 POD widget 设计，跟 esp_emote_gfx「host/device 共用 loader + 不透明对象句柄 + 坏包不崩」冲突。采用**解耦 index-table + factory 还原**，同时吸收 ITU 的「指针不落盘、按类型重绑定、只读数据就地引用」。

## 9. 决议记录

- **颜色编码** → **已定**：包内统一 888、面板格式由 display-port 决定，两类屏都支持（§7）。`StyleEntry` 固定 24B。
- **字符串生命周期 / mmap 就地** → **已定**：label/button 文本由 widget 拷贝；package buffer 需在 scene 存活期有效（device=mmap flash 常驻，host=调用方持有）；host/device 同一契约、零额外拷贝；真正零 copy 需未来 `set_text_static` API，不在 v1（§5.1）。
- **字体** → **已定**：包内不打包字体，`obj.font_str` 存注册名，host/device 各有 font registry 解析，查不到回退默认；v1 只注册一个默认字体（§6.1 / A11）。
- **对齐词汇布局** → **已定**：`scene_layout` 的「居中/底部居中」由上位机转成 x/y 存入 `obj.x/y`，包内不保留语义，无需 align anchor 字段。
- **alpha / 高色深**（后续）：`gfx_color_t` 目前 565 无 alpha；未来面板需要时，包内 888 已够用，仅需扩 `gfx_color_t` 与 loader 转换，不动格式。

## 10. 加载算法（Host / Device 共用）

```text
gfx_scene_load_package(disp, data, size, &scene):
  1. 校验 header: magic / version / header_size / file_size==size / crc32(可选)
     校验各 table 的 off+count*entry 不越界、4 字节对齐
     校验 screen_w/h == 显示分辨率(见坐标空间约定), 否则报错不缩放
     颜色一律按 RGB888 读取，用 gfx_color_hex() 转成 gfx_color_t；面板格式由 display-port 决定，loader 不关心
  2. disp 背景 = gfx_color_hex(screen_bg)
  3. objs = 分配 obj_count 个 gfx_object_t* 的临时数组
  4. for i in 0..obj_count-1 (先序):
       e = object_table[i]
       校验 e.type 已知; e.style_idx/*_str/callback_idx/font_str 各自 < 对应 count 或 == 0xFFFF
       obj = factory[e.type](disp)                 // gfx_container/label/button_create
       gfx_object_set_pos/size(obj, e.x,e.y,e.w,e.h)
       gfx_object_set_visible(obj, flags.visible)
       apply_style(obj, e.type, style_table[e.style_idx], flags)  // 颜色 gfx_color_hex(888→gfx_color_t)
       if e.font_str != 0xFFFF: set_font(obj, font_registry_get(str(e.font_str)) ?? default)  // 查不到回退默认
       if e.text_str != 0xFFFF: set_text(obj, str(e.text_str))   // widget 内部拷贝文本
       if e.callback_idx != 0xFFFF: 记录 (obj, callback_name) 到 scene 绑定表
       objs[i] = obj
       if e.parent_idx == -1: 挂到 display (create 时已自动挂)
       else: gfx_object_add_child(objs[e.parent_idx], obj)   // 父一定已建好(先序保证)
  5. scene->objs = objs; scene->count = obj_count; scene->pkg = data  // 持有 blob 引用(§5.1 契约)
  6. 返回 scene；业务侧再 gfx_scene_bind_callback 逐个绑定
```

- 因为**先序**，`e.parent_idx < i` 恒成立，父节点必已创建，一遍扫描即可建树。
- **buffer 生命周期**：`data` 须在 scene 存活期保持有效（§5.1）。device=mmap flash 常驻；host 调用方持有直到 `gfx_scene_delete`。
- `gfx_scene_delete(scene)` 逆序或直接遍历 `scene->objs` 全部销毁（对应 A10），reload = delete + load。
- 任何一步校验失败 → 释放已建对象、返回错误码，**不崩**（D6）。

## 11. 与 `.inc` 导出的关系（D3）

`.gsp` 是权威二进制；`.inc` 只是把同一字节流用 `static const uint8_t xxx_gsp[] = {...}` 包一层，方便 ESP 工程直接 `#include` 或放进 mmap assets 分区。二者内容逐字节一致，loader 走同一个 `gfx_scene_load_package(data, size)`。
