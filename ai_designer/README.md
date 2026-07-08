# ESP Emote GFX AI Tool 设计

> 文档状态：草案 v0.1
> 目标仓库：`esp_emote_gfx`
> 目标方向：通过 AI Chat Panel 修改 UI 描述文件，实时预览，并导出设备端可运行的 scene package / ITU-like 资源。

本设计文档已按类别拆分到本文件夹，本页为导航索引。

## 目录

| 文件 | 内容 |
|------|------|
| [01-principles.md](01-principles.md) | 核心原则（Chat Panel 定位、AI 不写 C、Host/设备端分工、上位机即编译器） |
| [02-architecture.md](02-architecture.md) | 总体架构 + 模块划分（Chat Panel、Agent Server、Loader、Preview Bridge、Compiler/Export） |
| [03-scene-format.md](03-scene-format.md) | Scene JSON 格式、**属性白名单（唯一数据源）**、推荐最小演示 scene |
| [04-tool-calling.md](04-tool-calling.md) | Tool Calling 设计（工具列表、请求/响应示例） |
| [05-skills-and-cloud.md](05-skills-and-cloud.md) | 云端交互与 GFX skill 暴露（参考 esp-claw） |
| [06-milestones.md](06-milestones.md) | 里程碑拆分（里程碑一 本地预览 / 里程碑二 二进制+设备 / 里程碑三 接模型，**模型放最后**） |
| [07-todo.md](07-todo.md) | TODO List（主线 A/B/C/D + 集成验收 E） |
| [08-risks-and-references.md](08-risks-and-references.md) | 风险与约束、与 ITE/ITU 的关系、文件目录建议 |
| [09-package-format.md](09-package-format.md) | Scene package 二进制格式 v1（`.gsp`）字节布局、枚举编码、三列映射、加载算法、与 ITU 的对比与取舍 |
| [10-loader-api.md](10-loader-api.md) | Scene Loader C API 定稿（`gfx/scene.h`）：类型、错误码、生命周期、各函数语义 |
| [11-testing.md](11-testing.md) | 测试策略：schema 校验、round-trip、loader 正确性、reload、坏包 fuzz、回调语义 |
| [12-roadmap.md](12-roadmap.md) | 执行路线：每步可单独验证的闭环楼梯（S1–S12 本地，S9 模型放最后） |

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

## 快速上手建议

先读 [01-principles.md](01-principles.md) 建立约束共识，再看 [03-scene-format.md](03-scene-format.md) 定契约，然后按 [12-roadmap.md](12-roadmap.md) 的 S1–S12 逐级开工（大模型放最后 S9）。里程碑归属见 [06-milestones.md](06-milestones.md)，任务清单见 [07-todo.md](07-todo.md) 主线 A。
