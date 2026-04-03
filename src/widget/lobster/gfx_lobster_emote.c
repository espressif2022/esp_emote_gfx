/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_disp_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/lobster/gfx_lobster_emote_priv.h"

#define CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LOBSTER_EMOTE, TAG)

static const char *TAG = "lobster_emote";

static const uint16_t s_default_pixel = 0x001F;
static const gfx_image_dsc_t s_default_img = {
    .header = {
        .magic = 0x19,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2,
    },
    .data_size = 2,
    .data = (const uint8_t *)&s_default_pixel,
};

static esp_err_t gfx_lobster_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_lobster_emote_delete_impl(gfx_obj_t *obj);
static void gfx_lobster_emote_anim_cb(void *user_data);
static esp_err_t gfx_lobster_emote_apply_color(gfx_lobster_emote_t *lobster);

static const gfx_widget_class_t s_gfx_lobster_emote_widget_class = {
    .type = GFX_OBJ_TYPE_LOBSTER_EMOTE,
    .name = "lobster_emote",
    .draw = gfx_lobster_emote_draw,
    .delete = gfx_lobster_emote_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

static void gfx_lobster_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_lobster_emote_t *lobster;

    if (obj == NULL || obj->src == NULL) {
        return;
    }

    lobster = (gfx_lobster_emote_t *)obj->src;
    if (lobster->assets == NULL) {
        return;
    }

    lobster->anim_tick++;
    if (gfx_lobster_emote_update_pose(obj, lobster) != ESP_OK) {
        GFX_LOGW(TAG, "animate lobster emote: update pose failed");
    }
}

static esp_err_t gfx_lobster_emote_apply_color(gfx_lobster_emote_t *lobster)
{
    const gfx_img_src_t solid_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &lobster->solid_img,
    };

    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_ARG, TAG, "lobster state is NULL");

    if (lobster->body_obj == NULL || lobster->tail_obj == NULL || lobster->claw_l_obj == NULL ||
            lobster->claw_r_obj == NULL || lobster->eye_white_l_obj == NULL || lobster->eye_white_r_obj == NULL ||
            lobster->eye_pupil_l_obj == NULL || lobster->eye_pupil_r_obj == NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->body_obj, &solid_src), TAG, "set body src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->tail_obj, &solid_src), TAG, "set tail src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->claw_l_obj, &solid_src), TAG, "set left claw src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->claw_r_obj, &solid_src), TAG, "set right claw src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->eye_white_l_obj, &solid_src), TAG, "set left eye src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->eye_white_r_obj, &solid_src), TAG, "set right eye src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->eye_pupil_l_obj, &solid_src), TAG, "set left pupil src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->eye_pupil_r_obj, &solid_src), TAG, "set right pupil src failed");

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->body_obj, true, lobster->color), TAG, "set body fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->tail_obj, true, lobster->color), TAG, "set tail fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->claw_l_obj, true, lobster->color), TAG, "set left claw fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->claw_r_obj, true, lobster->color), TAG, "set right claw fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->eye_white_l_obj, true, GFX_COLOR_HEX(0xFFF7ED)), TAG, "set left eye fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->eye_white_r_obj, true, GFX_COLOR_HEX(0xFFF7ED)), TAG, "set right eye fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->eye_pupil_l_obj, true, GFX_COLOR_HEX(0x111827)), TAG, "set left pupil fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->eye_pupil_r_obj, true, GFX_COLOR_HEX(0x111827)), TAG, "set right pupil fill failed");
    return ESP_OK;
}

static esp_err_t gfx_lobster_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    (void)obj;
    (void)ctx;
    return ESP_OK;
}

static esp_err_t gfx_lobster_emote_delete_impl(gfx_obj_t *obj)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    if (lobster != NULL) {
        if (lobster->anim_timer != NULL) {
            gfx_timer_delete(obj->disp->ctx, lobster->anim_timer);
        }
        if (lobster->eye_pupil_r_obj != NULL) {
            gfx_obj_delete(lobster->eye_pupil_r_obj);
        }
        if (lobster->eye_pupil_l_obj != NULL) {
            gfx_obj_delete(lobster->eye_pupil_l_obj);
        }
        if (lobster->eye_white_r_obj != NULL) {
            gfx_obj_delete(lobster->eye_white_r_obj);
        }
        if (lobster->eye_white_l_obj != NULL) {
            gfx_obj_delete(lobster->eye_white_l_obj);
        }
        if (lobster->claw_r_obj != NULL) {
            gfx_obj_delete(lobster->claw_r_obj);
        }
        if (lobster->claw_l_obj != NULL) {
            gfx_obj_delete(lobster->claw_l_obj);
        }
        if (lobster->body_obj != NULL) {
            gfx_obj_delete(lobster->body_obj);
        }
        if (lobster->tail_obj != NULL) {
            gfx_obj_delete(lobster->tail_obj);
        }
        free(lobster);
        obj->src = NULL;
    }
    return ESP_OK;
}

gfx_obj_t *gfx_lobster_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h)
{
    gfx_obj_t *obj = NULL;
    gfx_lobster_emote_t *lobster = NULL;

    if (disp == NULL || w == 0U || h == 0U) {
        GFX_LOGE(TAG, "create lobster emote: invalid arguments");
        return NULL;
    }

    lobster = calloc(1, sizeof(*lobster));
    if (lobster == NULL) {
        GFX_LOGE(TAG, "create lobster emote: no mem for state");
        return NULL;
    }

    lobster->cfg.display_w = w;
    lobster->cfg.display_h = h;
    lobster->cfg.timer_period_ms = 40;
    lobster->cfg.damping_div = 5;
    lobster->solid_pixel = s_default_pixel;
    lobster->solid_img = s_default_img;
    lobster->solid_img.data = (const uint8_t *)&lobster->solid_pixel;
    lobster->color = GFX_COLOR_HEX(0xFF6B4A);

    if (gfx_obj_create_class_instance(disp, &s_gfx_lobster_emote_widget_class,
                                      lobster, w, h, "gfx_lobster_emote_create", &obj) != ESP_OK) {
        free(lobster);
        GFX_LOGE(TAG, "create lobster emote: no mem for object");
        return NULL;
    }

    lobster->tail_obj = gfx_mesh_img_create(disp);
    lobster->body_obj = gfx_mesh_img_create(disp);
    lobster->claw_l_obj = gfx_mesh_img_create(disp);
    lobster->claw_r_obj = gfx_mesh_img_create(disp);
    lobster->eye_white_l_obj = gfx_mesh_img_create(disp);
    lobster->eye_white_r_obj = gfx_mesh_img_create(disp);
    lobster->eye_pupil_l_obj = gfx_mesh_img_create(disp);
    lobster->eye_pupil_r_obj = gfx_mesh_img_create(disp);

    if (lobster->tail_obj == NULL || lobster->body_obj == NULL || lobster->claw_l_obj == NULL ||
            lobster->claw_r_obj == NULL || lobster->eye_white_l_obj == NULL || lobster->eye_white_r_obj == NULL ||
            lobster->eye_pupil_l_obj == NULL || lobster->eye_pupil_r_obj == NULL) {
        gfx_obj_delete(obj);
        GFX_LOGE(TAG, "create lobster emote: part creation failed");
        return NULL;
    }

    lobster->body.tgt.s = 100;
    lobster->tail.tgt.s = 100;
    lobster->claw_l.tgt.s = 100;
    lobster->claw_r.tgt.s = 100;
    lobster->body.cur = lobster->body.tgt;
    lobster->tail.cur = lobster->tail.tgt;
    lobster->claw_l.cur = lobster->claw_l.tgt;
    lobster->claw_r.cur = lobster->claw_r.tgt;
    lobster->eye_open_cur = 100;
    lobster->eye_open_tgt = 100;

    if (gfx_lobster_emote_apply_color(lobster) != ESP_OK) {
        gfx_obj_delete(obj);
        GFX_LOGE(TAG, "create lobster emote: apply color failed");
        return NULL;
    }
    lobster->anim_timer = gfx_timer_create(disp->ctx, gfx_lobster_emote_anim_cb, lobster->cfg.timer_period_ms, obj);
    return obj;
}

esp_err_t gfx_lobster_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_emote_assets_t *assets)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    ESP_RETURN_ON_FALSE(assets != NULL, ESP_ERR_INVALID_ARG, TAG, "assets are NULL");
    ESP_RETURN_ON_FALSE(assets->pts_body != NULL && assets->pts_tail != NULL &&
                        assets->pts_claw_l != NULL && assets->pts_claw_r != NULL &&
                        assets->pts_eye_white != NULL && assets->pts_eye_pupil != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "asset points are incomplete");
    ESP_RETURN_ON_FALSE(assets->count_body >= 4U && assets->count_tail >= 4U &&
                        assets->count_claw_l >= 4U && assets->count_claw_r >= 4U &&
                        assets->count_eye_white >= 4U && assets->count_eye_pupil >= 4U,
                        ESP_ERR_INVALID_ARG, TAG, "asset counts are invalid");

    lobster = (gfx_lobster_emote_t *)obj->src;
    lobster->assets = assets;

    gfx_mesh_img_set_grid(lobster->body_obj, (assets->count_body / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->tail_obj, (assets->count_tail / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->claw_l_obj, (assets->count_claw_l / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->claw_r_obj, (assets->count_claw_r / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->eye_white_l_obj, (assets->count_eye_white / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->eye_white_r_obj, (assets->count_eye_white / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->eye_pupil_l_obj, (assets->count_eye_pupil / 2U) - 1U, 1U);
    gfx_mesh_img_set_grid(lobster->eye_pupil_r_obj, (assets->count_eye_pupil / 2U) - 1U, 1U);

    if (assets->sequence != NULL && assets->sequence_count > 0U) {
        return gfx_lobster_emote_set_state_name(obj, assets->sequence[0].name, true);
    }

    return gfx_lobster_emote_update_pose(obj, lobster);
}

esp_err_t gfx_lobster_emote_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    lobster->color = color;
    lobster->solid_pixel = color.full;
    return gfx_lobster_emote_apply_color(lobster);
}

esp_err_t gfx_lobster_emote_set_state_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    ESP_RETURN_ON_FALSE(name != NULL, ESP_ERR_INVALID_ARG, TAG, "state name is NULL");

    lobster = (gfx_lobster_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster->assets != NULL && lobster->assets->sequence != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "assets are not ready");

    for (size_t i = 0; i < lobster->assets->sequence_count; i++) {
        const gfx_lobster_emote_state_t *state = &lobster->assets->sequence[i];

        if (state->name != NULL && strcmp(state->name, name) == 0) {
            lobster->body.tgt = state->body;
            lobster->tail.tgt = state->tail;
            lobster->claw_l.tgt = state->claw_l;
            lobster->claw_r.tgt = state->claw_r;
            lobster->eye_open_tgt = state->eye_open;
            if (!lobster->manual_look_enabled) {
                lobster->eye_look_x_tgt = state->eye_look_x;
                lobster->eye_look_y_tgt = state->eye_look_y;
            }
            if (snap_now) {
                lobster->body.cur = state->body;
                lobster->tail.cur = state->tail;
                lobster->claw_l.cur = state->claw_l;
                lobster->claw_r.cur = state->claw_r;
                lobster->eye_open_cur = state->eye_open;
                if (!lobster->manual_look_enabled) {
                    lobster->eye_look_x_cur = state->eye_look_x;
                    lobster->eye_look_y_cur = state->eye_look_y;
                }
            }
            lobster->state_idx = i;
            return gfx_lobster_emote_update_pose(obj, lobster);
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_lobster_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    lobster->manual_look_enabled = enabled;
    if (enabled) {
        lobster->eye_look_x_tgt = look_x;
        lobster->eye_look_y_tgt = look_y;
    } else if (lobster->assets != NULL && lobster->assets->sequence != NULL &&
               lobster->state_idx < lobster->assets->sequence_count) {
        lobster->eye_look_x_tgt = lobster->assets->sequence[lobster->state_idx].eye_look_x;
        lobster->eye_look_y_tgt = lobster->assets->sequence[lobster->state_idx].eye_look_y;
    }
    return gfx_lobster_emote_update_pose(obj, lobster);
}
