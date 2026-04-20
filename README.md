# ESP Emote GFX

## Introduction
A lightweight graphics framework for ESP-IDF with support for images, labels, animations, buttons, QR codes, and fonts. Online documentation is published in **English** and **Simplified Chinese** (switch language from the docs header).

面向 ESP-IDF 的轻量级图形框架，支持图像、标签、动画、按钮、二维码与字体。在线文档提供**英文**与**简体中文**，可在文档页顶切换语言。

[![Component Registry](https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg)](https://components.espressif.com/components/espressif2022/esp_emote_gfx)

## Features

- **Images**: Display images in RGB565A8 format
- **Animations**: GIF animations with [ESP32 tools](https://esp32-gif.espressif.com/)
- **Buttons**: Interactive button widgets with text and state styling
- **Fonts**: LVGL fonts and FreeType TTF/OTF support
- **QR Codes**: Dynamic QR code generation and display
- **Timers**: Built-in timing system for smooth animations
- **Memory Optimized**: Designed for embedded systems

## Dependencies

1. **ESP-IDF**  
   Ensure your project includes ESP-IDF 5.0 or higher. Refer to the [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/) for setup instructions.

2. **FreeType**  
   This component depends on the FreeType library for font rendering.

3. **ESP New JPEG**  
   JPEG decoding support through the ESP New JPEG component.

## Quick Start

- Initialize the graphics core with `gfx_emote_init()`, then add a display with `gfx_disp_add()` and optionally touch with `gfx_touch_add()`.
- Widgets are created on a display (`gfx_label_create(disp)`, etc.).
- When modifying widgets from another task, use `gfx_emote_lock()` / `gfx_emote_unlock()`.

For step-by-step setup and code examples, see the [Quick Start Guide](https://espressif2022.github.io/esp_emote_gfx/en/quickstart.html) in the docs (Chinese: [快速入门](https://espressif2022.github.io/esp_emote_gfx/zh_CN/quickstart.html)).

## Examples

The documentation includes:
- Basic examples (Simple Label, Image Display)
- Advanced examples (Multiple Widgets, Text Scrolling, FreeType Font Usage, Timer-Based Updates, QR Code Generation, Thread-Safe Operations)
- Complete application examples

See the [documentation](https://espressif2022.github.io/esp_emote_gfx/en/examples.html) for runnable examples and full API reference.

### Running Test Applications

Test applications are available in `esp_emote_gfx/test_apps/`:

```bash
cd esp_emote_gfx/test_apps
idf.py build flash monitor
```

## Image Format

The framework supports RGB565A8 format images (16-bit RGB color with 8-bit alpha transparency).

### Converting PNG to RGB565A8

Use the provided conversion script:

```bash
# Convert single PNG file to C array format
python scripts/png_to_rgb565a8.py image.png

# Convert single PNG file to binary format
python scripts/png_to_rgb565a8.py image.png --bin

# Batch convert all PNG files in current directory
python scripts/png_to_rgb565a8.py ./ --bin
```

**Script Options:**
- `--bin`: Output binary format instead of C array format
- `--swap16`: Enable byte swapping for RGB565 data
- `--output`, `-o`: Specify output directory

## API Reference

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

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and enhancement requests.
