# Image Converter

将图片转换为 GFX 库支持的格式（RGB565/RGB565A8）的工具脚本。

## 功能特性

- ✅ 支持 RGB565 格式（无透明通道，节省 33% 内存）
- ✅ 支持 RGB565A8 格式（带独立 alpha 通道）
- ✅ 生成 C 文件或二进制文件
- ✅ 支持字节交换（适配不同硬件）
- ✅ 批量转换整个目录

## 格式对比

| 格式      | 文件大小 (64×64) | 透明支持 | 适用场景               |
|-----------|------------------|----------|------------------------|
| RGB565    | 8 KB             | ❌       | 不透明图标、背景图     |
| RGB565A8  | 12 KB            | ✅       | 需要透明效果的图标、UI |

## 安装依赖

```bash
pip install Pillow
```

## 使用方法

### 基础用法

```bash
# 转换为 RGB565A8 格式（默认，带透明通道）
python3 image_converter.py image.png

# 转换为 RGB565 格式（无透明通道，更小）
python3 image_converter.py image.png --format rgb565
```

### 高级选项

```bash
# 指定输出目录
python3 image_converter.py image.png --output ./output/

# 生成二进制文件（.bin）而不是 C 文件
python3 image_converter.py image.png --bin

# 启用字节交换（某些硬件需要）
python3 image_converter.py image.png --swap16

# 批量转换目录下所有 PNG 文件
python3 image_converter.py ./images/ --output ./converted/

# 组合使用：RGB565 + 二进制 + 字节交换
python3 image_converter.py icon.png --format rgb565 --bin --swap16

```

### 完整参数说明

| 参数                    | 说明                                    | 默认值      |
|-------------------------|----------------------------------------|-------------|
| `input`                 | 输入文件或目录路径                      | 必需        |
| `-o, --output`          | 输出目录                                | 当前目录    |
| `-f, --format`          | 输出格式：`rgb565` 或 `rgb565a8`        | `rgb565a8`  |
| `--bin`                 | 生成二进制文件而不是 C 文件             | 关闭        |
| `--swap16`              | 启用 RGB565 字节交换                    | 关闭        |
| `--embed-into-inc`      | 将图片嵌入现有 `.inc` 文件              | 不启用      |
| `--symbol`              | 指定导出的 C 符号名                     | 自动生成    |

## 输出示例

### C 文件输出 (默认)

```c
#include "gfx.h"

const uint8_t my_icon_map[] = {
    0xff, 0xff, 0xff, 0xff, ...
};

const gfx_image_dsc_t my_icon = {
    .header.cf = GFX_COLOR_FORMAT_RGB565,  // 或 GFX_COLOR_FORMAT_RGB565A8
    .header.magic = C_ARRAY_HEADER_MAGIC,
    .header.w = 64,
    .header.h = 64,
    .data_size = 8192,  // RGB565: width*height*2, RGB565A8: width*height*3
    .data = my_icon_map,
};
```

### 二进制文件输出 (--bin)

```
[12 bytes header]
[image data]

Header 结构:
- magic (0x19)
- cf (0x04=RGB565, 0x0A=RGB565A8)
- width, height
- stride
```

## 使用示例

### 示例 1: 创建不透明图标

```bash
# 转换不需要透明的图标，节省内存
python3 image_converter.py logo.png --format rgb565
```

生成的 C 文件可以这样使用：

```c
#include "logo.c"

gfx_obj_t *img = gfx_img_create(handle);
gfx_img_set_src(img, (void *)&logo);
gfx_obj_align(img, GFX_ALIGN_CENTER, 0, 0);
```

### 示例 2: 创建带透明效果的 UI 元素

```bash
# 转换需要透明效果的图标
python3 image_converter.py button.png --format rgb565a8
```

### 示例 3: 批量转换资源目录

```bash
# 转换 assets 目录下所有 PNG 为 RGB565 格式
python3 image_converter.py ./assets/ \
    --format rgb565 \
    --output ./src/images/
```

### 示例 4: 为特定硬件生成二进制文件

```bash
# 生成字节交换的二进制文件
python3 image_converter.py icon.png \
    --format rgb565 \
    --bin \
    --swap16 \
    --output ./flash_data/
```

## 数据布局

### RGB565 格式
```
[RGB565 pixel data]
- Size: width × height × 2 bytes
```

### RGB565A8 格式
```
[RGB565 pixel data] [Alpha mask data]
- RGB565 size: width × height × 2 bytes
- Alpha size: width × height × 1 byte
- Total: width × height × 3 bytes
```

## 常见问题

### Q: 什么时候用 RGB565，什么时候用 RGB565A8？

**A:** 
- **RGB565**: 不需要透明效果的图片（如背景、logo、纯色图标）
- **RGB565A8**: 需要透明或半透明效果的图片（如 UI 元素、图标）

### Q: 什么时候需要 --swap16？

**A:** 当目标硬件的字节序与生成的不匹配时使用。通常 ESP32 不需要此选项。

### Q: C 文件和二进制文件的区别？

**A:**
- **C 文件**: 直接编译到程序中，访问速度快，但增加程序大小
- **二进制文件**: 存储在外部存储（如 SPIFFS/SD卡），节省程序空间，但需要运行时加载

### Q: 如何查看生成的文件信息？

**A:** 运行脚本时会输出详细信息：
```
Successfully generated output.c
Format: RGB565
Image size: 64x64
Total data size: 8192 bytes
RGB565 data: 8192 bytes (4096 pixels)
Swap16: disabled
```

## 与现有代码兼容

该工具生成的文件与现有的 `gfx_img` API 完全兼容：

```c
// 两种格式使用方式完全相同
gfx_obj_t *img1 = gfx_img_create(handle);
gfx_img_set_src(img1, &rgb565_image);    // RGB565 图片

gfx_obj_t *img2 = gfx_img_create(handle);
gfx_img_set_src(img2, &rgb565a8_image);  // RGB565A8 图片

// 库会自动检测格式并正确渲染
```

## 性能对比

基于 ESP32-S3 测试（64×64 像素图片）：

| 格式      | 内存占用 | 加载时间 | 渲染帧率 |
|-----------|----------|----------|----------|
| RGB565    | 8 KB     | ~5ms     | ~60 FPS  |
| RGB565A8  | 12 KB    | ~7ms     | ~45 FPS  |

## 许可证

SPDX-License-Identifier: Apache-2.0

## Face Emote Asset Flow

当前推荐流程已经切换为网页直接导出，不再依赖旧的 JSON / Python 生成链。

### 当前主流程

1. 打开 `test_apps/face_expressions_vivid.html`
2. 在页面里调整 base geometry 和 expression library
3. 点击 `Export .inc Code`
4. 将导出的内容覆盖到 `test_apps/main/face_emote_expr_assets.inc`
5. 重新编译 `test_apps`

### 导出目标

- `test_apps/main/face_emote_expr_assets.inc`
  - `s_ref_eye`
  - `s_ref_brow`
  - `s_ref_mouth`
  - `s_face_sequence`

网页导出已经直接对齐 `gfx_face_emote_*` 类型，因此可以直接作为
`gfx_face_emote` 标准组件的资产输入。

### 旧流程状态

以下内容保留仅作历史说明，不再作为推荐链路：

- `scripts/face_keyframes.json`
- `generate_face_keyframes_c.py`
- `face_keyframe_debug.py`

如果后续重新引入脚本工具，应以网页导出的 `.inc` 结构为准，而不是再回到旧的
JSON 为单一事实来源。

### Lobster 单文件导出

当前龙虾链路已经收敛为单一导出源：

- `test_apps/main/lobster_emote_export.inc`

该文件已经直接包含：

- export meta
- layout
- 曲线基准数据
- expression sequence
- 静态几何点
- 内嵌背景图 `gfx_image_dsc_t`

因此当前推荐流程是：

1. 在网页里直接导出 `lobster_emote_export.inc`
2. 用 `validate_lobster_export.py` 校验
3. 重新编译 `test_lobster_expr_emote`

### Lobster 导出标准结构

`lobster_emote_export.inc` 现在按下面的标准结构组织：

- `s_lobster_export_meta`
  - 导出版本
  - 设计 viewBox
  - 导出尺寸
  - 导出 scale / offset
- `s_lobster_export_layout`
  - eye / pupil / mouth / antenna 的锚点坐标
- `s_lobster_export_semantics`
  - 运行时语义参数
  - pose 推导系数
  - mesh 分段与 timer/damping 默认值
- `s_lobster_*_base`
  - 局部曲线基准数据
- `s_lobster_expr_sequence`
  - 表情序列
- 内嵌背景图块
  - `LOBSTER_USE_EXPORTED_BG`
  - `LOBSTER_EXPORTED_BG_SYMBOL`
  - `gfx_image_dsc_t`

运行时约束：

- `layout` 负责锚点，不再混放导出元信息
- `export_meta` 负责版本、坐标系和导出尺寸
- 如果 `layout` 存在，则必须同时提供 `export_meta`
- `v2` 导出必须带 `s_lobster_export_semantics`
- 当前导出版本为 `2`

### Lobster 导出校验

导出完成后，可以用下面的脚本检查 `.inc` 是否满足当前龙虾标准结构：

```bash
python3 validate_lobster_export.py ../test_apps/main/lobster_emote_export.inc
```

会检查：

- `s_lobster_export_meta`
- `s_lobster_export_layout`
- `s_lobster_export_semantics`（当版本为 `2`）
- `s_lobster_expr_sequence`
- 内嵌背景图尺寸是否等于 `export_meta.export_width/export_height`
- 当前版本号是否受支持

运行时容错：

- 如果导出资产完整且合法，运行时直接使用导出布局
- 如果导出资产非法，但基础 sequence 仍然存在，运行时会记录 warning，并回退到默认布局/缩放路径
- 如果 sequence 本身缺失，运行时仍会报错拒绝加载

Copyright 2025 Espressif Systems (Shanghai) CO LTD
