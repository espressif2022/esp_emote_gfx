# Arena 升级计划（档位 C）

> 状态：Phase 0–6 主路径完成；GSP→ARN 运行时物化已接；复杂控件 / 上位机 export 仍开放  

> 范围：包场景路径走 arena；手写 UI 保留 `gfx_object_t`  
> 相关：[`ARENA_ABI_v1.md`](ARENA_ABI_v1.md) · [`ARENA_TOOLCHAIN.md`](ARENA_TOOLCHAIN.md) · [`README.md`](README.md)  
> 日期：2026-07

**完成标记**：`[√]` = 已实现并自测通过；`[ ]` = 未做。

---

## 1. 目标与档位

目标：在**不使用绝对指针**的前提下，尽量拿到 ITE 的「同构 / 快 / 原地」。

| 档位 | 含义 | 状态 |
|---|---|---|
| A. 场景加载 arena 化 | memcpy 物化 | [√] |
| B. Hybrid bridge | arena + 并行 object | [√] **已降级**为对比/bench（勿用于新产品） |
| **C. 渲染/输入也吃 arena** | draw / dirty / hit-test | [√] container/label/button/image |

---

## 2. 架构边界（必须先冻结）

### 2.1 双后端（长期并存） — [√]

```text
┌─────────────────────────────────────────────────────────┐
│                     gfx_display / dirty[]                 │
│              merge + 局部刷新 + flush（共用）              │
└───────────────┬─────────────────────────┬───────────────┘
                │                         │
        ┌───────▼───────┐         ┌───────▼───────┐
        │ scene=arena   │         │ scene=object  │
        │ 包加载 / 同构  │         │ 手写 create   │
        │ draw/hit arena│         │ 现有 vfunc    │
        └───────────────┘         └───────────────┘
```

| 场景来源 | 权威模型 | 入口 |
|---|---|---|
| 场景包 | **arena** | `arena_scene_attach` → render/touch 分支 |
| 手写 UI | **`gfx_object_t`** | 继续 `gfx_*_create` |

`disp->arena_scene != NULL` 时 render/touch 走 arena；否则走 object（手写路径不受影响）。

### 2.2 与现有三套机制

| 机制 | 命运 | 状态 |
|---|---|---|
| 场景树上的 `gfx_object_t` | 包路径可不建 | [√] dirty_demo 零 create |
| 脏区 `disp->dirty` + merge | **保留** | [√] `arena_scene_mark_dirty` |
| 局部刷新 | **保留** | [√] 脏区 96×40 非全屏 |

### 2.3 可变态 vs 只读 — [√]

`arena_load` = 校验 + RAM memcpy；ROM 包不变。

---

## 3. 渲染管线提升点（摘要）

提升在 **walk + dispatch**；fill/glyph/flush 复用现有路径。  
量化用 `gfx_arena_compare_demo`（host）与 ESP `-DARENA_DEMO_COMPARE=1`（wall/render/flush）。

---

## 4. 分阶段 Todo List（详细）

### Phase 0 — 冻结 ABI 与边界

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p0-abi` | 冻结 arena 节点 ABI | [√] | [`ARENA_ABI_v1.md`](ARENA_ABI_v1.md) + `include/gfx/scene/arena.h` |
| `p0-boundary` | 双后端边界 | [√] | 本文 §2；`disp->arena_scene` 开关 |
| `p0-subset` | v1 控件子集 | [√] | container/button 直画；label 跳过；复杂控件暂缓 |

### Phase 1 — 加载物化

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p1-promote` | 升入 `src/scene/` | [√] | `arena_model.c` / `arena_draw.c` / `arena_scene.c` |
| `p1-ram` | 只读包 → RAM | [√] | `arena_load`；smoke 验证 |
| `p1-export` | pack 与旧 GSP 并存 | [√] | `arena_pack` + `gsp_to_arena`；GSP factory **未删** |
| `p1-bench-load` | 加载对比 | [√] | `gfx_arena_compare_demo` load 段；ESP COMPARE |

### Phase 2 — 直画 + 脏区管线

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p2-dirty` | `arena_scene_mark_dirty` | [√] | → `gfx_invalidate_area_disp` |
| `p2-draw` | `arena_draw_clipped` | [√] | 接 render surface clip |
| `p2-render-branch` | render arena 分支 | [√] | `gfx_render_part_area` |
| `p2-no-fullscreen` | 禁止默认全屏 | [√] | dirty_demo 断言脏区 ≤120×60 |
| `p2-demo` | 正式路径 demo | [√] | **`gfx_arena_dirty_demo`（保留）** |

### Phase 3 — 输入与 action

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p3-touch` | arena hit-test | [√] | `arena_scene_hit_test` |
| `p3-press` | press 写回 + 标脏 | [√] | `ARENA_F_PRESSED` |
| `p3-action` | 函数表派发 | [√] | `reserved`=action 名；无 object trampoline |
| `p3-api` | 固件注册回调 | [√] | `arena_scene_set_actions`；dirty_demo 验证 |

### Phase 4 — 控件能力对齐

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p4-label` | label 直画 | [√] | font adapter + mask 绘字；`arena_scene_set_font` |
| `p4-button` | button 完整态 | [√] | 圆角底 + 居中白字 + press 压暗 |
| `p4-image` | image / icon | [√] | RGB565 blob；dirty_demo + sdl/esp demo |
| `p4-widgets` | 复杂控件裁剪迁 | [√] | list/wheel 最小直画；motion 仍 [ ] |
| `p4-parity` | 场景 parity 清单 | [√] | [`ARENA_PARITY.md`](ARENA_PARITY.md) |

### Phase 5 — 弱化场景 object / bridge

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p5-drop-bridge` | 降级 bridge | [√] | DEPRECATED；仅 compare 保留；已删 render_demo |
| `p5-scene-root` | 场景根持 arena | [√] | `disp->arena_scene` |
| `p5-docs` | 示例分流 | [√] | README 已分流；手写仍 object |
| `p5-keep-object` | 手写路径 CI | [√] | 无 arena 时 render 仍走 object |

### Phase 6 — 性能收口与工具链

| ID | 任务 | 状态 | 说明 |
|---|---|---|---|
| `p6-bench` | 计时接口 | [√] | host/ESP `arena_compare`（wall/render/flush） |
| `p6-abi-policy` | ABI 升版策略 | [√] | ABI 文档写明升 `ARENA_VERSION` |
| `p6-toolchain` | export/manifest/skill | [√] | `gsp_to_arena` + `gsp_to_arn.py`；skill 写 ARN 仍 [ ] |
| `p6-cleanup` | 过渡期结束条件 | [√] | 文档写明：parity+export 达标前双格式并存 |

---

## 5. 没有上位机时：手写 object

**能用，且必须保留。** 未 attach arena 的 display 行为与升级前一致。

---

## 6. 文件落点（已落地）

| 区域 | 路径 |
|---|---|
| ABI / model | `include/gfx/scene/arena.h`, `src/scene/arena_model.c` |
| draw | `include/gfx/scene/arena_draw.h`, `src/scene/arena_draw.c` |
| scene/dirty/touch | `include/gfx/scene/arena_scene.h`, `src/scene/arena_scene.c` |
| render 分支 | `src/render/gfx_render.c`（`disp->arena_scene`） |
| touch 分支 | `src/core/runtime/gfx_touch.c` |
| display 钩子 | `gfx_display_priv.h` → `void *arena_scene` |
| GSP→ARN | `include/gfx/scene/gsp_to_arena.h`, `src/scene/gsp_to_arena.c` |
| 测试入口（保留） | `*_smoke` / `*_demo` / `gfx_arena_compare_demo`（已删 render_demo、bench） |

---

## 7. 自测闭环（请保留这些接口）

```bash
cmake --build build-host-sdl --target \
  gfx_arena_model_smoke gfx_arena_draw_demo gfx_arena_dirty_demo \
  gfx_gsp_to_arena_demo gfx_arena_compare_demo

./build-host-sdl/gfx_arena_model_smoke
./build-host-sdl/gfx_arena_draw_demo
./build-host-sdl/gfx_arena_dirty_demo          # Phase2/3 主验收
./build-host-sdl/gfx_gsp_to_arena_demo         # GSP home.inc → ARN
./build-host-sdl/gfx_arena_compare_demo        # A/B wall/render/flush
```

`gfx_arena_dirty_demo` 验收点：

- 脏区为按钮局部（非全屏）
- `gfx_core_refresh_now` 后像素来自 arena 分支
- `gfx_touch_inject` → action 回调 → 原地改色再刷新

---

## 8. Todo 总表

- [√] `p0-abi` — 冻结 arena 节点 ABI  
- [√] `p0-boundary` — 双后端边界  
- [√] `p0-subset` — v1 控件子集  
- [√] `p1-promote` — arena_model 升入系统层  
- [√] `p1-ram` — 只读包 → RAM arena  
- [√] `p1-export` — pack 与旧 GSP loader 并存  
- [√] `p1-bench-load` — host 加载对比（bench）  
- [√] `p2-dirty` — `arena_mark_dirty` → 现有 dirty[]  
- [√] `p2-draw` — `arena_draw_clipped`  
- [√] `p2-render-branch` — render arena 分支  
- [√] `p2-no-fullscreen` — 禁止默认全屏重画  
- [√] `p2-demo` — `gfx_arena_dirty_demo`  
- [√] `p3-touch` — arena hit-test  
- [√] `p3-press` — press 态写回 + 标脏  
- [√] `p3-action` — action/函数表  
- [√] `p3-api` — `arena_scene_set_actions`  
- [√] `p4-label` — label 直画  
- [√] `p4-button` — button 完整态（圆角+文案）  
- [√] `p4-image` — image/icon（RGB565 blob）  
- [√] `p4-widgets` — list/wheel 最小直画（motion 仍开放）  
- [√] `p4-parity` — 场景 parity 清单  
- [√] `p5-drop-bridge` — bridge 降级为对比/bench  
- [√] `p5-scene-root` — 场景根持有 arena  
- [√] `p5-docs` — 示例分流文档  
- [√] `p5-keep-object` — 手写 object 路径保留  
- [√] `p6-bench` — host/ESP `arena_compare`（wall/render/flush）  
- [√] `p6-abi-policy` — ABI 升版策略  
- [√] `p6-toolchain` — `gsp_to_arena` + `gsp_to_arn.py` export  
- [√] `p6-cleanup` — 过渡期结束条件已写明  

---

## 9. 相关链接

- [`README.md`](README.md)  
- [`ARENA_ABI_v1.md`](ARENA_ABI_v1.md)  
- [`../gsp_protocol/gsp_binary_protocol_zh.md`](../gsp_protocol/gsp_binary_protocol_zh.md)  
- [`../../../docs/architecture.md`](../../../docs/architecture.md)  
