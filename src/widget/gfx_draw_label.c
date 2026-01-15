/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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
#include "widget/gfx_draw_label_priv.h"

static const char *TAG = "draw_label";

/* Use generic type checking macro from gfx_obj_priv.h */
#define CHECK_OBJ_TYPE_LABEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LABEL, TAG)

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_parse_text_lines(gfx_obj_t *obj, int total_line_height,
                                      char ***ret_lines, int *ret_line_count, int *ret_text_width, int **ret_line_widths);
static esp_err_t gfx_render_lines_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, char **lines, int line_count,
        int line_height, int base_line, int total_line_height, int *cached_line_widths);

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
int gfx_utf8_to_unicode(const char **p, uint32_t *unicode)
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

void gfx_label_scroll_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    if (!obj || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (!label || !label->scroll.scrolling || label->text.long_mode != GFX_LABEL_LONG_SCROLL) {
        return;
    }

    // means don't fetch glphy dsc
    if (label->scroll.offset != label->render.offset) {
        return;
    }
    label->scroll.offset += label->scroll.step;

    if (label->scroll.loop) {
        if (label->scroll.offset > label->text.text_width) {
            label->scroll.offset = -obj->geometry.width;
        }
    } else {
        if (label->scroll.offset > label->text.text_width) {
            label->scroll.scrolling = false;
            gfx_timer_pause(label->scroll.timer);
            return;
        }
    }

    label->scroll.changed = true;
    gfx_obj_invalidate(obj);
}

void gfx_label_snap_timer_callback(void *arg)
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
    int32_t aligned_offset = gfx_calculate_snap_offset(label, font, label->snap.offset, obj->geometry.width);

    /* If no valid offset found, use default */
    if (aligned_offset == 0) {
        aligned_offset = obj->geometry.width;
    }

    /* Jump to next section */
    label->snap.offset += aligned_offset;
    ESP_LOGI(TAG, "aligned_offset: %" PRId32 ", text_width: %" PRId32 ", snap_offset: %" PRId32,
             label->snap.offset - aligned_offset, label->text.text_width, label->snap.offset);

    /* Handle looping */
    if (label->snap.loop) {
        if (label->snap.offset >= label->text.text_width) {
            label->snap.offset = 0;
        }
    } else {
        if (label->snap.offset >= label->text.text_width) {
            label->snap.offset = label->text.text_width - obj->geometry.width;
            if (label->snap.offset < 0) {
                label->snap.offset = 0;
            }
            gfx_timer_pause(label->snap.timer);
        }
    }

    /* Trigger redraw */
    gfx_obj_invalidate(obj);
}

void gfx_update_scroll_state(gfx_obj_t *obj)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;

    /* Handle smooth scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL && label->text.text_width > obj->geometry.width) {
        if (!label->scroll.scrolling) {
            label->scroll.scrolling = true;
            if (label->scroll.timer) {
                gfx_timer_reset(label->scroll.timer);
                gfx_timer_resume(label->scroll.timer);
            }
        }
    } else if (label->scroll.scrolling) {
        label->scroll.scrolling = false;
        if (label->scroll.timer) {
            gfx_timer_pause(label->scroll.timer);
        }
        label->scroll.offset = 0;
    }

    /* Handle snap scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->text.text_width > obj->geometry.width) {
        /* snap_offset will be dynamically calculated in timer callback based on character boundaries */
        if (label->snap.timer) {
            gfx_timer_reset(label->snap.timer);
            gfx_timer_resume(label->snap.timer);
        }
    } else if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->snap.timer) {
        gfx_timer_pause(label->snap.timer);
        label->snap.offset = 0;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_parse_text_lines(gfx_obj_t *obj, int total_line_height,
                                      char ***ret_lines, int *ret_line_count, int *ret_text_width, int **ret_line_widths)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;

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
        int line_width, int y_pos)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;
    gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;

    int start_x = gfx_cal_text_start_x(label->style.text_align, obj->geometry.width, line_width);

    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL && label->scroll.scrolling) {
        start_x -= label->render.offset;

    } else if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        start_x -= label->render.offset;
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
        int line_height, int base_line, int total_line_height, int *cached_line_widths)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;
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

        gfx_render_line_to_mask(obj, mask, line_text, line_width, current_y);

        current_y += total_line_height;
    }

    return ESP_OK;
}

static bool gfx_can_use_cached_data(gfx_obj_t *obj)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    return ((label->text.long_mode == GFX_LABEL_LONG_SCROLL) &&
            (label->cache.lines != NULL) &&
            (label->cache.line_widths != NULL) &&
            (label->cache.line_count > 0) &&
            (label->scroll.changed == true));
}

static gfx_opa_t *gfx_allocate_mask_buffer(gfx_obj_t *obj)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
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


static esp_err_t gfx_render_cached(gfx_obj_t *obj, gfx_opa_t *mask)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;
    gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
    int line_height = font->get_line_height(font);
    int base_line = font->get_base_line(font);
    int total_line_height = line_height + label->text.line_spacing;

    ESP_LOGD(TAG, "Reusing %d cached lines for scroll", label->cache.line_count);
    return gfx_render_lines_to_mask(obj, mask, label->cache.lines,
                                    label->cache.line_count,
                                    line_height, base_line, total_line_height,
                                    label->cache.line_widths);
}

static esp_err_t gfx_render_parse(gfx_obj_t *obj, gfx_opa_t *mask)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;
    gfx_font_ctx_t *font = (gfx_font_ctx_t *)font_ctx;
    int line_height = font->get_line_height(font);
    int base_line = font->get_base_line(font);
    int total_line_height = line_height + label->text.line_spacing;

    char **lines = NULL;
    int line_count = 0;
    int *line_widths = NULL;
    int total_text_width = 0;

    esp_err_t parse_ret = gfx_parse_text_lines(obj, total_line_height,
                          &lines, &line_count, &total_text_width, &line_widths);
    if (parse_ret != ESP_OK) {
        free(mask);
        return parse_ret;
    }

    label->text.text_width = total_text_width;

    esp_err_t cache_ret = gfx_cache_line_data(label, lines, line_count, line_widths);
    if (cache_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to cache line data, continuing without cache");
    }

    esp_err_t render_ret = gfx_render_lines_to_mask(obj, mask, lines, line_count,
                           line_height, base_line, total_line_height, line_widths);
    if (render_ret != ESP_OK) {
        gfx_cleanup_line_data(lines, line_count, line_widths);
        free(mask);
        return render_ret;
    }

    gfx_cleanup_line_data(lines, line_count, line_widths);
    return ESP_OK;
}

esp_err_t gfx_get_glphy_dsc(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (!obj->state.dirty) {
        return ESP_OK;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    void *font_ctx = label->font.font_ctx;
    if (font_ctx == NULL) {
        ESP_LOGD(TAG, "font context is NULL");
        return ESP_OK;
    }

    gfx_opa_t *mask_buf = gfx_allocate_mask_buffer(obj);
    ESP_RETURN_ON_FALSE(mask_buf, ESP_ERR_NO_MEM, TAG, "no mem for mask_buf");

    esp_err_t render_ret;
    if (gfx_can_use_cached_data(obj)) {
        render_ret = gfx_render_cached(obj, mask_buf);
    } else {
        render_ret = gfx_render_parse(obj, mask_buf);
    }

    if (render_ret != ESP_OK) {
        free(mask_buf);
        return render_ret;
    }

    label->render.mask = mask_buf;
    label->scroll.changed = false;
    obj->state.dirty = false;

    gfx_update_scroll_state(obj);

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
esp_err_t gfx_draw_label(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    if (!obj) {
        ESP_LOGE(TAG, "invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (label->text.text == NULL) {
        ESP_LOGD(TAG, "text is NULL");
        return ESP_OK;
    }

    /* Get parent dimensions and calculate aligned position */
    gfx_obj_calc_pos_in_parent(obj);

    /* Calculate clipping area */
    gfx_area_t render_area = {x1, y1, x2, y2};
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + obj->geometry.width, obj->geometry.y + obj->geometry.height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        return ESP_OK;
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

    if (!label->render.mask) {
        return ESP_OK;
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

    gfx_sw_blend_draw(dest_pixels, dest_stride, mask, mask_stride, &clip_area, color, label->style.opa, swap);
    return ESP_OK;
}
