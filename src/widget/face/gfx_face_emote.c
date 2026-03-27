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
#include "widget/face/gfx_face_emote_priv.h"

#define CHECK_OBJ_TYPE_FACE_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_FACE_EMOTE, TAG)

static const char *TAG = "face_emote";

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

static esp_err_t gfx_face_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_face_emote_delete_impl(gfx_obj_t *obj);
static esp_err_t gfx_face_emote_update_impl(gfx_obj_t *obj);
static void gfx_face_emote_anim_cb(void *user_data);
static esp_err_t gfx_face_emote_apply_color(gfx_face_emote_t *face);

static const gfx_widget_class_t s_gfx_face_emote_widget_class = {
    .type = GFX_OBJ_TYPE_FACE_EMOTE,
    .name = "face_emote",
    .draw = gfx_face_emote_draw,
    .delete = gfx_face_emote_delete_impl,
    .update = gfx_face_emote_update_impl,
    .touch_event = NULL,
};

static void gfx_face_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_face_emote_t *face;

    if (obj == NULL || obj->src == NULL) {
        return;
    }

    face = (gfx_face_emote_t *)obj->src;
    if (face->assets == NULL) {
        return;
    }

    face->anim_tick++;
    if (gfx_face_emote_update_pose(obj, face) != ESP_OK) {
        GFX_LOGW(TAG, "animate face emote: apply pose failed");
    }
}

static esp_err_t gfx_face_emote_apply_color(gfx_face_emote_t *face)
{
    const gfx_img_src_t solid_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &face->solid_img,
    };

    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_ARG, TAG, "face state is NULL");

    if (face->mouth_obj == NULL || face->left_eye_obj == NULL || face->right_eye_obj == NULL ||
            face->left_brow_obj == NULL || face->right_brow_obj == NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(face->mouth_obj, &solid_src), TAG, "set mouth color src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(face->left_eye_obj, &solid_src), TAG, "set left eye color src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(face->right_eye_obj, &solid_src), TAG, "set right eye color src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(face->left_brow_obj, &solid_src), TAG, "set left brow color src failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(face->right_brow_obj, &solid_src), TAG, "set right brow color src failed");

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(face->mouth_obj, false, face->color), TAG, "set mouth color fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(face->left_brow_obj, false, face->color), TAG, "set left brow color fill failed");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(face->right_brow_obj, false, face->color), TAG, "set right brow color fill failed");
    return ESP_OK;
}

static esp_err_t gfx_face_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    (void)obj;
    (void)ctx;
    return ESP_OK;
}

static esp_err_t gfx_face_emote_update_impl(gfx_obj_t *obj)
{
    (void)obj;
    return ESP_OK;
}

static esp_err_t gfx_face_emote_delete_impl(gfx_obj_t *obj)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    if (face != NULL) {
        if (face->anim_timer != NULL) {
            gfx_timer_delete(obj->disp->ctx, face->anim_timer);
        }
        if (face->right_brow_obj != NULL) {
            gfx_obj_delete(face->right_brow_obj);
        }
        if (face->left_brow_obj != NULL) {
            gfx_obj_delete(face->left_brow_obj);
        }
        if (face->right_eye_obj != NULL) {
            gfx_obj_delete(face->right_eye_obj);
        }
        if (face->left_eye_obj != NULL) {
            gfx_obj_delete(face->left_eye_obj);
        }
        if (face->mouth_obj != NULL) {
            gfx_obj_delete(face->mouth_obj);
        }
        free(face);
        obj->src = NULL;
    }
    return ESP_OK;
}

void gfx_face_emote_cfg_init(gfx_face_emote_cfg_t *cfg, uint16_t display_w, uint16_t display_h,
                             uint16_t layout_ref_x, uint16_t layout_ref_y)
{
    int32_t rw;
    int32_t rh;
    int32_t refx;
    int32_t refy;
    int32_t sc;
    int32_t tb;
    int32_t tm;

    if (cfg == NULL) {
        return;
    }

    if (display_w == 0U) {
        display_w = 1U;
    }
    if (display_h == 0U) {
        display_h = 1U;
    }
    if (layout_ref_x == 0U || layout_ref_y == 0U) {
        memset(cfg, 0, sizeof(*cfg));
        cfg->display_w = display_w;
        cfg->display_h = display_h;
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->display_w = display_w;
    cfg->display_h = display_h;

    rw = (int32_t)display_w;
    rh = (int32_t)display_h;
    refx = (int32_t)layout_ref_x;
    refy = (int32_t)layout_ref_y;

    cfg->mouth_x_ofs = 0;
    cfg->mouth_y_ofs = (int16_t)((GFX_FACE_EMOTE_LAYOUT_MOUTH_Y_NUM * rh) / refy);
    cfg->eye_x_half_gap = (int16_t)((GFX_FACE_EMOTE_LAYOUT_EYE_HALF_GAP_NUM * rw) / refx);
    cfg->eye_y_ofs = (int16_t)((-(GFX_FACE_EMOTE_LAYOUT_EYE_Y_NUM) * rh) / refy);
    cfg->brow_y_ofs_extra = (int16_t)((-(GFX_FACE_EMOTE_LAYOUT_BROW_Y_EXTRA_NUM) * rh) / refy);

    sc = ((GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rw) / refx < (GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rh) / refy)
             ? ((GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rw) / refx)
             : ((GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rh) / refy);
    if (sc < 1) {
        sc = 1;
    }
    cfg->eye_scale_percent = (int16_t)sc;
    cfg->brow_scale_percent = cfg->eye_scale_percent;
    cfg->mouth_scale_percent = cfg->eye_scale_percent;

    tb = ((GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rw) / refx < (GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rh) / refy)
             ? ((GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rw) / refx)
             : ((GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rh) / refy);
    cfg->brow_thickness = (int16_t)((tb < 1) ? 1 : tb);
    tm = ((GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rw) / refx < (GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rh) / refy)
             ? ((GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rw) / refx)
             : ((GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rh) / refy);
    cfg->mouth_thickness = (int16_t)((tm < 1) ? 1 : tm);

    cfg->timer_period_ms = GFX_FACE_EMOTE_DEFAULT_TIMER_PERIOD_MS;
    cfg->eye_segs = GFX_FACE_EMOTE_DEFAULT_EYE_SEGS;
    cfg->brow_segs = GFX_FACE_EMOTE_DEFAULT_BROW_SEGS;
    cfg->mouth_segs = GFX_FACE_EMOTE_DEFAULT_MOUTH_SEGS;
}

gfx_obj_t *gfx_face_emote_create(gfx_disp_t *disp, uint16_t layout_ref_x, uint16_t layout_ref_y)
{
    gfx_obj_t *obj = NULL;
    gfx_face_emote_t *face = NULL;

    if (disp == NULL || layout_ref_x == 0U || layout_ref_y == 0U) {
        GFX_LOGE(TAG, "create face emote: display or layout reference is invalid");
        return NULL;
    }

    face = calloc(1, sizeof(*face));
    if (face == NULL) {
        GFX_LOGE(TAG, "create face emote: no mem for state");
        return NULL;
    }

    {
        uint32_t dw = gfx_disp_get_hor_res(disp);
        uint32_t dh = gfx_disp_get_ver_res(disp);

        if (dw == 0U) {
            dw = GFX_FACE_EMOTE_CREATE_FALLBACK_RES;
        }
        if (dh == 0U) {
            dh = GFX_FACE_EMOTE_CREATE_FALLBACK_RES;
        }
        if (dw > 65535U) {
            dw = 65535U;
        }
        if (dh > 65535U) {
            dh = 65535U;
        }
        gfx_face_emote_cfg_init(&face->cfg, (uint16_t)dw, (uint16_t)dh, layout_ref_x, layout_ref_y);
    }
    face->solid_pixel = s_default_pixel;
    face->solid_img = s_default_img;
    face->solid_img.data = (const uint8_t *)&face->solid_pixel;
    face->color.full = s_default_pixel;

    if (gfx_obj_create_class_instance(disp, &s_gfx_face_emote_widget_class,
                                      face, face->cfg.display_w, face->cfg.display_h,
                                      "gfx_face_emote_create", &obj) != ESP_OK) {
        free(face);
        GFX_LOGE(TAG, "create face emote: no mem for object");
        return NULL;
    }

    face->mouth_obj = gfx_mesh_img_create(disp);
    face->left_eye_obj = gfx_mesh_img_create(disp);
    face->right_eye_obj = gfx_mesh_img_create(disp);
    face->left_brow_obj = gfx_mesh_img_create(disp);
    face->right_brow_obj = gfx_mesh_img_create(disp);
    if (face->mouth_obj == NULL || face->left_eye_obj == NULL || face->right_eye_obj == NULL ||
            face->left_brow_obj == NULL || face->right_brow_obj == NULL) {
        gfx_obj_delete(obj);
        GFX_LOGE(TAG, "create face emote: create child mesh objects failed");
        return NULL;
    }

    gfx_mesh_img_set_grid(face->mouth_obj, face->cfg.mouth_segs * 2U, 1U);
    gfx_mesh_img_set_grid(face->left_eye_obj, face->cfg.eye_segs, 1U);
    gfx_mesh_img_set_grid(face->right_eye_obj, face->cfg.eye_segs, 1U);
    gfx_mesh_img_set_grid(face->left_brow_obj, face->cfg.brow_segs, 1U);
    gfx_mesh_img_set_grid(face->right_brow_obj, face->cfg.brow_segs, 1U);
    gfx_mesh_img_set_aa_inward(face->mouth_obj, true);
    gfx_mesh_img_set_aa_inward(face->left_brow_obj, true);
    gfx_mesh_img_set_aa_inward(face->right_brow_obj, true);
    gfx_mesh_img_set_wrap_cols(face->mouth_obj, true);
    gfx_face_emote_apply_color(face);

    gfx_face_emote_set_config(obj, &face->cfg);
    face->anim_timer = gfx_timer_create(disp->ctx, gfx_face_emote_anim_cb, face->cfg.timer_period_ms, obj);
    GFX_LOGD(TAG, "create face emote: object created");
    return obj;
}

esp_err_t gfx_face_emote_set_config(gfx_obj_t *obj, const gfx_face_emote_cfg_t *cfg)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");

    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_STATE, TAG, "face state is NULL");

    face->cfg = *cfg;
    obj->geometry.width = cfg->display_w;
    obj->geometry.height = cfg->display_h;
    gfx_mesh_img_set_grid(face->mouth_obj, cfg->mouth_segs * 2U, 1U);
    gfx_mesh_img_set_grid(face->left_eye_obj, cfg->eye_segs, 1U);
    gfx_mesh_img_set_grid(face->right_eye_obj, cfg->eye_segs, 1U);
    gfx_mesh_img_set_grid(face->left_brow_obj, cfg->brow_segs, 1U);
    gfx_mesh_img_set_grid(face->right_brow_obj, cfg->brow_segs, 1U);
    if (face->anim_timer != NULL) {
        gfx_timer_set_period(face->anim_timer, cfg->timer_period_ms);
    }
    return gfx_face_emote_update_pose(obj, face);
}

esp_err_t gfx_face_emote_set_assets(gfx_obj_t *obj, const gfx_face_emote_assets_t *assets)
{
    gfx_face_emote_t *face;
    gfx_face_emote_mix_t mix = {0};
    gfx_face_emote_eye_shape_t eye_next;
    gfx_face_emote_brow_shape_t brow_next;
    gfx_face_emote_mouth_shape_t mouth_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(gfx_face_emote_assets_valid(assets), ESP_ERR_INVALID_ARG, TAG, "assets are invalid");

    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_STATE, TAG, "face state is NULL");

    face->assets = assets;
    face->expr_idx = 0U;
    if (assets->sequence != NULL && assets->sequence_count > 0U) {
        gfx_face_emote_expr_to_mix(&assets->sequence[0], &mix);
    }
    face->manual_look_enabled = false;
    ESP_RETURN_ON_ERROR(gfx_face_emote_eval_mix(assets, &mix, &eye_next, &brow_next, &mouth_next,
                                                &look_x_next, &look_y_next),
                        TAG, "eval default expression failed");
    return gfx_face_emote_set_target_pose(obj, face, &eye_next, &brow_next, &mouth_next,
                                          look_x_next, look_y_next, true);
}

esp_err_t gfx_face_emote_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_STATE, TAG, "face state is NULL");

    face->color = color;
    face->solid_pixel = color.full;
    return gfx_face_emote_apply_color(face);
}

esp_err_t gfx_face_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_face_emote_t *face;
    size_t index;
    gfx_face_emote_mix_t mix;
    gfx_face_emote_eye_shape_t eye_next;
    gfx_face_emote_brow_shape_t brow_next;
    gfx_face_emote_mouth_shape_t mouth_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL && face->assets != NULL && face->assets->sequence != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "face sequence assets are not ready");
    ESP_RETURN_ON_ERROR(gfx_face_emote_find_expr_index(face->assets, name, &index),
                        TAG, "expression name not found");

    gfx_face_emote_expr_to_mix(&face->assets->sequence[index], &mix);
    ESP_RETURN_ON_ERROR(gfx_face_emote_eval_mix(face->assets, &mix, &eye_next, &brow_next, &mouth_next,
                                                &look_x_next, &look_y_next),
                        TAG, "eval expression mix failed");
    face->expr_idx = index;

    return gfx_face_emote_set_target_pose(obj, face, &eye_next, &brow_next, &mouth_next,
                                          look_x_next, look_y_next, snap_now);
}

esp_err_t gfx_face_emote_set_mix(gfx_obj_t *obj, const gfx_face_emote_mix_t *mix, bool snap_now)
{
    gfx_face_emote_t *face;
    gfx_face_emote_eye_shape_t eye_next;
    gfx_face_emote_brow_shape_t brow_next;
    gfx_face_emote_mouth_shape_t mouth_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(mix != NULL, ESP_ERR_INVALID_ARG, TAG, "mix is NULL");

    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL && face->assets != NULL, ESP_ERR_INVALID_STATE, TAG, "face assets are not ready");

    ESP_RETURN_ON_ERROR(gfx_face_emote_eval_mix(face->assets, mix, &eye_next, &brow_next, &mouth_next,
                                                &look_x_next, &look_y_next),
                        TAG, "eval custom mix failed");
    face->expr_idx = SIZE_MAX;

    return gfx_face_emote_set_target_pose(obj, face, &eye_next, &brow_next, &mouth_next,
                                          look_x_next, look_y_next, snap_now);
}

esp_err_t gfx_face_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL && face->assets != NULL, ESP_ERR_INVALID_STATE, TAG, "face assets are not ready");

    face->manual_look_enabled = enabled;
    if (enabled) {
        face->look_x_target = look_x;
        face->look_y_target = look_y;
    }
    return ESP_OK;
}
