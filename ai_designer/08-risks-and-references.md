# 风险、与 ITE/ITU 的关系、目录建议

> 上一篇：[TODO List](07-todo.md) ｜ 返回：[README / 索引](README.md)

## 1. 风险与约束

### 1.1 模型输出不稳定

处理方式：

- 模型只输出 tool call。
- 工具层做 schema 校验。
- 不允许模型直接写任意文件，只能输出 tool call 或受控 JSON patch。
- 修改前保存 undo snapshot。

### 1.2 设备端资源有限

处理方式：

- 设备端不解析 JSON。
- 上位机预编译 package。
- 静态布局和 style 尽量提前计算。

### 1.3 预览与设备端不一致

处理方式：

- SDL Preview 使用 `esp_emote_gfx` 同一套 object / renderer。
- package loader 在 Host 和 Device 尽量共用。
- 避免用浏览器 DOM 作为最终渲染预览。

### 1.4 过早设计复杂格式

处理方式：

- 第一版 JSON 简单、显式、绝对坐标。
- package 格式先满足 label/button/container。
- 动画、图片、布局系统后续扩展。

## 2. 与 ITE / ITU 的关系

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

## 3. 文件和目录建议

第一阶段建议新增：

```text
tools/ai_scene_studio/
  server/
    agent_server.py 或 agent_server.ts
    scene_tools.*
    json_patch.*
    preview_bridge.*
  web/
    ChatPanel.*
    App.*

examples/ai_scene_demo/
  scene.json
  assets/
  README.md

include/gfx/scene.h
src/scene/
  gfx_scene_json.c       # Host only, JSON loader / debug path
  gfx_scene_package.c    # Host/Device common package loader
  gfx_scene_priv.h

scripts/scene_compiler/
  compile_scene.py
```

如果第一版要更快，可以先不加 public `include/gfx/scene.h`，先放在 `simulation/host` 或 `examples/ai_scene_demo` 内部验证。
