# Scene 格式与属性白名单

> 上一篇：[总体架构](02-architecture.md) ｜ 下一篇：[Tool Calling](04-tool-calling.md)

本页是 scene 的**数据模型契约**。其中「属性白名单」是全项目属性定义的**唯一数据源**（single source of truth）。

## 1. Scene JSON

第一版内部 canonical format 使用 JSON。注意：motion 的 `scene.json → .inc` 链路**整套逻辑内联在 `gui_designer/gui_designer.html`（约 9400 行 JS）里，没有独立脚本**，而且是几何/姿态专用（parts / poses / bezier 拓扑），与 `container/label/button` 的 widget 树结构几乎不重叠。因此 UI scene **参考它的编译模式（schema 驱动 normalize、字符串 ID→整数索引、双产物、设备端 init 校验），但是独立新建一套 compiler，不复用 motion 的代码**。XML 可以后续作为导入/导出格式，但不作为第一版主路径。

> 属性字段以本页第 2 节「属性白名单」为**唯一数据源**。schema 校验、tool schema、skill 描述都从这一份派生，避免多处漂移。

```json
{
  "schema": "gfx_ui_scene_v1",
  "name": "ai_scene_demo",
  "screen": {
    "w": 320,
    "h": 240,
    "bg_color": "#101418"
  },
  "widgets": [
    {
      "type": "label",
      "id": "title_label",
      "x": 20,
      "y": 24,
      "w": 280,
      "h": 32,
      "text": "Hello GFX",
      "color": "#FFFFFF",
      "font": "default",
      "font_size": 24,
      "align": "center"
    },
    {
      "type": "button",
      "id": "ok_btn",
      "x": 90,
      "y": 160,
      "w": 140,
      "h": 44,
      "text": "OK",
      "bg_color": "#2F8CFF",
      "text_color": "#FFFFFF",
      "radius": 8,
      "border_color": "#76B7E8",
      "border_width": 2,
      "callback": "on_ok"
    }
  ]
}
```

第一版只支持绝对坐标。后续再加 align、layout、anchor。

**坐标空间约定**：第一版规定 `screen.w/h` 必须等于目标显示分辨率，`widget` 的 `x/y/w/h` 直接是显示像素，不做缩放（motion 是按 viewbox 缩放的，UI scene 不沿用）。若 scene 尺寸与实际 display 分辨率不一致，loader 报错而不是自动缩放，缩放策略留到后续版本再定。

**颜色格式约定**：JSON 里颜色统一用 `#RRGGBB`（RGB888），package 内也**无损保留 888**。loader 加载时用 `gfx_color_hex(0xRRGGBB)` 转成设备当前的 `gfx_color_t`（现为 RGB565）。**面板像素格式（565 / 888）是 display-port 的运行时配置，与 scene / package 无关——同一份 scene 在 565 和 888 屏上都能跑。** 详见 [09-package-format.md](09-package-format.md) §7。

## 1.1 顶层字段定义

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `schema` | string | 是 | 固定 `"gfx_ui_scene_v1"`，值不符直接拒绝 |
| `name` | string | 是 | scene 名，用于导出符号前缀（compiler 会 sanitize 成 C 标识符） |
| `screen` | object | 是 | `{ w, h, bg_color, bg_enable? }`；`w/h` 必须等于目标显示分辨率 |
| `widgets` | array | 是 | 顶层 widget 列表，可嵌套（见 §1.2）。数组顺序 = 绘制/z 顺序（后者覆盖前者） |
| `resources` | object | 否 | 资源引用表（v1 保留，见 §1.4）；缺省 = 空 |
| `callbacks` | array | 否 | 声明本 scene 用到的回调名清单（供工具/校验用）；缺省从 widget 的 `callback` 收集 |

`screen` 字段：

| 字段 | 类型 | 必填 | 默认 | 说明 |
|------|------|------|------|------|
| `w` / `h` | int | 是 | — | 必须 == display 分辨率，否则 loader 报错不缩放 |
| `bg_color` | `#RRGGBB` | 否 | `#000000` | 屏幕背景 |
| `bg_enable` | bool | 否 | `true` | 是否填充背景 |

## 1.2 widget 层级与 `children` 表达

widget **可以嵌套**：`container` 通过 `children` 数组持有子 widget，实现父子树（对应 `clip_children`、以及设备端 `gfx_object_add_child`）。

```json
{
  "type": "container",
  "id": "panel",
  "x": 16, "y": 16, "w": 288, "h": 208,
  "bg_color": "#17212B",
  "clip_children": true,
  "children": [
    { "type": "label",  "id": "title", "x": 16, "y": 24, "w": 256, "h": 32, "text": "ESP GFX" },
    { "type": "button", "id": "ok",    "x": 74, "y": 140, "w": 140, "h": 44, "text": "OK", "callback": "on_ok" }
  ]
}
```

约定：

- `children` 只有容器类（v1 只有 `container`）可带；`label` / `button` 带 `children` → 校验报错。
- **子 widget 的 `x/y` 相对父容器左上角**（与设备端 `gfx_object_add_child` 的相对坐标语义一致）；顶层 widget 的 `x/y` 相对屏幕。
- 嵌套深度 v1 限制 ≤ 8，防止病态深树。
- compiler 把嵌套树**深度优先先序展开**成 package 的 `object_table`（先序 + `parent_idx`，见 [09-package-format.md](09-package-format.md) §3）。
- 顶层 `widgets[]` 等价于「挂在 display 根下的一组节点」，其 `parent_idx = -1`。

## 1.3 字段校验规则

- **未知字段**：拒绝（fail-closed），避免模型写入拼错/不支持字段（对齐 [08-risks 1.1](08-risks-and-references.md)）。
- **未知 `type`**：拒绝。v1 合法 `type` 仅 `container` / `label` / `button`。
- **`id` 唯一性**：全 scene 内唯一；字符集 `[A-Za-z0-9_]`，首字符非数字；重复或非法 → 报错。`id` 用于 `scene.patch_widget` / `gfx_scene_find_obj`。
- **坐标 / 尺寸边界**：`x/y` 为 `int16`（允许负值，用于部分移出屏）；`w/h` 为 `uint16` 且 `>= 0`。compiler **告警**（非致命）widget 完全落在父/屏幕之外；`w/h` 超过 `int16` 上限 → 报错。
- **颜色**：必须 `#RRGGBB`（6 位十六进制），大小写不敏感；不接受 `#RGB` / 命名色 / rgba。
- **枚举**：见 §2 白名单「合法值」；非法枚举 → 报错。
- **受限字段**（`font`/`font_size`/`callback`）：schema 接受，loader 降级处理（见 §2 末与 [09 §6.1](09-package-format.md)）。

## 1.4 `resources` / `callbacks` 顶层字段（v1 保留）

- **`resources`**：为后续 image / 自定义 font 等资源引用预留。v1 **不支持 image widget**，故第一版可缺省或留空 `{}`；schema 接受但 compiler 目前不消费。结构留待 image 需求出现时再定（不在 v1 冻结范围）。
- **`callbacks`**：可显式声明本 scene 用到的回调名（如 `["on_ok"]`），供工具做「模型只能引用已声明回调」的约束与 host 端占位绑定。缺省时 compiler 从各 widget 的 `callback` 字段自动收集去重。回调**语义与绑定**见 [09 §6](09-package-format.md) / A12。

## 2. 属性白名单（唯一数据源 / single source of truth）

本节是全项目属性定义的唯一来源：schema 校验（A2）、tool schema（B5）、skill 描述（C2/C3/C4）都从这份派生，不要在别处再复制一份字段清单。`状态` 列说明该字段在第一版的支持程度。

**公共属性（所有 widget）**

| 属性 | 类型 | 状态 |
|------|------|------|
| `id` | string | v1 |
| `x` / `y` | int（显示像素） | v1 |
| `w` / `h` | int（显示像素） | v1 |
| `visible` | bool | v1 |

**`label`**

| 属性 | 类型 | 状态 | 备注 |
|------|------|------|------|
| `text` | string | v1 | |
| `color` | `#RRGGBB` | v1 | |
| `bg_color` | `#RRGGBB` | v1 | |
| `bg_enable` | bool | v1 | |
| `align` | enum: left/center/right/auto | v1 | 文本对齐 `gfx_text_align_t` |
| `long_mode` | enum: wrap/scroll/clip/scroll_snap | v1 | |
| `font` | string（注册名） | 受限 | 需 font registry，第一版仅支持已注册字体，见 A11 |
| `font_size` | int | 受限 | **无 setter**，字号在建 font 时定死；第一版默认字体固定或限枚举，见 A11 |

**`button`**

| 属性 | 类型 | 状态 | 备注 |
|------|------|------|------|
| `text` | string | v1 | |
| `text_color` | `#RRGGBB` | v1 | |
| `bg_color` | `#RRGGBB` | v1 | |
| `bg_color_pressed` | `#RRGGBB` | v1 | |
| `radius` | int | v1 | |
| `border_color` | `#RRGGBB` | v1 | |
| `border_width` | int | v1 | |
| `font` | string（注册名） | 受限 | 同 label，见 A11 |
| `callback` | string（回调名） | 受限 | button 无点击回调 API，需 name→cb 注册表 + click 语义，见 A12 |

**`container`**

| 属性 | 类型 | 状态 | 备注 |
|------|------|------|------|
| `bg_color` | `#RRGGBB` | v1 | |
| `bg_enable` | bool | v1 | |
| `radius` | int | v1 | |
| `border_color` | `#RRGGBB` | v1 | |
| `border_width` | int | v1 | |
| `clip_children` | bool | v1 | |
| `children` | array | v1 | 子 widget 列表（见 §1.2）；仅容器可带 |

属性必须经过 schema 校验，避免模型写入不支持字段。标记为「受限」的字段，第一版可先在 schema 中接受但由 loader 降级处理（如忽略 `font_size`、`callback` 仅登记不触发），闭环跑通后再补全。

> 上表中 A2 / A11 / A12 / B5 / C2-C4 等任务编号见 [07-todo.md](07-todo.md)。

## 3. 推荐最小演示 Scene

```json
{
  "schema": "gfx_ui_scene_v1",
  "name": "ai_scene_demo",
  "screen": {
    "w": 320,
    "h": 240,
    "bg_color": "#101418"
  },
  "widgets": [
    {
      "type": "container",
      "id": "panel",
      "x": 16,
      "y": 16,
      "w": 288,
      "h": 208,
      "bg_color": "#17212B",
      "radius": 8,
      "border_color": "#76B7E8",
      "border_width": 2
    },
    {
      "type": "label",
      "id": "title_label",
      "x": 32,
      "y": 40,
      "w": 256,
      "h": 32,
      "text": "ESP GFX",
      "color": "#FFFFFF",
      "font": "default",
      "font_size": 24,
      "align": "center"
    },
    {
      "type": "button",
      "id": "ok_btn",
      "x": 90,
      "y": 156,
      "w": 140,
      "h": 44,
      "text": "OK",
      "bg_color": "#2F8CFF",
      "text_color": "#FFFFFF",
      "radius": 8,
      "border_color": "#A9D8FF",
      "border_width": 1,
      "callback": "on_ok"
    }
  ]
}
```

推荐测试 prompt：

```text
把标题改成 AI Scene，颜色改成黄色。
把 ok_btn 文字改成 Start，背景改成绿色。
把按钮移动到底部居中。
把整个 panel 改成深灰色，边框改成蓝色。
```
