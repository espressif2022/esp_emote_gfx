/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_disp.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_display_t *gfx_display_add(gfx_handle_t handle, const gfx_display_config_t *cfg);
void gfx_display_delete(gfx_display_t *display);
void gfx_display_refresh_all(gfx_display_t *display);
bool gfx_display_flush_ready(gfx_display_t *display, bool swap_act_buf);
void *gfx_display_get_user_data(gfx_display_t *display);
uint32_t gfx_display_get_h_res(gfx_display_t *display);
uint32_t gfx_display_get_v_res(gfx_display_t *display);
gfx_err_t gfx_display_set_bg_color(gfx_display_t *display, gfx_color_t color);
gfx_err_t gfx_display_set_bg_enable(gfx_display_t *display, bool enable);
bool gfx_display_is_flushing_last(gfx_display_t *display);
gfx_err_t gfx_display_get_perf_stats(gfx_display_t *display, gfx_display_perf_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
