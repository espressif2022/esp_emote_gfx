# ESP Emote GFX

## Introduction
A lightweight graphics framework for ESP-IDF with support for images, labels, animations, and fonts.

[![Component Registry](https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg)](https://components.espressif.com/components/espressif2022/esp_emote_gfx)

## Features

- **Images**: Display images in RGB565A8 format
- **Animations**: GIF animations with [ESP32 tools](https://esp32-gif.espressif.com/)
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

Initialize the graphics core with `gfx_emote_init()`, then add a display with `gfx_disp_add()` and optionally touch with `gfx_touch_add()`. Widgets are created on a display (`gfx_label_create(disp)`, etc.). When modifying widgets from another task, use `gfx_emote_lock()` / `gfx_emote_unlock()`.

For step-by-step setup and code examples, see the [Quick Start Guide](docs/quickstart.rst) in the docs.

## Examples

For comprehensive examples and detailed API documentation, see the [official documentation website](https://espressif2022.github.io/esp_emote_gfx/examples.html#complete-application-example).

The documentation includes:
- Basic examples (Simple Label, Image Display)
- Advanced examples (Multiple Widgets, Text Scrolling, FreeType Font Usage, Timer-Based Updates, QR Code Generation, Thread-Safe Operations)
- Complete application examples

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
- `core/gfx_timer.h` - Timer and timing utilities
- `core/gfx_obj.h` - Graphics object system
- `widget/gfx_img.h` - Image widget functionality
- `widget/gfx_label.h` - Label widget functionality
- `widget/gfx_anim.h` - Animation framework
- `widget/gfx_qrcode.h` - QR Code widget functionality

For detailed API documentation and usage examples, visit the [official documentation](https://espressif2022.github.io/esp_emote_gfx/).

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and enhancement requests.
