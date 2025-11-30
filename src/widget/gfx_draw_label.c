/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "common/gfx_comm.h"
#include "core/gfx_blend_priv.h"
#include "core/gfx_core_priv.h"
#include "core/gfx_refr_priv.h"

#include "widget/gfx_label.h"
#include "widget/gfx_font_priv.h"

static const char *TAG = "gfx_label";

/**********************
 *      TYPEDEFS
 **********************/

/* Label context structure */
typedef struct {
    /* Text properties */
    struct {
        char *text;                     /**< Text string */
        gfx_label_long_mode_t long_mode; /**< Long text handling mode */
        uint16_t line_spacing;          /**< Spacing between lines */
    } text;

    /* Style properties */
    struct {
        gfx_color_t color;              /**< Text color */
        gfx_opa_t opa;                  /**< Text opacity */
        gfx_color_t bg_color;           /**< Background color */
        bool bg_enable;                 /**< Enable background */
        gfx_text_align_t text_align;    /**< Text alignment */
    } style;

    /* Font context */
    struct {
        gfx_font_ctx_t *font_ctx;       /**< Unified font context */
    } font;

    /* Render buffer */
    struct {
        gfx_opa_t *mask;                /**< Text mask buffer */
    } render;

    /* Cached line data for scroll optimization */
    struct {
        char **lines;                   /**< Cached parsed lines */
        int line_count;                 /**< Number of cached lines */
        int *line_widths;               /**< Cached line widths for alignment */
    } cache;

    /* Scroll properties */
    struct {
        int32_t scroll_offset;          /**< Current scroll offset */
        int32_t scroll_step;            /**< Scroll step size per timer tick (default: 1) */
        uint32_t scroll_speed;          /**< Scroll speed in ms per pixel */
        bool scroll_loop;               /**< Enable continuous looping */
        bool scrolling;                 /**< Is currently scrolling */
        bool scroll_changed;            /**< Scroll position changed */
        void *scroll_timer;             /**< Timer handle for scroll animation */
        int32_t text_width;             /**< Total text width */
    } scroll;

    /* Snap scroll properties */
    struct {
        uint32_t snap_interval;         /**< Snap interval time in ms (time to display each section) */
        int32_t snap_offset;            /**< Snap offset in pixels (auto-calculated as obj->geometry.width) */
        bool snap_loop;                 /**< Enable continuous snap looping */
        void *snap_timer;               /**< Timer handle for snap animation */
    } snap;
} gfx_label_t;

/* Use generic type checking macro from gfx_obj_priv.h */
#define CHECK_OBJ_TYPE_LABEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LABEL, TAG)

/**********************
 *  STATIC PROTOTYPES
 **********************/

/* Forward declarations for virtual functions */
static void gfx_draw_label(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);
static esp_err_t gfx_label_delete(gfx_obj_t *obj);

static esp_err_t gfx_parse_text_lines(gfx_obj_t *obj, void *font_ctx, int total_line_height,
                                      char ***ret_lines, int *ret_line_count, int *ret_text_width, int **ret_line_widths);
static esp_err_t gfx_render_lines_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, char **lines, int line_count,
        void *font_ctx, int line_height, int base_line,
        int total_line_height, int *cached_line_widths);

void gfx_label_clear_cached_lines(gfx_label_t *label)
{
    if (label->cache.lines) {
        for (int i = 0; i < label->cache.line_count; i++) {
            if (label->cache.lines[i]) {
                free(label->cache.lines[i]);
            }
        }
        free(label->cache.lines);
        label->cache.lines = NULL;
        label->cache.line_count = 0;
    }

    if (label->cache.line_widths) {
        free(label->cache.line_widths);
        label->cache.line_widths = NULL;
    }
}

/**
 * @brief Convert UTF-8 string to Unicode code point for LVGL font processing
 * @param p Pointer to the current position in the string (updated after conversion)
 * @param unicode Pointer to store the Unicode code point
 * @return Number of bytes consumed from the string, or 0 on error
 */
static int gfx_utf8_to_unicode(const char **p, uint32_t *unicode)
{
    const char *ptr = *p;
    uint8_t c = (uint8_t) * ptr;
    int bytes_in_char = 1;

    if (c < 0x80) {
        *unicode = c;
    } else if ((c & 0xE0) == 0xC0) {
        bytes_in_char = 2;
        if (*(ptr + 1) == 0) {
            return 0;
        }
        *unicode = ((c & 0x1F) << 6) | (*(ptr + 1) & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        bytes_in_char = 3;
        if (*(ptr + 1) == 0 || *(ptr + 2) == 0) {
            return 0;
        }
        *unicode = ((c & 0x0F) << 12) | ((*(ptr + 1) & 0x3F) << 6) | (*(ptr + 2) & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        bytes_in_char = 4;
        if (*(ptr + 1) == 0 || *(ptr + 2) == 0 || *(ptr + 3) == 0) {
            return 0;
        }
        *unicode = ((c & 0x07) << 18) | ((*(ptr + 1) & 0x3F) << 12) |
                   ((*(ptr + 2) & 0x3F) << 6) | (*(ptr + 3) & 0x3F);
    } else {
        *unicode = 0xFFFD;
        bytes_in_char = 1;
    }

    *p += bytes_in_char;
    return bytes_in_char;
}

static void gfx_label_scroll_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    if (!obj || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (!label || !label->scroll.scrolling || label->text.long_mode != GFX_LABEL_LONG_SCROLL) {
        return;
    }

    label->scroll.scroll_offset += label->scroll.scroll_step;

    if (label->scroll.scroll_loop) {
        if (label->scroll.scroll_offset > label->scroll.text_width) {
            label->scroll.scroll_offset = -obj->geometry.width;
        }
    } else {
        if (label->scroll.scroll_offset > label->scroll.text_width) {
            label->scroll.scrolling = false;
            gfx_timer_pause(label->scroll.scroll_timer);
            return;
        }
    }

    label->scroll.scroll_changed = true;
    gfx_obj_invalidate(obj);
}

/**
 * @brief Calculate snap offset aligned to character/word boundary
 * @param label Label context
 * @param font Font context
 * @param current_offset Current scroll offset
 * @param target_width Target width to display (usually obj->geometry.width)
 * @return Snap offset aligned to character/word boundary
 */
static int32_t gfx_calculate_snap_offset(gfx_label_t *label, gfx_font_ctx_t *font,
        int32_t current_offset, int32_t target_width)
{
    if (!label->text.text || !font) {
        return target_width;
    }

    const char *text = label->text.text;
    int accumulated_width = 0;
    const char *p = text;

    /* Skip characters until we reach current_offset */
    while (*p && accumulated_width < current_offset) {
        uint32_t unicode = 0;
        int bytes_in_char = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_in_char == 0) {
            p++;
            continue;
        }

        int char_width = font->get_glyph_width(font, unicode);
        accumulated_width += char_width;
    }

    /* Reset for calculating snap offset from current position */
    int section_width = 0;
    int last_valid_width = 0;
    int last_space_width = 0;  /* Width at last space (word boundary) */

    /* Calculate how many complete characters fit in target_width */
    while (*p) {
        uint32_t unicode = 0;
        const char *p_before = p;
        int bytes_in_char = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_in_char == 0) {
            p++;
            continue;
        }

        if (*p_before == '\n') {
            break;
        }

        uint8_t c = (uint8_t) * p_before;
        int char_width = font->get_glyph_width(font, unicode);

        /* Check if adding this character would exceed target_width */
        if (section_width + char_width > target_width) {
            /* Prefer to break at word boundary (space) if available */
            if (last_space_width > 0) {
                last_valid_width = last_space_width;
            }
            /* Otherwise use last complete character */
            break;
        }

        section_width += char_width;
        last_valid_width = section_width;

        /* Record position after space for word boundary */
        if (c == ' ') {
            last_space_width = section_width;
        }
    }

    /* Return the width of complete characters that fit */
    return last_valid_width > 0 ? last_valid_width : target_width;
}

static void gfx_label_snap_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    if (!obj || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (!label || label->text.long_mode != GFX_LABEL_LONG_SCROLL_SNAP) {
        return;
    }

    gfx_font_ctx_t *font = (gfx_font_ctx_t *)label->font.font_ctx;
    if (!font) {
        return;
    }

    /* Calculate snap offset aligned to character boundary */
    int32_t aligned_offset = gfx_calculate_snap_offset(label, font, label->scroll.scroll_offset, obj->geometry.width);

    /* If no valid offset found, use default */
    if (aligned_offset == 0) {
        aligned_offset = obj->geometry.width;
    }

    /* Jump to next section */
    label->scroll.scroll_offset += aligned_offset;
    // ESP_LOGI(TAG, "aligned_offset: %d, text_width: %d, scroll_offset: %d",
    //          label->scroll.scroll_offset - aligned_offset, label->scroll.text_width, label->scroll.scroll_offset);

    /* Handle looping */
    if (label->snap.snap_loop) {
        if (label->scroll.scroll_offset >= label->scroll.text_width) {
            label->scroll.scroll_offset = 0;
        }
    } else {
        if (label->scroll.scroll_offset >= label->scroll.text_width) {
            label->scroll.scroll_offset = label->scroll.text_width - obj->geometry.width;
            if (label->scroll.scroll_offset < 0) {
                label->scroll.scroll_offset = 0;
            }
            gfx_timer_pause(label->snap.snap_timer);
        }
    }

    /* Trigger redraw */
    gfx_obj_invalidate(obj);
}

/*=====================
 * Label object creation
 *====================*/

gfx_obj_t *gfx_label_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "No mem for label object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->type = GFX_OBJ_TYPE_LABEL;
    obj->parent_handle = handle;
    obj->state.is_visible = true;
    obj->vfunc.draw = (gfx_obj_draw_fn_t)gfx_draw_label;
    obj->vfunc.delete = gfx_label_delete;
    gfx_obj_invalidate(obj);

    gfx_label_t *label = (gfx_label_t *)malloc(sizeof(gfx_label_t));
    if (label == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for label object");
        free(obj);
        return NULL;
    }
    memset(label, 0, sizeof(gfx_label_t));

    label->style.opa = 0xFF;
    label->render.mask = NULL;
    label->style.bg_color = (gfx_color_t) {
        .full = 0x0000
    };
    label->style.bg_enable = false;
    label->style.text_align = GFX_TEXT_ALIGN_LEFT;
    label->text.long_mode = GFX_LABEL_LONG_CLIP;
    label->text.line_spacing = 2;

    label->scroll.scroll_offset = 0;
    label->scroll.scroll_step = 1;
    label->scroll.scroll_speed = 50;
    label->scroll.scroll_loop = true;
    label->scroll.scrolling = false;
    label->scroll.scroll_changed = false;
    label->scroll.scroll_timer = NULL;
    label->scroll.text_width = 0;

    label->snap.snap_interval = 2000;  /* Default 2000ms per section */
    label->snap.snap_offset = 0;       /* Will be auto-calculated as obj->geometry.width */
    label->snap.snap_loop = true;
    label->snap.snap_timer = NULL;

    label->cache.lines = NULL;
    label->cache.line_count = 0;
    label->cache.line_widths = NULL;

    obj->src = label;

    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_LABEL, obj);
    ESP_LOGD(TAG, "Created label object with default font config");
    return obj;
}

/*=====================
 * Label setter functions
 *====================*/

esp_err_t gfx_label_set_font(gfx_obj_t *obj, gfx_font_t font)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    gfx_label_t *label = (gfx_label_t *)obj->src;

    if (label->font.font_ctx) {
        free(label->font.font_ctx);
        label->font.font_ctx = NULL;
    }

    if (font) {
        gfx_font_ctx_t *font_ctx = (gfx_font_ctx_t *)calloc(1, sizeof(gfx_font_ctx_t));
        if (font_ctx) {
            if (gfx_is_lvgl_font(font)) {
                gfx_font_lv_init_context(font_ctx, font);
            } else {
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
                gfx_font_ft_init_context(font_ctx, font);
#else
                ESP_LOGW(TAG, "FreeType font detected but support is not enabled");
                free(font_ctx);
                font_ctx = NULL;
#endif
            }

            label->font.font_ctx = font_ctx;
        } else {
            ESP_LOGW(TAG, "Failed to allocate unified font interface");
        }
    }

    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_label_set_text(gfx_obj_t *obj, const char *text)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;

    if (text == NULL) {
        text = label->text.text;
    }

    if (label->text.text == text) {
        label->text.text = realloc(label->text.text, strlen(label->text.text) + 1);
        assert(label->text.text);
        if (label->text.text == NULL) {
            return ESP_FAIL;
        }
    } else {
        if (label->text.text != NULL) {
            free(label->text.text);
            label->text.text = NULL;
        }

        size_t len = strlen(text) + 1;

        label->text.text = malloc(len);
        assert(label->text.text);
        if (label->text.text == NULL) {
            return ESP_FAIL;
        }
        strcpy(label->text.text, text);
    }


    gfx_label_clear_cached_lines(label);

    /* Reset scroll state for smooth scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL) {
        if (label->scroll.scrolling) {
            label->scroll.scrolling = false;
            if (label->scroll.scroll_timer) {
                gfx_timer_pause(label->scroll.scroll_timer);
            }
        }
        label->scroll.scroll_offset = 0;
        label->scroll.text_width = 0;
    }

    /* Reset scroll state for snap mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        if (label->snap.snap_timer) {
            gfx_timer_pause(label->snap.snap_timer);
        }
        label->scroll.scroll_offset = 0;
        label->scroll.text_width = 0;
    }

    label->scroll.scroll_changed = false;
    gfx_obj_invalidate(obj);

    return ESP_OK;
}

esp_err_t gfx_label_set_text_fmt(gfx_obj_t *obj, const char *fmt, ...)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    ESP_RETURN_ON_FALSE(fmt, ESP_ERR_INVALID_ARG, TAG, "Format string is NULL");

    gfx_label_t *label = (gfx_label_t *)obj->src;

    if (label->text.text != NULL) {
        free(label->text.text);
        label->text.text = NULL;
    }

    va_list args;
    va_start(args, fmt);

    /*Allocate space for the new text by using trick from C99 standard section 7.19.6.12*/
    va_list args_copy;
    va_copy(args_copy, args);
    uint32_t len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    label->text.text = malloc(len + 1);
    if (label->text.text == NULL) {
        va_end(args);
        return ESP_ERR_NO_MEM;
    }
    label->text.text[len] = '\0';

    vsnprintf(label->text.text, len + 1, fmt, args);
    va_end(args);

    gfx_obj_invalidate(obj);

    return ESP_OK;
}

esp_err_t gfx_label_set_opa(gfx_obj_t *obj, gfx_opa_t opa)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.opa = opa;
    ESP_LOGD(TAG, "set font opa: %d", label->style.opa);

    return ESP_OK;
}

esp_err_t gfx_label_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.color = color;
    ESP_LOGD(TAG, "set font color: %d", label->style.color.full);

    return ESP_OK;
}

esp_err_t gfx_label_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.bg_color = bg_color;
    ESP_LOGD(TAG, "set background color: %d", label->style.bg_color.full);

    return ESP_OK;
}

esp_err_t gfx_label_set_bg_enable(gfx_obj_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.bg_enable = enable;
    gfx_obj_invalidate(obj);
    ESP_LOGD(TAG, "set background enable: %s", enable ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t gfx_label_set_text_align(gfx_obj_t *obj, gfx_text_align_t align)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.text_align = align;
    gfx_obj_invalidate(obj);
    ESP_LOGD(TAG, "set text align: %d", align);

    return ESP_OK;
}

esp_err_t gfx_label_set_long_mode(gfx_obj_t *obj, gfx_label_long_mode_t long_mode)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    gfx_label_long_mode_t old_mode = label->text.long_mode;
    label->text.long_mode = long_mode;

    if (old_mode != long_mode) {
        /* Stop smooth scrolling if switching from scroll mode */
        if (label->scroll.scrolling) {
            label->scroll.scrolling = false;
            if (label->scroll.scroll_timer) {
                gfx_timer_pause(label->scroll.scroll_timer);
            }
        }

        /* Stop snap scrolling if switching from snap mode */
        if (old_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->snap.snap_timer) {
            gfx_timer_pause(label->snap.snap_timer);
        }

        label->scroll.scroll_offset = 0;
        label->scroll.text_width = 0;

        /* Handle smooth scroll timer */
        if (long_mode == GFX_LABEL_LONG_SCROLL && !label->scroll.scroll_timer) {
            label->scroll.scroll_timer = gfx_timer_create(obj->parent_handle,
                                         gfx_label_scroll_timer_callback,
                                         label->scroll.scroll_speed,
                                         obj);
            if (label->scroll.scroll_timer) {
                gfx_timer_set_repeat_count(label->scroll.scroll_timer, -1);
            }
        } else if (long_mode != GFX_LABEL_LONG_SCROLL && label->scroll.scroll_timer) {
            gfx_timer_delete(obj->parent_handle, label->scroll.scroll_timer);
            label->scroll.scroll_timer = NULL;
        }

        /* Handle snap scroll timer */
        if (long_mode == GFX_LABEL_LONG_SCROLL_SNAP && !label->snap.snap_timer) {
            label->snap.snap_timer = gfx_timer_create(obj->parent_handle,
                                     gfx_label_snap_timer_callback,
                                     label->snap.snap_interval,
                                     obj);
            if (label->snap.snap_timer) {
                gfx_timer_set_repeat_count(label->snap.snap_timer, -1);
            }
        } else if (long_mode != GFX_LABEL_LONG_SCROLL_SNAP && label->snap.snap_timer) {
            gfx_timer_delete(obj->parent_handle, label->snap.snap_timer);
            label->snap.snap_timer = NULL;
        }

        gfx_obj_invalidate(obj);
    }

    label->scroll.scroll_changed = false;
    ESP_LOGD(TAG, "set long mode: %d", long_mode);
    return ESP_OK;
}

esp_err_t gfx_label_set_line_spacing(gfx_obj_t *obj, uint16_t spacing)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->text.line_spacing = spacing;
    gfx_obj_invalidate(obj);
    ESP_LOGD(TAG, "set line spacing: %d", spacing);

    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_speed(gfx_obj_t *obj, uint32_t speed_ms)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    ESP_RETURN_ON_FALSE(speed_ms > 0, ESP_ERR_INVALID_ARG, TAG, "invalid speed");

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->scroll.scroll_speed = speed_ms;

    if (label->scroll.scroll_timer) {
        gfx_timer_set_period(label->scroll.scroll_timer, speed_ms);
    }

    ESP_LOGD(TAG, "set scroll speed: %"PRIu32" ms", speed_ms);
    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_loop(gfx_obj_t *obj, bool loop)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->scroll.scroll_loop = loop;
    ESP_LOGD(TAG, "set scroll loop: %s", loop ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_step(gfx_obj_t *obj, int32_t step)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");
    ESP_RETURN_ON_FALSE(step != 0, ESP_ERR_INVALID_ARG, TAG, "scroll step cannot be zero");

    label->scroll.scroll_step = step;
    ESP_LOGD(TAG, "set scroll step: %"PRId32, step);
    return ESP_OK;
}

esp_err_t gfx_label_set_snap_interval(gfx_obj_t *obj, uint32_t interval_ms)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    ESP_RETURN_ON_FALSE(interval_ms > 0, ESP_ERR_INVALID_ARG, TAG, "invalid snap interval");

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->snap.snap_interval = interval_ms;

    if (label->snap.snap_timer) {
        gfx_timer_set_period(label->snap.snap_timer, interval_ms);
    }

    ESP_LOGD(TAG, "set snap interval: %"PRIu32" ms", interval_ms);
    return ESP_OK;
}

esp_err_t gfx_label_set_snap_loop(gfx_obj_t *obj, bool loop)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->snap.snap_loop = loop;
    ESP_LOGD(TAG, "set snap loop: %s", loop ? "enabled" : "disabled");

    return ESP_OK;
}

static esp_err_t gfx_parse_text_lines(gfx_obj_t *obj, void *font_ctx, int total_line_height,
                                      char ***ret_lines, int *ret_line_count, int *ret_text_width, int **ret_line_widths)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;

    int total_text_width = 0;
    const char *p_width = label->text.text;

    while (*p_width) {
        uint32_t unicode = 0;
        int bytes_in_char = gfx_utf8_to_unicode(&p_width, &unicode);
        if (bytes_in_char == 0) {
            p_width++;
            continue;
        }

        gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
        int glyph_width = font->get_glyph_width(font, unicode);
        total_text_width += glyph_width;

        if (*(p_width - bytes_in_char) == '\n') {
            break;
        }
    }

    *ret_text_width = total_text_width;

    const char *text = label->text.text;
    int max_lines = obj->geometry.height / total_line_height;
    if (max_lines <= 0) {
        max_lines = 1;
    }

    char **lines = (char **)malloc(max_lines * sizeof(char *));
    if (!lines) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < max_lines; i++) {
        lines[i] = NULL;
    }

    int *line_widths = NULL;
    if (ret_line_widths) {
        line_widths = (int *)malloc(max_lines * sizeof(int));
        if (!line_widths) {
            free(lines);
            return ESP_ERR_NO_MEM;
        }
        for (int i = 0; i < max_lines; i++) {
            line_widths[i] = 0;
        }
    }

    int line_count = 0;

    if (label->text.long_mode == GFX_LABEL_LONG_WRAP) {
        const char *line_start = text;
        while (*line_start && line_count < max_lines) {
            const char *line_end = line_start;
            int line_width = 0;
            const char *last_space = NULL;

            while (*line_end) {
                uint32_t unicode = 0;
                uint8_t c = (uint8_t) * line_end;
                int bytes_in_char;
                int char_width = 0;

                bytes_in_char = gfx_utf8_to_unicode(&line_end, &unicode);
                if (bytes_in_char == 0) {
                    break;
                }

                gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
                char_width = font->get_glyph_width(font, unicode);

                if (line_width + char_width > obj->geometry.width) {
                    if (last_space && last_space > line_start) {
                        line_end = last_space;
                    } else {
                        line_end -= bytes_in_char;
                    }
                    break;
                }

                line_width += char_width;

                if (c == ' ') {
                    last_space = line_end - bytes_in_char;
                }

                if (c == '\n') {
                    break;
                }
            }

            int line_len = line_end - line_start;
            if (line_len > 0) {
                lines[line_count] = (char *)malloc(line_len + 1);
                if (!lines[line_count]) {
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

                if (line_widths) {
                    line_widths[line_count] = line_width;
                }

                line_count++;
            }

            line_start = line_end;
            if (*line_start == ' ' || *line_start == '\n') {
                line_start++;
            }
        }
    } else {
        const char *line_start = text;
        const char *line_end = text;

        while (*line_end && line_count < max_lines) {
            if (*line_end == '\n' || *(line_end + 1) == '\0') {
                int line_len = line_end - line_start;
                if (*line_end != '\n') {
                    line_len++;
                }

                if (line_len > 0) {
                    lines[line_count] = (char *)malloc(line_len + 1);
                    if (!lines[line_count]) {
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

                    if (line_widths) {
                        int current_line_width = 0;
                        const char *p_calc = lines[line_count];
                        while (*p_calc) {
                            uint32_t unicode = 0;
                            int bytes_consumed = gfx_utf8_to_unicode(&p_calc, &unicode);
                            if (bytes_consumed == 0) {
                                p_calc++;
                                continue;
                            }

                            gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
                            int glyph_width = font->get_glyph_width(font, unicode);
                            current_line_width += glyph_width;
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

static int gfx_calculate_line_width(const char *line_text, gfx_font_ctx_t *font)
{
    int line_width = 0;
    const char *p = line_text;

    while (*p) {
        uint32_t unicode = 0;
        int bytes_consumed = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_consumed == 0) {
            p++;
            continue;
        }

        line_width += font->get_glyph_width(font, unicode);
    }

    return line_width;
}

static int gfx_cal_text_start_x(gfx_text_align_t align, int obj_width, int line_width)
{
    int start_x = 0;

    switch (align) {
    case GFX_TEXT_ALIGN_LEFT:
    case GFX_TEXT_ALIGN_AUTO:
        start_x = 0;
        break;
    case GFX_TEXT_ALIGN_CENTER:
        start_x = (obj_width - line_width) / 2;
        break;
    case GFX_TEXT_ALIGN_RIGHT:
        start_x = obj_width - line_width;
        break;
    }

    return start_x < 0 ? 0 : start_x;
}

static void gfx_render_glyph_to_mask(gfx_opa_t *mask, int obj_width, int obj_height,
                                     gfx_font_ctx_t *font, uint32_t unicode,
                                     const gfx_glyph_dsc_t *glyph_dsc,
                                     const uint8_t *glyph_bitmap, int x, int y)
{
    int ofs_x = glyph_dsc->ofs_x;
    int ofs_y = font->adjust_baseline_offset(font, (void *)glyph_dsc);

    for (int32_t iy = 0; iy < glyph_dsc->box_h; iy++) {
        for (int32_t ix = 0; ix < glyph_dsc->box_w; ix++) {
            int32_t pixel_x = ix + x + ofs_x;
            int32_t pixel_y = iy + y + ofs_y;

            if (pixel_x >= 0 && pixel_x < obj_width && pixel_y >= 0 && pixel_y < obj_height) {
                uint8_t pixel_value = font->get_pixel_value(font, glyph_bitmap, ix, iy, glyph_dsc->box_w);
                *(mask + pixel_y * obj_width + pixel_x) = pixel_value;
            }
        }
    }
}

static esp_err_t gfx_render_line_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, const char *line_text,
        gfx_font_ctx_t *font, int line_width, int y_pos)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;

    int start_x = gfx_cal_text_start_x(label->style.text_align, obj->geometry.width, line_width);

    /* Apply scroll offset for both smooth scroll and snap scroll modes */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL && label->scroll.scrolling) {
        start_x -= label->scroll.scroll_offset;
    } else if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        start_x -= label->scroll.scroll_offset;
    }

    /* For snap mode, find the last complete word that fits in viewport */
    const char *render_end = NULL;
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        int scan_x = start_x;
        const char *p_scan = line_text;
        const char *last_space_ptr = NULL;
        const char *last_valid_ptr = NULL;

        while (*p_scan) {
            uint32_t unicode = 0;
            const char *p_before = p_scan;
            int bytes_consumed = gfx_utf8_to_unicode(&p_scan, &unicode);
            if (bytes_consumed == 0) {
                p_scan++;
                continue;
            }

            uint8_t c = (uint8_t) * p_before;
            gfx_glyph_dsc_t glyph_dsc;
            if (font->get_glyph_dsc(font, &glyph_dsc, unicode, 0)) {
                int char_width = font->get_advance_width(font, &glyph_dsc);

                /* Check if this character would go beyond viewport */
                if (scan_x + char_width > obj->geometry.width) {
                    /* Use last space position if available, otherwise last complete character */
                    render_end = last_space_ptr ? last_space_ptr : last_valid_ptr;
                    break;
                }

                scan_x += char_width;
                last_valid_ptr = p_scan;

                /* Track space positions for word boundary */
                if (c == ' ') {
                    last_space_ptr = p_scan;
                }
            }
        }

        /* If we scanned everything without exceeding, render all */
        if (!render_end) {
            render_end = p_scan;
        }
    }

    int x = start_x;
    const char *p = line_text;

    while (*p) {
        /* In snap mode, stop at calculated end position */
        if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && render_end && p >= render_end) {
            break;
        }

        uint32_t unicode = 0;
        int bytes_consumed = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_consumed == 0) {
            p++;
            continue;
        }

        gfx_glyph_dsc_t glyph_dsc;
        const uint8_t *glyph_bitmap = NULL;

        if (!font->get_glyph_dsc(font, &glyph_dsc, unicode, 0)) {
            continue;
        }

        glyph_bitmap = font->get_glyph_bitmap(font, unicode, &glyph_dsc);
        if (!glyph_bitmap) {
            continue;
        }

        gfx_render_glyph_to_mask(mask, obj->geometry.width, obj->geometry.height, font, unicode,
                                 &glyph_dsc, glyph_bitmap, x, y_pos);

        x += font->get_advance_width(font, &glyph_dsc);

        if (x >= obj->geometry.width) {
            break;
        }
    }

    return ESP_OK;
}

static esp_err_t gfx_render_lines_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, char **lines, int line_count,
        void *font_ctx, int line_height, int base_line,
        int total_line_height, int *cached_line_widths)
{
    gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
    int current_y = 0;

    for (int line_idx = 0; line_idx < line_count; line_idx++) {
        if (current_y + line_height > obj->geometry.height) {
            break;
        }

        const char *line_text = lines[line_idx];
        int line_width;

        if (cached_line_widths) {
            line_width = cached_line_widths[line_idx];
        } else {
            line_width = gfx_calculate_line_width(line_text, font);
        }

        gfx_render_line_to_mask(obj, mask, line_text, font, line_width, current_y);

        current_y += total_line_height;
    }

    return ESP_OK;
}

static bool gfx_can_use_cached_data(gfx_label_t *label, gfx_obj_t *obj)
{
    return ((label->text.long_mode == GFX_LABEL_LONG_SCROLL) &&
            (label->cache.lines != NULL) &&
            (label->cache.line_widths != NULL) &&
            (label->cache.line_count > 0) &&
            (label->render.mask != NULL) &&
            (label->scroll.scroll_changed == true));
}

static gfx_opa_t *gfx_allocate_mask_buffer(gfx_obj_t *obj, gfx_label_t *label)
{
    if (label->render.mask) {
        free(label->render.mask);
        label->render.mask = NULL;
    }

    gfx_opa_t *mask_buf = (gfx_opa_t *)malloc(obj->geometry.width * obj->geometry.height);
    if (!mask_buf) {
        ESP_LOGE(TAG, "Failed to allocate mask buffer");
        return NULL;
    }

    memset(mask_buf, 0x00, obj->geometry.height * obj->geometry.width);
    return mask_buf;
}

static esp_err_t gfx_cache_line_data(gfx_label_t *label, char **lines,
                                     int line_count, int *line_widths)
{
    if (label->text.long_mode != GFX_LABEL_LONG_SCROLL || line_count <= 0) {
        return ESP_OK;
    }

    gfx_label_clear_cached_lines(label);

    label->cache.lines = (char **)malloc(line_count * sizeof(char *));
    label->cache.line_widths = (int *)malloc(line_count * sizeof(int));

    if (!label->cache.lines || !label->cache.line_widths) {
        ESP_LOGE(TAG, "Failed to allocate cache memory");
        return ESP_ERR_NO_MEM;
    }

    label->cache.line_count = line_count;
    for (int i = 0; i < line_count; i++) {
        if (lines[i]) {
            size_t len = strlen(lines[i]) + 1;
            label->cache.lines[i] = (char *)malloc(len);
            if (label->cache.lines[i]) {
                strcpy(label->cache.lines[i], lines[i]);
            }
        } else {
            label->cache.lines[i] = NULL;
        }
        label->cache.line_widths[i] = line_widths[i];
    }

    ESP_LOGD(TAG, "Cached %d lines with widths for scroll optimization", line_count);
    return ESP_OK;
}

static void gfx_cleanup_line_data(char **lines, int line_count, int *line_widths)
{
    if (lines) {
        for (int i = 0; i < line_count; i++) {
            if (lines[i]) {
                free(lines[i]);
            }
        }
        free(lines);
    }

    if (line_widths) {
        free(line_widths);
    }
}

static void gfx_update_scroll_state(gfx_label_t *label, gfx_obj_t *obj)
{
    /* Handle smooth scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL && label->scroll.text_width > obj->geometry.width) {
        if (!label->scroll.scrolling) {
            label->scroll.scrolling = true;
            if (label->scroll.scroll_timer) {
                gfx_timer_reset(label->scroll.scroll_timer);
                gfx_timer_resume(label->scroll.scroll_timer);
            }
        }
    } else if (label->scroll.scrolling) {
        label->scroll.scrolling = false;
        if (label->scroll.scroll_timer) {
            gfx_timer_pause(label->scroll.scroll_timer);
        }
        label->scroll.scroll_offset = 0;
    }

    /* Handle snap scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->scroll.text_width > obj->geometry.width) {
        /* snap_offset will be dynamically calculated in timer callback based on character boundaries */
        if (label->snap.snap_timer) {
            gfx_timer_reset(label->snap.snap_timer);
            gfx_timer_resume(label->snap.snap_timer);
        }
    } else if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->snap.snap_timer) {
        gfx_timer_pause(label->snap.snap_timer);
        label->scroll.scroll_offset = 0;
    }
}

static esp_err_t gfx_render_from_cache(gfx_obj_t *obj, gfx_opa_t *mask,
                                       gfx_label_t *label, void *font_ctx)
{
    gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
    int line_height = font->get_line_height(font);
    int base_line = font->get_base_line(font);
    int total_line_height = line_height + label->text.line_spacing;

    ESP_LOGD(TAG, "Reusing %d cached lines for scroll", label->cache.line_count);

    return gfx_render_lines_to_mask(obj, mask, label->cache.lines,
                                    label->cache.line_count, font_ctx,
                                    line_height, base_line, total_line_height,
                                    label->cache.line_widths);
}

static esp_err_t gfx_render_from_parsed_data(gfx_obj_t *obj, gfx_opa_t *mask,
        gfx_label_t *label,
        void *font_ctx, gfx_opa_t *mask_buf)
{
    gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
    int line_height = font->get_line_height(font);
    int base_line = font->get_base_line(font);
    int total_line_height = line_height + label->text.line_spacing;

    char **lines = NULL;
    int line_count = 0;
    int *line_widths = NULL;
    int total_text_width = 0;

    esp_err_t parse_ret = gfx_parse_text_lines(obj, font_ctx, total_line_height,
                          &lines, &line_count, &total_text_width, &line_widths);
    if (parse_ret != ESP_OK) {
        free(mask_buf);
        return parse_ret;
    }

    label->scroll.text_width = total_text_width;

    esp_err_t cache_ret = gfx_cache_line_data(label, lines, line_count, line_widths);
    if (cache_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to cache line data, continuing without cache");
    }

    esp_err_t render_ret = gfx_render_lines_to_mask(obj, mask, lines, line_count,
                           font_ctx, line_height, base_line,
                           total_line_height, line_widths);
    if (render_ret != ESP_OK) {
        gfx_cleanup_line_data(lines, line_count, line_widths);
        free(mask_buf);
        return render_ret;
    }

    gfx_cleanup_line_data(lines, line_count, line_widths);
    return ESP_OK;
}

esp_err_t gfx_get_glphy_dsc(gfx_obj_t *obj)
{
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;
    if (font_ctx == NULL) {
        ESP_LOGI(TAG, "font context is NULL");
        return ESP_OK;
    }

    gfx_opa_t *mask_buf = gfx_allocate_mask_buffer(obj, label);
    ESP_RETURN_ON_FALSE(mask_buf, ESP_ERR_NO_MEM, TAG, "no mem for mask_buf");

    esp_err_t render_ret;
    if (true == gfx_can_use_cached_data(label, obj)) {
        render_ret = gfx_render_from_cache(obj, mask_buf, label, font_ctx);
    } else {
        render_ret = gfx_render_from_parsed_data(obj, mask_buf, label, font_ctx, mask_buf);
    }

    if (render_ret != ESP_OK) {
        free(mask_buf);
        return render_ret;
    }

    label->render.mask = mask_buf;
    label->scroll.scroll_changed = false;

    gfx_update_scroll_state(label, obj);

    return ESP_OK;
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
static void gfx_draw_label(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    if (!obj) {
        ESP_LOGE(TAG, "invalid handle");
        return;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (label->text.text == NULL) {
        ESP_LOGI(TAG, "text is NULL");
        return;
    }

    /* Get parent dimensions and calculate aligned position */
    gfx_obj_calc_pos_in_parent(obj);

    /* Calculate clipping area */
    gfx_area_t render_area = {x1, y1, x2, y2};
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + obj->geometry.width, obj->geometry.y + obj->geometry.height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        return;
    }

    if (label->style.bg_enable) {
        gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf;
        gfx_coord_t buffer_width = (x2 - x1);
        gfx_color_t bg_color = label->style.bg_color;

        if (swap) {
            bg_color.full = __builtin_bswap16(bg_color.full);
        }

        for (int y = clip_area.y1; y < clip_area.y2; y++) {
            for (int x = clip_area.x1; x < clip_area.x2; x++) {
                int pixel_index = (y - y1) * buffer_width + (x - x1);
                dest_pixels[pixel_index] = bg_color;
            }
        }
    }

    gfx_get_glphy_dsc(obj);
    if (!label->render.mask) {
        return;
    }

    /* Calculate destination and mask buffer pointers with offset */
    gfx_coord_t dest_stride = (x2 - x1);
    gfx_color_t *dest_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(dest_buf,
                               clip_area.y1 - y1,
                               dest_stride,
                               clip_area.x1 - x1);

    gfx_coord_t mask_stride = obj->geometry.width;
    gfx_opa_t *mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(label->render.mask,
                      clip_area.y1 - obj->geometry.y,
                      mask_stride,
                      clip_area.x1 - obj->geometry.x);

    gfx_color_t color = label->style.color;
    if (swap) {
        color.full = __builtin_bswap16(color.full);
    }

    gfx_sw_blend_draw(dest_pixels, dest_stride, color, label->style.opa, mask, &clip_area, mask_stride, swap);
}

/*=====================
 * Label object deletion
 *====================*/

static esp_err_t gfx_label_delete(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (label) {
        if (label->scroll.scroll_timer) {
            gfx_timer_delete(obj->parent_handle, label->scroll.scroll_timer);
            label->scroll.scroll_timer = NULL;
        }

        if (label->snap.snap_timer) {
            gfx_timer_delete(obj->parent_handle, label->snap.snap_timer);
            label->snap.snap_timer = NULL;
        }

        gfx_label_clear_cached_lines(label);

        if (label->text.text) {
            free(label->text.text);
        }
        if (label->font.font_ctx) {
            free(label->font.font_ctx);
        }
        if (label->render.mask) {
            free(label->render.mask);
        }
        free(label);
    }

    return ESP_OK;
}
