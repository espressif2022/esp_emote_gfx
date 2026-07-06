/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if __has_include("driver/ppa.h") && defined(CONFIG_SOC_PPA_SUPPORTED) && CONFIG_SOC_PPA_SUPPORTED

#include <string.h>

#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "hal/color_types.h"
#include "common/gfx_check.h"
#include "platform/esp_idf/gfx_err_bridge.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "common/gfx_types_priv.h"
#include "platform/esp_idf/esp_idf_flush_staging.h"
#include "platform/gfx_platform_accel_priv.h"

#ifndef GFX_PLATFORM_PPA_MIN_BLEND_PIXELS
#define GFX_PLATFORM_PPA_MIN_BLEND_PIXELS 100
#endif

#ifndef GFX_PLATFORM_PPA_DEFAULT_ADDR_ALIGN
#define GFX_PLATFORM_PPA_DEFAULT_ADDR_ALIGN 128
#endif

static const char *const TAG = "plat_accel";

typedef struct {
    bool inited;
    ppa_client_handle_t fill_client;
    ppa_client_handle_t srm_client;
    ppa_client_handle_t blend_client;
} gfx_platform_ppa_state_t;

static gfx_platform_ppa_state_t s_ppa;

static gfx_render_alignment_t gfx_platform_ppa_alignment(void);

static size_t gfx_platform_ppa_align_up(size_t value, size_t align)
{
    if (align <= 1U) {
        return value;
    }
    return (value + align - 1U) & ~(align - 1U);
}

static size_t gfx_platform_ppa_cache_line_size(const void *addr)
{
    size_t align = 0;

    if (addr == NULL) {
        return 0U;
    }

    if (esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &align) != ESP_OK || align == 0U) {
        if (esp_cache_get_alignment(MALLOC_CAP_INTERNAL, &align) != ESP_OK || align == 0U) {
            align = GFX_PLATFORM_PPA_DEFAULT_ADDR_ALIGN;
        }
    }

    return align;
}

static size_t gfx_platform_ppa_aligned_buffer_size(const void *addr, size_t size)
{
    size_t align = gfx_platform_ppa_cache_line_size(addr);

    return align > 0U ? gfx_platform_ppa_align_up(size, align) : size;
}

static void gfx_platform_ppa_cache_msync(const void *addr, size_t size, int flags)
{
    size_t align = gfx_platform_ppa_cache_line_size(addr);

    if (addr == NULL || size == 0U || align == 0U) {
        return;
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t aligned_start = start & ~((uintptr_t)align - 1U);
    size_t aligned_size = gfx_platform_ppa_align_up(size + (size_t)(start - aligned_start), align);

    (void)esp_cache_msync((void *)aligned_start, aligned_size, flags);
}

static void gfx_platform_ppa_cache_msync_block(const gfx_backend_surface_t *surface,
        const gfx_area_t *block,
        int flags)
{
    if (surface == NULL || block == NULL || surface->buf == NULL) {
        return;
    }

    uint32_t px_size = gfx_color_format_get_size(surface->format);
    if (px_size == 0U) {
        return;
    }

    if (block->x1 < surface->area.x1 || block->y1 < surface->area.y1 ||
            block->x2 > surface->area.x2 || block->y2 > surface->area.y2) {
        return;
    }

    gfx_coord_t off_y = block->y1 - surface->area.y1;
    uint8_t *start = (uint8_t *)surface->buf + (size_t)off_y * (size_t)surface->stride * px_size;
    size_t bytes = (size_t)surface->stride * (size_t)(block->y2 - block->y1) * px_size;

    gfx_platform_ppa_cache_msync(start, bytes, flags);
}

static const char *gfx_platform_ppa_format_name(gfx_color_format_t format)
{
    switch (format) {
    case GFX_COLOR_FORMAT_RGB565:
        return "RGB565";
    case GFX_COLOR_FORMAT_RGB565_SWAPPED:
        return "RGB565_SWAPPED";
    case GFX_COLOR_FORMAT_RGB888:
        return "RGB888";
    case GFX_COLOR_FORMAT_BGR888:
        return "BGR888";
    case GFX_COLOR_FORMAT_XRGB8888:
        return "XRGB8888";
    case GFX_COLOR_FORMAT_ARGB8888:
        return "ARGB8888";
    case GFX_COLOR_FORMAT_RGB888A8:
        return "RGB888A8";
    case GFX_COLOR_FORMAT_RGB565A8:
        return "RGB565A8";
    case GFX_COLOR_FORMAT_RGB565A8_SWAPPED:
        return "RGB565A8_SWAPPED";
    default:
        return "UNKNOWN";
    }
}

static bool gfx_platform_ppa_srm_input_rgb_swap(gfx_color_format_t format)
{
    if (format != GFX_COLOR_FORMAT_RGB888) {
        return false;
    }

#if defined(ESP_COLOR_FOURCC_BGR24)
    if ((uint32_t)PPA_SRM_COLOR_MODE_RGB888 == (uint32_t)ESP_COLOR_FOURCC_BGR24) {
        return true;
    }
#endif

    return false;
}

static void gfx_platform_ppa_log_capability_summary(void)
{
    const char *rgb888_src_policy = gfx_platform_ppa_srm_input_rgb_swap(GFX_COLOR_FORMAT_RGB888)
                                    ? "rgb_swap"
                                    : "direct";

    GFX_LOGI(TAG,
             "PPA enabled: ops=fill|blit|blend|scale|transform, align=%ux%u stride=%u addr=%u",
             (unsigned)gfx_platform_ppa_alignment().width_px,
             (unsigned)gfx_platform_ppa_alignment().height_px,
             (unsigned)gfx_platform_ppa_alignment().stride_bytes,
             (unsigned)gfx_platform_ppa_alignment().addr_bytes);
    GFX_LOGI(TAG,
             "PPA format policy: fill/blend dst={RGB565,RGB888,ARGB8888}, srm dst={RGB565,RGB888,ARGB8888}, srm src={RGB565,RGB888,ARGB8888}, blend fg={RGB565,ARGB8888}, rgb888-src=%s, excluded={*SWAPPED,BGR888,XRGB8888,*A8 plane}",
             rgb888_src_policy);
}

static void gfx_platform_ppa_log_reject_once(const char *op,
        gfx_color_format_t dst_format,
        gfx_color_format_t src_format,
        gfx_opa_t opa,
        const char *reason)
{
    static bool s_fill_logged;
    static bool s_blit_logged;
    static bool s_blend_logged;
    static bool s_scale_logged;
    static bool s_transform_logged;
    bool *flag = NULL;

    if (strcmp(op, "fill") == 0) {
        flag = &s_fill_logged;
    } else if (strcmp(op, "blit") == 0) {
        flag = &s_blit_logged;
    } else if (strcmp(op, "blend") == 0) {
        flag = &s_blend_logged;
    } else if (strcmp(op, "scale") == 0) {
        flag = &s_scale_logged;
    } else if (strcmp(op, "transform") == 0) {
        flag = &s_transform_logged;
    }

    if (flag == NULL || *flag) {
        return;
    }
    *flag = true;

    GFX_LOGI(TAG,
             "PPA skip %s: dst=%s src=%s opa=%u reason=%s",
             op,
             gfx_platform_ppa_format_name(dst_format),
             gfx_platform_ppa_format_name(src_format),
             (unsigned)opa,
             reason);
}

static gfx_render_alignment_t gfx_platform_ppa_alignment(void)
{
    return (gfx_render_alignment_t) {
        .width_px = 1,
        .height_px = 1,
        .stride_bytes = 64,
        .addr_bytes = 64,
    };
}

static bool gfx_platform_ppa_is_supported_dst_format(gfx_color_format_t format)
{
    return format == GFX_COLOR_FORMAT_RGB565 ||
           format == GFX_COLOR_FORMAT_RGB888 ||
           format == GFX_COLOR_FORMAT_ARGB8888;
}

static bool gfx_platform_ppa_is_supported_srm_src_format(gfx_color_format_t format)
{
    return format == GFX_COLOR_FORMAT_RGB565 ||
           format == GFX_COLOR_FORMAT_RGB888 ||
           format == GFX_COLOR_FORMAT_ARGB8888;
}

static bool gfx_platform_ppa_format_is_byte_swapped(gfx_color_format_t format)
{
    return format == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
           format == GFX_COLOR_FORMAT_BGR888 ||
           format == GFX_COLOR_FORMAT_XRGB8888;
}

static ppa_fill_color_mode_t gfx_platform_ppa_to_fill_mode(gfx_color_format_t format)
{
    switch (format) {
    case GFX_COLOR_FORMAT_RGB565:
        return PPA_FILL_COLOR_MODE_RGB565;
    case GFX_COLOR_FORMAT_RGB888:
        return PPA_FILL_COLOR_MODE_RGB888;
    case GFX_COLOR_FORMAT_ARGB8888:
        return PPA_FILL_COLOR_MODE_ARGB8888;
    default:
        return (ppa_fill_color_mode_t)0;
    }
}

static ppa_srm_color_mode_t gfx_platform_ppa_to_srm_mode(gfx_color_format_t format)
{
    switch (format) {
    case GFX_COLOR_FORMAT_RGB565:
        return PPA_SRM_COLOR_MODE_RGB565;
    case GFX_COLOR_FORMAT_RGB888:
        return PPA_SRM_COLOR_MODE_RGB888;
    case GFX_COLOR_FORMAT_ARGB8888:
        return PPA_SRM_COLOR_MODE_ARGB8888;
    default:
        return (ppa_srm_color_mode_t)0;
    }
}

static ppa_blend_color_mode_t gfx_platform_ppa_to_blend_mode(gfx_color_format_t format)
{
    switch (format) {
    case GFX_COLOR_FORMAT_RGB565:
        return PPA_BLEND_COLOR_MODE_RGB565;
    case GFX_COLOR_FORMAT_RGB888:
        return PPA_BLEND_COLOR_MODE_RGB888;
    case GFX_COLOR_FORMAT_ARGB8888:
        return PPA_BLEND_COLOR_MODE_ARGB8888;
    default:
        return (ppa_blend_color_mode_t)0;
    }
}

static bool gfx_platform_ppa_angle_to_rotation(int16_t angle_deg, ppa_srm_rotation_angle_t *rotation)
{
    int16_t normalized = angle_deg % 360;

    if (normalized < 0) {
        normalized += 360;
    }

    if (rotation == NULL) {
        return false;
    }

    switch (normalized) {
    case 0:
        *rotation = PPA_SRM_ROTATION_ANGLE_0;
        return true;
    case 90:
        *rotation = PPA_SRM_ROTATION_ANGLE_270;
        return true;
    case 180:
        *rotation = PPA_SRM_ROTATION_ANGLE_180;
        return true;
    case 270:
        *rotation = PPA_SRM_ROTATION_ANGLE_90;
        return true;
    default:
        return false;
    }
}

static bool gfx_platform_ppa_make_fill_color(gfx_color_format_t dst_format,
        gfx_color_t color,
        color_pixel_argb8888_data_t *out_color)
{
    if (out_color == NULL) {
        return false;
    }

    uint8_t bytes[4] = {0};
    uint8_t r;
    uint8_t g;
    uint8_t b;

    gfx_color_write_rgb565_bytes(bytes, GFX_COLOR_FORMAT_RGB565, color.full);
    uint16_t semantic = gfx_color_read_rgb565_bytes(bytes, GFX_COLOR_FORMAT_RGB565);
    gfx_color_to_rgb888(semantic, &r, &g, &b);

    out_color->a = 0xffU;
    out_color->r = r;
    out_color->g = g;
    out_color->b = b;
    return gfx_platform_ppa_is_supported_dst_format(dst_format);
}

static size_t gfx_platform_ppa_surface_bytes(const gfx_backend_surface_t *surface)
{
    if (surface == NULL || surface->stride <= 0 ||
            surface->area.x2 <= surface->area.x1 || surface->area.y2 <= surface->area.y1) {
        return 0U;
    }

    uint32_t h = (uint32_t)(surface->area.y2 - surface->area.y1);
    uint32_t stride_bytes = (uint32_t)surface->stride * gfx_color_format_get_size(surface->format);
    return (size_t)stride_bytes * h;
}

static bool gfx_platform_ppa_blend_pair_supported(gfx_color_format_t dst_format,
        gfx_color_format_t src_format,
        bool src_has_plane_alpha)
{
    if (src_has_plane_alpha ||
            gfx_platform_ppa_format_is_byte_swapped(dst_format) ||
            gfx_platform_ppa_format_is_byte_swapped(src_format)) {
        return false;
    }

    if (dst_format == GFX_COLOR_FORMAT_RGB565 && src_format == GFX_COLOR_FORMAT_RGB565) {
        return true;
    }

    if (dst_format == GFX_COLOR_FORMAT_RGB888 && src_format == GFX_COLOR_FORMAT_ARGB8888) {
        return true;
    }

    if (dst_format == GFX_COLOR_FORMAT_ARGB8888 && src_format == GFX_COLOR_FORMAT_ARGB8888) {
        return true;
    }

    return false;
}

static gfx_err_t gfx_platform_ppa_run_srm(const gfx_backend_surface_t *dst,
        const gfx_area_t *dst_area,
        const gfx_backend_image_t *src,
        uint32_t src_block_w,
        uint32_t src_block_h,
        uint32_t src_off_x,
        uint32_t src_off_y,
        uint32_t src_pic_h,
        ppa_srm_rotation_angle_t rotation,
        float scale_x,
        float scale_y)
{
    ppa_srm_oper_config_t cfg = {
        .in.buffer = src->pixels,
        .in.pic_w = (uint32_t)src->stride,
        .in.pic_h = src_pic_h,
        .in.block_w = src_block_w,
        .in.block_h = src_block_h,
        .in.block_offset_x = src_off_x,
        .in.block_offset_y = src_off_y,
        .in.srm_cm = gfx_platform_ppa_to_srm_mode(src->format),
        .out.buffer = dst->buf,
        .out.buffer_size = gfx_platform_ppa_surface_bytes(dst),
        .out.pic_w = (uint32_t)dst->stride,
        .out.pic_h = (uint32_t)(dst->area.y2 - dst->area.y1),
        .out.block_offset_x = (uint32_t)(dst_area->x1 - dst->area.x1),
        .out.block_offset_y = (uint32_t)(dst_area->y1 - dst->area.y1),
        .out.srm_cm = gfx_platform_ppa_to_srm_mode(dst->format),
        .rotation_angle = rotation,
        .scale_x = scale_x,
        .scale_y = scale_y,
        .rgb_swap = gfx_platform_ppa_srm_input_rgb_swap(src->format),
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    uint32_t px_size = gfx_color_format_get_size(src->format);
    const uint8_t *src_start = (const uint8_t *)src->pixels +
                               (size_t)src_off_y * (size_t)src->stride * px_size;

    gfx_platform_ppa_cache_msync(src_start, (size_t)src->stride * src_block_h * px_size,
                                 ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    gfx_err_t ret = gfx_err_from_esp(ppa_do_scale_rotate_mirror(s_ppa.srm_client, &cfg));

    gfx_platform_ppa_cache_msync_block(dst, dst_area, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    return ret;
}

static gfx_err_t gfx_platform_ppa_fill(gfx_backend_t *backend, gfx_display_t *disp,
                                       const gfx_backend_surface_t *dst, const gfx_area_t *area,
                                       gfx_color_t color, gfx_opa_t opa)
{
    (void)backend;
    (void)disp;

    if (!s_ppa.inited || s_ppa.fill_client == NULL || dst == NULL || area == NULL ||
            dst->buf == NULL || opa != 0xffU ||
            !gfx_platform_ppa_is_supported_dst_format(dst->format) ||
            gfx_platform_ppa_format_is_byte_swapped(dst->format)) {
        if (dst != NULL) {
            gfx_platform_ppa_log_reject_once("fill", dst->format, GFX_COLOR_FORMAT_UNKNOWN, opa,
                                             "unsupported dst/opa/state");
        }
        return GFX_ERR_NOT_SUPPORTED;
    }

    color_pixel_argb8888_data_t fill_color;
    if (!gfx_platform_ppa_make_fill_color(dst->format, color, &fill_color)) {
        gfx_platform_ppa_log_reject_once("fill", dst->format, GFX_COLOR_FORMAT_UNKNOWN, opa,
                                         "fill color conversion");
        return GFX_ERR_NOT_SUPPORTED;
    }

    ppa_fill_oper_config_t cfg = {
        .out.buffer = dst->buf,
        .out.buffer_size = gfx_platform_ppa_surface_bytes(dst),
        .out.pic_w = (uint32_t)dst->stride,
        .out.pic_h = (uint32_t)(dst->area.y2 - dst->area.y1),
        .out.block_offset_x = (uint32_t)(area->x1 - dst->area.x1),
        .out.block_offset_y = (uint32_t)(area->y1 - dst->area.y1),
        .out.fill_cm = gfx_platform_ppa_to_fill_mode(dst->format),
        .fill_block_w = (uint32_t)(area->x2 - area->x1),
        .fill_block_h = (uint32_t)(area->y2 - area->y1),
        .fill_argb_color = fill_color,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    gfx_err_t ret = gfx_err_from_esp(ppa_do_fill(s_ppa.fill_client, &cfg));
    gfx_platform_ppa_cache_msync_block(dst, area, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    return ret;
}

static gfx_err_t gfx_platform_ppa_blit(gfx_backend_t *backend, gfx_display_t *disp,
                                       const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                       const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y)
{
    (void)backend;
    (void)disp;

    if (!s_ppa.inited || s_ppa.srm_client == NULL || dst == NULL || dst_area == NULL ||
            src == NULL || src->pixels == NULL ||
            !gfx_platform_ppa_is_supported_dst_format(dst->format) ||
            !gfx_platform_ppa_is_supported_srm_src_format(src->format) ||
            gfx_platform_ppa_format_is_byte_swapped(dst->format) ||
            gfx_platform_ppa_format_is_byte_swapped(src->format) ||
            src_x < 0 || src_y < 0) {
        if (dst != NULL && src != NULL) {
            gfx_platform_ppa_log_reject_once("blit", dst->format, src->format, 0xffU,
                                             "unsupported fmt/coords/state");
        }
        return GFX_ERR_NOT_SUPPORTED;
    }

    uint32_t w = (uint32_t)(dst_area->x2 - dst_area->x1);
    uint32_t h = (uint32_t)(dst_area->y2 - dst_area->y1);

    return gfx_platform_ppa_run_srm(dst, dst_area, src, w, h,
                                    (uint32_t)src_x, (uint32_t)src_y,
                                    (uint32_t)(src_y + (gfx_coord_t)h),
                                    PPA_SRM_ROTATION_ANGLE_0, 1.0f, 1.0f);
}

static gfx_err_t gfx_platform_ppa_scale(gfx_backend_t *backend, gfx_display_t *disp,
                                        const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                        const gfx_backend_image_t *src, const gfx_area_t *src_area,
                                        gfx_opa_t opa)
{
    (void)backend;
    (void)disp;

    if (!s_ppa.inited || s_ppa.srm_client == NULL || dst == NULL || dst_area == NULL ||
            src == NULL || src->pixels == NULL || src_area == NULL || opa != 0xffU ||
            !gfx_platform_ppa_is_supported_dst_format(dst->format) ||
            !gfx_platform_ppa_is_supported_srm_src_format(src->format) ||
            gfx_platform_ppa_format_is_byte_swapped(dst->format) ||
            gfx_platform_ppa_format_is_byte_swapped(src->format)) {
        if (dst != NULL && src != NULL) {
            gfx_platform_ppa_log_reject_once("scale", dst->format, src->format, opa,
                                             "unsupported fmt/opa/state");
        }
        return GFX_ERR_NOT_SUPPORTED;
    }

    uint32_t src_w = (uint32_t)(src_area->x2 - src_area->x1);
    uint32_t src_h = (uint32_t)(src_area->y2 - src_area->y1);
    uint32_t dst_w = (uint32_t)(dst_area->x2 - dst_area->x1);
    uint32_t dst_h = (uint32_t)(dst_area->y2 - dst_area->y1);
    if (src_w == 0U || src_h == 0U || dst_w == 0U || dst_h == 0U) {
        gfx_platform_ppa_log_reject_once("scale", dst->format, src->format, opa, "empty area");
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_platform_ppa_run_srm(dst, dst_area, src, src_w, src_h,
                                    (uint32_t)src_area->x1, (uint32_t)src_area->y1,
                                    (uint32_t)src_area->y2,
                                    PPA_SRM_ROTATION_ANGLE_0,
                                    (float)dst_w / (float)src_w,
                                    (float)dst_h / (float)src_h);
}

static gfx_err_t gfx_platform_ppa_blend(gfx_backend_t *backend, gfx_display_t *disp,
                                        const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                        const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y,
                                        gfx_opa_t opa)
{
    (void)backend;
    (void)disp;

    if (!s_ppa.inited || s_ppa.blend_client == NULL || dst == NULL || dst_area == NULL ||
            src == NULL || src->pixels == NULL || opa == 0U ||
            src_x < 0 || src_y < 0 ||
            !gfx_platform_ppa_blend_pair_supported(dst->format, src->format, src->alpha != NULL)) {
        if (dst != NULL && src != NULL) {
            gfx_platform_ppa_log_reject_once("blend", dst->format, src->format, opa,
                                             "unsupported fmt/alpha/state");
        }
        return GFX_ERR_NOT_SUPPORTED;
    }

    uint32_t block_w = (uint32_t)(dst_area->x2 - dst_area->x1);
    uint32_t block_h = (uint32_t)(dst_area->y2 - dst_area->y1);
    if (block_w == 0U || block_h == 0U ||
            (block_w * block_h) < GFX_PLATFORM_PPA_MIN_BLEND_PIXELS) {
        gfx_platform_ppa_log_reject_once("blend", dst->format, src->format, opa, "area too small");
        return GFX_ERR_NOT_SUPPORTED;
    }

    uint32_t bg_w = (uint32_t)dst->stride;
    uint32_t bg_h = (uint32_t)(dst->area.y2 - dst->area.y1);
    uint32_t bg_off_x = (uint32_t)(dst_area->x1 - dst->area.x1);
    uint32_t bg_off_y = (uint32_t)(dst_area->y1 - dst->area.y1);
    uint32_t fg_off_x = (uint32_t)src_x;
    uint32_t fg_off_y = (uint32_t)src_y;
    uint32_t fg_w = (uint32_t)src->stride;
    uint32_t fg_h = fg_off_y + block_h;
    size_t out_buffer_size = gfx_platform_ppa_aligned_buffer_size(dst->buf,
                             gfx_platform_ppa_surface_bytes(dst));

    ppa_blend_oper_config_t cfg = {
        .in_bg = {
            .buffer = dst->buf,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = bg_off_x,
            .block_offset_y = bg_off_y,
            .blend_cm = gfx_platform_ppa_to_blend_mode(dst->format),
        },
        .in_fg = {
            .buffer = (void *)src->pixels,
            .pic_w = fg_w,
            .pic_h = fg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = fg_off_x,
            .block_offset_y = fg_off_y,
            .blend_cm = gfx_platform_ppa_to_blend_mode(src->format),
        },
        .out = {
            .buffer = dst->buf,
            .buffer_size = out_buffer_size,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = bg_off_x,
            .block_offset_y = bg_off_y,
            .blend_cm = gfx_platform_ppa_to_blend_mode(dst->format),
        },
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    if (src->format == GFX_COLOR_FORMAT_ARGB8888) {
        cfg.bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
        cfg.bg_alpha_fix_val = 0xffU;
        if (opa >= 0xffU) {
            cfg.fg_alpha_update_mode = PPA_ALPHA_NO_CHANGE;
        } else {
            cfg.fg_alpha_update_mode = PPA_ALPHA_SCALE;
            cfg.fg_alpha_scale_ratio = (float)opa / 255.0f;
        }
    } else {
        cfg.bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
        cfg.bg_alpha_fix_val = (uint32_t)(0xffU - opa);
        cfg.fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
        cfg.fg_alpha_fix_val = opa;
    }

    gfx_platform_ppa_cache_msync_block(dst, dst_area, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    uint32_t src_px_size = gfx_color_format_get_size(src->format);
    const uint8_t *src_start = (const uint8_t *)src->pixels +
                               (size_t)fg_off_y * (size_t)src->stride * src_px_size;
    gfx_platform_ppa_cache_msync(src_start, (size_t)src->stride * block_h * src_px_size,
                                 ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    gfx_err_t ret = gfx_err_from_esp(ppa_do_blend(s_ppa.blend_client, &cfg));
    gfx_platform_ppa_cache_msync_block(dst, dst_area, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    return ret;
}

static gfx_err_t gfx_platform_ppa_transform(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_backend_image_t *src, const gfx_area_t *src_area,
        int16_t angle, gfx_opa_t opa)
{
    ppa_srm_rotation_angle_t rotation;

    (void)backend;
    (void)disp;

    if (!s_ppa.inited || s_ppa.srm_client == NULL || dst == NULL || dst_area == NULL ||
            src == NULL || src->pixels == NULL || src_area == NULL || opa != 0xffU ||
            !gfx_platform_ppa_is_supported_dst_format(dst->format) ||
            !gfx_platform_ppa_is_supported_srm_src_format(src->format) ||
            gfx_platform_ppa_format_is_byte_swapped(dst->format) ||
            gfx_platform_ppa_format_is_byte_swapped(src->format) ||
            !gfx_platform_ppa_angle_to_rotation(angle, &rotation)) {
        if (dst != NULL && src != NULL) {
            gfx_platform_ppa_log_reject_once("transform", dst->format, src->format, opa,
                                             "unsupported fmt/angle/opa/state");
        }
        return GFX_ERR_NOT_SUPPORTED;
    }

    uint32_t src_w = (uint32_t)(src_area->x2 - src_area->x1);
    uint32_t src_h = (uint32_t)(src_area->y2 - src_area->y1);
    if (src_w == 0U || src_h == 0U) {
        gfx_platform_ppa_log_reject_once("transform", dst->format, src->format, opa, "empty area");
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_platform_ppa_run_srm(dst, dst_area, src, src_w, src_h,
                                    (uint32_t)src_area->x1, (uint32_t)src_area->y1,
                                    (uint32_t)src_area->y2,
                                    rotation, 1.0f, 1.0f);
}

static const gfx_draw_ops_t s_platform_ppa_ops = {
    .fill = gfx_platform_ppa_fill,
    .blit = gfx_platform_ppa_blit,
    .blend = gfx_platform_ppa_blend,
    .scale = gfx_platform_ppa_scale,
    .transform = gfx_platform_ppa_transform,
};

static void gfx_platform_ppa_release_clients(void)
{
    if (s_ppa.blend_client != NULL) {
        ppa_unregister_client(s_ppa.blend_client);
        s_ppa.blend_client = NULL;
    }
    if (s_ppa.srm_client != NULL) {
        ppa_unregister_client(s_ppa.srm_client);
        s_ppa.srm_client = NULL;
    }
    if (s_ppa.fill_client != NULL) {
        ppa_unregister_client(s_ppa.fill_client);
        s_ppa.fill_client = NULL;
    }
}

gfx_err_t gfx_platform_accel_init(void)
{
    if (s_ppa.inited) {
        return GFX_OK;
    }

    ppa_client_config_t fill_cfg = {
        .oper_type = PPA_OPERATION_FILL,
        .max_pending_trans_num = 1,
    };
    ppa_client_config_t srm_cfg = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    ppa_client_config_t blend_cfg = {
        .oper_type = PPA_OPERATION_BLEND,
        .max_pending_trans_num = 1,
    };

    esp_err_t ret = ppa_register_client(&fill_cfg, &s_ppa.fill_client);
    if (ret != ESP_OK) {
        GFX_LOGW(TAG, "ppa fill unavailable: %s", esp_err_to_name(ret));
        return gfx_err_from_esp(ret);
    }

    ret = ppa_register_client(&srm_cfg, &s_ppa.srm_client);
    if (ret != ESP_OK) {
        GFX_LOGW(TAG, "ppa srm unavailable: %s", esp_err_to_name(ret));
        gfx_platform_ppa_release_clients();
        return gfx_err_from_esp(ret);
    }

    ret = ppa_register_client(&blend_cfg, &s_ppa.blend_client);
    if (ret != ESP_OK) {
        GFX_LOGW(TAG, "ppa blend unavailable: %s", esp_err_to_name(ret));
        gfx_platform_ppa_release_clients();
        return gfx_err_from_esp(ret);
    }

    s_ppa.inited = true;
    gfx_platform_ppa_log_capability_summary();
    return GFX_OK;
}

void gfx_platform_accel_deinit(void)
{
    gfx_platform_ppa_release_clients();
    s_ppa.inited = false;
}

const gfx_draw_ops_t *gfx_platform_accel_get_draw_ops(void)
{
    return s_ppa.inited ? &s_platform_ppa_ops : NULL;
}

uint32_t gfx_platform_accel_get_caps(void)
{
    if (!s_ppa.inited) {
        return GFX_BACKEND_CAP_NONE;
    }

    return GFX_BACKEND_CAP_FILL |
           GFX_BACKEND_CAP_BLIT |
           GFX_BACKEND_CAP_BLEND |
           GFX_BACKEND_CAP_SCALE |
           GFX_BACKEND_CAP_TRANSFORM;
}

gfx_render_alignment_t gfx_platform_accel_get_alignment(void)
{
    return gfx_platform_ppa_alignment();
}

bool gfx_platform_flush_staging_rotate_patch(const gfx_platform_flush_staging_patch_t *patch,
        int16_t rotation_cw)
{
    ppa_srm_rotation_angle_t rotation;
    gfx_backend_surface_t dst;
    gfx_backend_image_t src;
    gfx_area_t dst_area;
    gfx_coord_t rect_w;
    gfx_coord_t rect_h;
    int x_offset;
    int y_offset;

    if (!s_ppa.inited || s_ppa.srm_client == NULL || patch == NULL ||
            patch->staging_fb == NULL || patch->src == NULL ||
            patch->hor_res == 0U || patch->ver_res == 0U ||
            patch->x2 <= patch->x1 || patch->y2 <= patch->y1 || patch->src_stride_px <= 0 ||
            !gfx_platform_ppa_angle_to_rotation(rotation_cw, &rotation) ||
            (patch->format != GFX_COLOR_FORMAT_RGB565 && patch->format != GFX_COLOR_FORMAT_RGB888)) {
        return false;
    }

    rect_w = (gfx_coord_t)(patch->x2 - patch->x1);
    rect_h = (gfx_coord_t)(patch->y2 - patch->y1);

    switch ((rotation_cw % 360 + 360) % 360) {
    case 90:
        x_offset = (int)patch->hor_res - (int)patch->y2;
        y_offset = (int)patch->x1;
        break;
    case 180:
        x_offset = (int)patch->hor_res - (int)patch->x2;
        y_offset = (int)patch->ver_res - (int)patch->y2;
        break;
    case 270:
        x_offset = (int)patch->y1;
        y_offset = (int)patch->ver_res - (int)patch->x2;
        break;
    default:
        x_offset = (int)patch->x1;
        y_offset = (int)patch->y1;
        break;
    }

    dst = (gfx_backend_surface_t) {
        .buf = patch->staging_fb,
        .area = {
            .x1 = 0,
            .y1 = 0,
            .x2 = (gfx_coord_t)patch->hor_res,
            .y2 = (gfx_coord_t)patch->ver_res,
        },
        .stride = (gfx_coord_t)patch->hor_res,
        .format = patch->format,
    };
    dst_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)x_offset,
        .y1 = (gfx_coord_t)y_offset,
        .x2 = (gfx_coord_t)(x_offset + rect_w),
        .y2 = (gfx_coord_t)(y_offset + rect_h),
    };
    src = (gfx_backend_image_t) {
        .pixels = patch->src,
        .stride = patch->src_stride_px,
        .format = patch->format,
    };

    return gfx_platform_ppa_run_srm(&dst, &dst_area, &src,
                                    (uint32_t)rect_w, (uint32_t)rect_h,
                                    0U, 0U, (uint32_t)rect_h,
                                    rotation, 1.0f, 1.0f) == GFX_OK;
}

#else

#include "platform/esp_idf/esp_idf_flush_staging.h"
#include "platform/gfx_platform_accel_priv.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

static const char *const TAG = "plat_accel";

gfx_err_t gfx_platform_accel_init(void)
{
    GFX_LOGI(TAG, "PPA acceleration unavailable on current IDF/target, using software fallback");
    return GFX_OK;
}

void gfx_platform_accel_deinit(void)
{
}

const gfx_draw_ops_t *gfx_platform_accel_get_draw_ops(void)
{
    return NULL;
}

uint32_t gfx_platform_accel_get_caps(void)
{
    return GFX_BACKEND_CAP_NONE;
}

gfx_render_alignment_t gfx_platform_accel_get_alignment(void)
{
    return (gfx_render_alignment_t) {
        .width_px = 1,
        .height_px = 1,
        .stride_bytes = 1,
        .addr_bytes = 1,
    };
}

bool gfx_platform_flush_staging_rotate_patch(const gfx_platform_flush_staging_patch_t *patch,
        int16_t rotation_cw)
{
    (void)patch;
    (void)rotation_cw;
    return false;
}

#endif
