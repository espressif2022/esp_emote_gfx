/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include <string.h>
#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LABEL_OBJ
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/object/gfx_object_priv.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_LABEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LABEL, TAG)

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "label_obj";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_label_init_default_state(gfx_label_t *label);

static const gfx_widget_class_t s_gfx_label_widget_class = {
    .type = GFX_OBJ_TYPE_LABEL,
    .name = "label",
    .draw = gfx_label_draw,
    .delete = gfx_label_delete_impl,
    .load = gfx_label_load_impl,
    .release = gfx_label_release_impl,
    .update = gfx_label_update_impl,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_label_init_default_state(gfx_label_t *label)
{
    label->style.opa = 0xFF;
    label->render.mask = NULL;
    label->render.mask_capacity = 0;
    label->render.color_mask = NULL;
    label->render.color_mask_capacity = 0;
    label->render.inline_color = false;
    label->style.bg_color = (gfx_color_t) {
        .full = 0x0000
    };
    label->style.bg_enable = false;
    label->style.text_align = GFX_TEXT_ALIGN_LEFT;
    label->text.long_mode = GFX_LABEL_LONG_CLIP;
    label->text.line_spacing = 2;
    label->text.text_width = 0;

    label->scroll.offset = 0;
    label->scroll.step = 1;
    label->scroll.speed = 50;
    label->scroll.loop = true;
    label->scroll.scrolling = false;
    label->scroll.timer = NULL;

    label->snap.interval = 2000;
    label->snap.offset = 0;
    label->snap.loop = true;
    label->snap.timer = NULL;

}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_object_t *gfx_label_create(gfx_display_t *disp)
{
    gfx_object_t *obj = NULL;
    gfx_label_t *label = NULL;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create label: display is NULL");
        return NULL;
    }

    label = calloc(1, sizeof(gfx_label_t));
    if (label == NULL) {
        GFX_LOGE(TAG, "create label: no mem for state");
        return NULL;
    }

    if (gfx_object_create_class_instance(disp, &s_gfx_label_widget_class,
                                         label, 0, 0, "gfx_label_create", &obj) != GFX_OK) {
        free(label);
        GFX_LOGE(TAG, "create label: no mem for object");
        return NULL;
    }

    gfx_label_init_default_state(label);

    GFX_LOGD(TAG, "create label: object created");
    return obj;
}

gfx_err_t gfx_label_load_state(gfx_object_t *owner, gfx_label_t *label)
{
    gfx_font_handle_t font_handle;

    GFX_RETURN_ON_FALSE(label != NULL, GFX_ERR_INVALID_STATE, TAG, "load label: state is NULL");

    if (label->font.source == NULL) {
        return GFX_OK;
    }

    font_handle = calloc(1, sizeof(gfx_font_adapter_t));
    GFX_RETURN_ON_FALSE(font_handle != NULL, GFX_ERR_NO_MEM, TAG, "load label: no mem for font adapter");

    gfx_err_t ret = gfx_font_init_adapter(font_handle, label->font.source);
    if (ret != GFX_OK) {
        free(font_handle);
        return ret;
    }

    label->font.handle = font_handle;
    label->text.text_width = 0;
    (void)owner;
    return GFX_OK;
}

gfx_err_t gfx_label_load_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LABEL(obj);
    return gfx_label_load_state(obj, (gfx_label_t *)obj->src);
}

void gfx_label_release_state(gfx_label_t *label)
{
    if (label == NULL) {
        return;
    }

    gfx_label_clear_glyph_cache(label);
    free(label->font.handle);
    label->font.handle = NULL;
    label->text.text_width = 0;
}

void gfx_label_release_impl(gfx_object_t *obj)
{
    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_release_state((gfx_label_t *)obj->src);
}

gfx_err_t gfx_label_set_font_source(gfx_object_t *obj, gfx_label_t *label, gfx_font_t font)
{
    GFX_RETURN_IF_NULL(obj, GFX_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(label, GFX_ERR_INVALID_STATE);

    label->font.source = font;
    label->text.text_width = 0;
    gfx_object_mark_resource_dirty(obj);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

void gfx_label_delete_state(gfx_object_t *owner, gfx_label_t *label)
{
    if (label == NULL) {
        return;
    }

    if (label->scroll.timer) {
        gfx_timer_delete(owner->disp->ctx, label->scroll.timer);
        label->scroll.timer = NULL;
    }

    if (label->snap.timer) {
        gfx_timer_delete(owner->disp->ctx, label->snap.timer);
        label->snap.timer = NULL;
    }

    free(label->text.text);
    free(label->render.mask);
    label->render.mask_capacity = 0;
    free(label->render.color_mask);
    label->render.color_mask_capacity = 0;
    label->render.inline_color = false;
}

gfx_err_t gfx_label_delete_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    gfx_label_delete_state(obj, label);
    free(label);

    return GFX_OK;
}

gfx_err_t gfx_label_update_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    GFX_RETURN_ON_FALSE(label, GFX_ERR_INVALID_STATE, TAG, "label is NULL");

    if (label->text.text == NULL) {
        return GFX_OK;
    }

    switch (label->text.long_mode) {
    case GFX_LABEL_LONG_SCROLL:
        label->render.offset = label->scroll.offset;
        break;
    case GFX_LABEL_LONG_SCROLL_SNAP:
        label->render.offset = label->snap.offset;
        break;
    default:
        label->render.offset = 0;
        break;
    }

    gfx_err_t ret = gfx_label_prepare_glyphs(obj);
    if (ret != GFX_OK || !label->render.mask) {
        return GFX_FAIL;
    }

    return GFX_OK;
}
