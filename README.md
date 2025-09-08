# ESP Emote GFX

## Introduction
A lightweight graphics framework for ESP-IDF with support for images, labels, animations, and fonts.

[![Component Registry](https://components.espressif.com/components/espressif2022/esp_emote_gfx/badge.svg)](https://components.espressif.com/components/espressif2022/esp_emote_gfx)

## Features

- **Images**: Display images in RGB565A8 format
- **Animations**: GIF animations with [ESP32 tools](https://esp32-gif.espressif.com/)
- **Fonts**: LVGL fonts and FreeType TTF/OTF support
- **Timers**: Built-in timing system for smooth animations
- **Memory Optimized**: Designed for embedded systems

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
gfx_core_config_t gfx_cfg = {
    .flush_cb = flush_callback,
    .h_res = BSP_LCD_H_RES,
    .v_res = BSP_LCD_V_RES,
    .fps = 50,
    // other configuration...
};
gfx_handle_t emote_handle = gfx_emote_init(&gfx_cfg);
```

### Label Widget

The framework supports both LVGL fonts and FreeType rendering:

```c
// Create a label object
gfx_obj_t *label_obj = gfx_label_create(emote_handle);

// Using LVGL fonts
gfx_label_set_font(label_obj, (gfx_font_t)&font_puhui_16_4);

// Using FreeType fonts
gfx_label_cfg_t font_cfg = {
    .name = "DejaVuSans.ttf",
    .mem = font_data,
    .mem_size = font_size,
    .font_size = 20,
};
gfx_font_t font_freetype;
gfx_label_new_font(&font_cfg, &font_freetype);
gfx_label_set_font(label_obj, font_freetype);

// Set text and properties
gfx_label_set_text(label_obj, "Hello World");
gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x00FF00));
gfx_obj_set_pos(label_obj, 100, 200);
```

### Image Widget (RGB565A8 Format)

```c
// Create an image object
gfx_obj_t *img_obj = gfx_img_create(emote_handle);

// Set image source (supports RGB565A8 format)
gfx_img_set_src(img_obj, (void*)&image_data);
gfx_obj_set_pos(img_obj, 100, 100);
```

### Animation Widget

Create animations using the [ESP32 GIF animation tools](https://esp32-gif.espressif.com/) (converts GIF files to EAF animation format):

```c
// Create animation object
gfx_obj_t *anim_obj = gfx_anim_create(emote_handle);

// Set animation source data
gfx_anim_set_src(anim_obj, anim_data, anim_size);
gfx_obj_set_size(anim_obj, 200, 150);

// Configure animation segment
gfx_anim_set_segment(anim_obj, 0, 90, 15, true);

// Start animation
gfx_anim_start(anim_obj);
```

### Timer System

```c
// Create timer callback
void timer_callback(void *user_data) {
    // Timer callback code
}

// Create and configure timer
gfx_timer_handle_t timer = gfx_timer_create(emote_handle, timer_callback, 1000, user_data);

// Control timer
gfx_timer_pause(timer);
gfx_timer_resume(timer);
gfx_timer_delete(emote_handle, timer);
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

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and enhancement requests.
