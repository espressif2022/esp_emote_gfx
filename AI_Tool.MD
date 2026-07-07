# ESP Emote GFX AI Tool 设计

> 文档状态：草案 v0.1  
> 目标仓库：`esp_emote_gfx`  
> 目标方向：通过 AI Chat Panel 修改 UI 描述文件，实时预览，并导出设备端可运行的 scene package / ITU-like 资源。

---

## 1. 目标

当前 `esp_emote_gfx` 的 UI 主要由 C 代码主动 `create object` 构建。后续希望引入类似 ITE / ITU 的描述式 UI 工作流：

```text
AI / 上位机工具
      ↓
修改 XML / JSON / SVG 等 UI 描述
      ↓
Host SDL 实时渲染预览
      ↓
导出 scene package
      ↓
ESP 设备端直接加载运行
```

第一阶段只要求跑通最小闭环：

1. 支持简单控件：`label`、`button`、`container`。
2. 通过 Chat Panel 输入自然语言，修改 `label` 或 `button` 的样式。
3. 修改后实时刷新 `esp_emote_gfx` SDL 预览。
4. 导出的 package 后续可以放到设备端加载运行。

---

## 2. 核心原则

### 2.1 Chat Panel，不是 ChatPet

这里的交互入口是类似 VSCode Codex / Copilot 的 **Chat Panel**：

```text
左侧：GFX SDL Preview
右侧：AI Chat Panel
底部：修改记录 / Undo / Export
```

Chat Panel 是 AI 工具入口，不是桌面宠物，也不是普通聊天窗口。

### 2.2 AI 不直接写 C 代码

AI 不应该直接生成 `gfx_label_create()`、`gfx_button_create()` 这类 C 代码。

AI 应输出结构化修改意图：

```json
{
  "tool": "scene.patch_widget",
  "args": {
    "target_id": "ok_btn",
    "props": {
      "text": "Start",
      "bg_color": "#2F8CFF",
      "text_color": "#FFFFFF"
    }
  }
}
```

实际的 XML 修改、校验、保存、预览刷新由本地工具完成。

### 2.3 Host 可解析 XML，设备端优先加载 package

Host 侧为了开发效率可以直接解析 XML：

```text
scene.xml → gfx_scene_load_xml() → SDL Preview
```

设备端不建议运行 XML parser。设备端应加载上位机导出的二进制 package：

```text
scene.xml → scene compiler → scene.gsp / scene.itu-like → ESP load
```

### 2.4 上位机是资源编译器

AI Tool 不只是改文件，它应逐步承担资源编译器角色：

- 静态对象树展开
- style 默认值合并
- 静态布局预计算
- 字符串表生成
- 图片 / 字体 / 动画资源引用收集
- 目标颜色格式转换
- 资源压缩与打包
- scene package 导出

这样设备端只负责 load package、绑定回调、刷新渲染。

---

## 3. 总体架构

```text
┌──────────────────────────────────────────────────────────────┐
│                    AI Scene Studio                            │
├──────────────────────────────┬───────────────────────────────┤
│ SDL Preview                  │ Chat Panel                    │
│                              │                               │
│ esp_emote_gfx runtime         │ User prompt                   │
│ gfx object tree               │ AI response                   │
│ SDL backend                   │ Tool call result              │
│                              │ Apply / Undo / Export         │
├──────────────────────────────┴───────────────────────────────┤
│ Change Log: ok_btn.bg_color #33485F → #2F8CFF                 │
└──────────────────────────────────────────────────────────────┘

        │                         ▲
        │ preview reload           │ chat / tool result
        ▼                         │
┌──────────────────────────────────────────────────────────────┐
│ Local Agent Server                                            │
│                                                              │
│ - conversation context                                        │
│ - model API                                                   │
│ - tool call dispatcher                                        │
│ - XML patch / validation                                      │
│ - preview bridge                                              │
│ - package export                                              │
└──────────────────────────────────────────────────────────────┘

        │
        ▼
┌──────────────────────────────────────────────────────────────┐
│ Scene Files                                                   │
│                                                              │
│ scene.xml                                                     │
│ assets/                                                       │
│ scene.gsp / scene.itu-like                                    │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. 模块划分

### 4.1 Chat Panel

职责：

- 显示用户输入和 AI 回复。
- 展示模型调用了哪些工具。
- 显示 XML 修改摘要。
- 提供 `Apply` / `Undo` / `Reload` / `Export` 操作。

第一版可以用 Web / Electron / 浏览器页面实现，避免在 C/SDL 内部手写复杂文本输入和聊天 UI。

建议 UI：

```text
┌──────────────────────────────┬──────────────────────┐
│                              │ AI Scene Assistant   │
│      esp_emote_gfx SDL        │──────────────────────│
│      Preview                 │ You:                 │
│                              │ 把 ok 按钮改成蓝色     │
│                              │                      │
│                              │ AI:                  │
│                              │ 已修改 ok_btn 样式     │
│                              │                      │
│                              │ Tool:                │
│                              │ scene.patch_widget   │
│                              │                      │
│                              │ [Apply] [Undo]       │
│                              │                      │
│                              │ 输入框...             │
├──────────────────────────────┴──────────────────────┤
│ Changes: ok_btn.bg_color #33485F → #2F8CFF           │
└──────────────────────────────────────────────────────┘
```

### 4.2 Local Agent Server

职责：

- 接收 Chat Panel 请求。
- 调用大模型。
- 将模型输出转换成 tool call。
- 执行 scene 工具。
- 通知 SDL Preview reload。
- 管理 undo history。

建议接口：

```text
POST /chat
GET  /scene/current
POST /scene/patch
POST /scene/reload
POST /scene/export
POST /scene/undo
WS   /events
```

### 4.3 Scene XML

第一版 XML 示例：

```xml
<scene id="main" width="320" height="240" bg_color="#101418">
  <label id="title_label"
         x="20" y="24" w="280" h="32"
         text="Hello GFX"
         color="#FFFFFF"
         font="default"
         font_size="24"
         align="center" />

  <button id="ok_btn"
          x="90" y="160" w="140" h="44"
          text="OK"
          bg_color="#2F8CFF"
          text_color="#FFFFFF"
          radius="8"
          border_color="#76B7E8"
          border_width="2"
          callback="on_ok" />
</scene>
```

第一版只支持绝对坐标。后续再加 align、layout、anchor。

### 4.4 GFX Scene Loader

Host 侧需要一个 scene loader，将 XML 映射到现有 GFX object：

```text
<scene>     → gfx_display background / root container
<container> → gfx_container_create()
<label>     → gfx_label_create()
<button>    → gfx_button_create()
```

目标 API 草案：

```c
typedef struct gfx_scene gfx_scene_t;

gfx_err_t gfx_scene_load_xml(gfx_display_t *disp, const char *path, gfx_scene_t **out_scene);
gfx_err_t gfx_scene_reload(gfx_scene_t *scene);
gfx_object_t *gfx_scene_find_obj(gfx_scene_t *scene, const char *id);
gfx_err_t gfx_scene_bind_callback(gfx_scene_t *scene, const char *name, void *cb, void *user_data);
void gfx_scene_delete(gfx_scene_t *scene);
```

### 4.5 Preview Bridge

SDL Preview 进程需要接收 Agent Server 的刷新事件：

```json
{
  "event": "scene.reload",
  "path": "examples/ai_scene_demo/scene.xml"
}
```

收到事件后：

```text
stop current scene
delete old objects
load XML again
refresh display
send status back
```

### 4.6 Scene Compiler / Export

导出目标不是 C create 代码，而是设备端可加载 package。

package 草案：

```text
scene.gsp
├─ header
├─ object table
├─ style table
├─ string table
├─ resource table
└─ optional binary assets
```

设备端加载流程：

```text
gfx_scene_package_load()
      ↓
restore object tree
      ↓
bind callback by name
      ↓
run display refresh
```

---

## 5. Tool Calling 设计

### 5.1 工具列表

第一版工具：

| Tool | 作用 |
|------|------|
| `scene.get_widgets` | 返回当前 scene 中可修改控件 |
| `scene.patch_widget` | 修改指定控件属性 |
| `scene.reload_preview` | 通知 SDL 重新加载 scene |
| `scene.undo` | 回滚上一次修改 |
| `scene.export_package` | 导出设备端 package |
| `preview.screenshot` | 获取当前预览截图 |

### 5.2 `scene.get_widgets`

返回示例：

```json
{
  "widgets": [
    {
      "id": "title_label",
      "type": "label",
      "props": {
        "x": 20,
        "y": 24,
        "w": 280,
        "h": 32,
        "text": "Hello GFX",
        "color": "#FFFFFF"
      }
    },
    {
      "id": "ok_btn",
      "type": "button",
      "props": {
        "x": 90,
        "y": 160,
        "w": 140,
        "h": 44,
        "text": "OK",
        "bg_color": "#2F8CFF"
      }
    }
  ]
}
```

### 5.3 `scene.patch_widget`

请求：

```json
{
  "target_id": "ok_btn",
  "props": {
    "text": "Start",
    "bg_color": "#2F8CFF",
    "text_color": "#FFFFFF",
    "radius": 10
  }
}
```

响应：

```json
{
  "ok": true,
  "changes": [
    {
      "target_id": "ok_btn",
      "prop": "text",
      "old": "OK",
      "new": "Start"
    },
    {
      "target_id": "ok_btn",
      "prop": "radius",
      "old": 8,
      "new": 10
    }
  ]
}
```

### 5.4 属性白名单

`label` 支持：

```text
x
y
w
h
visible
text
color
font
font_size
align
```

`button` 支持：

```text
x
y
w
h
visible
text
bg_color
bg_color_pressed
text_color
radius
border_color
border_width
callback
```

`container` 支持：

```text
x
y
w
h
visible
bg_color
radius
border_color
border_width
```

属性必须经过 schema 校验，避免模型写入不支持字段。

---

## 6. 文件和目录建议

第一阶段建议新增：

```text
tools/ai_scene_studio/
  server/
    agent_server.py 或 agent_server.ts
    scene_tools.*
    xml_patch.*
    preview_bridge.*
  web/
    ChatPanel.*
    App.*

examples/ai_scene_demo/
  scene.xml
  assets/
  README.md

include/gfx/scene.h
src/scene/
  gfx_scene_xml.c        # Host only, XML loader
  gfx_scene_package.c    # Host/Device common package loader
  gfx_scene_priv.h

scripts/scene_compiler/
  compile_scene.py
```

如果第一版要更快，可以先不加 public `include/gfx/scene.h`，先放在 `simulation/host` 或 `examples/ai_scene_demo` 内部验证。

---

## 7. 实现阶段

### 阶段 1：手动 XML → SDL 预览

目标：

- 写一个最小 XML。
- Host 侧解析 XML。
- 创建 label/button/container。
- 支持手动保存 XML 后 reload。

验收：

```text
修改 scene.xml 中 title_label.text
点击 reload
SDL 画面更新
```

### 阶段 2：Chat Panel → XML Patch → SDL Reload

目标：

- Chat Panel 接入 Local Agent Server。
- 大模型输出 `scene.patch_widget`。
- 本地工具修改 XML。
- SDL 自动 reload。

验收：

```text
用户输入：把 ok 按钮改成蓝色，文字改成 Start
XML 被修改
SDL 自动刷新
底部 change log 显示修改项
```

### 阶段 3：Undo / Diff / Screenshot

目标：

- 每次 patch 前保存快照。
- 支持 undo。
- 支持 preview screenshot。

验收：

```text
Apply 后可 Undo
Chat Panel 能显示当前预览截图
```

### 阶段 4：Scene Package 导出

目标：

- XML 编译为二进制 package。
- Host 可以 load package。
- 设备端开始接入 package loader。

验收：

```text
scene.xml → scene.gsp
Host load scene.gsp 渲染结果与 XML 一致
```

### 阶段 5：ESP 设备端加载

目标：

- ESP 侧加载 package。
- 绑定业务 callback。
- 不依赖 XML parser。

验收：

```text
同一个 scene.gsp 能在 SDL 和 ESP 设备端运行
```

---

## 8. 与 ITE / ITU 的关系

ITE / ITU 可作为结构参考：

```text
上位机描述 / 编辑
      ↓
导出 scene / resource
      ↓
设备端 loader + renderer
```

但 `esp_emote_gfx` 不需要一开始完全复刻 ITU：

- 不直接照搬 ITU 文件格式。
- 不要求第一版支持复杂 action system。
- 不要求第一版支持所有 widget。
- 先验证 “描述式 scene + AI patch + SDL preview + package export” 的闭环。

后续可逐步吸收 ITU 的优点：

- widget tree 序列化
- name/id 查找
- callback name 绑定
- resource table
- action / animation 描述
- binary scene package

---

## 9. 风险与约束

### 9.1 模型输出不稳定

处理方式：

- 模型只输出 tool call。
- 工具层做 schema 校验。
- 不允许模型直接写任意 XML。
- 修改前保存 undo snapshot。

### 9.2 设备端资源有限

处理方式：

- 设备端不解析 XML。
- 上位机预编译 package。
- 静态布局和 style 尽量提前计算。

### 9.3 预览与设备端不一致

处理方式：

- SDL Preview 使用 `esp_emote_gfx` 同一套 object / renderer。
- package loader 在 Host 和 Device 尽量共用。
- 避免用浏览器 DOM 作为最终渲染预览。

### 9.4 过早设计复杂格式

处理方式：

- 第一版 XML 简单、显式、绝对坐标。
- package 格式先满足 label/button/container。
- 动画、图片、布局系统后续扩展。

---

## 10. 第一版完成定义

第一版完成后应能演示：

```text
1. 打开 AI Scene Studio。
2. 左侧显示 esp_emote_gfx SDL 预览。
3. 右侧 Chat Panel 输入：
   “把 ok_btn 改成蓝色，文字改成 Start”
4. Agent 调用 scene.patch_widget。
5. scene.xml 被安全修改。
6. SDL 预览自动刷新。
7. Change Log 显示修改内容。
8. 点击 Export 生成 scene package。
```

这条链路跑通后，再扩展 image、font、animation、resource compiler 和 ESP 设备端 package loader。

---

## 11. TODO：云端交互与 GFX Skill 暴露

后续 Chat Panel 与云端大模型交互时，重点是参考 `esp-claw` 将能力以 skill / capability 形式暴露给大模型的分层方式。

在 `esp_emote_gfx` 中，应该把已有 GFX 组件作为可理解、可约束、可调用的 UI skills 暴露给模型：

```text
gfx_label skill
gfx_button skill
gfx_container skill
scene_layout skill
scene_export skill
```

模型通过这些 skill 理解当前工程支持哪些控件、哪些属性可改、属性值有什么约束，然后输出结构化 `scene.patch_widget`，由本地工具安全修改 XML / scene 描述。

参考方向：

- `claw_core_control.c` 的 request 生命周期管理：`request_id`、inflight request、abort/cancel、用户打断。
- `esp-claw` 的 Agent Loop：会话上下文、memory、skills、context providers、capability tools。
- Skill / capability 的边界设计：模型只选择能力和参数，本地工具负责校验和执行。

映射到 `esp_emote_gfx`：

```text
ESP-Claw                         esp_emote_gfx AI Tool
────────────────────────         ─────────────────────────────
Skill                             gfx_label / gfx_button / gfx_container
Capability tool                   scene.patch_widget / scene.reload_preview
Capability execution              XML patch / scene package export
Agent loop phase                  Chat Panel task phase
Abort / user interrupt            Cancel current AI edit/export
```

GFX skill 示例：

```text
Skill: gfx_button
Purpose:
  修改或创建 button 控件。

Supported properties:
  id, x, y, w, h, visible, text, bg_color, bg_color_pressed,
  text_color, radius, border_color, border_width, callback

Rules:
  - 颜色必须是 #RRGGBB。
  - 坐标和尺寸必须在 scene 范围内。
  - 不允许生成 C 代码。
  - 必须通过 scene.patch_widget 修改。
```

建议 TODO：

- [ ] 设计 `agent_request_t`：包含 `request_id`、`session_id`、`phase`、`abort`、`error`、`tool_iteration_count`。
- [ ] 设计 Chat Panel 状态机：`idle`、`thinking`、`tool_calling`、`patching_xml`、`reloading_preview`、`exporting_package`、`done`、`error`、`canceled`。
- [ ] 设计云端模型接口适配层：隐藏 OpenAI / 内网模型 / 本地模型差异。
- [ ] 设计 GFX skill 描述文件：把 `label`、`button`、`container` 的属性白名单、默认值、取值范围、布局规则写成模型上下文。
- [ ] 设计 tool call schema：第一版只允许 `scene.get_widgets`、`scene.patch_widget`、`scene.reload_preview`、`scene.undo`、`scene.export_package`。
- [ ] 设计 skill 到 tool 的映射：例如 `gfx_button` skill 最终只能落到 `scene.patch_widget` 或后续 `scene.create_widget`。
- [ ] 设计取消机制：用户取消后停止继续调用模型，不再应用后续 tool call；已应用 XML patch 需要保留 undo。
- [ ] 设计事件流：Agent Server 通过 WebSocket 向 Chat Panel 和 SDL Preview 广播 phase、tool result、change log。
- [ ] 设计 skill 文档：描述 UI 修改规则、属性白名单、颜色格式、布局约束、禁止直接生成 C 代码。

参考文件：

```text
https://github.com/espressif/esp-claw/blob/master/components/claw_modules/claw_core/src/claw_core_control.c
https://github.com/espressif/esp-claw/blob/master/AGENTS.md
```

---

## 12. 推荐最小演示 Scene

```xml
<scene id="main" width="320" height="240" bg_color="#101418">
  <container id="panel"
             x="16" y="16" w="288" h="208"
             bg_color="#17212B"
             radius="8"
             border_color="#76B7E8"
             border_width="2" />

  <label id="title_label"
         x="32" y="40" w="256" h="32"
         text="ESP GFX"
         color="#FFFFFF"
         font="default"
         font_size="24"
         align="center" />

  <button id="ok_btn"
          x="90" y="156" w="140" h="44"
          text="OK"
          bg_color="#2F8CFF"
          text_color="#FFFFFF"
          radius="8"
          border_color="#A9D8FF"
          border_width="1"
          callback="on_ok" />
</scene>
```

推荐测试 prompt：

```text
把标题改成 AI Scene，颜色改成黄色。
把 ok_btn 文字改成 Start，背景改成绿色。
把按钮移动到底部居中。
把整个 panel 改成深灰色，边框改成蓝色。
```

---

*End of document*
