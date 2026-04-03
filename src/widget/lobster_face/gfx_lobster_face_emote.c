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
#include "widget/gfx_mesh_img.h"
#include "widget/lobster_face/gfx_lobster_face_emote_priv.h"

#define CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LOBSTER_FACE_EMOTE, TAG)

static const char *TAG = "lobster_face";

static const uint16_t s_default_pixel = 0xFBE9;
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

static esp_err_t gfx_lobster_face_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_lobster_face_emote_delete_impl(gfx_obj_t *obj);
static void gfx_lobster_face_emote_anim_cb(void *user_data);
static esp_err_t gfx_lobster_face_emote_apply_color(gfx_lobster_face_emote_t *lobster);

static const gfx_widget_class_t s_gfx_lobster_face_emote_widget_class = {
    .type = GFX_OBJ_TYPE_LOBSTER_FACE_EMOTE,
    .name = "lobster_face_emote",
    .draw = gfx_lobster_face_emote_draw,
    .delete = gfx_lobster_face_emote_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

static void gfx_lobster_face_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_lobster_face_emote_t *lobster;

    if (obj == NULL || obj->src == NULL) {
        return;
    }
    lobster = (gfx_lobster_face_emote_t *)obj->src;
    if (lobster->assets == NULL) {
        return;
    }
    if (gfx_lobster_face_emote_update_pose(obj, lobster) != ESP_OK) {
        GFX_LOGW(TAG, "animate lobster face emote: update pose failed");
    }
}

static esp_err_t gfx_lobster_face_emote_apply_color(gfx_lobster_face_emote_t *lobster)
{
    const gfx_img_src_t solid_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &lobster->solid_img,
    };

    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_ARG, TAG, "lobster state is NULL");

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->tail_obj, &solid_src), TAG, "tail src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->body_obj, &solid_src), TAG, "body src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->head_obj, &solid_src), TAG, "head src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->claw_l_obj, &solid_src), TAG, "left claw src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->claw_r_obj, &solid_src), TAG, "right claw src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->eye_l_obj, &solid_src), TAG, "left eye src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->eye_r_obj, &solid_src), TAG, "right eye src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->pupil_l_obj, &solid_src), TAG, "left pupil src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(lobster->pupil_r_obj, &solid_src), TAG, "right pupil src failed");

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->tail_obj, true, GFX_COLOR_HEX(0xF04F3E)), TAG, "tail fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->body_obj, true, GFX_COLOR_HEX(0xE8483B)), TAG, "body fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->head_obj, true, lobster->color), TAG, "head fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->claw_l_obj, true, GFX_COLOR_HEX(0xF65B43)), TAG, "left claw fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->claw_r_obj, true, GFX_COLOR_HEX(0xF65B43)), TAG, "right claw fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->eye_l_obj, true, GFX_COLOR_HEX(0xFFFDF8)), TAG, "left eye fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->eye_r_obj, true, GFX_COLOR_HEX(0xFFFDF8)), TAG, "right eye fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->pupil_l_obj, true, GFX_COLOR_HEX(0x171717)), TAG, "left pupil fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(lobster->pupil_r_obj, true, GFX_COLOR_HEX(0x171717)), TAG, "right pupil fill failed");
    return ESP_OK;
}

static esp_err_t gfx_lobster_face_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    (void)obj;
    (void)ctx;
    return ESP_OK;
}

static esp_err_t gfx_lobster_face_emote_delete_impl(gfx_obj_t *obj)
{
    gfx_lobster_face_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    lobster = (gfx_lobster_face_emote_t *)obj->src;
    if (lobster != NULL) {
        if (lobster->anim_timer != NULL) {
            gfx_timer_delete(obj->disp->ctx, lobster->anim_timer);
        }
        if (lobster->pupil_r_obj != NULL) gfx_obj_delete(lobster->pupil_r_obj);
        if (lobster->pupil_l_obj != NULL) gfx_obj_delete(lobster->pupil_l_obj);
        if (lobster->eye_r_obj != NULL) gfx_obj_delete(lobster->eye_r_obj);
        if (lobster->eye_l_obj != NULL) gfx_obj_delete(lobster->eye_l_obj);
        if (lobster->claw_r_obj != NULL) gfx_obj_delete(lobster->claw_r_obj);
        if (lobster->claw_l_obj != NULL) gfx_obj_delete(lobster->claw_l_obj);
        if (lobster->head_obj != NULL) gfx_obj_delete(lobster->head_obj);
        if (lobster->body_obj != NULL) gfx_obj_delete(lobster->body_obj);
        if (lobster->tail_obj != NULL) gfx_obj_delete(lobster->tail_obj);
        free(lobster);
        obj->src = NULL;
    }
    return ESP_OK;
}

void gfx_lobster_face_emote_cfg_init(gfx_lobster_face_emote_cfg_t *cfg, uint16_t display_w, uint16_t display_h)
{
    int16_t base;

    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->display_w = (display_w == 0U) ? 1U : display_w;
    cfg->display_h = (display_h == 0U) ? 1U : display_h;
    base = (cfg->display_w < cfg->display_h) ? (int16_t)cfg->display_w : (int16_t)cfg->display_h;

    cfg->eye_x_half_gap = base / 10;
    cfg->eye_y_ofs = -(base / 7);
    cfg->claw_x_half_gap = (base * 17) / 100;
    cfg->claw_y_ofs = base / 12;
    cfg->head_y_ofs = -(base / 7);
    cfg->tail_x_ofs = base / 7;
    cfg->tail_y_ofs = -(base / 9);
    cfg->timer_period_ms = 40;
    cfg->eye_segs = 12;
    cfg->claw_segs = 14;
    cfg->shell_segs = 18;
    cfg->eye_scale_percent = 120;
    cfg->claw_scale_percent = 135;
    cfg->shell_scale_percent = 170;
}

gfx_obj_t *gfx_lobster_face_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h)
{
    gfx_obj_t *obj = NULL;
    gfx_lobster_face_emote_t *lobster = NULL;

    if (disp == NULL || w == 0U || h == 0U) {
        return NULL;
    }

    lobster = calloc(1, sizeof(*lobster));
    if (lobster == NULL) {
        return NULL;
    }

    gfx_lobster_face_emote_cfg_init(&lobster->cfg, w, h);
    lobster->solid_pixel = s_default_pixel;
    lobster->solid_img = s_default_img;
    lobster->solid_img.data = (const uint8_t *)&lobster->solid_pixel;
    lobster->color = GFX_COLOR_HEX(0xF46144);

    if (gfx_obj_create_class_instance(disp, &s_gfx_lobster_face_emote_widget_class,
                                      lobster, w, h, "gfx_lobster_face_emote_create", &obj) != ESP_OK) {
        free(lobster);
        return NULL;
    }

    lobster->tail_obj = gfx_mesh_img_create(disp);
    lobster->body_obj = gfx_mesh_img_create(disp);
    lobster->head_obj = gfx_mesh_img_create(disp);
    lobster->claw_l_obj = gfx_mesh_img_create(disp);
    lobster->claw_r_obj = gfx_mesh_img_create(disp);
    lobster->eye_l_obj = gfx_mesh_img_create(disp);
    lobster->eye_r_obj = gfx_mesh_img_create(disp);
    lobster->pupil_l_obj = gfx_mesh_img_create(disp);
    lobster->pupil_r_obj = gfx_mesh_img_create(disp);

    if (lobster->tail_obj == NULL || lobster->body_obj == NULL || lobster->head_obj == NULL ||
            lobster->claw_l_obj == NULL || lobster->claw_r_obj == NULL || lobster->eye_l_obj == NULL ||
            lobster->eye_r_obj == NULL || lobster->pupil_l_obj == NULL || lobster->pupil_r_obj == NULL) {
        gfx_obj_delete(obj);
        return NULL;
    }

    gfx_mesh_img_set_grid(lobster->tail_obj, lobster->cfg.shell_segs, 1U);
    gfx_mesh_img_set_grid(lobster->body_obj, lobster->cfg.shell_segs, 1U);
    gfx_mesh_img_set_grid(lobster->head_obj, lobster->cfg.shell_segs, 1U);
    gfx_mesh_img_set_grid(lobster->claw_l_obj, lobster->cfg.claw_segs, 1U);
    gfx_mesh_img_set_grid(lobster->claw_r_obj, lobster->cfg.claw_segs, 1U);
    gfx_mesh_img_set_grid(lobster->eye_l_obj, lobster->cfg.eye_segs, 1U);
    gfx_mesh_img_set_grid(lobster->eye_r_obj, lobster->cfg.eye_segs, 1U);
    gfx_mesh_img_set_grid(lobster->pupil_l_obj, 10U, 1U);
    gfx_mesh_img_set_grid(lobster->pupil_r_obj, 10U, 1U);

    gfx_lobster_face_emote_apply_color(lobster);
    lobster->anim_timer = gfx_timer_create(disp->ctx, gfx_lobster_face_emote_anim_cb, lobster->cfg.timer_period_ms, obj);
    return obj;
}

esp_err_t gfx_lobster_face_emote_set_config(gfx_obj_t *obj, const gfx_lobster_face_emote_cfg_t *cfg)
{
    gfx_lobster_face_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");

    lobster = (gfx_lobster_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_STATE, TAG, "state is NULL");

    lobster->cfg = *cfg;
    obj->geometry.width = cfg->display_w;
    obj->geometry.height = cfg->display_h;
    return gfx_lobster_face_emote_update_pose(obj, lobster);
}

esp_err_t gfx_lobster_face_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_face_emote_assets_t *assets)
{
    gfx_lobster_face_emote_t *lobster;
    gfx_lobster_face_emote_mix_t mix = {0};
    gfx_lobster_face_emote_eye_shape_t eye_next;
    gfx_lobster_face_emote_claw_shape_t claw_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(gfx_lobster_face_emote_assets_valid(assets), ESP_ERR_INVALID_ARG, TAG, "assets invalid");

    lobster = (gfx_lobster_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_STATE, TAG, "state is NULL");

    lobster->assets = assets;
    if (assets->sequence != NULL && assets->sequence_count > 0U) {
        gfx_lobster_face_emote_expr_to_mix(&assets->sequence[0], &mix);
    }
    ESP_RETURN_ON_ERROR(gfx_lobster_face_emote_eval_mix(assets, &mix, &eye_next, &claw_next, &look_x_next, &look_y_next),
                        TAG, "eval default mix failed");
    return gfx_lobster_face_emote_set_target_pose(obj, lobster, &eye_next, &claw_next, look_x_next, look_y_next, true);
}

esp_err_t gfx_lobster_face_emote_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    gfx_lobster_face_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    lobster = (gfx_lobster_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_STATE, TAG, "state is NULL");
    lobster->color = color;
    lobster->solid_pixel = color.full;
    return gfx_lobster_face_emote_apply_color(lobster);
}

esp_err_t gfx_lobster_face_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_lobster_face_emote_t *lobster;
    gfx_lobster_face_emote_mix_t mix;
    gfx_lobster_face_emote_eye_shape_t eye_next;
    gfx_lobster_face_emote_claw_shape_t claw_next;
    int16_t look_x_next;
    int16_t look_y_next;
    size_t index;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    lobster = (gfx_lobster_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster != NULL && lobster->assets != NULL && lobster->assets->sequence != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "sequence assets not ready");

    ESP_RETURN_ON_ERROR(gfx_lobster_face_emote_find_expr_index(lobster->assets, name, &index), TAG, "expr not found");
    gfx_lobster_face_emote_expr_to_mix(&lobster->assets->sequence[index], &mix);
    ESP_RETURN_ON_ERROR(gfx_lobster_face_emote_eval_mix(lobster->assets, &mix, &eye_next, &claw_next, &look_x_next, &look_y_next),
                        TAG, "eval expression failed");
    lobster->expr_idx = index;
    return gfx_lobster_face_emote_set_target_pose(obj, lobster, &eye_next, &claw_next, look_x_next, look_y_next, snap_now);
}

esp_err_t gfx_lobster_face_emote_set_mix(gfx_obj_t *obj, const gfx_lobster_face_emote_mix_t *mix, bool snap_now)
{
    gfx_lobster_face_emote_t *lobster;
    gfx_lobster_face_emote_eye_shape_t eye_next;
    gfx_lobster_face_emote_claw_shape_t claw_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(mix != NULL, ESP_ERR_INVALID_ARG, TAG, "mix is NULL");
    lobster = (gfx_lobster_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster != NULL && lobster->assets != NULL, ESP_ERR_INVALID_STATE, TAG, "assets not ready");

    ESP_RETURN_ON_ERROR(gfx_lobster_face_emote_eval_mix(lobster->assets, mix, &eye_next, &claw_next, &look_x_next, &look_y_next),
                        TAG, "eval custom mix failed");
    return gfx_lobster_face_emote_set_target_pose(obj, lobster, &eye_next, &claw_next, look_x_next, look_y_next, snap_now);
}

esp_err_t gfx_lobster_face_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y)
{
    gfx_lobster_face_emote_t *lobster;

    CHECK_OBJ_TYPE_LOBSTER_FACE_EMOTE(obj);
    lobster = (gfx_lobster_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(lobster != NULL, ESP_ERR_INVALID_STATE, TAG, "state is NULL");
    lobster->manual_look_enabled = enabled;
    if (enabled) {
        lobster->look_x_target = look_x;
        lobster->look_y_target = look_y;
    }
    return ESP_OK;
}
