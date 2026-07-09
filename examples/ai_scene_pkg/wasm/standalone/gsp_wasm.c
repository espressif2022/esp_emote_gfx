/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gsp_core.h"

#include <stdlib.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define GSP_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define GSP_WASM_EXPORT
#endif

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
    return gsp_core_validate(buf, size);
}

GSP_WASM_EXPORT int gsp_wasm_get_width(const uint8_t *buf, size_t size)
{
    gsp_core_header_t hdr;
    int rc = gsp_core_parse_header(buf, size, &hdr);
    return rc == GSP_CORE_OK ? (int)hdr.screen_w : rc;
}

GSP_WASM_EXPORT int gsp_wasm_get_height(const uint8_t *buf, size_t size)
{
    gsp_core_header_t hdr;
    int rc = gsp_core_parse_header(buf, size, &hdr);
    return rc == GSP_CORE_OK ? (int)hdr.screen_h : rc;
}

GSP_WASM_EXPORT uint8_t *gsp_wasm_render_alloc(const uint8_t *buf, size_t size,
                                               uint32_t *out_width, uint32_t *out_height)
{
    gsp_core_header_t hdr;
    if (gsp_core_parse_header(buf, size, &hdr) != GSP_CORE_OK) {
        return NULL;
    }
    uint64_t fb_size = (uint64_t)hdr.screen_w * hdr.screen_h * 4u;
    if (fb_size == 0 || fb_size > (uint64_t)SIZE_MAX) {
        return NULL;
    }
    uint8_t *rgba = (uint8_t *)malloc((size_t)fb_size);
    if (rgba == NULL) {
        return NULL;
    }
    if (gsp_core_render_rgba(buf, size, rgba, hdr.screen_w, hdr.screen_h,
                             (uint32_t)hdr.screen_w * 4u) != GSP_CORE_OK) {
        free(rgba);
        return NULL;
    }
    if (out_width != NULL) {
        *out_width = hdr.screen_w;
    }
    if (out_height != NULL) {
        *out_height = hdr.screen_h;
    }
    return rgba;
}

GSP_WASM_EXPORT int gsp_wasm_render_into(const uint8_t *buf, size_t size,
                                         uint8_t *rgba, uint32_t width,
                                         uint32_t height, uint32_t stride)
{
    return gsp_core_render_rgba(buf, size, rgba, width, height, stride);
}

GSP_WASM_EXPORT gsp_core_runtime_t *gsp_wasm_runtime_create(const uint8_t *buf, size_t size)
{
    return gsp_core_runtime_create(buf, size);
}

GSP_WASM_EXPORT void gsp_wasm_runtime_destroy(gsp_core_runtime_t *rt)
{
    gsp_core_runtime_destroy(rt);
}

GSP_WASM_EXPORT int gsp_wasm_runtime_render_into(gsp_core_runtime_t *rt,
                                                 uint8_t *rgba, uint32_t width,
                                                 uint32_t height, uint32_t stride)
{
    return gsp_core_runtime_render_rgba(rt, rgba, width, height, stride);
}

GSP_WASM_EXPORT int gsp_wasm_runtime_hit_test(gsp_core_runtime_t *rt, int32_t x, int32_t y)
{
    return gsp_core_runtime_hit_test(rt, x, y);
}

GSP_WASM_EXPORT int gsp_wasm_runtime_click(gsp_core_runtime_t *rt, int32_t x, int32_t y)
{
    return gsp_core_runtime_click(rt, x, y);
}

GSP_WASM_EXPORT const char *gsp_wasm_runtime_last_call(gsp_core_runtime_t *rt)
{
    return gsp_core_runtime_last_call(rt);
}

GSP_WASM_EXPORT void gsp_wasm_runtime_clear_last_call(gsp_core_runtime_t *rt)
{
    gsp_core_runtime_clear_last_call(rt);
}
