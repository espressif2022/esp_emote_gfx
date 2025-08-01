# ESP Emote GFX

## Introduction
`esp_emote_gfx` is a lightweight and efficient graphics framework component designed for ESP-IDF embedded systems. It provides a comprehensive graphics API with object system, drawing functions, color utilities, and animation capabilities. This module ensures high performance and flexibility for modern embedded applications that require efficient graphics rendering.

[![Component Registry](https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg)](https://components.espressif.com/components/espressif2022/esp_emote_gfx)

## Features

- **Object System**: Support for images, labels, and other graphics objects
- **Drawing Functions**: Efficient rendering to frame buffers
- **Color Utilities**: Comprehensive color management and type definitions
- **Software Blending**: Advanced blending capabilities for smooth graphics
- **Animation Support**: Built-in animation framework for dynamic graphics
- **Timer System**: Integrated timing functions for smooth animations
- **Memory Efficient**: Optimized for embedded systems with limited resources

## Dependencies

1. **ESP-IDF**  
   Ensure your project includes ESP-IDF 5.0 or higher. Refer to the [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/) for setup instructions.

2. **FreeType**  
   This component depends on the FreeType library for font rendering.

3. **ESP New JPEG**  
   JPEG decoding support through the ESP New JPEG component.

## Usage

### Basic Setup

```c
#include "gfx.h"

// Initialize the GFX framework
// Your initialization code here
```

### Creating Graphics Objects

```c
// Create an image object
gfx_img_t *img = gfx_img_create();

// Create a label object
gfx_label_t *label = gfx_label_create();
```

### Animation

```c
// Create and configure animations
gfx_anim_t *anim = gfx_anim_create();
// Configure your animation parameters
```

## API Reference

The main API is exposed through the `gfx.h` header file, which includes:

- `core/gfx_types.h` - Type definitions and constants
- `core/gfx_core.h` - Core graphics functions
- `core/gfx_timer.h` - Timer and timing utilities
- `core/gfx_obj.h` - Graphics object system
- `widget/gfx_img.h` - Image widget functionality
- `widget/gfx_label.h` - Label widget functionality
- `widget/gfx_anim.h` - Animation framework

## Font Support

This component includes support for LVGL fonts. For detailed information about font integration and usage, please refer to [LVGL_FONT_SUPPORT.md](docs/LVGL_FONT_SUPPORT.md).

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and enhancement requests.
