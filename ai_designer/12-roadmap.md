# 执行路线：每步可单独验证的闭环（模型放最后）

> 上一篇：[测试策略](11-testing.md) ｜ 返回：[README / 索引](README.md)

本页把 [06-milestones.md](06-milestones.md) 的里程碑落成一条**可逐级验证**的直线楼梯。核心纪律：

1. **每级都产出一个能跑 / 能看 / 能测的东西**，绿了才走下一级，不带病往上叠。
2. **把最不可控的一环（大模型）放到最后**。S1–S12 全程本地确定性、不接模型；到最后一步 S9 才引入模型，且此时它依赖的工具/服务全部已验证。
3. **S1、S3 是“正确答案”锚点**：后续步骤的验收用“画面与 S1/S3 一致”这种同构对比代替主观判断。

## 走法总览

```text
第一大段：本地可控闭环（全部先做完并验证）
  S1  C 硬编码场景 ────────────── 证明现有 API 能建目标对象树
  S2  schema + scene.json + 校验器  契约可测（不依赖渲染）
  S3  JSON loader ───────────────  画面 == S1
  S4  delete/reload + scene.h ───  连续 reload 无泄漏
  S5  mtime 自动预览 ────────────  改文本→即见（E1）
  S6  确定性 patch 工具 ─────────  patch→自动刷新（E2）
  S7  undo ──────────────────────  安全网
  S10 compiler json→.gsp ────────  round-trip 一致（T2）
  S11 package loader (Host) ─────  只读包渲染 == S3（E3）
  S12 package 上 ESP ────────────  设备端运行（E4）
  S8  Agent Server 骨架 ─────────  curl 驱动 /scene/*，不接模型

第二大段（最后）：
  S9  接入大模型 ────────────────  自然语言→tool call→预览→导出（E5）
```

## 每一级：做什么 / 单独验证 / 闭环产物

| 步骤 | 做什么（对应任务） | 单独验证（可跑/可测） | 闭环产物 |
|---|---|---|---|
| **S1** | 新建 demo，用现有 C API 直接拼 1 容器+1 标签+1 按钮（不碰 JSON） | 运行 demo，肉眼见 3 控件按预期显示 | 证明目标对象树用现有 API 能建出来；提前暴露字体(A11)/回调(A12)真实边界 |
| **S2** | 冻结 `gfx_ui_scene_v1`（A2）+ 写 `examples/ai_scene_demo/scene.json`（A7）+ JSON Schema | 校验器对合法/非法用例分别通过/拒绝并报明确错误（T1） | 契约可独立测试，不依赖渲染 |
| **S3** | `gfx_scene_load_json()`（A8），映射到 S1 那套 create 调用 | 画面与 **S1 完全一致**（T3） | JSON→渲染打通 |
| **S4** | 记录所建对象，`gfx_scene_delete/reload`（A10）+ API 落 `gfx/scene.h`（A14） | 循环 reload N 次，对象数不增长；ASan 无泄漏（T4） | 可反复重建场景 |
| **S5** | 主循环轮询 `scene.json` mtime，变更则校验+reload（A13） | 运行时手改 JSON 文本保存 → 1s 内 SDL 刷新（**E1**） | 「改文本→即见」闭环 |
| **S6** | 本地执行 `scene.patch_widget` + 原子校验（04 §3.1） | CLI patch → JSON 安全改写+刷新（**E2**）；非法值整条原子拒绝 | patch→预览闭环（里程碑一完成） |
| **S7** | patch 前存整份快照，`scene.undo`（04 §5） | patch 后 undo → 文件字节还原、画面回退 | 安全网 |
| **S10** | scene compiler `json→.gsp`（D1/D2） | `json→gsp→解析` 语义一致、逐字节确定（T2）；坏包 fuzz 不崩（T5）。纯 host 测 | 二进制格式自证 |
| **S11** | `gfx_scene_load_package()`（D4/D6） | host 只读 `.gsp` 渲染，画面 == S3（**E3**）；565/888 都跑（T7） | 包路径与 JSON 路径同构 |
| **S12** | 设备端加载同一包/`.inc`（D3/D5/D8） | ESP 上控件正常显示 + 按钮回调触发（**E4**） | 两端同源 |
| **S8** | 本地服务骨架：`/scene/current`、`/scene/patch`、`/scene/undo`、`WS /events`（B3 子集），内部调 S6/S7 | `curl` 打接口返回正确 JSON、WS 收到 `scene_changed`/`preview_reloaded` | 模型接入点就绪，用 curl 全测（里程碑二完成） |
| **S9** *(最后)* | 模型适配层 + tool dispatcher + Chat Panel（B/C 线），模型只出 tool call | 输入「把 ok_btn 改蓝、文字改 Start」→ 模型发 `patch_widget` → JSON 改 → SDL 刷新（**E5**） | 自然语言编辑闭环 |

## 关键路径

```text
S1 → S2 → S3 → S4 → S5 → S6   （里程碑一，最高优先级）
                         → S10 → S11 → S12   （里程碑二，本地二进制/设备）
                                          → S8 → S9   （最后接模型）
```

- **可并行**：S2 的 JSON Schema、S10 的包格式定稿（D1）都只依赖 A2 冻结的 schema，可与 S3–S6 并行准备，不占关键路径。
- **降级先行**：S1 阶段字体锁单一默认、回调用 click→function 单条即可（A11/A12 降级），不阻塞闭环；完整字体注册表/action 模型后置。
- **起点**：从 **S1 + S2** 开工（可并行）——S1 摸清 API 真实边界，S2 冻结契约。
