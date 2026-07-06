/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "gfx/backends/custom.h"
#include "core/display/gfx_backend_priv.h"

#define GFX_STATIC_ASSERT(cond, name) typedef char static_assert_##name[(cond) ? 1 : -1]

GFX_STATIC_ASSERT(sizeof(gfx_custom_backend_alignment_t) == sizeof(gfx_render_alignment_t),
                  backend_alignment_layout);
GFX_STATIC_ASSERT(sizeof(gfx_custom_backend_surface_t) == sizeof(gfx_backend_surface_t),
                  backend_surface_layout);
GFX_STATIC_ASSERT(sizeof(gfx_custom_backend_image_t) == sizeof(gfx_backend_image_t),
                  backend_image_layout);
GFX_STATIC_ASSERT(sizeof(gfx_custom_backend_ops_t) == sizeof(gfx_backend_ops_t),
                  backend_ops_layout);
GFX_STATIC_ASSERT(sizeof(gfx_custom_backend_draw_ops_t) == sizeof(gfx_draw_ops_t),
                  backend_draw_ops_layout);

gfx_backend_t *gfx_custom_backend_create(const gfx_custom_backend_config_t *cfg)
{
    if (cfg == NULL) {
        return NULL;
    }

    return gfx_backend_create_custom((const gfx_backend_ops_t *)cfg->ops,
                                     (const gfx_draw_ops_t *)cfg->draw_ops,
                                     *(const gfx_render_alignment_t *)&cfg->alignment,
                                     cfg->caps,
                                     cfg->user_data);
}

void gfx_custom_backend_delete(gfx_backend_t *backend)
{
    gfx_backend_destroy(backend);
}
