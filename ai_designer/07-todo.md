# TODO List

> 上一篇：[里程碑拆分](06-milestones.md) ｜ 下一篇：[风险与参考](08-risks-and-references.md)

里程碑归属见 [06-milestones.md](06-milestones.md)；属性白名单唯一数据源见 [03-scene-format.md](03-scene-format.md)。

## 主线 A：在现有 motion / gui_designer 链路中接入 `button`、`label` 描述

- [ ] **A1. 梳理现有 motion `scene.json → .inc` 数据流**
  Description: 以 `gui_designer/gui_designer.html` 为基线，记录现有 normalize、compile、export `.inc` 的输入输出结构，提炼**可参考的编译模式**（schema 驱动 normalize、字符串 ID→整数索引、双产物、设备端 init 校验）。注意：编译逻辑全在 HTML 里、且是几何/姿态专用，UI scene 不复用其代码。**基线数据文件用 `gui_designer/claw_motion.scene (2).json` + `examples/motion/claw_motion.inc`（二者匹配）；`examples/motion/claw_motion.scene.json` 是旧版、与 `.inc` 不同步，勿用。**

- [ ] **A2. 定义通用 UI scene schema：`gfx_ui_scene_v1`（第 0 号契约）**
  Description: 这是所有下游的输入契约，应最先冻结。字段定义直接引用 [03-scene-format.md](03-scene-format.md) 「属性白名单」（唯一数据源）。第一版支持 `screen`、`widgets`、`resources`、`callbacks`；`widgets` 只放 `container`、`label`、`button`，全部绝对坐标，`screen.w/h` 必须等于显示分辨率（见 [03-scene-format.md](03-scene-format.md) 坐标空间约定）。

- [ ] **A3. 在 scene JSON 中补齐 `label` 描述**
  Description: 按 [03-scene-format.md](03-scene-format.md) 「属性白名单」的 label 字段实现（唯一数据源，勿再复制清单）。注意 `font/font_size` 为「受限」，依赖 A11。

- [ ] **A4. 在 scene JSON 中补齐 `button` 描述**
  Description: 按 [03-scene-format.md](03-scene-format.md) 「属性白名单」的 button 字段实现。注意 `callback` 为「受限」，依赖 A12；`font` 依赖 A11。

- [ ] **A5. 在 scene JSON 中补齐 `container` 描述**
  Description: 按 [03-scene-format.md](03-scene-format.md) 「属性白名单」的 container 字段实现。

- [ ] **A6. 拆分 `gui_designer` 中的编译逻辑**
  Description: 将 HTML 内的 scene normalize / compile / export 思路迁移到 `scripts/scene_compiler/`，避免编译器只存在于网页脚本中。第一版可以先保留 JS 或 Python 任一实现，但要从 UI 中解耦。

- [ ] **A7. 新增 `examples/ai_scene_demo/scene.json`**
  Description: 放一个最小 demo，包含一个 container、一个 label、一个 button，作为 Chat Panel、compiler、package loader 的共同测试输入。

- [ ] **A8. Host 侧支持从 JSON 创建 GFX object tree**
  Description: 增加调试路径 `gfx_scene_load_json()` 或示例内 loader，把 `label/button/container` JSON 映射到现有 `gfx_label_create()`、`gfx_button_create()`、`gfx_container_create()`。

- [ ] **A9. Host SDL demo 接入 UI scene 预览入口**
  Description: 新增一个独立 host demo 或在现有 SDL demo 中加入 AI scene demo，能加载 `examples/ai_scene_demo/scene.json` 并显示真实 `esp_emote_gfx` 渲染结果。可参考 `simulation/host/gfx_host_sdl_demo.c` 的 `gfx_display_port_open` → build scene → `gfx_host_runner_run` 模式，用 `gfx_fs_load("scene.json")` 读文件。

- [ ] **A10. scene 生命周期：销毁 + reload（隐藏必做项）**
  Description: loader 必须记录自己创建的所有 `gfx_object_t`，实现 `gfx_scene_delete()`（整树删除）与 `gfx_scene_reload()`（销毁旧树 → 重新加载 → 刷新）。现有 host 无“清空场景重建”路径（display 即根，widget 建了就挂上），不做会导致 reload 泄漏或对象叠加。这是 [02-architecture.md](02-architecture.md) Preview Bridge 的前置依赖。

- [ ] **A11. font registry：解决 font / font_size 无 setter**
  Description: `gfx_label/gfx_button` 无 `set_font_size`，字号只能在 `gfx_label_font_create()` 建 font 时定死，host 也未接磁盘字体。实现按注册名解析的 font registry（`gfx_font_registry_register/get/default`，形态见 [09-package-format.md](09-package-format.md) §6.1）。命名规范 `<family>_<pxsize>`，host/device 注册表内容一致。**第一版可先锁定单一默认字体**，schema 接受 `font/font_size` 但 loader 降级，闭环跑通后再扩展。

- [ ] **A12. button callback 名称绑定 + click 语义**
  Description: button 无点击回调 API，只有基类 `gfx_object_set_touch_cb`（PRESS/RELEASE/MOVE）。定义“click = 按下且在范围内抬起”，并维护 **callback 名称 → 函数指针注册表**，由 `gfx_scene_bind_callback(scene, "on_ok", cb, user_data)` 绑定。JSON 里 `callback` 只登记名称，不允许模型新造未注册的名字。与设备端 D5 共用同一套语义。API 定稿见 [10-loader-api.md](10-loader-api.md)。

- [ ] **A13. Preview Bridge：MVP-1 文件 mtime 轮询**
  Description: host demo 在 `gfx_host_runner_config_t.loop_cb` 里轮询 `scene.json` 的 mtime，变化则校验 JSON 完整后 `gfx_scene_reload` + 刷新，实现 E1/E2 的自动刷新，不依赖 Agent Server。机制见 [02-architecture.md](02-architecture.md) §2.5。MVP-2 再换/加 socket/WS 事件（归入 B7）。

- [ ] **A14. scene loader C API 定稿并落头文件**
  Description: 按 [10-loader-api.md](10-loader-api.md) 落 `gfx/scene.h`（或 host 内部头）：`gfx_scene_t`、错误码、`load_json/load_package/reload/find_obj/bind_callback/delete` 的签名与语义。A8/A10/D4/D5 依赖它。

## 主线 B：参考 `esp-claw` 暴露大模型接入接口

- [ ] **B1. 设计 Agent request 生命周期**
  Description: 参考 `esp-claw` request / inflight / abort 思路，定义 `request_id`、`session_id`、`phase`、`abort`、`error`、`tool_iteration_count`、`created_ms`、`updated_ms`。

- [ ] **B2. 设计 Chat Panel 状态机**
  Description: 状态包括 `idle`、`thinking`、`tool_calling`、`patching_json`、`compiling_scene`、`reloading_preview`、`exporting_package`、`done`、`error`、`canceled`。

- [ ] **B3. 设计 Local Agent Server 接口**
  Description: 第一版接口包括 `POST /chat`、`GET /scene/current`、`POST /scene/patch`、`POST /scene/reload`、`POST /scene/export`、`POST /scene/undo`、`WS /events`。

- [ ] **B4. 设计云端模型适配层**
  Description: 隔离 OpenAI / 内网模型 / 本地模型差异，对上层只暴露 message、tool schema、tool result、stream event。

- [ ] **B5. 设计 tool call dispatcher**
  Description: 模型只能调用白名单工具。第一版工具包括 `scene.get_widgets`、`scene.patch_widget`、`scene.reload_preview`、`scene.undo`、`scene.export_package`。

- [ ] **B6. 设计取消和回滚机制**
  Description: 用户取消后停止继续请求模型，不再应用后续 tool call；已应用 JSON patch 通过 undo snapshot 回滚。

- [ ] **B7. 设计事件流协议**
  Description: Agent Server 通过 WebSocket 向 Chat Panel / SDL Preview 广播 `phase_changed`、`tool_started`、`tool_result`、`scene_changed`、`preview_reloaded`、`export_done`。

## 主线 C：暴露 `button`、`label` 组件 skill

- [ ] **C1. 定义 GFX skill 描述文件格式**
  Description: 每个 skill 描述组件用途、可修改属性、默认值、取值范围、布局约束、禁止事项，以及最终可落到哪些 tool。

- [ ] **C2. 暴露 `gfx_label` skill**
  Description: skill 的可编辑属性、取值范围、约束**从 [03-scene-format.md](03-scene-format.md) 派生**，勿另写清单；要求模型输出 `scene.patch_widget`，不生成 C 代码。对「受限」字段（font/font_size）在 skill 里明确标注当前支持边界。

- [ ] **C3. 暴露 `gfx_button` skill**
  Description: 属性从 [03-scene-format.md](03-scene-format.md) 派生；要求模型只能改 JSON 或发 tool call。skill 里说明 `callback` 只能引用已注册回调名（A12），不能新造。

- [ ] **C4. 暴露 `gfx_container` skill**
  Description: 属性从 [03-scene-format.md](03-scene-format.md) 派生。

- [ ] **C5. 暴露 `scene_layout` skill**
  Description: 第一版只支持绝对坐标和简单对齐词汇，例如 “居中”、“底部居中”、“靠左”。本地工具负责把这些意图转换为 x/y。

- [ ] **C6. 暴露 `scene_export` skill**
  Description: 告诉模型导出不是生成 C create 代码，而是调用 `scene.export_package` 生成二进制 package / `.inc`。

- [ ] **C7. 生成模型上下文摘要**
  Description: 从当前 `scene.json` 生成简洁 widget tree 摘要，供模型判断可修改对象，例如 `ok_btn: button at (90,156,140,44)`。

## 主线 D：导出二进制文件并让设备端直接解析运行

- [ ] **D1. 定义 UI scene package 二进制格式 v1**
  Description: 格式包含 header、string table、object table、style table、callback table。第一版不打包图片/字体二进制，只引用资源名或默认字体。**具体字节布局、加载算法、校验、与 ITU 的对比取舍已固化在 [09-package-format.md](09-package-format.md)；本任务收尾时确认 §7/§9 的开放点（颜色 565/888、StyleEntry 宽度等）拍板。**

- [ ] **D2. 编写 scene compiler：`scene.json → scene.gsp`**
  Description: 参考 motion `.inc` 导出方式，把 JSON 编译成稳定二进制。`.gsp` 作为第一版 package 后缀，后续可再决定是否兼容 ITU-like 格式。

- [ ] **D3. 编写可选 `.inc` 导出：`scene.gsp → C array`**
  Description: 方便 ESP 工程直接 include，类似 `claw_motion.inc`。也支持把 `.gsp` 放入 mmap assets 分区。

- [ ] **D4. 实现 Host/Device 共用 package loader**
  Description: 新增 `gfx_scene_load_package()`，从二进制 package 恢复 object tree，创建 label/button/container，并设置属性。按 [09-package-format.md](09-package-format.md) §10 的加载算法实现（先序 + parent_idx 一遍扫描建树）。

- [ ] **D5. 实现 callback name 绑定**
  Description: package 中只保存 callback 名称，设备端通过 `gfx_scene_bind_callback(scene, "on_ok", cb, user_data)` 绑定业务逻辑。

- [ ] **D6. 增加 package 校验**
  Description: loader 校验 magic、version、size、offset、string table 越界、object count、属性范围，避免坏包导致设备端崩溃。校验清单见 [09-package-format.md](09-package-format.md) §10（含 parent_idx 无环、索引越界、screen 分辨率一致等）。

- [ ] **D7. Host package smoke test**
  Description: 编译 `examples/ai_scene_demo/scene.json` 为 `scene.gsp`，Host 加载 package 并验证 object 创建成功。

- [ ] **D8. ESP device smoke test**
  Description: 在 ESP 示例中加载同一个 package 或 `.inc`，验证 label/button/container 可显示，button callback 可触发。

## 集成验收 TODO

- [ ] **E1. 完整链路演示：JSON 手动修改 → SDL 刷新**
  Description: 修改 `scene.json` 中 `title_label.text`，触发 reload 后 SDL 显示变化。

- [ ] **E2. 完整链路演示：tool patch → JSON 修改 → SDL 刷新**
  Description: 调用 `scene.patch_widget(ok_btn, {"text":"Start","bg_color":"#00A86B"})` 后，JSON 被修改，SDL 自动刷新。

- [ ] **E3. 完整链路演示：JSON → package → Host load**
  Description: 编译 `scene.json` 得到 `scene.gsp`，Host 不读 JSON，只读 package 也能渲染一致画面。

- [ ] **E4. 完整链路演示：package → ESP load**
  Description: 同一个 package 在 ESP 设备端加载运行，不依赖 JSON parser。

- [ ] **E5. 完整链路演示：Chat Panel → 模型 tool call → preview → export**
  Description: 用户输入“把 ok_btn 改成蓝色，文字改成 Start”，模型调用 skill/tool，本地 patch JSON，SDL 刷新，最终导出 package。

## 单元 / 契约 / 健壮性测试 TODO

> 细化策略与用例见 [11-testing.md](11-testing.md)。

- [ ] **T1. schema 校验测试**：合法/非法用例集，非法必须被拒且给出明确错误（[11](11-testing.md) §1）。
- [ ] **T2. compiler round-trip 测试**：`json→gsp→解析` 一致、先序+parent_idx 正确、去重、逐字节确定性（§2）。
- [ ] **T3. loader 正确性测试**：json 路径与 package 路径产出同构 object tree、find_obj、属性落地（§3）。
- [ ] **T4. reload 生命周期测试**：连续 reload 无泄漏无叠加、delete 干净、坏输入保留旧场景（§4）。
- [ ] **T5. 坏包 fuzz 测试**：越界/坏 parent_idx/坏枚举/随机字节，绝不崩，仅返回错误码（host + ASan，§5）。
- [ ] **T6. 回调 click 语义测试**：范围内抬起触发、移出不触发、解绑不触发（§6）。
- [ ] **T7. 颜色/面板无关测试**：同 scene 在 565/888 两种 display-port 下均可加载渲染（§7）。
