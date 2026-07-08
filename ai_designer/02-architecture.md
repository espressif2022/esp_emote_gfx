# 总体架构与模块划分

> 上一篇：[核心原则](01-principles.md) ｜ 下一篇：[Scene 格式与属性白名单](03-scene-format.md)

Scene JSON 的数据模型与属性白名单单独放在 [03-scene-format.md](03-scene-format.md)（唯一数据源）。

## 1. 总体架构

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
│ - JSON patch / validation                                     │
│ - preview bridge                                              │
│ - package export                                              │
└──────────────────────────────────────────────────────────────┘

        │
        ▼
┌──────────────────────────────────────────────────────────────┐
│ Scene Files                                                   │
│                                                              │
│ scene.json                                                    │
│ assets/                                                       │
│ scene.gsp / scene.itu-like                                    │
└──────────────────────────────────────────────────────────────┘
```

## 2. 模块划分

### 2.1 Chat Panel

职责：

- 显示用户输入和 AI 回复。
- 展示模型调用了哪些工具。
- 显示 JSON 修改摘要。
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

### 2.2 Local Agent Server

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

### 2.3 Scene JSON

Scene JSON 的 canonical 格式、坐标/颜色约定、属性白名单见 [03-scene-format.md](03-scene-format.md)。

### 2.4 GFX Scene Loader

GFX Scene Loader 的目标是把上位机导出的 scene package **直接恢复成设备端 `gfx_object_t` object tree**，而不是在设备端保留一套独立的 UI 描述运行时。

Host 调试路径可以解析 JSON，方便快速预览：

```text
scene.json
  → JSON loader / scene compiler
  → gfx_object_t tree
```

设备端运行路径只加载二进制 package：

```text
scene.gsp / scene package
  → package object table
  → GFX object factory
  → gfx_object_t tree
```

两条路径最终都落到同一组设备端 object factory：

```text
scene.screen       → gfx_display background
type: container    → gfx_container_create()
type: label        → gfx_label_create()
type: button       → gfx_button_create()
```

设备端不解析 JSON / XML 字符串属性；颜色、坐标、尺寸、字符串表偏移、callback 名称等都由上位机 compiler 预处理进 package。

目标 API 草案：

```c
typedef struct gfx_scene gfx_scene_t;

gfx_err_t gfx_scene_load_json(gfx_display_t *disp, const char *path, gfx_scene_t **out_scene);
gfx_err_t gfx_scene_load_package(gfx_display_t *disp, const void *data, size_t size, gfx_scene_t **out_scene);
gfx_err_t gfx_scene_reload(gfx_scene_t *scene);
gfx_object_t *gfx_scene_find_obj(gfx_scene_t *scene, const char *id);
gfx_err_t gfx_scene_bind_callback(gfx_scene_t *scene, const char *name, void *cb, void *user_data);
void gfx_scene_delete(gfx_scene_t *scene);
```

**落地时要注意的三个现有 API 现实**（对照 `include/gfx/widgets/*.h` 与 `include/gfx/object.h`）：

1. **属性映射主体很轻**：三个组件的 setter 高度统一（`gfx_object_t* + 类型化 setter`），坐标/尺寸/可见性走基类 `gfx_object_set_pos/size/visible`，颜色走 `gfx_color_hex(0xRRGGBB)`。约 90% 属性能一一对应，主要复杂度在下面三点。

2. **font / font_size 没有 setter**：`gfx_label` / `gfx_button` **没有 `set_font_size`**，字号只能在 `gfx_label_font_create()` 建 font 时定死；host 目前用编译进二进制的 LVGL 字体 blob，未接磁盘字体。所以 `font/font_size` 需要一个 **font registry（按 name+size 缓存/创建 `gfx_font_t`）**。第一版建议锁定单一默认字体，`font/font_size` 只支持有限枚举或先标为不可改（见 [07-todo.md](07-todo.md) A11）。

3. **button 没有点击回调 API**：只有基类通用的 `gfx_object_set_touch_cb`（PRESS/RELEASE/MOVE）。JSON 里的 `"callback": "on_ok"` 要落地，需要定义“点击”语义（按下 + 在范围内抬起）并维护 **name→函数指针注册表**，再由 `gfx_scene_bind_callback` 绑定（见 [07-todo.md](07-todo.md) A12 / D5）。

4. **reload 需要整树销毁**：host 目前没有“清空场景重建”的路径（display 就是根，widget 建了就挂上）。`gfx_scene_reload` / `gfx_scene_delete` 要求 loader **记录自己创建的所有对象并能整树删除**，否则 reload 会泄漏或叠加（见 [07-todo.md](07-todo.md) A10）。

### 2.5 Preview Bridge

SDL Preview（C/SDL 进程）与改文件的一方（人手 / Agent Server，通常是另一个 python/ts 进程）之间需要一条「scene 变了 → 重新加载」的通路。分两档实现，按里程碑推进：

**MVP-1：文件 mtime 轮询（最简，无需 Agent Server）**

host demo 在主循环 `gfx_host_runner_config_t.loop_cb` 里周期性 `stat(scene.json)`，mtime 变化就 reload：

```text
loop_cb(now):
  if stat("scene.json").mtime != last_mtime:
      last_mtime = mtime
      gfx_scene_reload(scene)          # 见 §2.4 / A10：销毁旧树→重载→刷新
      gfx_core_refresh_now(gfx)
```

这样 E1（手动改 json → 刷新）、E2（本地工具 patch → 刷新）都能跑通，**完全不依赖模型/服务器**。debounce：mtime 抖动或写到一半时，先校验 JSON 完整（parse 成功）再 reload，失败则保留旧场景并告警。

**MVP-2：本地 socket / WebSocket 事件（接入 Agent Server 后）**

Agent Server 写盘后主动推事件，避免轮询延迟：

```json
{ "event": "scene.reload", "path": "examples/ai_scene_demo/scene.json" }
```

host 侧收事件后：

```text
stop current scene
delete old objects (gfx_scene_delete)
load scene again  (gfx_scene_load_json)
refresh display   (gfx_core_refresh_now)
send status back  ({ "event":"preview_reloaded", "ok":true } / 或 error)
```

传输选型（MVP-2 再定，二选一）：本地 TCP/UDS 行协议（host 起一个轻量监听线程），或复用 Agent Server 的 `WS /events`。第一版优先 mtime 轮询，socket 作为增量优化，不阻塞闭环。

### 2.6 Scene Compiler / Export

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

> 完整的二进制字节布局、加载算法、校验、以及为什么不照搬 ITU 的内存镜像重定位（而用 index-table + factory），见 [09-package-format.md](09-package-format.md)。
