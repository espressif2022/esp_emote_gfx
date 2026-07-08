# Tool Calling 设计

> 上一篇：[Scene 格式与属性白名单](03-scene-format.md) ｜ 下一篇：[Skill 与云端交互](05-skills-and-cloud.md)

属性字段以 [03-scene-format.md](03-scene-format.md) 的「属性白名单」为唯一数据源，tool schema 从中派生。

## 1. 工具列表

第一版工具：

| Tool | 作用 |
|------|------|
| `scene.get_widgets` | 返回当前 scene 中可修改控件 |
| `scene.patch_widget` | 修改指定控件属性 |
| `scene.reload_preview` | 通知 SDL 重新加载 scene |
| `scene.undo` | 回滚上一次修改 |
| `scene.export_package` | 导出设备端 package |
| `preview.screenshot` | 获取当前预览截图 |

## 2. `scene.get_widgets`

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

## 3. `scene.patch_widget`

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

### 3.1 patch 语义（全有或全无）

- **原子性**：一次 `patch_widget` 的多个 prop 采用**全有或全无**。先对所有 prop 跑 schema 校验（字段属于该 type 白名单、类型/枚举/颜色/范围合法），**任一非法则整条 patch 失败、不写盘、不刷新**，返回错误列表；不做「部分应用」。这样 undo 边界清晰、模型也能拿到明确反馈重试。
- **未知/不适用字段**：`target_id` 不存在、prop 不在该 widget 白名单、改「受限」字段超出当前支持 → 归入校验失败。
- **失败响应**：

```json
{
  "ok": false,
  "errors": [
    { "target_id": "ok_btn", "prop": "bg_color", "reason": "invalid_color", "value": "blueish" },
    { "target_id": "ok_btn", "prop": "shadow", "reason": "unknown_prop" }
  ]
}
```

- **成功副作用顺序**：校验通过 → 保存 undo snapshot（见 §5） → 写回 `scene.json` → 触发 reload/刷新（见 [02 §2.5](02-architecture.md)）。

## 4. `scene.get_widgets` 的层级

`get_widgets` 返回的列表可带 `children`（与 [03 §1.2](03-scene-format.md) 一致）或附带 `parent` 字段，供模型理解树结构；`scene.patch_widget` 只按 `target_id` 定位，不关心层级。

## 5. undo / 回滚语义

- **快照粒度**：每次成功的 `patch_widget`（或任何写 `scene.json` 的工具）前，保存**整份 `scene.json` 的快照**（第一版简单可靠，scene 很小，不做增量 diff）。
- **栈深度**：维护一个 undo 栈，默认深度 16（可配置）；超出丢弃最旧。
- **`scene.undo`**：弹出栈顶快照覆盖当前 `scene.json` → reload 刷新。无可回滚项时返回 `{ "ok": false, "reason": "nothing_to_undo" }`。
- **与取消的关系**（B6）：用户中途取消 AI 请求后，已应用的 patch 通过 undo 栈回滚；未应用的后续 tool call 不再执行。
- redo 第一版不做（保持简单），后续可加对称的 redo 栈。
