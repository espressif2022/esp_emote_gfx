# `gfx_fs` Multi-Mount 架构设计

> 文档状态：**草案 v0.1**  
> 作者：Espressif  
> 适用仓库：`esp_emote_gfx`

---

## 1. 背景与问题

### 1.1 现有资源访问模型

`gfx_fs` 目前有两个独立的文件访问路径，在 `gfx_fs_fopen()` 中顺序执行：

```
gfx_fs_fopen(name)
  │
  ├─ 1. s_default_fs  ← 全局唯一，通过 gfx_fs_set_default() 设置
  │      └─ open_by_name(name)   (pack / partition / dir 后端)
  │
  └─ 2. stdio fallback
         └─ fopen(name, "rb")    (SPIFFS VFS / FATFS / host 路径)
```

**优点：** 简单，零配置情况下 stdio 兜底自动工作。

**缺点：** 只有一个 `default_fs`，不能同时挂载多个资源源；平台差异被推给应用层，导致两类问题：

### 1.2 问题一：只有一个 `default_fs`

如果 demo 既要从 **flash 分区**（pack 格式）加载 mmap 资源，又要从 **SPIFFS 分区**（松散文件）加载图片，当前架构只能二选一作为 `default_fs`，另一个要绕行通过 stdio fallback。

### 1.3 问题二：路径前缀平台不一致

以 `format_playground` demo 为例：

```c
// widget_image.c
#define DEMO_SPIFFS_IMAGE_MOUNT  "/spiffs"        // ESP 上的 VFS 挂载点
#define DEMO_FILE_IMAGE_MOUNT    "examples/esp/format_rgb565/spiffs_anim"  // Host 重映射

static const demo_image_clip_t s_image_clips[] = {
    { .path = DEMO_FILE_IMAGE_MOUNT "/flow_quiet_trail.jpg" },  // 路径拼接靠宏
};
```

在 ESP 上：
1. `demo_mount_spiffs_assets()` 调用 `esp_vfs_spiffs_register()`，把 SPIFFS 挂到 `/spiffs`
2. `fopen("/spiffs/flow_quiet_trail.jpg")` 通过 ESP-IDF VFS 成功

在 Host 上：
1. `demo_mount_spiffs_assets()` 是空函数（`#ifndef GFX_HOST_BUILD`）
2. `DEMO_FILE_IMAGE_MOUNT` 宏改为相对路径，路径拼接行为与 ESP 不同
3. 如果运行目录不对，文件直接找不到

**根本矛盾：** 路径语义（`/spiffs/…`）只对 ESP VFS 有效，Host 需要另一套路径，两套逻辑由应用层通过条件编译区分，维护成本高、易出错。

---

## 2. 设计目标

| 目标 | 说明 |
|------|------|
| **统一路径** | 应用层写一个路径字符串，在 ESP 和 Host 均能解析 |
| **多源共存** | 同时挂载 pack 分区（mmap 资产）+ SPIFFS/目录（松散文件）|
| **最长前缀匹配** | `/spiffs/a.jpg` 优先匹配 `/spiffs` 而非 `""` |
| **向后兼容** | `gfx_fs_set_default()` 继续工作，等价于挂载前缀 `""` |
| **无动态内存分配开销** | 挂载表用固定数组，上限可编译期配置 |
| **跨平台透明** | stdio fallback 保留作最终兜底，不破坏现有 ESP VFS 集成 |

---

## 3. 方案设计

### 3.1 核心概念：挂载表（Mount Table）

将 `s_default_fs` 从单个指针改为一个带前缀的挂载项数组：

```
挂载表（全局静态，最多 GFX_FS_MOUNT_MAX 条）
┌────────────────────────────────────────────────┐
│  slot 0 │ prefix: ""        │ fs: pack_partition │  ← 旧 default_fs 等价
│  slot 1 │ prefix: "/spiffs" │ fs: spiffs_dir_fs  │  ← 新增
│  slot 2 │ (空)              │                    │
│  ...                                            │
└────────────────────────────────────────────────┘
```

`gfx_fs_fopen(name)` 改为：

```
1. 遍历挂载表，找所有 prefix 是 name 前缀的条目
2. 取最长前缀匹配的条目 → 用对应 fs 解析 (name + prefix_len) 部分
3. 若无匹配 → 保留现有 stdio fallback
```

### 3.2 前缀匹配规则

| 请求路径 | 已挂载前缀 | 匹配？ | 传给后端的 name |
|----------|-----------|--------|-----------------|
| `"flow_misty.jpg"` | `""` | ✓ | `"flow_misty.jpg"` |
| `"/spiffs/trail.jpg"` | `""` | ✓ (弱) | `"/spiffs/trail.jpg"` |
| `"/spiffs/trail.jpg"` | `"/spiffs"` | ✓ (强) | `"trail.jpg"` |
| `"/sdcard/vid.bin"` | `"/spiffs"` | ✗ | — |

规则细节：
- 前缀 `""` 匹配所有路径（优先级最低）
- 前缀 `"/spiffs"` 要求 `name` 以 `"/spiffs/"` 或恰好等于 `"/spiffs"` 开头
- 多个前缀都匹配时，取**最长前缀**

### 3.3 内部结构变更（`gfx_fs_priv.h`）

```c
/* 新增：挂载项 */
typedef struct {
    char     prefix[GFX_FS_MOUNT_PREFIX_MAX]; /**< 路径前缀，如 "" "/spiffs" */
    gfx_fs_t *fs;                              /**< NULL 表示空槽 */
} gfx_fs_mount_t;

#ifndef GFX_FS_MOUNT_MAX
#define GFX_FS_MOUNT_MAX 4
#endif
#ifndef GFX_FS_MOUNT_PREFIX_MAX
#define GFX_FS_MOUNT_PREFIX_MAX 32
#endif

/* 替换原来的 static gfx_fs_t *s_default_fs */
static gfx_fs_mount_t s_mount_table[GFX_FS_MOUNT_MAX];
```

### 3.4 公共 API 变更（`gfx_fs.h`）

#### 新增：`gfx_fs_mount` / `gfx_fs_unmount`

```c
/**
 * @brief 以路径前缀挂载一个 fs 实例。
 *
 * 后续 gfx_fs_fopen(name) 若 name 以 prefix 开头，将优先通过此 fs 解析。
 * 前缀 "" 等价于原 gfx_fs_set_default()。
 *
 * @param prefix  路径前缀，如 "" 或 "/spiffs"。不含尾部 '/'。
 * @param fs      已打开的 fs 实例，由调用方持有生命周期。
 * @return GFX_OK，或 GFX_ERR_NO_MEM（挂载表已满）。
 */
gfx_err_t gfx_fs_mount(const char *prefix, gfx_fs_t *fs);

/**
 * @brief 卸载指定前缀的 fs。
 *
 * 不关闭 fs 本身，由调用方决定是否 gfx_fs_close()。
 *
 * @param prefix  与 gfx_fs_mount() 时相同的前缀字符串。
 * @return GFX_OK，或 GFX_ERR_NOT_FOUND。
 */
gfx_err_t gfx_fs_unmount(const char *prefix);
```

#### 保留（向后兼容）：`gfx_fs_set_default` / `gfx_fs_get_default`

`gfx_fs_set_default(fs)` 内部改为 `gfx_fs_mount("", fs)`，行为不变。

`gfx_fs_get_default()` 返回挂载在 `""` 的 fs，如果没有则返回 NULL。

---

## 4. 内部实现

### 4.1 `gfx_fs_fopen` 新逻辑（`gfx_fs_file.c`）

```c
gfx_fs_file_t *gfx_fs_fopen(const char *name)
{
    gfx_fs_file_t *file;
    const gfx_fs_mount_t *best = NULL;
    size_t best_len = 0;

    if (name == NULL) {
        return NULL;
    }

    /* 最长前缀匹配 */
    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        const gfx_fs_mount_t *m = &s_mount_table[i];
        if (m->fs == NULL) {
            continue;
        }
        size_t plen = strlen(m->prefix);
        if (plen < best_len) {
            continue;  /* 比当前最优更短，跳过 */
        }
        if (plen == 0) {
            /* 空前缀匹配所有 */
            if (best == NULL) {
                best = m;
                best_len = 0;
            }
        } else if (strncmp(name, m->prefix, plen) == 0 &&
                   (name[plen] == '/' || name[plen] == '\0')) {
            best = m;
            best_len = plen;
        }
    }

    if (best != NULL) {
        /* 去掉前缀后传给后端 */
        const char *subname = name + best_len;
        if (*subname == '/') {
            subname++;  /* 跳过分隔符 */
        }
        if (*subname == '\0') {
            subname = name;  /* 前缀恰好等于路径，回退到完整名 */
        }
        file = gfx_fs_file_open_fs(best->fs, subname);
        if (file != NULL) {
            return file;
        }
        /* 前缀匹配但后端找不到文件：继续 stdio fallback */
    }

    /* stdio fallback：SPIFFS VFS / FATFS / host fopen */
    return gfx_fs_file_open_stdio(name);
}
```

### 4.2 `gfx_fs_mount` / `gfx_fs_unmount` 实现（`gfx_fs.c`）

```c
gfx_err_t gfx_fs_mount(const char *prefix, gfx_fs_t *fs)
{
    if (prefix == NULL || fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    /* 覆盖已有相同前缀 */
    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs != NULL &&
                strcmp(s_mount_table[i].prefix, prefix) == 0) {
            s_mount_table[i].fs = fs;
            return GFX_OK;
        }
    }
    /* 找空槽 */
    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs == NULL) {
            snprintf(s_mount_table[i].prefix,
                     GFX_FS_MOUNT_PREFIX_MAX, "%s", prefix);
            s_mount_table[i].fs = fs;
            return GFX_OK;
        }
    }
    return GFX_ERR_NO_MEM;
}

gfx_err_t gfx_fs_unmount(const char *prefix)
{
    if (prefix == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs != NULL &&
                strcmp(s_mount_table[i].prefix, prefix) == 0) {
            s_mount_table[i].fs = NULL;
            s_mount_table[i].prefix[0] = '\0';
            return GFX_OK;
        }
    }
    return GFX_ERR_NOT_FOUND;
}

/* 向后兼容 */
void gfx_fs_set_default(gfx_fs_t *fs)
{
    if (fs == NULL) {
        (void)gfx_fs_unmount("");
    } else {
        (void)gfx_fs_mount("", fs);
    }
}

gfx_fs_t *gfx_fs_get_default(void)
{
    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs != NULL &&
                s_mount_table[i].prefix[0] == '\0') {
            return s_mount_table[i].fs;
        }
    }
    return NULL;
}
```

---

## 5. 应用层迁移

### 5.1 ESP 设备端（以 `format_rgb565` 为例）

**Before（现在）：**

```c
// main.c
static void init_optional_asset_fs(void)
{
    gfx_fs_open(&cfg, &s_assets_fs);
    gfx_format_demo_set_asset_fs(s_assets_fs);  // 内部调 gfx_fs_set_default()
}

// widget_image.c
static void demo_mount_spiffs_assets(void)
{
#ifndef GFX_HOST_BUILD
    esp_vfs_spiffs_register(&spiffs_conf);  // 挂到 /spiffs，依赖 stdio fallback
#endif
}

// 路径写法：硬编码 /spiffs/... 或 #define 宏
{ .path = DEMO_FILE_IMAGE_MOUNT "/flow_quiet_trail.jpg" }
```

**After（新方案）：**

```c
// main.c
static void init_asset_fs(void)
{
    /* 主资产分区，前缀 "" → gfx_fs_fopen("flow_xxx.jpg") 直接命中 */
    gfx_fs_open(&pack_cfg, &s_pack_fs);
    gfx_fs_mount("", s_pack_fs);

#ifndef GFX_HOST_BUILD
    /* SPIFFS 分区：挂载到 /spiffs 前缀 */
    gfx_fs_open(&spiffs_pack_cfg, &s_spiffs_fs);
    gfx_fs_mount("/spiffs", s_spiffs_fs);
    /* 不再需要 esp_vfs_spiffs_register() */
#endif
}

// widget_image.c — 移除 demo_mount_spiffs_assets()，移除条件宏
static const demo_image_clip_t s_image_clips[] = {
    { .path = "flow_format_probe.jpg" },       // 命中 "" → pack
    { .path = "/spiffs/flow_quiet_trail.jpg" }, // 命中 "/spiffs" → spiffs_fs
};
// 路径字符串在 ESP 和 Host 完全一致，无需 #ifdef
```

### 5.2 Host 端

```c
// Host demo 初始化
static void init_asset_fs_host(void)
{
    gfx_fs_t *pack_fs, *spiffs_dir_fs;

    /* 主资产：pack 文件 */
    gfx_fs_open(&(gfx_fs_open_config_t){
        .source_type = GFX_FS_SOURCE_PACK_FILE,
        .path_or_label = "assets/esp32_s3_assets.bin",
    }, &pack_fs);
    gfx_fs_mount("", pack_fs);

    /* SPIFFS 等价目录：挂到相同前缀 /spiffs */
    gfx_fs_open_dir("examples/esp/format_rgb565/spiffs_anim", &spiffs_dir_fs);
    gfx_fs_mount("/spiffs", spiffs_dir_fs);
}
```

应用层路径字符串与 ESP 端**完全相同**，无需任何 `#ifdef GFX_HOST_BUILD` 宏。

---

## 6. 文件变更清单

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `include/gfx/fs.h` | 修改 | 新增 `gfx_fs_mount()` / `gfx_fs_unmount()` 声明 |
| `src/core/fs/gfx_fs_priv.h` | 修改 | 新增 `gfx_fs_mount_t`、挂载表宏，移除 `s_default_fs` 声明 |
| `src/core/fs/gfx_fs.c` | 修改 | 实现 `gfx_fs_mount/unmount`，重写 `set_default/get_default` |
| `src/core/fs/gfx_fs_file.c` | 修改 | 重写 `gfx_fs_fopen` 最长前缀匹配逻辑 |
| `examples/**/widget_image.c` | 修改 | 删除 `demo_mount_spiffs_assets()`，路径统一，去掉 `#ifdef` |
| `examples/**/widget_anim.c` | 修改 | 同上 |
| `examples/**/main.c` | 修改 | 用 `gfx_fs_mount()` 替换 `gfx_format_demo_set_asset_fs()` + SPIFFS 挂载 |

---

## 7. 不在本次范围内

| 事项 | 原因 |
|------|------|
| 线程安全 / mutex | 当前 `gfx_fs` 整体无锁，挂载表通常在启动阶段一次性配置 |
| 动态前缀（运行时频繁挂载/卸载）| 当前 demo 场景不需要 |
| 目录遍历 / `opendir` | 与文件访问无关，另议 |
| `GFX_FS_SOURCE_PARTITION` 在 Host 的支持 | 平台限制，不变 |

---

## 8. 现有 API 兼容性总结

| 函数 | 行为变化 |
|------|----------|
| `gfx_fs_set_default(fs)` | 内部改调 `gfx_fs_mount("", fs)`，**外部行为不变** |
| `gfx_fs_get_default()` | 返回挂载在 `""` 的 fs，**外部行为不变** |
| `gfx_fs_fopen(name)` | 增加前缀路由步骤，无匹配时仍走 stdio fallback，**兼容现有调用** |
| `gfx_fs_fopen_from(fs, name)` | **不变** |
| `gfx_fs_open / close / load / unload` | **不变** |

---

## 9. 变更后调用流程图

```
应用层                       gfx_fs 核心                    后端
────────                     ──────────────────              ──────
gfx_fs_mount("", pack_fs)
gfx_fs_mount("/spiffs", dir_fs)

gfx_fs_fopen("/spiffs/trail.jpg")
  │
  ├─ 遍历挂载表
  │    "" → 不匹配(比 "/spiffs" 短，且 /spiffs 匹配)
  │    "/spiffs" → 匹配，subname = "trail.jpg"
  │
  ├─ gfx_fs_file_open_fs(dir_fs, "trail.jpg")
  │    └─ dir_fs.open_by_name(fs, "trail.jpg")
  │         └─ open("spiffs_anim/trail.jpg")  ──────────────► 文件系统 / mmap
  │
  └─ 返回 gfx_fs_file_t*

gfx_fs_fopen("flow_misty.jpg")
  │
  ├─ 遍历挂载表
  │    "" → 匹配，subname = "flow_misty.jpg"
  │    "/spiffs" → 不匹配
  │
  ├─ gfx_fs_file_open_fs(pack_fs, "flow_misty.jpg")
  │    └─ pack_fs.open_by_name(fs, "flow_misty.jpg")
  │         └─ 在 pack 索引表中查找  ─────────────────────► mmap 分区 / pack 文件
  │
  └─ 返回 gfx_fs_file_t*
```

---

*End of document*
