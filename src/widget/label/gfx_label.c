/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "esp_err.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LABEL
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"

#include "widget/gfx_label.h"
#include "widget/label/gfx_label_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_LABEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LABEL, TAG)

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "label";

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

esp_err_t gfx_label_set_font(gfx_obj_t *obj, gfx_font_t font)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    gfx_label_t *label = (gfx_label_t *)obj->src;

    if (label->font.handle != NULL) {
        free(label->font.handle);
        label->font.handle = NULL;
    }

    gfx_label_clear_glyph_cache(label);
    label->text.text_width = 0;

    if (font) {
        gfx_font_handle_t font_handle = (gfx_font_handle_t)calloc(1, sizeof(gfx_font_adapter_t));
        if (font_handle != NULL) {
            if (gfx_is_lvgl_font(font)) {
                gfx_font_lv_init_adapter(font_handle, font);
            } else {
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
                gfx_font_ft_init_adapter(font_handle, font);
#else
                GFX_LOGW(TAG, "set label font: freetype support is not enabled");
                free(font_handle);
                font_handle = NULL;
#endif
            }

            label->font.handle = font_handle;
        } else {
            GFX_LOGW(TAG, "set label font: allocate font adapter failed");
        }
    }

    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_label_set_text(gfx_obj_t *obj, const char *text)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    const char *new_text = text;

    if (new_text == NULL) {
        new_text = label->text.text ? label->text.text : "";
    }

    if (label->text.text != new_text) {
        size_t len = strlen(new_text) + 1;
        char *dup_text = malloc(len);
        if (dup_text == NULL) {
            return ESP_ERR_NO_MEM;
        }
        memcpy(dup_text, new_text, len);

        free(label->text.text);
        label->text.text = dup_text;
    }

    /* Reset scroll state for smooth scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL) {
        if (label->scroll.scrolling) {
            label->scroll.scrolling = false;
            if (label->scroll.timer) {
                gfx_timer_pause(label->scroll.timer);
            }
        }
        label->scroll.offset = 0;
        label->text.text_width = 0;
    }

    /* Reset scroll state for snap mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        if (label->snap.timer) {
            gfx_timer_pause(label->snap.timer);
        }
        label->scroll.offset = 0;
        label->text.text_width = 0;
    }

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

    label->text.text_width = 0;
    label->scroll.offset = 0;
    label->snap.offset = 0;

    gfx_obj_invalidate(obj);

    return ESP_OK;
}

esp_err_t gfx_label_set_opa(gfx_obj_t *obj, gfx_opa_t opa)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.opa = opa;
    GFX_LOGD(TAG, "set font opa: %d", label->style.opa);

    return ESP_OK;
}

esp_err_t gfx_label_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.color = color;
    GFX_LOGD(TAG, "set font color: %d", label->style.color.full);

    return ESP_OK;
}

esp_err_t gfx_label_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.bg_color = bg_color;
    GFX_LOGD(TAG, "set background color: %d", label->style.bg_color.full);

    return ESP_OK;
}

esp_err_t gfx_label_set_bg_enable(gfx_obj_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.bg_enable = enable;
    gfx_obj_invalidate(obj);
    GFX_LOGD(TAG, "set background enable: %s", enable ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t gfx_label_set_text_align(gfx_obj_t *obj, gfx_text_align_t align)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->style.text_align = align;
    gfx_obj_invalidate(obj);
    GFX_LOGD(TAG, "set text align: %d", align);

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
            if (label->scroll.timer) {
                gfx_timer_pause(label->scroll.timer);
            }
        }

        /* Stop snap scrolling if switching from snap mode */
        if (old_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->snap.timer) {
            gfx_timer_pause(label->snap.timer);
        }

        label->scroll.offset = 0;
        label->text.text_width = 0;

        /* Handle smooth scroll timer */
        if (long_mode == GFX_LABEL_LONG_SCROLL && !label->scroll.timer) {
            label->scroll.timer = gfx_timer_create(obj->disp->ctx,
                                                   gfx_label_scroll_timer_callback,
                                                   label->scroll.speed,
                                                   obj);
            if (label->scroll.timer) {
                gfx_timer_set_repeat_count(label->scroll.timer, -1);
            }
        } else if (long_mode != GFX_LABEL_LONG_SCROLL && label->scroll.timer) {
            gfx_timer_delete(obj->disp->ctx, label->scroll.timer);
            label->scroll.timer = NULL;
        }

        /* Handle snap scroll timer */
        if (long_mode == GFX_LABEL_LONG_SCROLL_SNAP && !label->snap.timer) {
            label->snap.timer = gfx_timer_create(obj->disp->ctx,
                                                 gfx_label_snap_timer_callback,
                                                 label->snap.interval,
                                                 obj);
            if (label->snap.timer) {
                gfx_timer_set_repeat_count(label->snap.timer, -1);
            }
        } else if (long_mode != GFX_LABEL_LONG_SCROLL_SNAP && label->snap.timer) {
            gfx_timer_delete(obj->disp->ctx, label->snap.timer);
            label->snap.timer = NULL;
        }

        gfx_obj_invalidate(obj);
    }

    GFX_LOGD(TAG, "set long mode: %d", long_mode);
    return ESP_OK;
}

esp_err_t gfx_label_set_line_spacing(gfx_obj_t *obj, uint16_t spacing)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    label->text.line_spacing = spacing;
    gfx_obj_invalidate(obj);
    GFX_LOGD(TAG, "set line spacing: %d", spacing);

    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_speed(gfx_obj_t *obj, uint32_t speed_ms)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    ESP_RETURN_ON_FALSE(speed_ms > 0, ESP_ERR_INVALID_ARG, TAG, "invalid speed");

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->scroll.speed = speed_ms;

    if (label->scroll.timer) {
        gfx_timer_set_period(label->scroll.timer, speed_ms);
    }

    GFX_LOGD(TAG, "set scroll speed: %"PRIu32" ms", speed_ms);
    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_loop(gfx_obj_t *obj, bool loop)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->scroll.loop = loop;
    GFX_LOGD(TAG, "set scroll loop: %s", loop ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t gfx_label_set_scroll_step(gfx_obj_t *obj, int32_t step)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");
    ESP_RETURN_ON_FALSE(step != 0, ESP_ERR_INVALID_ARG, TAG, "scroll step cannot be zero");

    label->scroll.step = step;
    GFX_LOGD(TAG, "set scroll step: %"PRId32, step);
    return ESP_OK;
}

esp_err_t gfx_label_set_snap_interval(gfx_obj_t *obj, uint32_t interval_ms)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    ESP_RETURN_ON_FALSE(interval_ms > 0, ESP_ERR_INVALID_ARG, TAG, "invalid snap interval");

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->snap.interval = interval_ms;

    if (label->snap.timer) {
        gfx_timer_set_period(label->snap.timer, interval_ms);
    }

    GFX_LOGD(TAG, "set snap interval: %"PRIu32" ms", interval_ms);
    return ESP_OK;
}

esp_err_t gfx_label_set_snap_loop(gfx_obj_t *obj, bool loop)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label property is NULL");

    label->snap.loop = loop;
    GFX_LOGD(TAG, "set snap loop: %s", loop ? "enabled" : "disabled");

    return ESP_OK;
}
