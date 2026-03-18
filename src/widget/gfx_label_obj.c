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
#include "esp_log.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LABEL_OBJ
#include "common/gfx_log.h"
#include "common/gfx_comm.h"
#include "core/gfx_refr_priv.h"
#include "core/gfx_obj_priv.h"
#include "core/gfx_disp_priv.h"
#include "widget/gfx_label_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_LABEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LABEL, TAG)

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "label_obj";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_label_init_default_state(gfx_label_t *label);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_label_init_default_state(gfx_label_t *label)
{
    label->style.opa = 0xFF;
    label->render.mask = NULL;
    label->render.mask_capacity = 0;
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

gfx_obj_t *gfx_label_create(gfx_disp_t *disp)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "disp must be from gfx_emote_add_disp");
        return NULL;
    }

    gfx_obj_t *obj = calloc(1, sizeof(gfx_obj_t));
    if (obj == NULL) {
        GFX_LOGE(TAG, "No mem for label object");
        return NULL;
    }

    gfx_label_t *label = calloc(1, sizeof(gfx_label_t));
    if (label == NULL) {
        GFX_LOGE(TAG, "Failed to allocate memory for label object");
        free(obj);
        return NULL;
    }

    obj->type = GFX_OBJ_TYPE_LABEL;
    obj->disp = disp;
    obj->state.is_visible = true;
    obj->vfunc.draw = gfx_draw_label;
    obj->vfunc.delete = gfx_label_delete_impl;
    obj->vfunc.update = gfx_label_update_impl;
    obj->src = label;

    gfx_label_init_default_state(label);
    gfx_obj_invalidate(obj);

    if (gfx_disp_add_child(disp, obj) != ESP_OK) {
        free(label);
        free(obj);
        return NULL;
    }

    GFX_LOGD(TAG, "Created label object with default font config");
    return obj;
}

esp_err_t gfx_label_delete_impl(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (label == NULL) {
        return ESP_OK;
    }

    if (label->scroll.timer) {
        gfx_timer_delete(obj->disp->ctx, label->scroll.timer);
        label->scroll.timer = NULL;
    }

    if (label->snap.timer) {
        gfx_timer_delete(obj->disp->ctx, label->snap.timer);
        label->snap.timer = NULL;
    }

    gfx_label_clear_glyph_cache(label);

    free(label->text.text);
    free(label->font.handle);
    free(label->render.mask);
    label->render.mask_capacity = 0;
    free(label);

    return ESP_OK;
}

esp_err_t gfx_label_update_impl(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_LABEL(obj);

    gfx_label_t *label = (gfx_label_t *)obj->src;
    ESP_RETURN_ON_FALSE(label, ESP_ERR_INVALID_STATE, TAG, "label is NULL");

    if (label->text.text == NULL) {
        return ESP_OK;
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

    esp_err_t ret = gfx_get_glphy_dsc(obj);
    if (ret != ESP_OK || !label->render.mask) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
