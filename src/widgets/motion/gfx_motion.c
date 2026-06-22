/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>

#include "common/gfx_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_MOTION
#define GFX_LOG_TAG    "gfx_motion"
#include "common/gfx_log_priv.h"

#include "core/display/gfx_display_priv.h"
#include "widgets/motion/gfx_motion_priv.h"

/*********************
 *      TYPEDEFS
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void s_timer_cb(void *user_data);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void s_timer_cb(void *user_data)
{
    gfx_motion_t *motion = (gfx_motion_t *)user_data;

    if (motion == NULL) {
        return;
    }

    (void)gfx_motion_step(motion, false);
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_motion_cfg_init(gfx_motion_cfg_t *cfg, uint16_t timer_period_ms, int16_t damping_div)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->timer_period_ms = timer_period_ms;
    cfg->damping_div     = damping_div;
}

gfx_err_t gfx_motion_init(gfx_motion_t *motion,
                          gfx_display_t *disp,
                          gfx_object_t *anchor,
                          const gfx_motion_cfg_t *cfg,
                          gfx_motion_tick_cb_t tick_cb,
                          gfx_motion_apply_cb_t apply_cb,
                          void *user_data)
{
    GFX_RETURN_ON_FALSE(motion != NULL, GFX_ERR_INVALID_ARG, TAG, "motion is NULL");
    GFX_RETURN_ON_FALSE(disp != NULL, GFX_ERR_INVALID_ARG, TAG, "disp is NULL");
    GFX_RETURN_ON_FALSE(anchor != NULL, GFX_ERR_INVALID_ARG, TAG, "anchor is NULL");
    GFX_RETURN_ON_FALSE(cfg != NULL, GFX_ERR_INVALID_ARG, TAG, "cfg is NULL");
    GFX_RETURN_ON_FALSE(tick_cb != NULL, GFX_ERR_INVALID_ARG, TAG, "tick_cb is NULL");
    GFX_RETURN_ON_FALSE(apply_cb != NULL, GFX_ERR_INVALID_ARG, TAG, "apply_cb is NULL");

    memset(motion, 0, sizeof(*motion));
    motion->disp      = disp;
    motion->anchor    = anchor;
    motion->cfg       = *cfg;
    motion->tick_cb   = tick_cb;
    motion->apply_cb  = apply_cb;
    motion->user_data = user_data;

    motion->timer = gfx_timer_create(disp->ctx, s_timer_cb, motion->cfg.timer_period_ms, motion);
    GFX_RETURN_ON_FALSE(motion->timer != NULL, GFX_ERR_NO_MEM, TAG, "create timer failed");
    return GFX_OK;
}

void gfx_motion_deinit(gfx_motion_t *motion)
{
    if (motion == NULL) {
        return;
    }
    if (motion->timer != NULL && motion->disp != NULL) {
        gfx_timer_delete(motion->disp->ctx, motion->timer);
    }
    memset(motion, 0, sizeof(*motion));
}

gfx_err_t gfx_motion_set_period(gfx_motion_t *motion, uint16_t period_ms)
{
    GFX_RETURN_ON_FALSE(motion != NULL, GFX_ERR_INVALID_ARG, TAG, "motion is NULL");
    GFX_RETURN_ON_FALSE(motion->timer != NULL, GFX_ERR_INVALID_STATE, TAG, "timer is NULL");
    GFX_RETURN_ON_FALSE(period_ms > 0U, GFX_ERR_INVALID_ARG, TAG, "period is 0");

    motion->cfg.timer_period_ms = period_ms;
    gfx_timer_set_period(motion->timer, period_ms);
    return GFX_OK;
}

gfx_timer_handle_t gfx_motion_get_timer(const gfx_motion_t *motion)
{
    return (motion != NULL) ? motion->timer : NULL;
}

gfx_err_t gfx_motion_step(gfx_motion_t *motion, bool force_apply)
{
    bool changed;

    GFX_RETURN_ON_FALSE(motion != NULL, GFX_ERR_INVALID_ARG, TAG, "motion is NULL");
    GFX_RETURN_ON_FALSE(motion->tick_cb != NULL && motion->apply_cb != NULL, GFX_ERR_INVALID_STATE, TAG, "callbacks not ready");

    changed = motion->tick_cb(motion, motion->user_data);
    if (changed || force_apply) {
        return motion->apply_cb(motion, motion->user_data, force_apply);
    }
    return GFX_OK;
}

gfx_err_t gfx_motion_apply(gfx_motion_t *motion, bool force_apply)
{
    GFX_RETURN_ON_FALSE(motion != NULL, GFX_ERR_INVALID_ARG, TAG, "motion is NULL");
    GFX_RETURN_ON_FALSE(motion->apply_cb != NULL, GFX_ERR_INVALID_STATE, TAG, "apply callback not ready");

    return motion->apply_cb(motion, motion->user_data, force_apply);
}

int16_t gfx_motion_ease_i16(int16_t cur, int16_t tgt, int16_t div)
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
