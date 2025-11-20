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

Copyright 2025 Espressif Systems (Shanghai) CO LTD

