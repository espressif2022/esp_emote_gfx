/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/base.h"
#include "gfx/fs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GFX_DISPLAY_PORT_BACKEND_EXTERNAL = 0,
    GFX_DISPLAY_PORT_BACKEND_HOST_SDL,
} gfx_display_port_backend_type_t;

typedef enum {
    GFX_DISPLAY_PORT_FS_NONE = 0,
    GFX_DISPLAY_PORT_FS_DIR,
    GFX_DISPLAY_PORT_FS_PARTITION_DIRECT,
    GFX_DISPLAY_PORT_FS_PARTITION_COPY,
    GFX_DISPLAY_PORT_FS_PACK_FILE,
} gfx_display_port_fs_type_t;

typedef struct {
    uint32_t h_res;
    uint32_t v_res;
    uint32_t fps;
    gfx_color_format_t color_format;

    gfx_display_port_backend_type_t backend_type;
    gfx_backend_t *backend;              /**< Used with GFX_DISPLAY_PORT_BACKEND_EXTERNAL. */

    struct {
        const char *title;
        uint32_t scale;
    } sdl;

    struct {
        bool manual_tick;
        gfx_core_config_t core;
    } runtime;

    struct {
        bool buff_dma;
        bool buff_spiram;
        bool double_buffer;
        bool full_frame;
        void *buf1;
        void *buf2;
        size_t buf_pixels;
    } display;

    struct {
        gfx_display_port_fs_type_t type;
        const char *path_or_label;
        bool set_default;
    } fs;
} gfx_display_port_config_t;

typedef struct {
    gfx_handle_t gfx;
    gfx_display_t *disp;
    gfx_backend_t *backend;
    gfx_fs_t *fs;
} gfx_display_port_t;

gfx_err_t gfx_display_port_open(const gfx_display_port_config_t *cfg,
                                gfx_display_port_t *out_port);
void gfx_display_port_close(gfx_display_port_t *port);

#ifdef __cplusplus
}
#endif
