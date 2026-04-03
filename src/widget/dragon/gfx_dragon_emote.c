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
#include "widget/dragon/gfx_dragon_emote_priv.h"

static const char *TAG = "dragon_emote";

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

static esp_err_t gfx_dragon_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_dragon_emote_delete_impl(gfx_obj_t *obj);
static void gfx_dragon_emote_anim_cb(void *user_data);

static const gfx_widget_class_t s_gfx_dragon_emote_widget_class = {
    .type = GFX_OBJ_TYPE_DRAGON_EMOTE,
    .name = "dragon_emote",
    .draw = gfx_dragon_emote_draw,
    .delete = gfx_dragon_emote_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

static void gfx_dragon_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_dragon_emote_t *dragon;

    if (obj == NULL || obj->src == NULL) return;
    dragon = (gfx_dragon_emote_t *)obj->src;
    
    gfx_dragon_emote_update_pose(obj, dragon);
}

gfx_obj_t *gfx_dragon_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h)
{
    gfx_obj_t *obj = NULL;
    gfx_dragon_emote_t *dragon = NULL;

    if (disp == NULL) return NULL;

    dragon = calloc(1, sizeof(*dragon));
    if (dragon == NULL) return NULL;

    dragon->cfg.display_w = w;
    dragon->cfg.display_h = h;
    dragon->cfg.timer_period_ms = 40;
    dragon->cfg.damping_div = 8;
    dragon->color.full = 0xFF6B4A; // Default accent

    dragon->solid_pixel = dragon->color.full;
    dragon->solid_img = s_default_img;
    dragon->solid_img.data = (const uint8_t *)&dragon->solid_pixel;

    if (gfx_obj_create_class_instance(disp, &s_gfx_dragon_emote_widget_class,
                                      dragon, w, h, "dragon_emote", &obj) != ESP_OK) {
        free(dragon);
        return NULL;
    }

    // Create parts in the same visual order as dragon_animator.html
    dragon->clawL_obj    = gfx_mesh_img_create(disp);
    dragon->body_obj     = gfx_mesh_img_create(disp);
    dragon->tail_obj     = gfx_mesh_img_create(disp);
    dragon->tail_stripe1_obj = gfx_mesh_img_create(disp);
    dragon->tail_stripe2_obj = gfx_mesh_img_create(disp);
    dragon->antennaL_obj = gfx_mesh_img_create(disp);
    dragon->antennaR_obj = gfx_mesh_img_create(disp);
    dragon->head_obj     = gfx_mesh_img_create(disp);
    dragon->eyeL_obj     = gfx_mesh_img_create(disp);
    dragon->eyeR_obj     = gfx_mesh_img_create(disp);
    dragon->eyeLineL_obj = gfx_mesh_img_create(disp);
    dragon->eyeLineR_obj = gfx_mesh_img_create(disp);
    dragon->clawR_obj    = gfx_mesh_img_create(disp);
    dragon->dot1_obj     = gfx_mesh_img_create(disp);
    dragon->dot2_obj     = gfx_mesh_img_create(disp);
    dragon->dot3_obj     = gfx_mesh_img_create(disp);

    if (dragon->clawL_obj == NULL || dragon->body_obj == NULL || dragon->tail_obj == NULL ||
            dragon->tail_stripe1_obj == NULL || dragon->tail_stripe2_obj == NULL ||
            dragon->antennaL_obj == NULL || dragon->antennaR_obj == NULL || dragon->head_obj == NULL ||
            dragon->eyeL_obj == NULL || dragon->eyeR_obj == NULL || dragon->eyeLineL_obj == NULL ||
            dragon->eyeLineR_obj == NULL || dragon->clawR_obj == NULL ||
            dragon->dot1_obj == NULL || dragon->dot2_obj == NULL || dragon->dot3_obj == NULL) {
        gfx_obj_delete(obj);
        return NULL;
    }

    // Initial state setup (X, Y, R, S)
    dragon->head.tgt.s = 100;
    dragon->body.tgt.s = 100;
    dragon->tail.tgt.s = 100;
    dragon->clawL.tgt.s = 100;
    dragon->clawR.tgt.s = 100;
    dragon->antenna.tgt.s = 100;
    dragon->eyeL.tgt.s = 100;
    dragon->eyeR.tgt.s = 100;
    dragon->dots.tgt.s = 100;
    
    dragon->head.cur = dragon->head.tgt;
    dragon->body.cur = dragon->body.tgt;
    dragon->tail.cur = dragon->tail.tgt;
    dragon->clawL.cur = dragon->clawL.tgt;
    dragon->clawR.cur = dragon->clawR.tgt;
    dragon->antenna.cur = dragon->antenna.tgt;
    dragon->eyeL.cur = dragon->eyeL.tgt;
    dragon->eyeR.cur = dragon->eyeR.tgt;
    dragon->dots.cur = dragon->dots.tgt;

    gfx_dragon_emote_set_color(obj, dragon->color);

    gfx_mesh_img_set_grid(dragon->tail_stripe1_obj, 8, 1);
    gfx_mesh_img_set_grid(dragon->tail_stripe2_obj, 8, 1);
    gfx_mesh_img_set_grid(dragon->antennaL_obj, 10, 1);
    gfx_mesh_img_set_grid(dragon->antennaR_obj, 10, 1);
    gfx_mesh_img_set_grid(dragon->eyeLineL_obj, 10, 1);
    gfx_mesh_img_set_grid(dragon->eyeLineR_obj, 10, 1);
    gfx_mesh_img_set_grid(dragon->dot1_obj, 8, 1);
    gfx_mesh_img_set_grid(dragon->dot2_obj, 8, 1);
    gfx_mesh_img_set_grid(dragon->dot3_obj, 8, 1);

    dragon->anim_timer = gfx_timer_create(disp->ctx, gfx_dragon_emote_anim_cb, dragon->cfg.timer_period_ms, obj);

    return obj;
}

esp_err_t gfx_dragon_emote_set_assets(gfx_obj_t *obj, const gfx_dragon_emote_assets_t *assets)
{
    gfx_dragon_emote_t *dragon;
    CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_DRAGON_EMOTE, TAG);
    dragon = (gfx_dragon_emote_t *)obj->src;
    dragon->assets = assets;

    if (assets) {
        // Initialize grids to match point counts (assuming even counts, rows=1)
        if (assets->count_head)    gfx_mesh_img_set_grid(dragon->head_obj,    (assets->count_head / 2) - 1,    1);
        if (assets->count_body)    gfx_mesh_img_set_grid(dragon->body_obj,    (assets->count_body / 2) - 1,    1);
        if (assets->count_tail)    gfx_mesh_img_set_grid(dragon->tail_obj,    (assets->count_tail / 2) - 1,    1);
        if (assets->count_clawL)   gfx_mesh_img_set_grid(dragon->clawL_obj,   (assets->count_clawL / 2) - 1,   1);
        if (assets->count_clawR)   gfx_mesh_img_set_grid(dragon->clawR_obj,   (assets->count_clawR / 2) - 1,   1);
        if (assets->count_antenna) gfx_mesh_img_set_grid(dragon->antennaL_obj, (assets->count_antenna / 2) - 1, 1);
        if (assets->count_antenna) gfx_mesh_img_set_grid(dragon->antennaR_obj, (assets->count_antenna / 2) - 1, 1);
        if (assets->count_eyeL)    gfx_mesh_img_set_grid(dragon->eyeL_obj,    (assets->count_eyeL / 2) - 1,    1);
        if (assets->count_eyeR)    gfx_mesh_img_set_grid(dragon->eyeR_obj,    (assets->count_eyeR / 2) - 1,    1);
    }

    return ESP_OK;
}

esp_err_t gfx_dragon_emote_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    gfx_dragon_emote_t *dragon;
    CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_DRAGON_EMOTE, TAG);
    dragon = (gfx_dragon_emote_t *)obj->src;
    dragon->color = color;
    dragon->solid_pixel = color.full;

    const gfx_img_src_t solid_src = { .type = GFX_IMG_SRC_TYPE_IMAGE_DSC, .data = &dragon->solid_img };

    gfx_mesh_img_set_src_desc(dragon->head_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->body_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->tail_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->clawL_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->clawR_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->tail_stripe1_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->tail_stripe2_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->antennaL_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->antennaR_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->eyeL_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->eyeR_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->eyeLineL_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->eyeLineR_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->dot1_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->dot2_obj, &solid_src);
    gfx_mesh_img_set_src_desc(dragon->dot3_obj, &solid_src);

    gfx_mesh_img_set_scanline_fill(dragon->head_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->body_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->tail_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->tail_stripe1_obj, true, GFX_COLOR_HEX(0x1A1A1A));
    gfx_mesh_img_set_scanline_fill(dragon->tail_stripe2_obj, true, GFX_COLOR_HEX(0x1A1A1A));
    gfx_mesh_img_set_scanline_fill(dragon->clawL_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->clawR_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->antennaL_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->antennaR_obj, true, color);
    gfx_mesh_img_set_scanline_fill(dragon->eyeL_obj, true, GFX_COLOR_HEX(0xFFFFFF));
    gfx_mesh_img_set_scanline_fill(dragon->eyeR_obj, true, GFX_COLOR_HEX(0xFFFFFF));
    gfx_mesh_img_set_scanline_fill(dragon->eyeLineL_obj, true, GFX_COLOR_HEX(0x191919));
    gfx_mesh_img_set_scanline_fill(dragon->eyeLineR_obj, true, GFX_COLOR_HEX(0x191919));
    gfx_mesh_img_set_scanline_fill(dragon->dot1_obj, true, GFX_COLOR_HEX(0xF74C49));
    gfx_mesh_img_set_scanline_fill(dragon->dot2_obj, true, GFX_COLOR_HEX(0xF74C49));
    gfx_mesh_img_set_scanline_fill(dragon->dot3_obj, true, GFX_COLOR_HEX(0xF74C49));

    return ESP_OK;
}

esp_err_t gfx_dragon_emote_set_pose_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_dragon_emote_t *dragon;
    CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_DRAGON_EMOTE, TAG);
    dragon = (gfx_dragon_emote_t *)obj->src;

    if (!dragon->assets) return ESP_ERR_INVALID_STATE;

    for (size_t i = 0; i < dragon->assets->sequence_count; i++) {
        if (strcmp(dragon->assets->sequence[i].name, name) == 0) {
            const gfx_dragon_emote_pose_t *p = &dragon->assets->sequence[i];
            dragon->head.tgt = p->head;
            dragon->body.tgt = p->body;
            dragon->tail.tgt = p->tail;
            dragon->clawL.tgt = p->clawL;
            dragon->clawR.tgt = p->clawR;
            dragon->antenna.tgt = p->antenna;
            dragon->eyeL.tgt = p->eyeL;
            dragon->eyeR.tgt = p->eyeR;
            dragon->dots.tgt = p->dots;

            if (snap_now) {
                dragon->head.cur = p->head;
                dragon->body.cur = p->body;
                dragon->tail.cur = p->tail;
                dragon->clawL.cur = p->clawL;
                dragon->clawR.cur = p->clawR;
                dragon->antenna.cur = p->antenna;
                dragon->eyeL.cur = p->eyeL;
                dragon->eyeR.cur = p->eyeR;
                dragon->dots.cur = p->dots;
            }
            dragon->pose_idx = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t gfx_dragon_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    return ESP_OK;
}

static esp_err_t gfx_dragon_emote_delete_impl(gfx_obj_t *obj)
{
    gfx_dragon_emote_t *dragon = (gfx_dragon_emote_t *)obj->src;
    if (dragon) {
        if (dragon->anim_timer) gfx_timer_delete(obj->disp->ctx, dragon->anim_timer);
        gfx_obj_delete(dragon->head_obj);
        gfx_obj_delete(dragon->body_obj);
        gfx_obj_delete(dragon->tail_obj);
        gfx_obj_delete(dragon->tail_stripe1_obj);
        gfx_obj_delete(dragon->tail_stripe2_obj);
        gfx_obj_delete(dragon->clawL_obj);
        gfx_obj_delete(dragon->clawR_obj);
        gfx_obj_delete(dragon->antennaL_obj);
        gfx_obj_delete(dragon->antennaR_obj);
        gfx_obj_delete(dragon->eyeL_obj);
        gfx_obj_delete(dragon->eyeR_obj);
        gfx_obj_delete(dragon->eyeLineL_obj);
        gfx_obj_delete(dragon->eyeLineR_obj);
        gfx_obj_delete(dragon->dot1_obj);
        gfx_obj_delete(dragon->dot2_obj);
        gfx_obj_delete(dragon->dot3_obj);
        free(dragon);
    }
    return ESP_OK;
}
