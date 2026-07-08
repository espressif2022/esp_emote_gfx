# 测试策略

> 上一篇：[Scene Loader C API](10-loader-api.md) ｜ 下一篇：[执行路线（Roadmap）](12-roadmap.md)

[07-todo.md](07-todo.md) 的 E 系列是**端到端集成验收**；本页补齐更细的**单元/契约/健壮性**测试，让 compiler 和 loader 有回归护栏。测试尽量跑在 host（`simulation/host` 已有 smoke 框架，参考 `gfx_host_*_smoke.c`）。

## 1. Schema 校验测试（对应 A2 / A3–A5）

对 `gfx_ui_scene_v1` 校验器，正反用例各一组：

- **合法**：最小 scene、嵌套 container/children、各枚举合法值、缺省字段取默认。
- **非法（必须被拒）**：`schema` 值错、未知 `type`、未知字段、`id` 重复 / 非法字符、`label`/`button` 带 `children`、颜色非 `#RRGGBB`、枚举非法值、`screen.w/h` 缺失、嵌套深度 > 8。
- 每条非法用例断言返回明确错误（字段 + 原因），不是笼统失败。

## 2. Compiler round-trip 测试（对应 D2）

- **json → gsp → 解析回结构**：编译 demo scene，再解析 `.gsp`，断言 obj_count、每个 obj 的 type/x/y/w/h/flags/各索引、style/string/callback 表内容与源 JSON 一致。
- **先序 + parent_idx 正确性**：嵌套树编译后，校验 `parent_idx < 自身下标`、父在子前、根为 -1、还原出的层级与 JSON `children` 一致。
- **去重**：相同字符串/相同 style 只在表中出现一份。
- **确定性**：同一 JSON 多次编译产出**逐字节一致**的 `.gsp`（便于 diff 和缓存）。

## 3. Loader 正确性测试（对应 D4 / A8）

- **json 路径 与 package 路径产出同构 object tree**：分别 `gfx_scene_load_json` 与 `gfx_scene_load_package` 加载同一 demo，断言对象数、类型、坐标、颜色（经 `gfx_color_hex` 后）、层级一致 → 支撑 E3「host 只读包也渲染一致」。
- **find_obj**：按各 id 能取到对应对象；不存在 id 返回 NULL。
- **属性落地**：抽查 label text / color、button bg/pressed/radius、container border/clip 是否真的调用了对应 setter（可用测试后端或读回值）。

## 4. 生命周期 / reload 测试（对应 A10）

- **reload 无泄漏无叠加**：连续 `gfx_scene_reload` N 次，断言对象总数稳定（不随次数增长）、无内存增长（host 下配合 leak 检测 / 计数分配器）。
- **delete 干净**：`gfx_scene_delete` 后 display child_list 为空、无悬挂。
- **reload 失败保留旧场景**：喂一个坏 JSON，断言旧场景仍在、返回错误。

## 5. 坏包健壮性 / fuzz 测试（对应 D6）

坏包**绝不崩**，只返回错误。至少覆盖：

- magic / version 错；`file_size != size`；`header_size` 异常。
- 各表 `offset/count` 越界、未对齐；表相互重叠。
- `parent_idx` 越界 / 指向自身或后继（构造环）/ 非 -1 的根。
- 各 `*_idx` / `*_str` 越界；string blob 无 `\0` 结尾。
- 未知 `type` / 非法枚举值 / `flags` 高位置位。
- `obj_count = 0`、超大 count。
- **随机字节 fuzz**：对合法包做随机位翻转 + 完全随机 buffer，跑 loader，断言只返回错误码、无崩溃/越界（host 上配合 ASan）。

## 6. 回调 / click 语义测试（对应 A12 / D5）

- 绑定 `on_ok` 后，注入 PRESS 再在 widget 内 RELEASE → 触发一次；PRESS 后移出范围 RELEASE → 不触发。
- 未声明的回调名 `bind` 返回 `NOT_FOUND`；`cb=NULL` 解绑后不再触发。

## 7. 颜色 / 面板无关测试（对应 §7）

- 同一 scene 在 565 与 888 两种 display-port 配置下都能加载渲染（host 可配置两种 color_format 的 backend），验证 package 面板无关。
- `#RRGGBB` → `gfx_color_hex` → `gfx_color_t` 的换算抽查若干典型色。

## 8. 建议落地形式

- host 单测挂到现有 `ctest`（`simulation` 的 smoke 体系），命名如 `gfx_host_scene_schema_smoke`、`gfx_host_scene_roundtrip_smoke`、`gfx_host_scene_badpkg_smoke`。
- fuzz 用 host + ASan 简易 harness；CI 至少跑固定语料 + 小规模随机。
- E 系列（[07-todo.md](07-todo.md)）保持为人工/脚本化的端到端演示。
