/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "core/gfx_obj.h"
#include "widget/gfx_rig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Optional pose driver for `gfx_stickman_emote` using `gfx_rig`.
 *
 * Keeps stickman widget implementation unchanged: this host suspends the widget's
 * internal `anim_timer` while attached, drives pose with `gfx_rig`, then restores
 * the internal timer on detach.
 */
typedef struct {
    gfx_rig_t rig;
    gfx_obj_t *stickman_obj;
    bool attached;
} gfx_stickman_rig_host_t;

void gfx_stickman_rig_host_init(gfx_stickman_rig_host_t *host);

esp_err_t gfx_stickman_rig_host_attach(gfx_stickman_rig_host_t *host, gfx_disp_t *disp, gfx_obj_t *stickman_obj);

void gfx_stickman_rig_host_detach(gfx_stickman_rig_host_t *host);

/** Call after `gfx_stickman_emote_set_export` if the rig host is attached (internal timer is NULL). */
esp_err_t gfx_stickman_rig_host_sync_period(gfx_stickman_rig_host_t *host);

#ifdef __cplusplus
}
#endif
