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
static const uint8_t LOBSTER_EYE_SEGS = 18;
static const uint8_t LOBSTER_PUPIL_SEGS = 14;
static const uint8_t LOBSTER_ANTENNA_SEGS = 18;
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

static const int16_t s_pupil_curve_o[14] = { -18, 0, -18, -14, 18, -14, 18, 0, 18, 14, -18, 14, -18, 0 };
static const int16_t s_pupil_curve_u[14] = { -24, -2, -14, 22, 14, 22, 24, -2, 12, 10, -12, 10, -24, -2 };
static const int16_t s_pupil_curve_n[14] = { -24, 2, -14, -22, 14, -22, 24, 2, 12, -10, -12, -10, -24, 2 };
static const int16_t s_pupil_curve_line[14] = { -26, 0, -8, -2, 8, -2, 26, 0, 8, 2, -8, 2, -26, 0 };

static esp_err_t gfx_lobster_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_lobster_emote_delete_impl(gfx_obj_t *obj);
static void gfx_lobster_emote_anim_cb(void *user_data);
static void gfx_lobster_emote_apply_pupil_shape(gfx_lobster_emote_t *lobster, gfx_lobster_pupil_shape_t shape, bool snap_now);
static esp_err_t gfx_lobster_emote_apply_layer_mask(gfx_lobster_emote_t *lobster);

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
    gfx_lobster_emote_update_pose(obj, lobster);
}

static void gfx_lobster_emote_apply_pupil_shape(gfx_lobster_emote_t *lobster,
                                                gfx_lobster_pupil_shape_t shape,
                                                bool snap_now)
{
    const int16_t *src = NULL;

    if (lobster == NULL) {
        return;
    }

    switch (shape) {
    case GFX_LOBSTER_PUPIL_SHAPE_U:
        src = s_pupil_curve_u;
        break;
    case GFX_LOBSTER_PUPIL_SHAPE_N:
        src = s_pupil_curve_n;
        break;
    case GFX_LOBSTER_PUPIL_SHAPE_LINE:
        src = s_pupil_curve_line;
        break;
    case GFX_LOBSTER_PUPIL_SHAPE_AUTO:
    case GFX_LOBSTER_PUPIL_SHAPE_O:
    default:
        src = s_pupil_curve_o;
        break;
    }

    for (size_t i = 0; i < 14; i++) {
        lobster->pupil_tgt.pts[i] = src[i];
        if (snap_now) {
            lobster->pupil_cur.pts[i] = src[i];
        }
    }
    lobster->pupil_shape = shape;
}

static esp_err_t gfx_lobster_emote_apply_layer_mask(gfx_lobster_emote_t *lobster)
{
    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_ARG, TAG, "lobster state is NULL");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->antenna_l_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_ANTENNA) != 0), TAG, "left antenna visibility failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->antenna_r_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_ANTENNA) != 0), TAG, "right antenna visibility failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->eye_l_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE) != 0), TAG, "left eye visibility failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->eye_r_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE) != 0), TAG, "right eye visibility failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->pupil_l_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_PUPIL) != 0), TAG, "left pupil visibility failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->pupil_r_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_PUPIL) != 0), TAG, "right pupil visibility failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(lobster->mouth_obj, (lobster->layer_mask & GFX_LOBSTER_EMOTE_LAYER_MOUTH) != 0), TAG, "mouth visibility failed");
    return ESP_OK;
}

static esp_err_t gfx_lobster_emote_apply_color(gfx_lobster_emote_t *lobster)
{
    const gfx_img_src_t accent_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &lobster->accent_img,
    };
    const gfx_img_src_t eye_white_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &lobster->eye_white_img,
    };
    const gfx_img_src_t pupil_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &lobster->pupil_img,
    };
    bool swap;

    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_ARG, TAG, "lobster state is NULL");
    ESP_RETURN_ON_FALSE(lobster->antenna_l_obj != NULL && lobster->antenna_l_obj->disp != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "lobster display is not ready");

    swap = lobster->antenna_l_obj->disp->flags.swap;
    lobster->accent_pixel = gfx_color_to_native_u16(GFX_COLOR_HEX(0xF46144), swap);
    lobster->eye_white_pixel = gfx_color_to_native_u16(GFX_COLOR_HEX(0xFFFFFF), swap);
    lobster->pupil_pixel = gfx_color_to_native_u16(GFX_COLOR_HEX(0x191919), swap);

    gfx_mesh_img_set_src_desc(lobster->antenna_l_obj, &accent_src);
    gfx_mesh_img_set_src_desc(lobster->antenna_r_obj, &accent_src);
    gfx_mesh_img_set_src_desc(lobster->eye_l_obj, &eye_white_src);
    gfx_mesh_img_set_src_desc(lobster->eye_r_obj, &eye_white_src);
    gfx_mesh_img_set_src_desc(lobster->pupil_l_obj, &pupil_src);
    gfx_mesh_img_set_src_desc(lobster->pupil_r_obj, &pupil_src);
    gfx_mesh_img_set_src_desc(lobster->mouth_obj, &pupil_src);
#if 0
    gfx_mesh_img_set_scanline_fill(lobster->antenna_l_obj, true, GFX_COLOR_HEX(0xF46144));
    gfx_mesh_img_set_scanline_fill(lobster->antenna_r_obj, true, GFX_COLOR_HEX(0xF46144));
    gfx_mesh_img_set_scanline_fill(lobster->eye_l_obj, true, GFX_COLOR_HEX(0xFFFFFF));
    gfx_mesh_img_set_scanline_fill(lobster->eye_r_obj, true, GFX_COLOR_HEX(0xFFFFFF));
    gfx_mesh_img_set_scanline_fill(lobster->pupil_l_obj, true, GFX_COLOR_HEX(0x191919));
    gfx_mesh_img_set_scanline_fill(lobster->pupil_r_obj, true, GFX_COLOR_HEX(0x191919));
#else
    gfx_mesh_img_set_scanline_fill(lobster->antenna_l_obj, false, GFX_COLOR_HEX(0xF46144));
    gfx_mesh_img_set_scanline_fill(lobster->antenna_r_obj, false, GFX_COLOR_HEX(0xF46144));
    gfx_mesh_img_set_scanline_fill(lobster->eye_l_obj, false, GFX_COLOR_HEX(0xFFFFFF));
    gfx_mesh_img_set_scanline_fill(lobster->eye_r_obj, false, GFX_COLOR_HEX(0xFFFFFF));
    gfx_mesh_img_set_scanline_fill(lobster->pupil_l_obj, false, GFX_COLOR_HEX(0x191919));
    gfx_mesh_img_set_scanline_fill(lobster->pupil_r_obj, false, GFX_COLOR_HEX(0x191919));
    gfx_mesh_img_set_scanline_fill(lobster->mouth_obj, false, GFX_COLOR_HEX(0x191919));
#endif
    return ESP_OK;
}

gfx_obj_t *gfx_lobster_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h)
{
    gfx_obj_t *obj = NULL;
    gfx_lobster_emote_t *lobster = NULL;

    if (disp == NULL) {
        return NULL;
    }

    lobster = calloc(1, sizeof(*lobster));
    if (lobster == NULL) {
        return NULL;
    }

    lobster->cfg.display_w = w;
    lobster->cfg.display_h = h;
    lobster->cfg.timer_period_ms = 33;
    lobster->cfg.damping_div = 4;
    lobster->color = GFX_COLOR_HEX(0xF46144);
    lobster->accent_img = s_default_img;
    lobster->eye_white_img = s_default_img;
    lobster->pupil_img = s_default_img;
    lobster->accent_img.data = (const uint8_t *)&lobster->accent_pixel;
    lobster->eye_white_img.data = (const uint8_t *)&lobster->eye_white_pixel;
    lobster->pupil_img.data = (const uint8_t *)&lobster->pupil_pixel;
    lobster->layer_mask = GFX_LOBSTER_EMOTE_LAYER_ALL;

    if (gfx_obj_create_class_instance(disp, &s_gfx_lobster_emote_widget_class,
                                      lobster, w, h, "gfx_lobster_emote_create", &obj) != ESP_OK) {
        free(lobster);
        return NULL;
    }

    lobster->antenna_l_obj = gfx_mesh_img_create(disp);
    lobster->antenna_r_obj = gfx_mesh_img_create(disp);
    lobster->eye_l_obj = gfx_mesh_img_create(disp);
    lobster->eye_r_obj = gfx_mesh_img_create(disp);
    lobster->pupil_l_obj = gfx_mesh_img_create(disp);
    lobster->pupil_r_obj = gfx_mesh_img_create(disp);
    lobster->mouth_obj = gfx_mesh_img_create(disp);

    if (lobster->antenna_l_obj == NULL || lobster->antenna_r_obj == NULL ||
            lobster->eye_l_obj == NULL || lobster->eye_r_obj == NULL ||
            lobster->pupil_l_obj == NULL || lobster->pupil_r_obj == NULL ||
            lobster->mouth_obj == NULL) {
        gfx_obj_delete(obj);
        return NULL;
    }

    lobster->antenna.tgt.s = 100;
    lobster->eye_l.tgt.s = 100;
    lobster->eye_r.tgt.s = 100;
    lobster->pupil_l.tgt.s = 100;
    lobster->pupil_r.tgt.s = 100;
    lobster->mouth.tgt.s = 100;
    lobster->antenna.cur = lobster->antenna.tgt;
    lobster->eye_l.cur = lobster->eye_l.tgt;
    lobster->eye_r.cur = lobster->eye_r.tgt;
    lobster->pupil_l.cur = lobster->pupil_l.tgt;
    lobster->pupil_r.cur = lobster->pupil_r.tgt;
    lobster->mouth.cur = lobster->mouth.tgt;
    memset(&lobster->eye_white_cur, 0, sizeof(lobster->eye_white_cur));
    memset(&lobster->eye_white_tgt, 0, sizeof(lobster->eye_white_tgt));
    memset(&lobster->pupil_cur, 0, sizeof(lobster->pupil_cur));
    memset(&lobster->pupil_tgt, 0, sizeof(lobster->pupil_tgt));
    memset(&lobster->mouth_cur, 0, sizeof(lobster->mouth_cur));
    memset(&lobster->mouth_tgt, 0, sizeof(lobster->mouth_tgt));
    memset(&lobster->antenna_curve_l_cur, 0, sizeof(lobster->antenna_curve_l_cur));
    memset(&lobster->antenna_curve_l_tgt, 0, sizeof(lobster->antenna_curve_l_tgt));
    memset(&lobster->antenna_curve_r_cur, 0, sizeof(lobster->antenna_curve_r_cur));
    memset(&lobster->antenna_curve_r_tgt, 0, sizeof(lobster->antenna_curve_r_tgt));

    gfx_mesh_img_set_grid(lobster->antenna_l_obj, LOBSTER_ANTENNA_SEGS, 1);
    gfx_mesh_img_set_grid(lobster->antenna_r_obj, LOBSTER_ANTENNA_SEGS, 1);
    gfx_mesh_img_set_grid(lobster->eye_l_obj, LOBSTER_EYE_SEGS, 1);
    gfx_mesh_img_set_grid(lobster->eye_r_obj, LOBSTER_EYE_SEGS, 1);
    gfx_mesh_img_set_grid(lobster->pupil_l_obj, LOBSTER_PUPIL_SEGS, 1);
    gfx_mesh_img_set_grid(lobster->pupil_r_obj, LOBSTER_PUPIL_SEGS, 1);
    gfx_mesh_img_set_grid(lobster->mouth_obj, LOBSTER_PUPIL_SEGS, 1);

    gfx_lobster_emote_apply_pupil_shape(lobster, GFX_LOBSTER_PUPIL_SHAPE_O, true);
    gfx_lobster_emote_apply_color(lobster);
    gfx_lobster_emote_apply_layer_mask(lobster);
    lobster->anim_timer = gfx_timer_create(disp->ctx, gfx_lobster_emote_anim_cb, lobster->cfg.timer_period_ms, obj);
    return obj;
}

esp_err_t gfx_lobster_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_emote_assets_t *assets)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_validate_assets(assets), TAG, "invalid lobster assets");
    lobster->assets = assets;

    if (lobster->assets != NULL) {
        if (lobster->assets->sequence != NULL && lobster->assets->sequence_count > 0U) {
            return gfx_lobster_emote_set_state_name(obj, lobster->assets->sequence[0].name, true);
        }
    }
    return ESP_OK;
}

esp_err_t gfx_lobster_emote_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    lobster->color = color;
    return gfx_lobster_emote_apply_color(lobster);
}

esp_err_t gfx_lobster_emote_set_state_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_lobster_emote_t *lobster;
    size_t index;
    gfx_lobster_transform_t head_next, body_next, tail_next, claw_l_next, claw_r_next, antenna_next, eye_next, pupil_next, mouth_next, dots_next;
    gfx_lobster_curve14_t eye_curve_next;
    gfx_lobster_curve14_t pupil_curve_next;
    gfx_lobster_curve14_t mouth_curve_next;
    gfx_lobster_curve8_t antenna_curve_l_next;
    gfx_lobster_curve8_t antenna_curve_r_next;
    gfx_lobster_pupil_shape_t pupil_shape = GFX_LOBSTER_PUPIL_SHAPE_O;
    int16_t look_x_next = 0;
    int16_t look_y_next = 0;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster->assets != NULL && lobster->assets->sequence != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "assets are not ready");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_find_state_index(lobster->assets, name, &index), TAG, "state not found");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_eval_state(lobster->assets, &lobster->assets->sequence[index],
                                                     &head_next, &body_next, &tail_next,
                                                     &claw_l_next, &claw_r_next, &antenna_next,
                                                     &eye_next, &pupil_next, &mouth_next, &dots_next,
                                                     &eye_curve_next, &pupil_curve_next, &mouth_curve_next, &antenna_curve_l_next, &antenna_curve_r_next,
                                                     &pupil_shape, &look_x_next, &look_y_next),
                        TAG, "eval state failed");

    lobster->head.tgt = head_next;
    lobster->body.tgt = body_next;
    lobster->tail.tgt = tail_next;
    lobster->claw_l.tgt = claw_l_next;
    lobster->claw_r.tgt = claw_r_next;
    lobster->antenna.tgt = antenna_next;
    lobster->eye_l.tgt = eye_next;
    lobster->eye_r.tgt = eye_next;
    lobster->pupil_l.tgt = pupil_next;
    lobster->pupil_r.tgt = pupil_next;
    lobster->mouth.tgt = mouth_next;
    lobster->dots.tgt = dots_next;
    lobster->eye_white_tgt = eye_curve_next;
    lobster->pupil_tgt = pupil_curve_next;
    lobster->mouth_tgt = mouth_curve_next;
    lobster->antenna_curve_l_tgt = antenna_curve_l_next;
    lobster->antenna_curve_r_tgt = antenna_curve_r_next;
    if (!lobster->manual_look_enabled) {
        lobster->look_x_tgt = look_x_next;
        lobster->look_y_tgt = look_y_next;
    }

    if (snap_now) {
        lobster->head.cur = head_next;
        lobster->body.cur = body_next;
        lobster->tail.cur = tail_next;
        lobster->claw_l.cur = claw_l_next;
        lobster->claw_r.cur = claw_r_next;
        lobster->antenna.cur = antenna_next;
        lobster->eye_l.cur = eye_next;
        lobster->eye_r.cur = eye_next;
        lobster->pupil_l.cur = pupil_next;
        lobster->pupil_r.cur = pupil_next;
        lobster->mouth.cur = mouth_next;
        lobster->dots.cur = dots_next;
        lobster->eye_white_cur = eye_curve_next;
        lobster->pupil_cur = pupil_curve_next;
        lobster->mouth_cur = mouth_curve_next;
        lobster->antenna_curve_l_cur = antenna_curve_l_next;
        lobster->antenna_curve_r_cur = antenna_curve_r_next;
        if (!lobster->manual_look_enabled) {
            lobster->look_x_cur = look_x_next;
            lobster->look_y_cur = look_y_next;
        }
    }

    gfx_lobster_emote_apply_pupil_shape(lobster, pupil_shape, snap_now);
    lobster->state_idx = index;
    return gfx_lobster_emote_update_pose(obj, lobster);
}

esp_err_t gfx_lobster_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    lobster->manual_look_enabled = enabled;
    if (enabled) {
        lobster->look_x_tgt = look_x;
        lobster->look_y_tgt = look_y;
    } else if (lobster->assets != NULL && lobster->assets->sequence != NULL &&
               lobster->state_idx < lobster->assets->sequence_count) {
        lobster->look_x_tgt = lobster->assets->sequence[lobster->state_idx].w_look_x;
        lobster->look_y_tgt = lobster->assets->sequence[lobster->state_idx].w_look_y;
    }
    return gfx_lobster_emote_update_pose(obj, lobster);
}

esp_err_t gfx_lobster_emote_set_layer_mask(gfx_obj_t *obj, uint32_t mask)
{
    gfx_lobster_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_EMOTE(obj);
    lobster = (gfx_lobster_emote_t *)obj->src;
    lobster->layer_mask = mask;
    return gfx_lobster_emote_apply_layer_mask(lobster);
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
        if (lobster->pupil_r_obj) gfx_obj_delete(lobster->pupil_r_obj);
        if (lobster->pupil_l_obj) gfx_obj_delete(lobster->pupil_l_obj);
        if (lobster->mouth_obj) gfx_obj_delete(lobster->mouth_obj);
        if (lobster->eye_r_obj) gfx_obj_delete(lobster->eye_r_obj);
        if (lobster->eye_l_obj) gfx_obj_delete(lobster->eye_l_obj);
        if (lobster->antenna_r_obj) gfx_obj_delete(lobster->antenna_r_obj);
        if (lobster->antenna_l_obj) gfx_obj_delete(lobster->antenna_l_obj);
        free(lobster);
        obj->src = NULL;
    }
    return ESP_OK;
}
