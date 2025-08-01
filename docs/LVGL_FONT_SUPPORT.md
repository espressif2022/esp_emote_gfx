# LVGL Font Support

This document explains how to use both FreeType fonts (TTF/OTF) and LVGL C format fonts with the unified font system.

## Overview

The graphics library now supports two font formats:

1. **FreeType Fonts**: Traditional TTF/OTF fonts loaded from memory, with runtime size adjustment
2. **LVGL C Format Fonts**: Pre-compiled font arrays optimized for embedded systems

## LVGL C Format Fonts

### What are LVGL C Format Fonts?

LVGL C format fonts are bitmap fonts that have been converted from TTF files into C arrays. They offer several advantages:

- **Fast Loading**: No parsing needed, direct memory access
- **Predictable Size**: Known memory footprint at compile time
- **No Runtime Dependencies**: No FreeType library needed
- **Optimized Storage**: Only includes characters you need

### Using Your font_16.c File

Your `font_16.c` file is a perfect example of an LVGL font. Here's how to use it:

#### Step 1: Include the Font

```c
// Include your font file in your project
#include "font_16.c"  // or add to CMakeLists.txt

// Declare the external font variable
extern const lv_font_t font_16;
```

#### Step 2: Create Font Configuration

```c
#include "widget/gfx_label.h"

// Method 1: Using convenience macro
gfx_label_cfg_t font_cfg = GFX_FONT_LVGL_CONFIG("KaiTi_16", &font_16);

// Method 2: Manual configuration
gfx_label_cfg_t font_cfg = {
    .name = "KaiTi_16",
    .font_src = GFX_FONT_SRC_LVGL_C,
    .font.lvgl_c = {
        .font_data = &font_16
    }
};
```

#### Step 3: Create and Use the Font

```c
esp_err_t ret;
gfx_font_t my_font;
gfx_obj_t *label;

// Create the font
ret = gfx_label_new_font(handle, &font_cfg, &my_font);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create font");
    return;
}

// Create a label and set the font
label = gfx_label_create(handle);
gfx_label_set_font(label, my_font);
gfx_label_set_text(label, "Hello LVGL Font! 你好");
```

## Complete Example

```c
#include "esp_log.h"
#include "core/gfx_core.h"
#include "widget/gfx_label.h"

// Your font from font_16.c
extern const lv_font_t font_16;

void lvgl_font_example(gfx_handle_t handle)
{
    // Configure LVGL font
    gfx_label_cfg_t cfg = GFX_FONT_LVGL_CONFIG("KaiTi_16", &font_16);
    
    // Create font
    gfx_font_t font;
    esp_err_t ret = gfx_label_new_font(handle, &cfg, &font);
    if (ret != ESP_OK) {
        ESP_LOGE("FONT", "Failed to create font: %s", esp_err_to_name(ret));
        return;
    }
    
    // Create label and configure
    gfx_obj_t *label = gfx_label_create(handle);
    gfx_label_set_font(label, font);
    gfx_label_set_text(label, "LVGL Font Test: 中文测试");
    gfx_label_set_color(label, 0xFFFFFF);  // White text
    gfx_label_set_text_align(label, GFX_TEXT_ALIGN_CENTER);
    
    ESP_LOGI("FONT", "LVGL font setup complete");
}
```

## Font Properties from your font_16.c

Based on your font file, here are the specifications:

- **Size**: 16 pixels
- **Format**: 4 BPP (4 bits per pixel) for anti-aliasing
- **Character Range**: 
  - ASCII: 32-112 (space to 'p')
  - Additional ranges as defined in the cmap
- **Font Family**: KaiTi (楷体)
- **Line Height**: 17 pixels
- **Base Line**: 3 pixels from bottom

## FreeType vs LVGL Font Comparison

| Feature | FreeType | LVGL C Format |
|---------|----------|---------------|
| File Size | Larger TTF/OTF | Smaller compiled arrays |
| Loading Speed | Slower (parsing) | Faster (direct access) |
| Runtime Size Change | ✅ Yes | ❌ No (fixed size) |
| Memory Usage | Variable | Predictable |
| Character Sets | Full font | Only included chars |
| Anti-aliasing | Runtime configurable | Pre-rendered |

## Creating New LVGL Fonts

To create your own LVGL C format fonts, use the LVGL font converter:

```bash
# Install lv_font_conv
npm install lv_font_conv -g

# Convert TTF to LVGL C format
lv_font_conv --bpp 4 --size 16 --font KaiTi.ttf \
    --range 32-112 --range 0x4e00-0x9fff \
    --format lvgl -o my_font_16.c
```

Parameters explanation:
- `--bpp 4`: 4 bits per pixel (16 grayscale levels)
- `--size 16`: Font size in pixels
- `--range 32-112`: ASCII characters
- `--range 0x4e00-0x9fff`: Common Chinese characters
- `--format lvgl`: Output LVGL C format

## Advanced Usage

### Multiple Font Sizes

```c
// Create multiple sizes of the same font family
extern const lv_font_t font_12;
extern const lv_font_t font_16;
extern const lv_font_t font_24;

gfx_font_t small_font, medium_font, large_font;

gfx_label_cfg_t cfg_12 = GFX_FONT_LVGL_CONFIG("Small", &font_12);
gfx_label_cfg_t cfg_16 = GFX_FONT_LVGL_CONFIG("Medium", &font_16);
gfx_label_cfg_t cfg_24 = GFX_FONT_LVGL_CONFIG("Large", &font_24);

gfx_label_new_font(handle, &cfg_12, &small_font);
gfx_label_new_font(handle, &cfg_16, &medium_font);
gfx_label_new_font(handle, &cfg_24, &large_font);
```

### Mixed Font Usage

```c
// Use both FreeType and LVGL fonts in the same application
gfx_font_t title_font;    // LVGL font for titles
gfx_font_t body_font;     // FreeType font for body text

// Title with LVGL font (faster, fixed size)
gfx_label_cfg_t title_cfg = GFX_FONT_LVGL_CONFIG("Title", &font_16);
gfx_label_new_font(handle, &title_cfg, &title_font);

// Body with FreeType font (flexible sizing)
gfx_label_cfg_t body_cfg = GFX_FONT_FREETYPE_CONFIG("Body", ttf_data, ttf_size);
gfx_label_new_font(handle, &body_cfg, &body_font);
```

## Build Configuration

Add to your `CMakeLists.txt`:

```cmake
# Add font file to sources
set(COMPONENT_SRCS
    "src/widget/gfx_draw_label.c"
    "src/widget/gfx_font_parser.c"
    "font_16.c"  # Your LVGL font file
    # ... other sources
)

# Include directories
set(COMPONENT_ADD_INCLUDEDIRS
    "include"
    "include_priv"
    # ... other includes  
)
```

## Memory Considerations

### LVGL Font Memory Usage

Your `font_16.c` uses approximately:
- Glyph bitmap data: ~2.5KB (varies by character set)
- Glyph descriptors: ~640 bytes (80 glyphs × 8 bytes)
- Character maps: ~48 bytes (3 maps × 16 bytes)
- Total: ~3.2KB (stored in flash, not RAM)

### Runtime Memory

```c
// Each font handle uses about 32 bytes RAM
gfx_font_handle_t font_handle;  // 32 bytes

// Each label with font reference uses about 4 bytes
gfx_label_property_t label_props;  // font_handle pointer: 4 bytes
```

## Troubleshooting

### Common Issues

1. **Font not found**: Ensure the font file is included in build
2. **Missing characters**: Check the character ranges in your font
3. **Compilation errors**: Verify all include paths are correct
4. **Garbled text**: Check UTF-8 encoding and character support

### Debug Information

Enable debug logging to see font loading details:

```c
esp_log_level_set("gfx_font_parser", ESP_LOG_DEBUG);
esp_log_level_set("gfx_draw_label", ESP_LOG_DEBUG);
```

## Performance Tips

1. **Use LVGL fonts for fixed text**: Faster rendering
2. **Use FreeType for dynamic sizing**: More flexible
3. **Cache font handles**: Don't recreate fonts repeatedly
4. **Optimize character sets**: Only include needed characters
5. **Consider font fallback**: Provide backup fonts for missing characters

## API Reference

### Font Configuration Macros

```c
GFX_FONT_LVGL_CONFIG(name, font_ptr)        // LVGL font configuration
GFX_FONT_FREETYPE_CONFIG(name, data, size)  // FreeType font configuration
```

### Font Management Functions

```c
esp_err_t gfx_label_new_font(handle, cfg, ret_font);
esp_err_t gfx_label_set_font(obj, font);
esp_err_t gfx_convert_external_lvgl_font(external_font, name, ret_handle);
``` 