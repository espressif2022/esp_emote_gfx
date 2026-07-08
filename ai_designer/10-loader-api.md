# Scene Loader C API 定稿（`gfx/scene.h`）

> 上一篇：[Package 二进制格式](09-package-format.md) ｜ 下一篇：[测试策略](11-testing.md) ｜ 返回：[README](README.md)

本页把 [02-architecture.md](02-architecture.md) §2.4 的草案 API 定稿：类型、错误码、生命周期与各函数语义。对应任务 A8 / A10 / D4 / D5。

## 1. 头文件位置与依赖

- 公共头：`include/gfx/scene.h`（若第一版想更快，可先放 `simulation/host` 或 `examples/ai_scene_demo` 内部，稳定后再提升为 public，见 [08 §3](08-risks-and-references.md)）。
- 依赖：`gfx/object.h`（`gfx_object_t`、`gfx_display_t`）、`gfx/error.h`（`gfx_err_t`）、`gfx/types.h`。
- `gfx_scene_load_json()` **仅 host 构建可用**（依赖 JSON parser），用 `#if defined(GFX_HOST_BUILD)` 或独立 .c 隔离；`gfx_scene_load_package()` host/device 共用。

## 2. 类型

```c
typedef struct gfx_scene gfx_scene_t;   // 不透明句柄

// 回调：click = 按下且在范围内抬起（loader 内部基于 gfx_object_set_touch_cb 合成）
typedef void (*gfx_scene_cb_t)(gfx_object_t *obj, void *user_data);
```

`gfx_scene_t` 内部（实现私有，`gfx_scene_priv.h`）至少含：

```c
struct gfx_scene {
    gfx_display_t *disp;
    gfx_object_t **objs;      // 按 object_table 顺序, 供 find/delete
    uint16_t       obj_count;
    const void    *pkg;       // package buffer 引用(§5.1 生命周期契约); JSON 路径下为 NULL
    /* id 表: id_str -> obj 下标;  callback 绑定表: name -> {obj, cb, user_data} */
};
```

## 3. 错误码

复用 `gfx/error.h`（`gfx_err_t`，`GFX_OK==0`）。loader 至少区分：

| 语义 | 建议码 |
|------|--------|
| 成功 | `GFX_OK` |
| 参数空/非法 | `GFX_ERR_INVALID_ARG` |
| 内存不足 | `GFX_ERR_NO_MEM` |
| 包/JSON 格式错误（magic/version/越界/坏枚举/坏 parent_idx/id 重复等） | `GFX_ERR_INVALID_STATE`（或新增 `GFX_ERR_SCENE_FORMAT`） |
| screen 分辨率与 display 不一致 | 同上，日志区分 |
| 未找到（find/bind 名字不存在） | `GFX_ERR_NOT_FOUND`（或返回 NULL） |

**原则**：任何加载期错误都要「释放已建对象 + 返回错误码」，绝不崩（D6）。

## 4. 函数语义

```c
gfx_err_t gfx_scene_load_json(gfx_display_t *disp, const char *path, gfx_scene_t **out_scene);
```
- **仅 host**。读 `path`（经 `gfx_fs_load`）→ 校验 schema（[03](03-scene-format.md)）→ 建 object tree。
- 语义等价于 compiler 内存态 + `load_package`，但直接从 JSON 建树，方便预览。
- 失败：`*out_scene` 置 NULL，返回错误码，无残留对象。

```c
gfx_err_t gfx_scene_load_package(gfx_display_t *disp, const void *data, size_t size, gfx_scene_t **out_scene);
```
- **host/device 共用**。按 [09 §10](09-package-format.md) 算法从 `.gsp` 建树。
- **生命周期契约**：`data` 必须在 scene 存活期保持有效（[09 §5.1](09-package-format.md)）。device=mmap flash 常驻；host 调用方持有。scene 内部保存 `pkg=data`。
- 不拷贝整包；label/button 文本由 widget 自行拷贝，id/callback 名引用 `data`。

```c
gfx_err_t gfx_scene_reload(gfx_scene_t *scene);
```
- 就地重载：销毁当前 object tree → 重新从来源（JSON 路径或原 package）加载 → 标脏。来源信息在 scene 内保存。
- 用于 Preview Bridge（[02 §2.5](02-architecture.md)）。失败时**保留旧场景**（先加载新的到临时，成功再切换；或加载失败回滚），避免预览变空白。

```c
gfx_object_t *gfx_scene_find_obj(gfx_scene_t *scene, const char *id);
```
- 按 widget `id` 查对象；找不到返回 NULL。用于工具/业务按 id 操作。

```c
gfx_err_t gfx_scene_bind_callback(gfx_scene_t *scene, const char *name, gfx_scene_cb_t cb, void *user_data);
```
- 把回调名（package/JSON 中 button 的 `callback`）绑定到实际函数。
- loader 内部为每个带 callback 的 button 设 `gfx_object_set_touch_cb`，在 PRESS→RELEASE（且抬起点在 widget 内）时触发 `cb`（click 语义，A12）。
- `name` 未在 scene 中声明 → 返回 `GFX_ERR_NOT_FOUND`；`cb==NULL` = 解绑。

```c
void gfx_scene_delete(gfx_scene_t *scene);
```
- 销毁整棵 object tree（遍历 `scene->objs`）、释放 id/callback 表与 `scene` 本体。
- 不释放 `pkg` 指向的 buffer（所有权在调用方 / flash）。
- 不释放 font registry（应用级单例，见 [09 §6.1](09-package-format.md)）。

## 5. 典型用法

**Host 预览（JSON 路径 + reload）**

```c
gfx_scene_t *scene = NULL;
gfx_scene_load_json(disp, "scene.json", &scene);
gfx_scene_bind_callback(scene, "on_ok", on_ok_cb, &app);
gfx_core_refresh_now(gfx);
// 主循环 loop_cb 里检测 mtime 变化 → gfx_scene_reload(scene)（见 02 §2.5）
...
gfx_scene_delete(scene);
```

**设备端（package）**

```c
const void *pkg; size_t sz;                 // 来自 mmap assets 分区 / .inc 数组
gfx_scene_t *scene = NULL;
gfx_scene_load_package(disp, pkg, sz, &scene);
gfx_scene_bind_callback(scene, "on_ok", on_ok_cb, &app);
// pkg 常驻 flash, 无需释放; 退出场景时:
gfx_scene_delete(scene);
```

## 6. 与任务对应

| 函数 | 任务 |
|------|------|
| `gfx_scene_load_json` | A8 |
| `gfx_scene_load_package` | D4 |
| `gfx_scene_reload` / `gfx_scene_delete` | A10 |
| `gfx_scene_bind_callback`（click 语义） | A12 / D5 |
| 全量校验 | D6（清单见 [09 §10](09-package-format.md)） |
