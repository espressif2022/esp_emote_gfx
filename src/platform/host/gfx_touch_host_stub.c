/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core/runtime/gfx_touch_priv.h"
#include <stddef.h>
#include "core/gfx_touch.h"

void gfx_touch_delete_all(struct gfx_core_context *ctx)
{
    (void)ctx;
}

gfx_touch_t *gfx_touch_add(gfx_handle_t handle, const gfx_touch_config_t *cfg)
{
    (void)handle;
    (void)cfg;
    return NULL;
}

gfx_err_t gfx_touch_set_disp(gfx_touch_t *touch, gfx_display_t *disp)
{
    (void)touch;
    (void)disp;
    return GFX_ERR_NOT_SUPPORTED;
}

void gfx_touch_delete(gfx_touch_t *touch)
{
    (void)touch;
}
