# ESP Emote GFX

**Languages / 语言:** [English](#english) · [中文](#中文)

---

## English

### Introduction

A lightweight graphics framework for ESP-IDF with support for images, labels, animations, buttons, QR codes, and fonts. Online documentation is published in **English** and **Simplified Chinese** (switch language from the docs header).

[![Component Registry](https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg)](https://components.espressif.com/components/espressif2022/esp_emote_gfx)

### Features

- **Images**: Display images in RGB565A8 format
- **Animations**: GIF animations with [ESP32 tools](https://esp32-gif.espressif.com/)
- **Buttons**: Interactive button widgets with text and state styling
- **Fonts**: LVGL fonts and FreeType TTF/OTF support
- **QR Codes**: Dynamic QR code generation and display
- **Timers**: Built-in timing system for smooth animations
- **Memory Optimized**: Designed for embedded systems

### Dependencies

1. **ESP-IDF**  
   Ensure your project includes ESP-IDF 5.0 or higher. Refer to the [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/) for setup instructions.

2. **FreeType**  
   This component depends on the FreeType library for font rendering.

3. **ESP New JPEG**  
   JPEG decoding support through the ESP New JPEG component.

### Quick Start

- Initialize the graphics core with `gfx_emote_init()`, then add a display with `gfx_disp_add()` and optionally touch with `gfx_touch_add()`.
- Widgets are created on a display (`gfx_label_create(disp)`, etc.).
- When modifying widgets from another task, use `gfx_emote_lock()` / `gfx_emote_unlock()`.

For step-by-step setup and code examples, see the [Quick Start Guide](https://espressif2022.github.io/esp_emote_gfx/en/quickstart.html) in the docs (Chinese: [快速入门](https://espressif2022.github.io/esp_emote_gfx/zh_CN/quickstart.html)).

### Examples

The documentation includes:

- Basic examples (Simple Label, Image Display)
- Advanced examples (Multiple Widgets, Text Scrolling, FreeType Font Usage, Timer-Based Updates, QR Code Generation, Thread-Safe Operations)
- Complete application examples

See the [documentation](https://espressif2022.github.io/esp_emote_gfx/en/examples.html) for runnable examples and full API reference.

#### Running test applications

Test applications are available in `esp_emote_gfx/test_apps/`:

```bash
cd esp_emote_gfx/test_apps
idf.py build flash monitor
```

### Image format

The framework supports RGB565A8 format images (16-bit RGB color with 8-bit alpha transparency).

#### Converting PNG to RGB565A8

Use the provided conversion script:

```bash
# Convert single PNG file to C array format
python scripts/png_to_rgb565a8.py image.png

# Convert single PNG file to binary format
python scripts/png_to_rgb565a8.py image.png --bin

# Batch convert all PNG files in current directory
python scripts/png_to_rgb565a8.py ./ --bin
```

**Script options:**

- `--bin`: Output binary format instead of C array format
- `--swap16`: Enable byte swapping for RGB565 data
- `--output`, `-o`: Specify output directory

### API reference

The main API is exposed through the `gfx.h` header file, which includes:

- `core/gfx_types.h` - Type definitions and constants
- `core/gfx_core.h` - Core graphics functions
- `core/gfx_disp.h` - Display setup and flush
- `core/gfx_log.h` - Log level configuration
- `core/gfx_timer.h` - Timer and timing utilities
- `core/gfx_touch.h` - Touch input
- `core/gfx_obj.h` - Graphics object system
- `widget/gfx_img.h` - Image widget functionality
- `widget/gfx_label.h` - Label widget functionality
- `widget/gfx_anim.h` - Animation framework
- `widget/gfx_button.h` - Button widget functionality
- `widget/gfx_qrcode.h` - QR Code widget functionality
- `widget/gfx_font_lvgl.h` - LVGL font compatibility

For the full API reference, see the English [documentation index](https://espressif2022.github.io/esp_emote_gfx/en/index.html) or [简体中文首页](https://espressif2022.github.io/esp_emote_gfx/zh_CN/index.html).

To build the docs locally:

```bash
./docs/preview.sh
# then open http://127.0.0.1:8090/  (or run: bash docs/scripts/postprocess_docs.sh)
```

### License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.

### Contributing

Contributions are welcome! Please feel free to submit issues and enhancement requests.

---

## 中文

### 简介

面向 ESP-IDF 的轻量级图形框架，支持图像、标签、动画、按钮、二维码与字体。在线文档提供**英文**与**简体中文**，可在文档页顶切换语言。

[![Component Registry](https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg)](https://components.espressif.com/components/espressif2022/esp_emote_gfx)

### 功能特性

- **图像**：RGB565A8 格式显示
- **动画**：配合 [ESP32 工具](https://esp32-gif.espressif.com/) 使用 GIF 等动画
- **按钮**：带文案与按下/常态样式的可交互按钮控件
- **字体**：LVGL 字体与 FreeType TTF/OTF 支持
- **二维码**：动态生成与显示二维码
- **定时器**：内置时序，便于流畅动画
- **内存友好**：面向嵌入式场景优化

### 依赖

1. **ESP-IDF**  
   工程需使用 ESP-IDF 5.0 及以上，安装与配置见 [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)。

2. **FreeType**  
   组件依赖 FreeType 进行字体渲染。

3. **ESP New JPEG**  
   通过 ESP New JPEG 组件提供 JPEG 解码支持。

### 快速入门

- 使用 `gfx_emote_init()` 初始化图形核心，再用 `gfx_disp_add()` 添加显示器，可选 `gfx_touch_add()` 添加触摸。
- 控件在显示器上创建（如 `gfx_label_create(disp)` 等）。
- 若在其他任务中修改控件，请使用 `gfx_emote_lock()` / `gfx_emote_unlock()`。

分步说明与示例代码见文档：[快速入门](https://espressif2022.github.io/esp_emote_gfx/zh_CN/quickstart.html)。

### 示例与文档

文档中包含：

- 基础示例（简单标签、图像显示）
- 进阶示例（多控件、文本滚动、FreeType、定时更新、二维码、线程安全操作等）
- 完整应用示例

可运行示例与完整 API 说明见 [文档示例页](https://espressif2022.github.io/esp_emote_gfx/zh_CN/examples.html)。

#### 运行测试工程

测试应用位于 `esp_emote_gfx/test_apps/`：

```bash
cd esp_emote_gfx/test_apps
idf.py build flash monitor
```

### 图像格式

框架支持 RGB565A8（16 位 RGB + 8 位 Alpha）。

#### PNG 转 RGB565A8

使用仓库提供的转换脚本：

```bash
# 单文件转 C 数组
python scripts/png_to_rgb565a8.py image.png

# 单文件转二进制
python scripts/png_to_rgb565a8.py image.png --bin

# 当前目录批量转二进制
python scripts/png_to_rgb565a8.py ./ --bin
```

**常用参数：**

- `--bin`：输出二进制而非 C 数组
- `--swap16`：RGB565 字节交换
- `--output` / `-o`：输出目录

### API 说明

主入口为 `gfx.h`，聚合了例如：

- `core/gfx_types.h` — 类型与常量
- `core/gfx_core.h` — 核心图形接口
- `core/gfx_disp.h` — 显示与刷新
- `core/gfx_log.h` — 日志级别
- `core/gfx_timer.h` — 定时与时序
- `core/gfx_touch.h` — 触摸输入
- `core/gfx_obj.h` — 图形对象系统
- `widget/gfx_img.h` — 图像控件
- `widget/gfx_label.h` — 标签控件
- `widget/gfx_anim.h` — 动画
- `widget/gfx_button.h` — 按钮控件
- `widget/gfx_qrcode.h` — 二维码控件
- `widget/gfx_font_lvgl.h` — LVGL 字体兼容

完整索引：[简体中文文档首页](https://espressif2022.github.io/esp_emote_gfx/zh_CN/index.html)。

本地构建文档：

```bash
./docs/preview.sh
# 浏览器打开 http://127.0.0.1:8090/（或执行: bash docs/scripts/postprocess_docs.sh）
```

### 许可证

本项目采用 Apache License 2.0，详见 [LICENSE](LICENSE)。

### 参与贡献

欢迎提交 Issue 与改进建议。
