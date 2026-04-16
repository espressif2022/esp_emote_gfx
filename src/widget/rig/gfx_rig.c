/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/display/gfx_disp_priv.h"
#include "widget/gfx_rig.h"

static const char *TAG = "gfx_rig";

static void gfx_rig_timer_cb(void *user_data)
{
    gfx_rig_t *rig = (gfx_rig_t *)user_data;
    if (rig == NULL) {
        return;
    }
    (void)gfx_rig_step(rig, false);
}

void gfx_rig_cfg_init(gfx_rig_cfg_t *cfg, uint16_t timer_period_ms, int16_t damping_div)
{
    if (cfg == NULL) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->timer_period_ms = timer_period_ms;
    cfg->damping_div = damping_div;
}

esp_err_t gfx_rig_init(gfx_rig_t *rig,
                       gfx_disp_t *disp,
                       gfx_obj_t *root,
                       const gfx_rig_cfg_t *cfg,
                       gfx_rig_tick_cb_t tick_cb,
                       gfx_rig_apply_cb_t apply_cb,
                       void *user_data)
{
    ESP_RETURN_ON_FALSE(rig != NULL, ESP_ERR_INVALID_ARG, TAG, "rig is NULL");
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_INVALID_ARG, TAG, "disp is NULL");
    ESP_RETURN_ON_FALSE(root != NULL, ESP_ERR_INVALID_ARG, TAG, "root is NULL");
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is NULL");
    ESP_RETURN_ON_FALSE(tick_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "tick_cb is NULL");
    ESP_RETURN_ON_FALSE(apply_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "apply_cb is NULL");

    memset(rig, 0, sizeof(*rig));
    rig->disp = disp;
    rig->root = root;
    rig->cfg = *cfg;
    rig->tick_cb = tick_cb;
    rig->apply_cb = apply_cb;
    rig->user_data = user_data;

    rig->timer = gfx_timer_create(disp->ctx, gfx_rig_timer_cb, rig->cfg.timer_period_ms, rig);
    ESP_RETURN_ON_FALSE(rig->timer != NULL, ESP_ERR_NO_MEM, TAG, "create timer failed");
    return ESP_OK;
}

void gfx_rig_deinit(gfx_rig_t *rig)
{
    if (rig == NULL) {
        return;
    }
    if (rig->timer != NULL && rig->disp != NULL) {
        gfx_timer_delete(rig->disp->ctx, rig->timer);
    }
    memset(rig, 0, sizeof(*rig));
}

esp_err_t gfx_rig_set_period(gfx_rig_t *rig, uint16_t period_ms)
{
    ESP_RETURN_ON_FALSE(rig != NULL, ESP_ERR_INVALID_ARG, TAG, "rig is NULL");
    ESP_RETURN_ON_FALSE(rig->timer != NULL, ESP_ERR_INVALID_STATE, TAG, "timer is NULL");
    ESP_RETURN_ON_FALSE(period_ms > 0U, ESP_ERR_INVALID_ARG, TAG, "period is 0");
    rig->cfg.timer_period_ms = period_ms;
    gfx_timer_set_period(rig->timer, period_ms);
    return ESP_OK;
}

esp_err_t gfx_rig_step(gfx_rig_t *rig, bool force_apply)
{
    bool changed;

    ESP_RETURN_ON_FALSE(rig != NULL, ESP_ERR_INVALID_ARG, TAG, "rig is NULL");
    ESP_RETURN_ON_FALSE(rig->tick_cb != NULL && rig->apply_cb != NULL, ESP_ERR_INVALID_STATE, TAG, "callbacks not ready");

    changed = rig->tick_cb(rig, rig->user_data);
    if (changed || force_apply) {
        return rig->apply_cb(rig, rig->user_data, force_apply);
    }
    return ESP_OK;
}

int16_t gfx_rig_ease_i16(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }
    if (div < 1) {
        div = 1;
    }
    step = diff / div;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)(cur + step);
}

