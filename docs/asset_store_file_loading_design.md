# Asset Store File Loading Design

本文只定义 GFX 资源层的文件打开、读取和加载合同，不定义资源内容格式，也不解析
`index.json`、manifest、动画条目或图片/字体/动画文件内容。

## Top-Level TODO

- [ ] 定义统一 `gfx_asset_store_open()` 配置入口，同时保留现有 `gfx_asset_store_open_dir()` 和 `gfx_asset_store_open_mmap()` 兼容 wrapper。
- [ ] 将 `gfx_asset_view_t` 的生命周期和访问方式改为 flags 表达，区分 direct mapped address、owned copy、persistent view。
- [ ] ESP-IDF 保留 `esp_mmap_assets` backend 的零拷贝语义，不把 mmap-assets 降级为普通文件读取。
- [ ] ESP-IDF 增加 VFS file/dir backend，使用 `fopen`/`fread` 加载 FATFS、SPIFFS、SD card 文件到 owned buffer。
- [ ] ESP-IDF 增加 raw partition backend，按配置选择 `esp_partition_mmap` 直接映射或 `esp_partition_read` 拷贝读取。
- [ ] Linux directory backend 保持 `mmap` 优先、read fallback 的行为，并补齐与新 flags/caps 的语义。
- [ ] 增加 store capability 查询 API，让调用方判断是否支持 direct address、owned copy、open by name、open by id。
- [ ] 将示例和播放器代码从直接依赖 `esp_mmap_assets` 迁移到 `gfx_asset_store_t`，但不在资源层解析业务内容。
- [ ] 增加 host 和 ESP-IDF smoke test，验证 open/read/load/view close 生命周期，不验证资源内容语义。

## Goals

GFX 资源层的职责是把一个只读资源加载成 `gfx_asset_view_t`：

- `data` 指向连续只读字节；
- `size` 表示字节长度；
- `name` 和 `id` 仅作为资源定位元数据；
- `gfx_asset_view_close()` 释放该 view 拥有的资源；
- decoder/widget 只消费字节，不关心底层来自 mmap、文件系统还是 partition。

本层不做以下事情：

- 不解析 `index.json`、manifest 或资源包目录；
- 不理解 `.eaf`、`.aaf`、`.jpg`、`.ttf` 等文件格式；
- 不负责 decode cache；
- 不负责业务名称到文件名的映射策略。

## Current State

当前实现已经有 `gfx_asset_store_t`、vtable 和 `gfx_asset_view_t`：

```text
gfx_asset_store_t
  -> open_by_name()
  -> open_by_id()
  -> view_close()
  -> store_close()
```

但 backend 能力按平台固定：

- ESP-IDF 只支持 `esp_mmap_assets`；
- Linux 只支持 directory file；
- ESP-IDF `gfx_asset_store_open_dir()` 返回 `GFX_ERR_NOT_SUPPORTED`；
- Linux `gfx_asset_store_open_mmap()` 返回 `GFX_ERR_NOT_SUPPORTED`。

这导致应用层仍然要知道资源来自哪个平台 backend。更严重的是，ESP-IDF 侧的业务代码容易直接持有
`mmap_assets_handle_t`，绕过 GFX 资源抽象。

## Design Principles

### mmap is a direct-address backend

`esp_mmap_assets` 的独特价值是提供 flash-backed direct address：

- `mmap_assets_get_mem()` 返回可直接读取的映射地址；
- 打开 view 不需要拷贝资源内容；
- view close 只释放 view state，不释放 `data`；
- store close 才释放底层 mmap-assets handle。

因此 mmap-assets backend 不能被抽象成普通 `fopen` 语义，也不能默认复制数据。

### filesystem is a copy or host-mmap backend

普通文件系统 backend 的语义不同：

- ESP-IDF VFS 使用 `fopen`/`fread`，资源内容进入 owned buffer；
- Linux host 可以优先用 POSIX `mmap`，失败后 fallback 到 read buffer；
- `view_close()` 需要根据 view flags 执行 `munmap` 或 `free`。

### partition is not mmap-assets

raw partition backend 没有 mmap-assets 的文件表。它只知道 partition label、offset、size 和读取方式：

- `esp_partition_mmap`：直接映射指定区域；
- `esp_partition_read`：读取指定区域到 owned buffer。

资源层只提供按 offset/size 打开一段 bytes 的能力。offset/size 如何得出，不属于本文范围。

## Public API Shape

保留现有 API：

```c
gfx_err_t gfx_asset_store_open_dir(const char *root_dir,
                                   gfx_asset_store_t **out_store);

gfx_err_t gfx_asset_store_open_mmap(const gfx_asset_mmap_config_t *config,
                                    gfx_asset_store_t **out_store);
```

新增统一入口：

```c
typedef enum {
    GFX_ASSET_STORE_TYPE_AUTO = 0,
    GFX_ASSET_STORE_TYPE_MMAP_ASSETS,
    GFX_ASSET_STORE_TYPE_DIR,
    GFX_ASSET_STORE_TYPE_PARTITION,
    GFX_ASSET_STORE_TYPE_MEMORY_TABLE,
} gfx_asset_store_type_t;

typedef enum {
    GFX_ASSET_LOAD_PREFER_DIRECT = 0,
    GFX_ASSET_LOAD_FORCE_COPY,
    GFX_ASSET_LOAD_FORCE_DIRECT,
} gfx_asset_load_mode_t;

typedef struct {
    gfx_asset_store_type_t type;
    gfx_asset_load_mode_t load_mode;

    const char *root_dir;
    const char *partition_label;

    int32_t max_files;
    uint32_t checksum;
    bool full_check;

    uint32_t alloc_caps;
} gfx_asset_store_config_t;

gfx_err_t gfx_asset_store_open(const gfx_asset_store_config_t *config,
                               gfx_asset_store_t **out_store);
```

`AUTO` 的 V1 规则保持保守：

- `partition_label != NULL` 时，ESP-IDF 优先尝试 mmap-assets；
- `root_dir != NULL` 时尝试 dir/VFS；
- 失败时返回明确错误，不做隐藏的多级业务 fallback。

## View Contract

建议用 flags 替换单个 `is_mapped` 布尔值：

```c
typedef enum {
    GFX_ASSET_VIEW_FLAG_NONE        = 0,
    GFX_ASSET_VIEW_FLAG_DIRECT_ADDR = 1U << 0,
    GFX_ASSET_VIEW_FLAG_OWNED       = 1U << 1,
    GFX_ASSET_VIEW_FLAG_MAPPED      = 1U << 2,
    GFX_ASSET_VIEW_FLAG_PERSISTENT  = 1U << 3,
} gfx_asset_view_flags_t;

typedef struct {
    const void *data;
    size_t size;
    const char *name;
    int32_t id;
    uint32_t flags;
    void *priv;
} gfx_asset_view_t;
```

Flags 语义：

- `DIRECT_ADDR`：`data` 是底层可直接读取地址，不是新分配的 copy；
- `OWNED`：`data` 由 view 拥有，`view_close` 必须释放；
- `MAPPED`：`data` 来自 mmap 类映射，`view_close` 可能需要 unmap；
- `PERSISTENT`：store 存活期间 `data` 稳定，view close 不释放底层 bytes。

兼容期可以保留 `is_mapped` 字段，由 flags 派生赋值。

## Backend Contracts

### ESP-IDF mmap-assets backend

Store type：`GFX_ASSET_STORE_TYPE_MMAP_ASSETS`

Open:

- 调用 `mmap_assets_new()`；
- 记录 `mmap_assets_handle_t`；
- `store_close()` 调用 `mmap_assets_del()`。

Load:

- `open_by_id()` 调用 `mmap_assets_get_mem()` / `mmap_assets_get_size()` / `mmap_assets_get_name()`；
- `open_by_name()` 遍历 `mmap_assets_get_name()`，找到后复用 `open_by_id()`；
- 不复制文件内容。

View flags:

```text
DIRECT_ADDR | MAPPED | PERSISTENT
```

### ESP-IDF VFS dir backend

Store type：`GFX_ASSET_STORE_TYPE_DIR`

Open:

- 保存 VFS root directory；
- root 可以是 FATFS、SPIFFS、SD card 等已挂载路径。

Load:

- `open_by_name()` 检查相对路径安全性；
- 拼接 root + name；
- 使用 `fopen`/`fseek`/`ftell`/`fread`；
- buffer 用 `heap_caps_malloc(size, alloc_caps)`，未配置时用普通 `malloc`；
- `open_by_id()` V1 返回 `GFX_ERR_NOT_SUPPORTED`，不做 index 解析。

View flags:

```text
OWNED
```

### Linux dir backend

Store type：`GFX_ASSET_STORE_TYPE_DIR`

Open:

- 保存 root directory；
- 校验 root 是目录。

Load:

- `open_by_name()` 检查相对路径安全性；
- `open` + `fstat`；
- 优先 `mmap(PROT_READ, MAP_PRIVATE)`；
- mmap 失败 fallback 到 `read` buffer；
- `open_by_id()` V1 返回 `GFX_ERR_NOT_SUPPORTED`。

View flags:

- mmap 成功：`DIRECT_ADDR | MAPPED`
- read fallback：`OWNED`

### ESP-IDF raw partition backend

Store type：`GFX_ASSET_STORE_TYPE_PARTITION`

V1 需要一个单资源或显式 offset/size 打开接口，避免引入内容索引：

```c
typedef struct {
    uint32_t offset;
    size_t size;
    const char *name;
    int32_t id;
} gfx_asset_region_t;

gfx_err_t gfx_asset_open_region(gfx_asset_store_t *store,
                                const gfx_asset_region_t *region,
                                gfx_asset_view_t *out_view);
```

Load:

- `FORCE_DIRECT` / `PREFER_DIRECT`：尝试 `esp_partition_mmap`；
- `FORCE_COPY`：使用 `esp_partition_read` 到 owned buffer；
- 如果 direct mmap 不可用且 load mode 是 `PREFER_DIRECT`，fallback 到 copy；
- 如果 load mode 是 `FORCE_DIRECT`，直接返回不支持或失败。

View flags:

- `esp_partition_mmap` 成功：`DIRECT_ADDR | MAPPED`
- `esp_partition_read` 成功：`OWNED`

## Capability Query

业务层不应根据 backend enum 猜能力。新增：

```c
typedef struct {
    bool open_by_name;
    bool open_by_id;
    bool open_region;
    bool can_direct_addr;
    bool can_owned_copy;
    bool can_force_copy;
    bool can_force_direct;
} gfx_asset_store_caps_t;

gfx_err_t gfx_asset_store_get_caps(const gfx_asset_store_t *store,
                                   gfx_asset_store_caps_t *out_caps);
```

示例：

- mmap-assets：`open_by_name=true`、`open_by_id=true`、`can_direct_addr=true`；
- ESP-IDF VFS dir：`open_by_name=true`、`can_owned_copy=true`；
- Linux dir：`open_by_name=true`、`can_direct_addr=true`、`can_owned_copy=true`；
- raw partition：`open_region=true`，direct/copy 取决于配置和平台支持。

## Error Semantics

建议保持已有 `gfx_err_t`，并固定常用映射：

- path/name 参数非法：`GFX_ERR_INVALID_ARG`
- 文件不存在或 asset id 不存在：`GFX_ERR_NOT_FOUND`
- size 为 0 或无法读取完整内容：`GFX_ERR_INVALID_SIZE`
- backend 不支持该打开方式：`GFX_ERR_NOT_SUPPORTED`
- 分配失败：`GFX_ERR_NO_MEM`
- 底层 IO 失败：`GFX_FAIL`

`open_*()` 失败时必须保证 `out_view` 被清零，`id = -1`。

## Ownership Rules

- store 拥有 backend handle 和长期配置；
- view 拥有一次打开的临时状态；
- `view.data` 的释放策略只由 view flags 和 backend view state 决定；
- `gfx_asset_store_close()` 不应隐式关闭仍在使用的 view，调用方必须先 close view；
- decoder/widget 不拥有 asset view，除非其 API 明确复制数据。

## Implementation Plan

1. Public API
   - 新增 `gfx_asset_store_config_t`、`gfx_asset_store_open()`；
   - 新增 `gfx_asset_view_flags_t`；
   - 新增 `gfx_asset_store_caps_t` 和 `gfx_asset_store_get_caps()`；
   - 保留旧 API wrapper。

2. Internal vtable
   - vtable 增加可选 `open_region()` 和 `get_caps()`；
   - store 增加 `load_mode` 或 backend 私有配置；
   - view close 逻辑继续由 backend 负责。

3. ESP-IDF backend split
   - 将现有 `gfx_asset_esp_idf.c` 拆成 mmap-assets backend；
   - 新增 ESP-IDF VFS dir backend；
   - 新增 raw partition backend，先支持 explicit region open。

4. Linux backend update
   - 为现有 dir backend 填充 flags/caps；
   - 保持 `mmap` 优先、read fallback；
   - 暂不实现 `open_by_id` 的内容索引。

5. Call site migration
   - test app 继续可用 `gfx_asset_store_open_mmap()` wrapper；
   - 新代码使用 `gfx_asset_store_open()`；
   - 直接使用 `esp_mmap_assets` 的示例迁移到 asset store，但不把业务 index 解析放入资源层。

6. Tests
   - Host smoke：打开目录、按 name 读取、校验 size/data 非空、close view/store；
   - Host fallback smoke：模拟 mmap 失败或用 force copy 路径；
   - ESP-IDF smoke：mmap-assets direct view flags；
   - ESP-IDF smoke：VFS dir copy view flags；
   - ESP-IDF smoke：partition mmap/read region 生命周期。

## Non-Goals

- 不定义 `index.json` 格式；
- 不实现 id 到文件名的 manifest 映射；
- 不解析动画、图片、字体资源；
- 不做资源下载、缓存、解压；
- 不在 decoder 内部打开文件。

## Migration Notes

`mmap_assets_handle_t` 可以继续存在于 mmap backend 私有结构中，但不应出现在业务播放器结构里。业务只保留：

```c
gfx_asset_store_t *store;
gfx_asset_view_t view;
```

播放动画时，仍然将 view 转成 memory source：

```c
gfx_anim_src_t src = {
    .type = GFX_ANIM_SRC_TYPE_MEMORY,
    .data = view.data,
    .data_len = view.size,
};
```

如果业务必须要求零拷贝，可检查：

```c
if ((view.flags & GFX_ASSET_VIEW_FLAG_DIRECT_ADDR) == 0) {
    /* decide whether copied data is acceptable */
}
```

这样 mmap 的 direct-address 特性被保留，同时普通 fs 和 partition read 也能通过同一套 API 提供只读字节视图。
