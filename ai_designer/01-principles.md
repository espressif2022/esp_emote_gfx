# 核心原则

> 上一篇：[README / 目标](README.md) ｜ 下一篇：[总体架构](02-architecture.md)

## 1. Chat Panel，不是 ChatPet

这里的交互入口是类似 VSCode Codex / Copilot 的 **Chat Panel**：

```text
左侧：GFX SDL Preview
右侧：AI Chat Panel
底部：修改记录 / Undo / Export
```

Chat Panel 是 AI 工具入口，不是桌面宠物，也不是普通聊天窗口。

## 2. AI 不直接写 C 代码

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

实际的 JSON 修改、校验、保存、预览刷新由本地工具完成。

## 3. Host 可解析 JSON，设备端优先加载 package

Host 侧为了开发效率可以直接解析 JSON：

```text
scene.json → gfx_scene_load_json() → SDL Preview
```

设备端不建议运行 JSON parser。设备端应加载上位机导出的二进制 package：

```text
scene.json → scene compiler → scene.gsp / scene.itu-like → ESP load
```

## 4. 上位机是资源编译器

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
