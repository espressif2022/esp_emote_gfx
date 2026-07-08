# 云端交互与 GFX Skill 暴露

> 上一篇：[Tool Calling](04-tool-calling.md) ｜ 下一篇：[里程碑拆分](06-milestones.md)

本页的 skill / tool 设计与 [07-todo.md](07-todo.md) 主线 B（大模型接入）、主线 C（组件 skill）对应；属性白名单以 [03-scene-format.md](03-scene-format.md) 为唯一数据源。

后续 Chat Panel 与云端大模型交互时，重点是参考 `esp-claw` 将能力以 skill / capability 形式暴露给大模型的分层方式。

在 `esp_emote_gfx` 中，应该把已有 GFX 组件作为可理解、可约束、可调用的 UI skills 暴露给模型：

```text
gfx_label skill
gfx_button skill
gfx_container skill
scene_layout skill
scene_export skill
```

模型通过这些 skill 理解当前工程支持哪些控件、哪些属性可改、属性值有什么约束，然后输出结构化 `scene.patch_widget`，由本地工具安全修改 JSON / scene 描述。

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
Capability execution              JSON patch / scene package export
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

## 设计要点清单

> 这些要点的可执行任务已落到 [07-todo.md](07-todo.md) 主线 B / C，此处仅作能力清单参考。

- `agent_request_t`：包含 `request_id`、`session_id`、`phase`、`abort`、`error`、`tool_iteration_count`。
- Chat Panel 状态机：`idle`、`thinking`、`tool_calling`、`patching_json`、`reloading_preview`、`exporting_package`、`done`、`error`、`canceled`。
- 云端模型接口适配层：隐藏 OpenAI / 内网模型 / 本地模型差异。
- GFX skill 描述文件：把 `label`、`button`、`container` 的属性白名单、默认值、取值范围、布局规则写成模型上下文（派生自 [03-scene-format.md](03-scene-format.md)）。
- tool call schema：第一版只允许 `scene.get_widgets`、`scene.patch_widget`、`scene.reload_preview`、`scene.undo`、`scene.export_package`。
- skill 到 tool 的映射：例如 `gfx_button` skill 最终只能落到 `scene.patch_widget` 或后续 `scene.create_widget`。
- 取消机制：用户取消后停止继续调用模型，不再应用后续 tool call；已应用 JSON patch 需要保留 undo。
- 事件流：Agent Server 通过 WebSocket 向 Chat Panel 和 SDL Preview 广播 phase、tool result、change log。
- skill 文档：描述 UI 修改规则、属性白名单、颜色格式、布局约束、禁止直接生成 C 代码。

参考文件：

```text
https://github.com/espressif/esp-claw/blob/master/components/claw_modules/claw_core/src/claw_core_control.c
https://github.com/espressif/esp-claw/blob/master/AGENTS.md
```
