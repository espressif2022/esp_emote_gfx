/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gsp_format.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/backends/memory.h"
#include "gfx/core.h"
#include "gfx/input.h"
#include "core/display/gfx_display_priv.h"
#include "core/object/gfx_object_priv.h"
#include "platform/host/host_font_priv.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define GSP_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define GSP_WASM_EXPORT
#endif

typedef struct {
    const uint8_t *pkg;
    size_t pkg_size;
    gfx_handle_t gfx;
    gfx_display_t *disp;
    gfx_backend_t *backend;
    gsp_scene_t scene;
    uint16_t width;
    uint16_t height;
    uint32_t tick_ms;
    char last_call[96];
} gsp_wasm_runtime_t;

GSP_WASM_EXPORT gsp_wasm_runtime_t *gsp_wasm_runtime_create(const uint8_t *buf, size_t size);
GSP_WASM_EXPORT void gsp_wasm_runtime_destroy(gsp_wasm_runtime_t *rt);
GSP_WASM_EXPORT int gsp_wasm_runtime_render_into(gsp_wasm_runtime_t *rt, uint8_t *rgba,
                                                 uint32_t width, uint32_t height, uint32_t stride);

static uint32_t gsp_wasm_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t gsp_wasm_read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int gsp_wasm_read_header(const uint8_t *buf, size_t size,
                                uint16_t *width, uint16_t *height)
{
    if (buf == NULL || size < GSP_HEADER_SIZE) {
        return GSP_ERR_SIZE;
    }
    if (gsp_wasm_read_u32(buf) != GSP_MAGIC) {
        return GSP_ERR_MAGIC;
    }
    if (gsp_wasm_read_u32(buf + 4) != GSP_VERSION) {
        return GSP_ERR_VERSION;
    }
    uint32_t total = gsp_wasm_read_u32(buf + 36);
    if (total > size || total < GSP_HEADER_SIZE) {
        return GSP_ERR_BOUNDS;
    }
    if (gsp_crc32_scene(buf, total) != gsp_wasm_read_u32(buf + 40)) {
        return GSP_ERR_CRC;
    }
    if (width != NULL) {
        *width = gsp_wasm_read_u16(buf + 8);
    }
    if (height != NULL) {
        *height = gsp_wasm_read_u16(buf + 10);
    }
    return GSP_OK;
}

static void wasm_call_cb(gfx_object_t *obj, const gfx_touch_event_t *ev, void *user_data)
{
    gsp_wasm_runtime_t *rt = (gsp_wasm_runtime_t *)user_data;
    (void)obj;
    (void)ev;
    if (rt != NULL) {
        strncpy(rt->last_call, "callback", sizeof(rt->last_call) - 1U);
        rt->last_call[sizeof(rt->last_call) - 1U] = '\0';
    }
}

static void rgb_to_rgba(const uint8_t *rgb, uint8_t *rgba, uint32_t width, uint32_t height,
                        uint32_t stride)
{
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t *src = rgb + (size_t)y * width * 3U;
        uint8_t *dst = rgba + (size_t)y * stride;
        for (uint32_t x = 0; x < width; x++) {
            dst[x * 4U + 0U] = src[x * 3U + 0U];
            dst[x * 4U + 1U] = src[x * 3U + 1U];
            dst[x * 4U + 2U] = src[x * 3U + 2U];
            dst[x * 4U + 3U] = 0xffU;
        }
    }
}

static int hit_test_scene(gsp_wasm_runtime_t *rt, int32_t x, int32_t y)
{
    if (rt == NULL || rt->scene.refs == NULL) {
        return -1;
    }
    if (x < 0 || y < 0 || x >= rt->width || y >= rt->height) {
        return -1;
    }
    gfx_object_t *hit = gfx_display_hit_test(rt->disp, (uint16_t)x, (uint16_t)y);
    if (hit == NULL) {
        return -1;
    }
    for (uint16_t i = 0; i < rt->scene.ref_count; i++) {
        if (rt->scene.refs[i].obj == hit) {
            return (int)i;
        }
    }
    return -1;
}

static int inject_touch(gsp_wasm_runtime_t *rt, gfx_touch_event_type_t type, int32_t x, int32_t y)
{
    if (rt == NULL || rt->disp == NULL) {
        return GSP_ERR_BOUNDS;
    }
    if (x < 0) {
        x = 0;
    } else if (x >= rt->width) {
        x = (int32_t)rt->width - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= rt->height) {
        y = (int32_t)rt->height - 1;
    }
    rt->tick_ms += 16U;
    gfx_err_t err = gfx_touch_inject(rt->disp, &(gfx_touch_event_t) {
        .type = type,
        .x = (uint16_t)x,
        .y = (uint16_t)y,
        .strength = type == GFX_TOUCH_EVENT_RELEASE ? 0U : 1U,
        .timestamp_ms = rt->tick_ms,
    });
    if (err != GFX_OK) {
        return err;
    }
    (void)gfx_core_tick(rt->gfx);
    return GSP_OK;
}

GSP_WASM_EXPORT void *gsp_wasm_malloc(size_t size)
{
    return malloc(size);
}

GSP_WASM_EXPORT void gsp_wasm_free(void *ptr)
{
    free(ptr);
}

GSP_WASM_EXPORT int gsp_wasm_validate(const uint8_t *buf, size_t size)
{
    return gsp_wasm_read_header(buf, size, NULL, NULL);
}

GSP_WASM_EXPORT int gsp_wasm_get_width(const uint8_t *buf, size_t size)
{
    uint16_t width = 0;
    int rc = gsp_wasm_read_header(buf, size, &width, NULL);
    return rc == GSP_OK ? (int)width : rc;
}

GSP_WASM_EXPORT int gsp_wasm_get_height(const uint8_t *buf, size_t size)
{
    uint16_t height = 0;
    int rc = gsp_wasm_read_header(buf, size, NULL, &height);
    return rc == GSP_OK ? (int)height : rc;
}

GSP_WASM_EXPORT int gsp_wasm_render_into(const uint8_t *buf, size_t size,
                                         uint8_t *rgba, uint32_t width,
                                         uint32_t height, uint32_t stride)
{
    gsp_wasm_runtime_t *rt = gsp_wasm_runtime_create(buf, size);
    if (rt == NULL) {
        return GSP_ERR_CREATE;
    }
    int rc = gsp_wasm_runtime_render_into(rt, rgba, width, height, stride);
    gsp_wasm_runtime_destroy(rt);
    return rc;
}

GSP_WASM_EXPORT gsp_wasm_runtime_t *gsp_wasm_runtime_create(const uint8_t *buf, size_t size)
{
    uint16_t width = 0;
    uint16_t height = 0;
    if (gsp_wasm_read_header(buf, size, &width, &height) != GSP_OK || width == 0 || height == 0) {
        return NULL;
    }

    gsp_wasm_runtime_t *rt = (gsp_wasm_runtime_t *)calloc(1, sizeof(*rt));
    if (rt == NULL) {
        return NULL;
    }
    rt->pkg = buf;
    rt->pkg_size = size;
    rt->width = width;
    rt->height = height;

    rt->gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 60,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    if (rt->gfx == NULL) {
        free(rt);
        return NULL;
    }

    rt->backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = width,
        .v_res = height,
        .color_format = GFX_COLOR_FORMAT_RGB888,
    });
    if (rt->backend == NULL) {
        gfx_core_deinit(rt->gfx);
        free(rt);
        return NULL;
    }

    rt->disp = gfx_display_add(rt->gfx, &(gfx_display_config_t) {
        .h_res = width,
        .v_res = height,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = rt->backend,
    });
    if (rt->disp == NULL) {
        gfx_core_deinit(rt->gfx);
        free(rt);
        return NULL;
    }

    const gsp_cb_binding_t cbs[] = {
        {
            .name = "on_ok",
            .cb = wasm_call_cb,
            .user_data = rt,
        },
        {
            .name = "callback",
            .cb = wasm_call_cb,
            .user_data = rt,
        },
    };
    int rc = gsp_load(buf, size, rt->disp, gfx_host_font_default(),
                      cbs, sizeof(cbs) / sizeof(cbs[0]), &rt->scene);
    if (rc != GSP_OK) {
        gfx_core_deinit(rt->gfx);
        free(rt);
        return NULL;
    }

    for (uint16_t i = 0; i < rt->scene.ref_count; i++) {
        if (rt->scene.refs[i].type == GSP_OBJ_CONTAINER ||
                rt->scene.refs[i].type == GSP_OBJ_LAYER) {
            gfx_object_set_input_passthrough(rt->scene.refs[i].obj, true);
        }
    }

    (void)gfx_core_refresh_now(rt->gfx);
    return rt;
}

GSP_WASM_EXPORT void gsp_wasm_runtime_destroy(gsp_wasm_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }
    gsp_scene_free(&rt->scene);
    gfx_core_deinit(rt->gfx);
    free(rt);
}

GSP_WASM_EXPORT int gsp_wasm_runtime_render_into(gsp_wasm_runtime_t *rt, uint8_t *rgba,
                                                 uint32_t width, uint32_t height, uint32_t stride)
{
    if (rt == NULL || rgba == NULL || width != rt->width || height != rt->height ||
            stride < width * 4U) {
        return GSP_ERR_BOUNDS;
    }
    gfx_err_t err = gfx_core_refresh_now(rt->gfx);
    if (err != GFX_OK) {
        return err;
    }
    const uint8_t *rgb = (const uint8_t *)gfx_memory_backend_get_buffer_data(rt->backend);
    if (rgb == NULL) {
        return GSP_ERR_BOUNDS;
    }
    rgb_to_rgba(rgb, rgba, width, height, stride);
    return GSP_OK;
}

GSP_WASM_EXPORT int gsp_wasm_runtime_hit_test(gsp_wasm_runtime_t *rt, int32_t x, int32_t y)
{
    return hit_test_scene(rt, x, y);
}

GSP_WASM_EXPORT int gsp_wasm_runtime_pointer_down(gsp_wasm_runtime_t *rt, int32_t x, int32_t y)
{
    int rc = inject_touch(rt, GFX_TOUCH_EVENT_PRESS, x, y);
    return rc == GSP_OK ? hit_test_scene(rt, x, y) : rc;
}

GSP_WASM_EXPORT int gsp_wasm_runtime_pointer_move(gsp_wasm_runtime_t *rt, int32_t x, int32_t y)
{
    int rc = inject_touch(rt, GFX_TOUCH_EVENT_MOVE, x, y);
    return rc == GSP_OK ? hit_test_scene(rt, x, y) : rc;
}

GSP_WASM_EXPORT int gsp_wasm_runtime_pointer_up(gsp_wasm_runtime_t *rt, int32_t x, int32_t y)
{
    int rc = inject_touch(rt, GFX_TOUCH_EVENT_RELEASE, x, y);
    return rc == GSP_OK ? hit_test_scene(rt, x, y) : rc;
}

GSP_WASM_EXPORT int gsp_wasm_runtime_click(gsp_wasm_runtime_t *rt, int32_t x, int32_t y)
{
    int rc = inject_touch(rt, GFX_TOUCH_EVENT_PRESS, x, y);
    if (rc != GSP_OK) {
        return rc;
    }
    rc = inject_touch(rt, GFX_TOUCH_EVENT_RELEASE, x, y);
    return rc == GSP_OK ? hit_test_scene(rt, x, y) : rc;
}

GSP_WASM_EXPORT const char *gsp_wasm_runtime_last_call(gsp_wasm_runtime_t *rt)
{
    return rt != NULL && rt->last_call[0] != '\0' ? rt->last_call : NULL;
}

GSP_WASM_EXPORT void gsp_wasm_runtime_clear_last_call(gsp_wasm_runtime_t *rt)
{
    if (rt != NULL) {
        rt->last_call[0] = '\0';
    }
}
