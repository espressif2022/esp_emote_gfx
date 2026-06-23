/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx_display_port.h"

#include <string.h>

#if defined(GFX_HOST_BUILD)
#include "gfx/backends/sdl.h"
#endif

static gfx_err_t display_port_open_fs(const gfx_display_port_config_t *cfg, gfx_fs_t **out_fs)
{
    gfx_fs_open_config_t fs_cfg;

    if (out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_fs = NULL;

    if (cfg->fs.type == GFX_DISPLAY_PORT_FS_NONE) {
        return GFX_OK;
    }
    if (cfg->fs.path_or_label == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    switch (cfg->fs.type) {
    case GFX_DISPLAY_PORT_FS_DIR:
        return gfx_fs_open_dir(cfg->fs.path_or_label, out_fs);
    case GFX_DISPLAY_PORT_FS_PARTITION_DIRECT:
        fs_cfg = (gfx_fs_open_config_t) {
            .source_type = GFX_FS_SOURCE_PARTITION,
            .access_mode = GFX_FS_ACCESS_DIRECT,
            .path_or_label = cfg->fs.path_or_label,
        };
        return gfx_fs_open(&fs_cfg, out_fs);
    case GFX_DISPLAY_PORT_FS_PARTITION_COPY:
        fs_cfg = (gfx_fs_open_config_t) {
            .source_type = GFX_FS_SOURCE_PARTITION,
            .access_mode = GFX_FS_ACCESS_COPY,
            .path_or_label = cfg->fs.path_or_label,
        };
        return gfx_fs_open(&fs_cfg, out_fs);
    case GFX_DISPLAY_PORT_FS_PACK_FILE:
        fs_cfg = (gfx_fs_open_config_t) {
            .source_type = GFX_FS_SOURCE_PACK_FILE,
            .access_mode = GFX_FS_ACCESS_COPY,
            .path_or_label = cfg->fs.path_or_label,
        };
        return gfx_fs_open(&fs_cfg, out_fs);
    default:
        return GFX_ERR_INVALID_ARG;
    }
}

static gfx_backend_t *display_port_create_backend(const gfx_display_port_config_t *cfg)
{
    switch (cfg->backend_type) {
    case GFX_DISPLAY_PORT_BACKEND_EXTERNAL:
        return cfg->backend;
    case GFX_DISPLAY_PORT_BACKEND_HOST_SDL:
#if defined(GFX_HOST_BUILD)
        return gfx_backend_sdl_create(&(gfx_backend_sdl_config_t) {
            .h_res = cfg->h_res,
            .v_res = cfg->v_res,
            .scale = cfg->sdl.scale,
            .title = cfg->sdl.title,
        });
#else
        return NULL;
#endif
    default:
        return NULL;
    }
}

static void display_port_delete_backend_if_owned(const gfx_display_port_config_t *cfg, gfx_backend_t *backend)
{
    if (cfg == NULL || backend == NULL) {
        return;
    }

#if defined(GFX_HOST_BUILD)
    if (cfg->backend_type == GFX_DISPLAY_PORT_BACKEND_HOST_SDL) {
        gfx_backend_sdl_delete(backend);
    }
#else
    (void)cfg;
    (void)backend;
#endif
}

gfx_err_t gfx_display_port_open(const gfx_display_port_config_t *cfg,
                                gfx_display_port_t *out_port)
{
    gfx_display_port_t port;
    gfx_core_config_t core_cfg;
    gfx_err_t err;

    if (cfg == NULL || out_port == NULL || cfg->h_res == 0U || cfg->v_res == 0U) {
        return GFX_ERR_INVALID_ARG;
    }

    memset(&port, 0, sizeof(port));
    core_cfg = cfg->runtime.core;
    if (core_cfg.fps == 0U) {
        core_cfg.fps = cfg->fps > 0U ? cfg->fps : 30U;
    }
    if (core_cfg.task.task_stack == 0U) {
        const gfx_core_config_t default_core = {
            .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
        };
        core_cfg.task = default_core.task;
    }
    core_cfg.manual_tick = cfg->runtime.manual_tick;

    err = display_port_open_fs(cfg, &port.fs);
    if (err != GFX_OK) {
        return err;
    }
    if (cfg->fs.set_default && port.fs != NULL) {
        gfx_fs_set_default(port.fs);
    }

    port.backend = display_port_create_backend(cfg);
    if (port.backend == NULL) {
        gfx_display_port_close(&port);
        return GFX_ERR_INVALID_STATE;
    }

    port.gfx = gfx_core_init(&core_cfg);
    if (port.gfx == NULL) {
        display_port_delete_backend_if_owned(cfg, port.backend);
        port.backend = NULL;
        gfx_display_port_close(&port);
        return GFX_FAIL;
    }

    port.disp = gfx_display_add(port.gfx, &(gfx_display_config_t) {
        .h_res = cfg->h_res,
        .v_res = cfg->v_res,
        .color_format = cfg->color_format,
        .backend = port.backend,
        .flags = {
            .buff_dma = cfg->display.buff_dma,
            .buff_spiram = cfg->display.buff_spiram,
            .double_buffer = cfg->display.double_buffer,
            .full_frame = cfg->display.full_frame,
        },
        .buffers = {
            .buf1 = cfg->display.buf1,
            .buf2 = cfg->display.buf2,
            .buf_pixels = cfg->display.buf_pixels,
        },
    });
    if (port.disp == NULL) {
        if (cfg->backend_type == GFX_DISPLAY_PORT_BACKEND_HOST_SDL) {
            display_port_delete_backend_if_owned(cfg, port.backend);
            port.backend = NULL;
        }
        gfx_display_port_close(&port);
        return GFX_FAIL;
    }

    port.backend = NULL; /* Display owns the backend after successful add. */
    *out_port = port;
    return GFX_OK;
}

void gfx_display_port_close(gfx_display_port_t *port)
{
    if (port == NULL) {
        return;
    }

    if (port->gfx != NULL) {
        gfx_core_deinit(port->gfx);
        port->gfx = NULL;
        port->disp = NULL;
    }
    if (port->fs != NULL) {
        if (gfx_fs_get_default() == port->fs) {
            gfx_fs_set_default(NULL);
        }
        gfx_fs_close(port->fs);
        port->fs = NULL;
    }
    memset(port, 0, sizeof(*port));
}
