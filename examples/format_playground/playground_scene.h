/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "gfx/error.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_err_t gfx_format_demo_build_playground_scene(gfx_display_t *disp, const char *title_text,
        const char *format_tag);
gfx_err_t gfx_format_demo_set_asset_fs(gfx_asset_source_t *fs);
gfx_err_t gfx_format_demo_mount_loose_assets(void);
void gfx_format_demo_update_perf_label(uint32_t fps, uint32_t frame_ms);

#ifdef __cplusplus
}
#endif
