# 里程碑拆分（按风险从小到大）

> 上一篇：[Skill 与云端交互](05-skills-and-cloud.md) ｜ 下一篇：[TODO List](07-todo.md)

任务编号（A/B/C/D/E）见 [07-todo.md](07-todo.md)。

**核心排序原则：把最不可控的一环（大模型）放到最后。** 先把本地确定性、可反复验证的链路（描述 → 预览 → patch → 二进制包 → 设备端）从头到尾全部打通并测试通过，模型只是最后叠加在一个已经完整可用的系统之上。因此本文件把原“MVP-2=接模型”与“Phase-2=二进制”**对调**：二进制/设备端先做，接模型放最后。

依赖关系（A 是地基；二进制链路先行，模型最后）：

```text
A（schema + JSON loader + host 预览 + reload）   ← 地基
      │
      ├─→ 确定性 patch + reload（不接模型）→ E1 / E2          = 里程碑一
      ├─→ D（package）+ 本地服务骨架（不接模型）→ E3 / E4     = 里程碑二
      └─→ B（模型 + Chat Panel）+ C（skills）→ E5             = 里程碑三（最后）
```

对应「每步可单独验证」的走法见 [12-roadmap.md](12-roadmap.md) 的 S1–S12（本地）+ S9（模型，最后）。

## 里程碑一：描述式 scene + 手动/工具 patch + SDL 预览（不接模型、不碰二进制）

对应任务：A2、A3、A4、A5、A7、A8、A9、A10（+ A11/A12 降级版）、A13、A14。对应 roadmap 步骤 S1–S7。验收：E1、E2。

```text
1. 定义并冻结 gfx_ui_scene_v1 schema。
2. host 加载 examples/ai_scene_demo/scene.json，SDL 显示真实 esp_emote_gfx 渲染。
3. 手动改 scene.json 中 title_label.text → reload → SDL 刷新（E1）。
4. 用本地确定性工具调用 scene.patch_widget(ok_btn, {...}) → JSON 被安全修改 → SDL 自动刷新（E2）。
```

这一版**不需要大模型、不需要 package**，先证明“描述 + patch + reload 预览”闭环成立。

## 里程碑二：二进制 package + 设备端加载 + 本地服务骨架（仍不接模型）

对应任务：D1–D8、A6（compiler 解耦）、B3 的**本地服务骨架子集**（`/scene/*` 接口 + `WS /events`，用 curl 驱动，不接模型）。对应 roadmap 步骤 S8、S10–S12。验收：E3、E4。

```text
1. 编译 scene.json → scene.gsp，host 只读 package 也渲染一致画面（E3）。
2. 同一 package 在 ESP 设备端加载运行，不依赖 JSON parser（E4）。
3. 起本地 Agent Server 骨架，curl 打 /scene/patch、/scene/reload 均可用，WS 广播事件正确——为里程碑三预留模型接入点，但此时不含任何模型。
```

跑完这一步，你手里就是一个**改 JSON / 发 patch / 编包 / 上设备全部可用、且每环都有独立测试**的确定性系统。

## 里程碑三（最后）：接入大模型 Chat Panel

对应任务：B1–B2、B4–B7（模型相关部分）、C1–C7。对应 roadmap 步骤 S9。验收：E5。

```text
1. 打开 AI Scene Studio，左侧 SDL 预览，右侧 Chat Panel。
2. 输入“把 ok_btn 改成蓝色，文字改成 Start”。
3. Agent 调用 scene.patch_widget（模型只出 tool call，走里程碑二已验证的服务与工具）。
4. scene.json 被安全修改，SDL 自动刷新，Change Log 显示修改。
```

模型接入时，它依赖的 `patch_widget`、reload、undo、export 全部已在里程碑一/二验证过；模型只需“会调用这些已验证的工具”，风险降到最低。

这三步跑通后，再扩展 image、font、animation、resource compiler 等更复杂能力。
