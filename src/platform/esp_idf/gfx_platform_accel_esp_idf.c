/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if __has_include("driver/ppa.h") && defined(CONFIG_SOC_PPA_SUPPORTED) && CONFIG_SOC_PPA_SUPPORTED

#include <string.h>

#include "driver/ppa.h"
#include "hal/color_types.h"
#include "common/gfx_check.h"
#include "platform/esp_idf/gfx_err_bridge.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "common/gfx_types_priv.h"
#include "platform/gfx_platform_accel_priv.h"

static const char *const TAG = "plat_accel";

typedef struct {
    bool inited;
    ppa_client_handle_t fill_client;
    ppa_client_handle_t srm_client;
} gfx_platform_ppa_state_t;

static gfx_platform_ppa_state_t s_ppa;

static gfx_render_alignment_t gfx_platform_ppa_alignment(void);

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
             "PPA enabled: ops=fill|blit|scale, align=%ux%u stride=%u addr=%u",
             (unsigned)gfx_platform_ppa_alignment().width_px,
             (unsigned)gfx_platform_ppa_alignment().height_px,
             (unsigned)gfx_platform_ppa_alignment().stride_bytes,
             (unsigned)gfx_platform_ppa_alignment().addr_bytes);
    GFX_LOGI(TAG,
             "PPA format policy: fill dst={RGB565,RGB888,ARGB8888}, srm dst={RGB565,RGB888,ARGB8888}, srm src={RGB565,RGB888,ARGB8888}, rgb888-src=%s, excluded={RGB565_SWAPPED,BGR888,XRGB8888,*alpha blend}",
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
    static bool s_scale_logged;
    bool *flag = NULL;

    if (strcmp(op, "fill") == 0) {
        flag = &s_fill_logged;
    } else if (strcmp(op, "blit") == 0) {
        flag = &s_blit_logged;
    } else if (strcmp(op, "scale") == 0) {
        flag = &s_scale_logged;
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

    return ppa_do_fill(s_ppa.fill_client, &cfg);
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

    ppa_srm_oper_config_t cfg = {
        .in.buffer = src->pixels,
        .in.pic_w = (uint32_t)src->stride,
        .in.pic_h = (uint32_t)(src_y + (gfx_coord_t)h),
        .in.block_w = w,
        .in.block_h = h,
        .in.block_offset_x = (uint32_t)src_x,
        .in.block_offset_y = (uint32_t)src_y,
        .in.srm_cm = gfx_platform_ppa_to_srm_mode(src->format),
        .out.buffer = dst->buf,
        .out.buffer_size = gfx_platform_ppa_surface_bytes(dst),
        .out.pic_w = (uint32_t)dst->stride,
        .out.pic_h = (uint32_t)(dst->area.y2 - dst->area.y1),
        .out.block_offset_x = (uint32_t)(dst_area->x1 - dst->area.x1),
        .out.block_offset_y = (uint32_t)(dst_area->y1 - dst->area.y1),
        .out.srm_cm = gfx_platform_ppa_to_srm_mode(dst->format),
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .rgb_swap = gfx_platform_ppa_srm_input_rgb_swap(src->format),
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    return ppa_do_scale_rotate_mirror(s_ppa.srm_client, &cfg);
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

    ppa_srm_oper_config_t cfg = {
        .in.buffer = src->pixels,
        .in.pic_w = (uint32_t)src->stride,
        .in.pic_h = (uint32_t)src_area->y2,
        .in.block_w = src_w,
        .in.block_h = src_h,
        .in.block_offset_x = (uint32_t)src_area->x1,
        .in.block_offset_y = (uint32_t)src_area->y1,
        .in.srm_cm = gfx_platform_ppa_to_srm_mode(src->format),
        .out.buffer = dst->buf,
        .out.buffer_size = gfx_platform_ppa_surface_bytes(dst),
        .out.pic_w = (uint32_t)dst->stride,
        .out.pic_h = (uint32_t)(dst->area.y2 - dst->area.y1),
        .out.block_offset_x = (uint32_t)(dst_area->x1 - dst->area.x1),
        .out.block_offset_y = (uint32_t)(dst_area->y1 - dst->area.y1),
        .out.srm_cm = gfx_platform_ppa_to_srm_mode(dst->format),
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = (float)dst_w / (float)src_w,
        .scale_y = (float)dst_h / (float)src_h,
        .rgb_swap = gfx_platform_ppa_srm_input_rgb_swap(src->format),
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    return ppa_do_scale_rotate_mirror(s_ppa.srm_client, &cfg);
}

static const gfx_draw_ops_t s_platform_ppa_ops = {
    .fill = gfx_platform_ppa_fill,
    .blit = gfx_platform_ppa_blit,
    .scale = gfx_platform_ppa_scale,
};

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

    esp_err_t ret = ppa_register_client(&fill_cfg, &s_ppa.fill_client);
    if (ret != ESP_OK) {
        GFX_LOGW(TAG, "ppa fill unavailable: %s", esp_err_to_name(ret));
        return gfx_err_from_esp(ret);
    }

    ret = ppa_register_client(&srm_cfg, &s_ppa.srm_client);
    if (ret != ESP_OK) {
        GFX_LOGW(TAG, "ppa srm unavailable: %s", esp_err_to_name(ret));
        ppa_unregister_client(s_ppa.fill_client);
        s_ppa.fill_client = NULL;
        return gfx_err_from_esp(ret);
    }

    s_ppa.inited = true;
    gfx_platform_ppa_log_capability_summary();
    return GFX_OK;
}

void gfx_platform_accel_deinit(void)
{
    if (s_ppa.srm_client != NULL) {
        ppa_unregister_client(s_ppa.srm_client);
        s_ppa.srm_client = NULL;
    }
    if (s_ppa.fill_client != NULL) {
        ppa_unregister_client(s_ppa.fill_client);
        s_ppa.fill_client = NULL;
    }
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
           GFX_BACKEND_CAP_SCALE;
}

gfx_render_alignment_t gfx_platform_accel_get_alignment(void)
{
    return gfx_platform_ppa_alignment();
}

#else

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

#endif
