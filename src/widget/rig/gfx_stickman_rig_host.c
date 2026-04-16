/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_disp_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/gfx_stickman_rig_host.h"
#include "widget/stickman/gfx_stickman_emote_priv.h"

static const char *TAG = "stickman_rig_host";

#define CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_STICKMAN_EMOTE, TAG)

static bool gfx_stickman_rig_host_tick(gfx_rig_t *rig, void *user_data)
{
    gfx_stickman_rig_host_t *host = (gfx_stickman_rig_host_t *)user_data;
    gfx_obj_t *obj;
    gfx_stickman_emote_t *stickman;

    if (rig == NULL || host == NULL) {
        return false;
    }
    obj = host->stickman_obj;
    if (obj == NULL || obj->src == NULL) {
        return false;
    }
    stickman = (gfx_stickman_emote_t *)obj->src;
    if (stickman->export_data == NULL) {
        return false;
    }
    return gfx_stickman_emote_update_pose(obj, stickman);
}

static esp_err_t gfx_stickman_rig_host_apply(gfx_rig_t *rig, void *user_data, bool force_apply)
{
    gfx_stickman_rig_host_t *host = (gfx_stickman_rig_host_t *)user_data;
    gfx_obj_t *obj;
    gfx_stickman_emote_t *stickman;

    if (rig == NULL || host == NULL) {
        return ESP_OK;
    }
    obj = host->stickman_obj;
    if (obj == NULL || obj->src == NULL) {
        return ESP_OK;
    }
    stickman = (gfx_stickman_emote_t *)obj->src;
    if (stickman->export_data == NULL) {
        return ESP_OK;
    }
    if (!force_apply && !stickman->mesh_dirty) {
        return ESP_OK;
    }
    return gfx_stickman_emote_sync_meshes(obj, stickman);
}

void gfx_stickman_rig_host_init(gfx_stickman_rig_host_t *host)
{
    if (host == NULL) {
        return;
    }
    memset(host, 0, sizeof(*host));
}

esp_err_t gfx_stickman_rig_host_attach(gfx_stickman_rig_host_t *host, gfx_disp_t *disp, gfx_obj_t *stickman_obj)
{
    gfx_stickman_emote_t *stickman;
    gfx_rig_cfg_t rig_cfg;

    ESP_RETURN_ON_FALSE(host != NULL, ESP_ERR_INVALID_ARG, TAG, "host is NULL");
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_INVALID_ARG, TAG, "disp is NULL");
    ESP_RETURN_ON_FALSE(stickman_obj != NULL, ESP_ERR_INVALID_ARG, TAG, "stickman is NULL");
    CHECK_OBJ_TYPE_STICKMAN_EMOTE(stickman_obj);

    stickman = (gfx_stickman_emote_t *)stickman_obj->src;
    ESP_RETURN_ON_FALSE(stickman != NULL, ESP_ERR_INVALID_STATE, TAG, "stickman state is NULL");

    if (host->attached) {
        gfx_stickman_rig_host_detach(host);
    }

    if (stickman->anim_timer != NULL) {
        gfx_timer_delete(stickman_obj->disp->ctx, stickman->anim_timer);
        stickman->anim_timer = NULL;
    }

    host->stickman_obj = stickman_obj;
    gfx_rig_cfg_init(&rig_cfg, stickman->cfg.timer_period_ms, stickman->cfg.damping_div);
    ESP_RETURN_ON_ERROR(gfx_rig_init(&host->rig, disp, stickman_obj, &rig_cfg,
                                     gfx_stickman_rig_host_tick, gfx_stickman_rig_host_apply, host),
                        TAG, "rig init failed");

    host->attached = true;
    (void)gfx_rig_step(&host->rig, true);
    return ESP_OK;
}

void gfx_stickman_rig_host_detach(gfx_stickman_rig_host_t *host)
{
    gfx_obj_t *obj;
    gfx_stickman_emote_t *stickman;

    if (host == NULL || !host->attached) {
        return;
    }

    gfx_rig_deinit(&host->rig);

    obj = host->stickman_obj;
    if (obj != NULL && obj->src != NULL) {
        stickman = (gfx_stickman_emote_t *)obj->src;
        if (stickman->anim_timer == NULL && obj->disp != NULL) {
            stickman->anim_timer = gfx_timer_create(obj->disp->ctx, gfx_stickman_emote_anim_cb,
                                                    stickman->cfg.timer_period_ms, obj);
            if (stickman->anim_timer == NULL) {
                GFX_LOGW(TAG, "restore stickman anim timer failed");
            }
        }
    }

    host->stickman_obj = NULL;
    host->attached = false;
}

esp_err_t gfx_stickman_rig_host_sync_period(gfx_stickman_rig_host_t *host)
{
    gfx_obj_t *obj;
    gfx_stickman_emote_t *stickman;

    ESP_RETURN_ON_FALSE(host != NULL, ESP_ERR_INVALID_ARG, TAG, "host is NULL");
    ESP_RETURN_ON_FALSE(host->attached, ESP_ERR_INVALID_STATE, TAG, "host not attached");

    obj = host->stickman_obj;
    ESP_RETURN_ON_FALSE(obj != NULL && obj->src != NULL, ESP_ERR_INVALID_STATE, TAG, "stickman is not ready");
    stickman = (gfx_stickman_emote_t *)obj->src;

    return gfx_rig_set_period(&host->rig, stickman->cfg.timer_period_ms);
}
