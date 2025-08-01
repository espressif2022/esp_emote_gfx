/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/queue.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "core/gfx_types.h"
#include "core/gfx_core.h"
#include "core/gfx_obj.h"
#include "core/gfx_core_internal.h"
#include "core/gfx_timer.h"
#include "widget/gfx_font_internal.h"
#include "widget/gfx_label.h"
#include "widget/gfx_label_internal.h"
#include "widget/gfx_comm.h"
#include "core/gfx_blend_internal.h"
#include "core/gfx_obj_internal.h"

static const char *TAG = "gfx_label";

// Default font configuration (internal use)
static gfx_font_t g_default_font = NULL;
static uint16_t g_default_font_size = 20;
static gfx_color_t g_default_font_color = {.full = 0xFFFF}; // White
static gfx_opa_t g_default_font_opa = 0xFF;

// Helper function to clean cached line data
static void gfx_label_clear_cached_lines(gfx_label_property_t *font_info)
{
    if (font_info->cached_lines) {
        for (int i = 0; i < font_info->cached_line_count; i++) {
            if (font_info->cached_lines[i]) {
                free(font_info->cached_lines[i]);
            }
        }
        free(font_info->cached_lines);
        font_info->cached_lines = NULL;
        font_info->cached_line_count = 0;
    }

    if (font_info->cached_line_widths) {
        free(font_info->cached_line_widths);
        font_info->cached_line_widths = NULL;
    }
}

static void gfx_label_scroll_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    if (!obj || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    if (!font_info || !font_info->scroll_active || font_info->long_mode != GFX_LABEL_LONG_SCROLL) {
        return;
    }

    font_info->scroll_offset++;

    if (font_info->scroll_loop) {
        if (font_info->scroll_offset > font_info->text_width + obj->width) {
            font_info->scroll_offset = -obj->width;
        }
    } else {
        if (font_info->scroll_offset > font_info->text_width) {
            font_info->scroll_active = false;
            gfx_timer_pause(font_info->scroll_timer);
            return;
        }
    }

    // Mark scroll position as dirty (no need to reparse text)
    font_info->scroll_dirty = true;
}

// Internal function to get default font configuration
void gfx_get_default_font_config(gfx_font_t *font, uint16_t *size, gfx_color_t *color, gfx_opa_t *opa)
{
    if (font) {
        *font = g_default_font;
    }
    if (size) {
        *size = g_default_font_size;
    }
    if (color) {
        *color = g_default_font_color;
    }
    if (opa) {
        *opa = g_default_font_opa;
    }
}

esp_err_t gfx_ft_lib_create(gfx_ft_lib_handle_t *ret_lib)
{
    ESP_RETURN_ON_FALSE(ret_lib, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    FT_Error error;

    gfx_ft_lib_t *lib = (gfx_ft_lib_t *)calloc(1, sizeof(gfx_ft_lib_t));
    ESP_RETURN_ON_FALSE(lib, ESP_ERR_NO_MEM, TAG, "no mem for FT library");

    // Initialize the linked list manually since we removed SLIST macros
    lib->ft_face_head = NULL;

    error = FT_Init_FreeType((FT_Library *)&lib->ft_library);
    if (error) {
        ESP_LOGE(TAG, "error initializing FT library: %d", error);
        free(lib);
        return ESP_ERR_INVALID_STATE;
    }

    *ret_lib = lib;
    ESP_LOGD(TAG, "gfx_ft_lib_create: %p", lib);
    return ESP_OK;
}

esp_err_t gfx_ft_lib_cleanup(gfx_ft_lib_handle_t lib_handle)
{
    ESP_RETURN_ON_FALSE(lib_handle, ESP_ERR_INVALID_ARG, TAG, "invalid library");

    gfx_ft_lib_t *lib = (gfx_ft_lib_t *)lib_handle;

    // Clean up the linked list manually
    gfx_ft_face_entry_t *entry = lib->ft_face_head;
    while (entry != NULL) {
        gfx_ft_face_entry_t *next = entry->next;
        FT_Done_Face((FT_Face)entry->face);
        free(entry);
        entry = next;
    }

    FT_Done_FreeType((FT_Library)lib->ft_library);
    free(lib);

    return ESP_OK;
}

esp_err_t gfx_label_new_font(gfx_handle_t handle, const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    ESP_RETURN_ON_FALSE(handle && cfg && ret_font, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    ESP_RETURN_ON_FALSE(cfg->mem && cfg->mem_size, ESP_ERR_INVALID_ARG, TAG, "invalid memory input");

    FT_Face face = NULL;
    FT_Error error;

    gfx_ft_lib_t *lib = gfx_get_font_lib(handle);
    ESP_RETURN_ON_FALSE(lib, ESP_ERR_INVALID_STATE, TAG, "font library is NULL");

    gfx_ft_face_entry_t *entry;

    // Search for existing font
    entry = lib->ft_face_head;
    while (entry != NULL) {
        if (entry->mem == cfg->mem) {
            face = (FT_Face)entry->face;
            break;
        }
        entry = entry->next;
    }

    if (!face) {
        error = FT_New_Memory_Face((FT_Library)lib->ft_library, cfg->mem, cfg->mem_size, 0, &face);
        ESP_RETURN_ON_FALSE(!error, ESP_ERR_INVALID_ARG, TAG, "error loading font");

        gfx_ft_face_entry_t *new_face_entry = (gfx_ft_face_entry_t *)calloc(1, sizeof(gfx_ft_face_entry_t));
        ESP_RETURN_ON_FALSE(new_face_entry, ESP_ERR_NO_MEM, TAG, "no mem for ft_face_entry");

        new_face_entry->face = face;
        new_face_entry->mem = cfg->mem;
        new_face_entry->next = lib->ft_face_head;
        lib->ft_face_head = new_face_entry;
    }

    gfx_font_t font_handle = (gfx_font_t)face;

    // Set first font as default font automatically
    if (g_default_font == NULL) {
        g_default_font = font_handle;
        ESP_LOGI(TAG, "Set default font: %s", cfg->name);
    }

    ESP_LOGI(TAG, "new font(%s):@%p", cfg->name, face);
    *ret_font = font_handle;

    return ESP_OK;
}

esp_err_t gfx_label_set_font(gfx_obj_t *obj, gfx_font_t font)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->face = (void *)font;
    return ESP_OK;
}

esp_err_t gfx_label_set_text(gfx_obj_t * obj, const char *text)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;

    if (text == NULL) {
        text = font_info->text;
    }

    if (font_info->text == text) {
        font_info->text = realloc(font_info->text, strlen(font_info->text) + 1);
        assert(font_info->text);
        if (font_info->text == NULL) {
            return ESP_FAIL;
        }
    } else {
        if (font_info->text != NULL) {
            free(font_info->text);
            font_info->text = NULL;
        }

        size_t len = strlen(text) + 1;

        font_info->text = malloc(len);
        assert(font_info->text);
        if (font_info->text == NULL) {
            return ESP_FAIL;
        }
        strcpy(font_info->text, text);
    }

    obj->is_dirty = true;

    // Clear cached line data when text changes
    gfx_label_clear_cached_lines(font_info);

    // Reset scroll state when text changes (if in scroll mode)
    if (font_info->long_mode == GFX_LABEL_LONG_SCROLL) {
        if (font_info->scroll_active) {
            font_info->scroll_active = false;
            if (font_info->scroll_timer) {
                gfx_timer_pause(font_info->scroll_timer);
            }
        }
        font_info->scroll_offset = 0; // Reset scroll position
        font_info->text_width = 0;    // Reset cached width to force recalculation
    }

    // Reset scroll dirty flag since we're doing a full redraw
    font_info->scroll_dirty = false;

    return ESP_OK;
}

esp_err_t gfx_label_set_text_fmt(gfx_obj_t * obj, const char * fmt, ...)
{
    ESP_RETURN_ON_FALSE(obj && fmt, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;

    if (font_info->text != NULL) {
        free(font_info->text);
        font_info->text = NULL;
    }

    va_list args;
    va_start(args, fmt);

    /*Allocate space for the new text by using trick from C99 standard section 7.19.6.12*/
    va_list args_copy;
    va_copy(args_copy, args);
    uint32_t len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    font_info->text = malloc(len + 1);
    if (font_info->text == NULL) {
        va_end(args);
        return ESP_ERR_NO_MEM;
    }
    font_info->text[len] = '\0'; /*Ensure NULL termination*/

    vsnprintf(font_info->text, len + 1, fmt, args);
    va_end(args);

    obj->is_dirty = true;

    return ESP_OK;
}

esp_err_t gfx_label_set_font_size(gfx_obj_t * obj, uint8_t font_size)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(font_size > 0, ESP_ERR_INVALID_ARG, TAG, "invalid font size");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->font_size = font_size;
    obj->is_dirty = true;

    // Clear cached line data when font size changes
    gfx_label_clear_cached_lines(font_info);

    // Reset scroll state when font size changes (if in scroll mode)
    if (font_info->long_mode == GFX_LABEL_LONG_SCROLL) {
        if (font_info->scroll_active) {
            font_info->scroll_active = false;
            if (font_info->scroll_timer) {
                gfx_timer_pause(font_info->scroll_timer);
            }
        }
        font_info->scroll_offset = 0; // Reset scroll position
        font_info->text_width = 0;    // Reset cached width to force recalculation
    }

    // Reset scroll dirty flag since we're doing a full redraw
    font_info->scroll_dirty = false;

    ESP_LOGD(TAG, "set font size: %d", font_info->font_size);

    return ESP_OK;
}

esp_err_t gfx_label_set_opa(gfx_obj_t * obj, gfx_opa_t opa)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->opa = opa;
    ESP_LOGD(TAG, "set font opa: %d", font_info->opa);

    return ESP_OK;
}

esp_err_t gfx_label_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->color = color;
    ESP_LOGD(TAG, "set font color: %d", font_info->color.full);

    return ESP_OK;
}

esp_err_t gfx_label_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->bg_color = bg_color;
    ESP_LOGD(TAG, "set background color: %d", font_info->bg_color.full);

    return ESP_OK;
}

esp_err_t gfx_label_set_bg_enable(gfx_obj_t *obj, bool enable)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->bg_enable = enable;
    obj->is_dirty = true;  // Background change requires re-rendering
    ESP_LOGD(TAG, "set background enable: %s", enable ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t gfx_label_set_text_align(gfx_obj_t *obj, gfx_text_align_t align)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->text_align = align;
    obj->is_dirty = true;  // Mark as dirty to trigger re-rendering
    ESP_LOGD(TAG, "set text align: %d", align);

    return ESP_OK;
}

esp_err_t gfx_label_set_long_mode(gfx_obj_t *obj, gfx_label_long_mode_t long_mode)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    ESP_RETURN_ON_FALSE(font_info, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    gfx_label_long_mode_t old_mode = font_info->long_mode;
    font_info->long_mode = long_mode;

    // Handle mode transitions
    if (old_mode != long_mode) {
        // Reset scroll state when changing modes
        if (font_info->scroll_active) {
            font_info->scroll_active = false;
            if (font_info->scroll_timer) {
                gfx_timer_pause(font_info->scroll_timer);
            }
        }
        font_info->scroll_offset = 0;
        font_info->text_width = 0;  // Force recalculation

        // Create/cleanup scroll timer based on new mode
        if (long_mode == GFX_LABEL_LONG_SCROLL && !font_info->scroll_timer) {
            font_info->scroll_timer = gfx_timer_create(obj->parent_handle,
                                                       gfx_label_scroll_timer_callback,
                                                       font_info->scroll_speed_ms,
                                                       obj);
            if (font_info->scroll_timer) {
                gfx_timer_set_repeat_count(font_info->scroll_timer, -1); // Infinite repeat
            }
        } else if (long_mode != GFX_LABEL_LONG_SCROLL && font_info->scroll_timer) {
            gfx_timer_delete(obj->parent_handle, font_info->scroll_timer);
            font_info->scroll_timer = NULL;
        }

        obj->is_dirty = true;  // Mark as dirty to trigger re-rendering
    }

    // Reset scroll dirty flag since we're doing a full redraw
    font_info->scroll_dirty = false;

    ESP_LOGD(TAG, "set long mode: %d", long_mode);
    return ESP_OK;
}

esp_err_t gfx_label_set_line_spacing(gfx_obj_t *obj, uint16_t spacing)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    font_info->line_spacing = spacing;
    obj->is_dirty = true;  // Mark as dirty to trigger re-rendering
    ESP_LOGD(TAG, "set line spacing: %d", spacing);

    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_speed(gfx_obj_t *obj, uint32_t speed_ms)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    ESP_RETURN_ON_FALSE(speed_ms > 0, ESP_ERR_INVALID_ARG, TAG, "invalid speed");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    ESP_RETURN_ON_FALSE(font_info, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    font_info->scroll_speed_ms = speed_ms;

    // Update timer period if timer exists
    if (font_info->scroll_timer) {
        gfx_timer_set_period(font_info->scroll_timer, speed_ms);
    }

    ESP_LOGD(TAG, "set scroll speed: %lu ms", speed_ms);
    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_loop(gfx_obj_t *obj, bool loop)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    ESP_RETURN_ON_FALSE(font_info, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    font_info->scroll_loop = loop;
    ESP_LOGD(TAG, "set scroll loop: %s", loop ? "enabled" : "disabled");

    return ESP_OK;
}

// Function to render parsed lines to mask buffer
static esp_err_t gfx_render_lines_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, char **lines, int line_count,
                                          FT_Face face, int line_height, int base_line, int total_line_height, int *cached_line_widths)
{
    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;

    // Render each line
    int current_y = 0;
    for (int line_idx = 0; line_idx < line_count; line_idx++) {
        if (current_y + line_height > obj->height) {
            break; // No more space for lines
        }

        const char *line_text = lines[line_idx];

        // Calculate line width for alignment (use cached if available)
        int line_width = 0;
        if (cached_line_widths) {
            line_width = cached_line_widths[line_idx];
        } else {
            const char *p_calc = line_text;
            while (*p_calc) {
                FT_UInt glyph_index;
                uint8_t c = (uint8_t) * p_calc;
                int bytes_in_char = 1;

                if (c < 0x80) {
                    glyph_index = FT_Get_Char_Index(face, c);
                } else if ((c & 0xE0) == 0xC0) {
                    bytes_in_char = 2;
                    if (*(p_calc + 1) == 0) {
                        break;
                    }
                    glyph_index = FT_Get_Char_Index(face, ((c & 0x1F) << 6) | (*(p_calc + 1) & 0x3F));
                } else if ((c & 0xF0) == 0xE0) {
                    bytes_in_char = 3;
                    if (*(p_calc + 1) == 0 || *(p_calc + 2) == 0) {
                        break;
                    }
                    glyph_index = FT_Get_Char_Index(face, ((c & 0x0F) << 12) | ((*(p_calc + 1) & 0x3F) << 6) | (*(p_calc + 2) & 0x3F));
                } else if ((c & 0xF8) == 0xF0) {
                    bytes_in_char = 4;
                    if (*(p_calc + 1) == 0 || *(p_calc + 2) == 0 || *(p_calc + 3) == 0) {
                        break;
                    }
                    glyph_index = FT_Get_Char_Index(face, ((c & 0x07) << 18) | ((*(p_calc + 1) & 0x3F) << 12) | ((*(p_calc + 2) & 0x3F) << 6) | (*(p_calc + 3) & 0x3F));
                } else {
                    glyph_index = 0;
                }
                p_calc += bytes_in_char;

                if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
                    line_width += face->glyph->advance.x >> 6;
                }
            }
        }

        // Calculate starting x position based on alignment
        int start_x = 0;
        switch (font_info->text_align) {
        case GFX_TEXT_ALIGN_LEFT:
        case GFX_TEXT_ALIGN_AUTO:
            start_x = 0;
            break;
        case GFX_TEXT_ALIGN_CENTER:
            start_x = (obj->width - line_width) / 2;
            if (start_x < 0) {
                start_x = 0;
            }
            break;
        case GFX_TEXT_ALIGN_RIGHT:
            start_x = obj->width - line_width;
            if (start_x < 0) {
                start_x = 0;
            }
            break;
        }

        // Apply horizontal scroll offset if scrolling is enabled
        if (font_info->long_mode == GFX_LABEL_LONG_SCROLL && font_info->scroll_active) {
            start_x -= font_info->scroll_offset;
        }

        // Render characters in current line
        int x = start_x;
        const char *p = line_text;

        while (*p) {
            FT_UInt glyph_index;
            uint8_t c = (uint8_t) * p;
            int bytes_in_char = 1;

            if (c < 0x80) {
                glyph_index = FT_Get_Char_Index(face, c);
            } else if ((c & 0xE0) == 0xC0) {
                bytes_in_char = 2;
                if (*(p + 1) == 0) {
                    break;
                }
                glyph_index = FT_Get_Char_Index(face, ((c & 0x1F) << 6) | (*(p + 1) & 0x3F));
            } else if ((c & 0xF0) == 0xE0) {
                bytes_in_char = 3;
                if (*(p + 1) == 0 || *(p + 2) == 0) {
                    break;
                }
                glyph_index = FT_Get_Char_Index(face, ((c & 0x0F) << 12) | ((*(p + 1) & 0x3F) << 6) | (*(p + 2) & 0x3F));
            } else if ((c & 0xF8) == 0xF0) {
                bytes_in_char = 4;
                if (*(p + 1) == 0 || *(p + 2) == 0 || *(p + 3) == 0) {
                    break;
                }
                glyph_index = FT_Get_Char_Index(face, ((c & 0x07) << 18) | ((*(p + 1) & 0x3F) << 12) | ((*(p + 2) & 0x3F) << 6) | (*(p + 3) & 0x3F));
            } else {
                glyph_index = 0;
            }
            p += bytes_in_char;

            // Load and render glyph
            FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
            if (error) {
                continue; // Skip failed glyphs
            }

            error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            if (error) {
                continue; // Skip failed glyphs
            }

            // Copy glyph bitmap to mask buffer
            FT_GlyphSlot slot = face->glyph;
            int ofs_x = slot->bitmap_left;
            int ofs_y = line_height - base_line - slot->bitmap_top;

            for (int32_t iy = 0; iy < slot->bitmap.rows; iy++) {
                for (int32_t ix = 0; ix < slot->bitmap.width; ix++) {
                    int32_t res_x = ix + x + ofs_x;
                    int32_t res_y = iy + current_y + ofs_y;
                    if (res_x >= 0 && res_x < obj->width && res_y >= 0 && res_y < obj->height) {
                        uint8_t value = slot->bitmap.buffer[ix + iy * slot->bitmap.width];
                        *(mask + res_y * obj->width + res_x) = value;
                    }
                }
            }

            // Advance x position
            x += slot->advance.x >> 6;
            if (x >= obj->width) {
                break;
            }
        }

        // Move to next line
        current_y += total_line_height;
    }

    return ESP_OK;
}

// Function to parse text into lines and calculate width
static esp_err_t gfx_parse_text_lines(gfx_obj_t *obj, FT_Face face, int total_line_height,
                                      char ***ret_lines, int *ret_line_count, int *ret_text_width, int **ret_line_widths)
{
    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;

    // Calculate total text width for scrolling
    int total_text_width = 0;
    const char *p_width = font_info->text;

    while (*p_width) {
        FT_UInt glyph_index;
        uint8_t c = (uint8_t) * p_width;
        int bytes_in_char = 1;

        // Handle UTF-8 encoding for width calculation
        if (c < 0x80) {
            glyph_index = FT_Get_Char_Index(face, c);
        } else if ((c & 0xE0) == 0xC0) {
            bytes_in_char = 2;
            if (*(p_width + 1) == 0) {
                break;
            }
            glyph_index = FT_Get_Char_Index(face, ((c & 0x1F) << 6) | (*(p_width + 1) & 0x3F));
        } else if ((c & 0xF0) == 0xE0) {
            bytes_in_char = 3;
            if (*(p_width + 1) == 0 || *(p_width + 2) == 0) {
                break;
            }
            glyph_index = FT_Get_Char_Index(face, ((c & 0x0F) << 12) | ((*(p_width + 1) & 0x3F) << 6) | (*(p_width + 2) & 0x3F));
        } else if ((c & 0xF8) == 0xF0) {
            bytes_in_char = 4;
            if (*(p_width + 1) == 0 || *(p_width + 2) == 0 || *(p_width + 3) == 0) {
                break;
            }
            glyph_index = FT_Get_Char_Index(face, ((c & 0x07) << 18) | ((*(p_width + 1) & 0x3F) << 12) | ((*(p_width + 2) & 0x3F) << 6) | (*(p_width + 3) & 0x3F));
        } else {
            glyph_index = 0;
        }

        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
            total_text_width += face->glyph->advance.x >> 6;
        }

        p_width += bytes_in_char;

        // Break at newlines for single line width calculation
        if (c == '\n') {
            break;
        }
    }

    *ret_text_width = total_text_width;

    // Parse text into lines
    const char *text = font_info->text;
    int max_lines = obj->height / total_line_height;
    if (max_lines <= 0) {
        max_lines = 1; // At least allow one line
    }

    char **lines = (char **)malloc(max_lines * sizeof(char *));
    if (!lines) {
        return ESP_ERR_NO_MEM;
    }

    // Initialize all line pointers to NULL for safe cleanup
    for (int i = 0; i < max_lines; i++) {
        lines[i] = NULL;
    }

    // Allocate line widths array if needed
    int *line_widths = NULL;
    if (ret_line_widths) {
        line_widths = (int *)malloc(max_lines * sizeof(int));
        if (!line_widths) {
            free(lines);
            return ESP_ERR_NO_MEM;
        }
        // Initialize widths
        for (int i = 0; i < max_lines; i++) {
            line_widths[i] = 0;
        }
    }

    int line_count = 0;

    if (font_info->long_mode == GFX_LABEL_LONG_WRAP) {
        // Word wrap mode - break text into multiple lines that fit within width
        const char *line_start = text;
        while (*line_start && line_count < max_lines) {
            const char *line_end = line_start;
            int line_width = 0;
            const char *last_space = NULL;

            // Find the end of current line
            while (*line_end) {
                FT_UInt glyph_index;
                uint8_t c = (uint8_t) * line_end;
                int bytes_in_char = 1;
                int char_width = 0;

                // Handle UTF-8 encoding
                if (c < 0x80) {
                    glyph_index = FT_Get_Char_Index(face, c);
                } else if ((c & 0xE0) == 0xC0) {
                    bytes_in_char = 2;
                    if (*(line_end + 1) == 0) {
                        break;
                    }
                    glyph_index = FT_Get_Char_Index(face, ((c & 0x1F) << 6) | (*(line_end + 1) & 0x3F));
                } else if ((c & 0xF0) == 0xE0) {
                    bytes_in_char = 3;
                    if (*(line_end + 1) == 0 || *(line_end + 2) == 0) {
                        break;
                    }
                    glyph_index = FT_Get_Char_Index(face, ((c & 0x0F) << 12) | ((*(line_end + 1) & 0x3F) << 6) | (*(line_end + 2) & 0x3F));
                } else if ((c & 0xF8) == 0xF0) {
                    bytes_in_char = 4;
                    if (*(line_end + 1) == 0 || *(line_end + 2) == 0 || *(line_end + 3) == 0) {
                        break;
                    }
                    glyph_index = FT_Get_Char_Index(face, ((c & 0x07) << 18) | ((*(line_end + 1) & 0x3F) << 12) | ((*(line_end + 2) & 0x3F) << 6) | (*(line_end + 3) & 0x3F));
                } else {
                    glyph_index = 0;
                }

                if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
                    char_width = face->glyph->advance.x >> 6;
                }

                // Check if adding this character would exceed line width
                if (line_width + char_width > obj->width) {
                    // Try to break at last space
                    if (last_space && last_space > line_start) {
                        line_end = last_space;
                    }
                    break;
                }

                line_width += char_width;

                // Remember last space position for word wrapping
                if (c == ' ') {
                    last_space = line_end;
                }

                line_end += bytes_in_char;

                // Handle explicit line breaks
                if (c == '\n') {
                    break;
                }
            }

            // Create line string
            int line_len = line_end - line_start;
            if (line_len > 0) {
                lines[line_count] = (char *)malloc(line_len + 1);
                if (!lines[line_count]) {
                    // Cleanup on error
                    for (int i = 0; i < line_count; i++) {
                        if (lines[i]) {
                            free(lines[i]);
                        }
                    }
                    free(lines);
                    if (line_widths) {
                        free(line_widths);
                    }
                    return ESP_ERR_NO_MEM;
                }
                memcpy(lines[line_count], line_start, line_len);
                lines[line_count][line_len] = '\0';

                // Calculate and store line width if needed
                if (line_widths) {
                    line_widths[line_count] = line_width;
                }

                line_count++;
            }

            // Move to next line
            line_start = line_end;
            if (*line_start == ' ' || *line_start == '\n') {
                line_start++; // Skip space or newline
            }
        }
    } else {
        // No word wrap - split only on explicit newlines
        const char *line_start = text;
        const char *line_end = text;

        while (*line_end && line_count < max_lines) {
            if (*line_end == '\n' || *(line_end + 1) == '\0') {
                int line_len = line_end - line_start;
                if (*line_end != '\n') {
                    line_len++;    // Include last character if not newline
                }

                if (line_len > 0) {
                    lines[line_count] = (char *)malloc(line_len + 1);
                    if (!lines[line_count]) {
                        // Cleanup on error
                        for (int i = 0; i < line_count; i++) {
                            if (lines[i]) {
                                free(lines[i]);
                            }
                        }
                        free(lines);
                        if (line_widths) {
                            free(line_widths);
                        }
                        return ESP_ERR_NO_MEM;
                    }
                    memcpy(lines[line_count], line_start, line_len);
                    lines[line_count][line_len] = '\0';

                    // Calculate line width if needed
                    if (line_widths) {
                        int current_line_width = 0;
                        const char *p_calc = lines[line_count];
                        while (*p_calc) {
                            FT_UInt glyph_index;
                            uint8_t c = (uint8_t) * p_calc;
                            int bytes_in_char = 1;

                            // Handle UTF-8 encoding
                            if (c < 0x80) {
                                glyph_index = FT_Get_Char_Index(face, c);
                            } else if ((c & 0xE0) == 0xC0) {
                                bytes_in_char = 2;
                                if (*(p_calc + 1) == 0) {
                                    break;
                                }
                                glyph_index = FT_Get_Char_Index(face, ((c & 0x1F) << 6) | (*(p_calc + 1) & 0x3F));
                            } else if ((c & 0xF0) == 0xE0) {
                                bytes_in_char = 3;
                                if (*(p_calc + 1) == 0 || *(p_calc + 2) == 0) {
                                    break;
                                }
                                glyph_index = FT_Get_Char_Index(face, ((c & 0x0F) << 12) | ((*(p_calc + 1) & 0x3F) << 6) | (*(p_calc + 2) & 0x3F));
                            } else if ((c & 0xF8) == 0xF0) {
                                bytes_in_char = 4;
                                if (*(p_calc + 1) == 0 || *(p_calc + 2) == 0 || *(p_calc + 3) == 0) {
                                    break;
                                }
                                glyph_index = FT_Get_Char_Index(face, ((c & 0x07) << 18) | ((*(p_calc + 1) & 0x3F) << 12) | ((*(p_calc + 2) & 0x3F) << 6) | (*(p_calc + 3) & 0x3F));
                            } else {
                                glyph_index = 0;
                            }

                            if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
                                current_line_width += face->glyph->advance.x >> 6;
                            }

                            p_calc += bytes_in_char;
                        }
                        line_widths[line_count] = current_line_width;
                    }

                    line_count++;
                }

                line_start = line_end + 1;
            }
            line_end++;
        }
    }

    *ret_lines = lines;
    *ret_line_count = line_count;

    if (ret_line_widths) {
        *ret_line_widths = line_widths;
    }

    return ESP_OK;
}

esp_err_t gfx_get_glphy_dsc(gfx_obj_t * obj)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    esp_err_t ret = ESP_OK;
    FT_Error error;

    char **lines = NULL;
    int line_count = 0;
    int *line_widths = NULL;
    gfx_opa_t *mask_buf = NULL;

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    FT_Face face = (FT_Face)font_info->face;
    if(!face) {
        return ESP_ERR_INVALID_STATE;
    }

    error = FT_Set_Pixel_Sizes(face, 0, font_info->font_size);
    ESP_GOTO_ON_FALSE(!error, ESP_ERR_INVALID_STATE, err, TAG, "error setting font size");

    // Optimization for scroll mode: check if we can reuse cached data
    bool can_use_cached = false;
    if (font_info->long_mode == GFX_LABEL_LONG_SCROLL &&
            font_info->cached_lines != NULL &&
            font_info->cached_line_widths != NULL &&
            font_info->cached_line_count > 0 &&
            font_info->mask != NULL &&
            !obj->is_dirty &&
            font_info->scroll_dirty) {
        // Only scroll position changed, can reuse cached line data
        can_use_cached = true;
        ESP_LOGD(TAG, "Using cached line data and widths for scroll optimization");
    }

    if (font_info->mask && !obj->is_dirty && !can_use_cached) {
        return ESP_OK;
    }

    // Always free old mask before creating new one (even when using cached lines)
    if (font_info->mask) {
        free(font_info->mask);
        font_info->mask = NULL;
    }

    mask_buf = (gfx_opa_t *)malloc(obj->width * obj->height);
    ESP_RETURN_ON_FALSE(mask_buf, ESP_ERR_NO_MEM, TAG, "no mem for mask_buf");
    gfx_opa_t *mask = (gfx_opa_t *)mask_buf;
    memset(mask, 0x00, obj->height * obj->width);

    int line_height = (face->size->metrics.height >> 6);
    int base_line = -(face->size->metrics.descender >> 6);
    int total_line_height = line_height + font_info->line_spacing;

    if (can_use_cached) {
        // Reuse cached line data, skip to rendering
        lines = font_info->cached_lines;
        line_count = font_info->cached_line_count;
        line_widths = font_info->cached_line_widths;
        ESP_LOGD(TAG, "Reusing %d cached lines for scroll", line_count);

        // Call rendering function with cached data
        esp_err_t render_ret = gfx_render_lines_to_mask(obj, mask, lines, line_count, face, line_height, base_line, total_line_height, line_widths);
        if (render_ret != ESP_OK) {
            free(mask_buf);
            return render_ret;
        }
    } else {
        // Calculate total text width and parse lines
        char **lines = NULL;
        int line_count = 0;
        int total_text_width = 0;
        int *line_widths = NULL;

        esp_err_t parse_ret = gfx_parse_text_lines(obj, face, total_line_height, &lines, &line_count, &total_text_width, &line_widths);
        if (parse_ret != ESP_OK) {
            free(mask_buf);
            return parse_ret;
        }

        font_info->text_width = total_text_width;

        // Cache the parsed lines for scroll optimization (only in scroll mode)
        if (font_info->long_mode == GFX_LABEL_LONG_SCROLL) {
            // Clear old cached data first
            gfx_label_clear_cached_lines(font_info);

            // Cache the new line data
            if (line_count > 0) {
                font_info->cached_lines = (char **)malloc(line_count * sizeof(char *));
                font_info->cached_line_widths = (int *)malloc(line_count * sizeof(int));
                if (font_info->cached_lines && font_info->cached_line_widths) {
                    font_info->cached_line_count = line_count;
                    for (int i = 0; i < line_count; i++) {
                        if (lines[i]) {
                            size_t len = strlen(lines[i]) + 1;
                            font_info->cached_lines[i] = (char *)malloc(len);
                            if (font_info->cached_lines[i]) {
                                strcpy(font_info->cached_lines[i], lines[i]);
                            }
                        } else {
                            font_info->cached_lines[i] = NULL;
                        }
                        font_info->cached_line_widths[i] = line_widths[i];
                    }
                    ESP_LOGD(TAG, "Cached %d lines with widths for scroll optimization", line_count);
                }
            }
        }

        // Call rendering function with parsed data
        esp_err_t render_ret = gfx_render_lines_to_mask(obj, mask, lines, line_count, face, line_height, base_line, total_line_height, line_widths);
        if (render_ret != ESP_OK) {
            // Cleanup on error
            for (int i = 0; i < line_count; i++) {
                if (lines[i]) {
                    free(lines[i]);
                }
            }
            free(lines);
            if (line_widths) {
                free(line_widths);
            }
            free(mask_buf);
            return render_ret;
        }

        // Cleanup parsed lines (we've cached them if needed)
        for (int i = 0; i < line_count; i++) {
            if (lines[i]) {
                free(lines[i]);
            }
        }
        free(lines);
        if (line_widths) {
            free(line_widths);
        }
    }

    font_info->mask = mask;
    obj->is_dirty = false;
    font_info->scroll_dirty = false;  // Clear scroll dirty flag after rendering

    // Auto start scrolling if enabled and text width exceeds object width
    if (font_info->long_mode == GFX_LABEL_LONG_SCROLL && font_info->text_width > obj->width) {
        if (!font_info->scroll_active) {
            font_info->scroll_active = true;
            if (font_info->scroll_timer) {
                gfx_timer_reset(font_info->scroll_timer);
                gfx_timer_resume(font_info->scroll_timer);
                ESP_LOGI(TAG, "auto started scroll: text_width=%ld, obj_width=%d", font_info->text_width, obj->width);
            }
        }
    } else if (font_info->scroll_active) {
        // Stop scrolling if mode changed or text now fits within width
        font_info->scroll_active = false;
        if (font_info->scroll_timer) {
            gfx_timer_pause(font_info->scroll_timer);
        }
        font_info->scroll_offset = 0; // Reset offset
        ESP_LOGI(TAG, "auto stopped scroll: text fits in width or mode changed");
    }

    return ESP_OK;

err:
    // Cleanup lines on error (don't free cached lines)
    if (!can_use_cached && lines) {
        for (int i = 0; i < line_count; i++) {
            if (lines[i]) {
                free(lines[i]);
            }
        }
        free(lines);
    }
    if (mask_buf) {
        free(mask_buf);
    }
    return ret;
}

/**
 * @brief Blend label object to destination buffer
 *
 * @param obj Graphics object containing label data
 * @param x1 Left boundary of destination area
 * @param y1 Top boundary of destination area
 * @param x2 Right boundary of destination area
 * @param y2 Bottom boundary of destination area
 * @param dest_buf Destination buffer for blending
 */
esp_err_t gfx_draw_label(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_property_t *font_info = (gfx_label_property_t *)obj->src;
    ESP_RETURN_ON_FALSE(font_info->text, ESP_ERR_INVALID_ARG, TAG, "Text is NULL");

    // Get parent container dimensions for alignment calculation
    uint32_t parent_width, parent_height;
    if (obj->parent_handle != NULL) {
        esp_err_t ret = gfx_emote_get_screen_size(obj->parent_handle, &parent_width, &parent_height);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get screen size, using defaults");
            parent_width = DEFAULT_SCREEN_WIDTH;
            parent_height = DEFAULT_SCREEN_HEIGHT;
        }
    } else {
        parent_width = DEFAULT_SCREEN_WIDTH;
        parent_height = DEFAULT_SCREEN_HEIGHT;
    }

    gfx_coord_t obj_x = obj->x;
    gfx_coord_t obj_y = obj->y;

    gfx_obj_calculate_aligned_position(obj, parent_width, parent_height, &obj_x, &obj_y);

    gfx_area_t clip_region;
    clip_region.x1 = MAX(x1, obj_x);
    clip_region.y1 = MAX(y1, obj_y);
    clip_region.x2 = MIN(x2, obj_x + obj->width);
    clip_region.y2 = MIN(y2, obj_y + obj->height);

    // Check if there's any overlap
    if (clip_region.x1 >= clip_region.x2 || clip_region.y1 >= clip_region.y2) {
        return ESP_ERR_INVALID_STATE;
    }

    // Draw background if enabled
    if (font_info->bg_enable) {
        gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf;
        gfx_coord_t buffer_width = (x2 - x1);
        gfx_color_t bg_color = font_info->bg_color;

        if (swap) {
            bg_color.full = __builtin_bswap16(bg_color.full);
        }

        // Fill background area
        for (int y = clip_region.y1; y < clip_region.y2; y++) {
            for (int x = clip_region.x1; x < clip_region.x2; x++) {
                int pixel_index = (y - y1) * buffer_width + (x - x1);
                dest_pixels[pixel_index] = bg_color;
            }
        }
    }

    gfx_get_glphy_dsc(obj);
    if(!font_info->mask) {
        return ESP_ERR_INVALID_STATE;
    }

    gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf + (clip_region.y1 - y1) * (x2 - x1) + (clip_region.x1 - x1);
    gfx_coord_t dest_buffer_stride = (x2 - x1);
    gfx_coord_t mask_offset_y = (clip_region.y1 - obj_y);

    gfx_opa_t *mask = font_info->mask;
    gfx_coord_t mask_stride = obj->width;
    mask += mask_offset_y * mask_stride;

    gfx_color_t color = font_info->color;
    if (swap) {
        color.full = __builtin_bswap16(color.full);
    }

    gfx_sw_blend_draw(dest_pixels, dest_buffer_stride, color, font_info->opa, mask, &clip_region, mask_stride, swap);

    return ESP_OK;
}
