/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "platform/gfx_platform_jpeg_priv.h"

#if GFX_HOST_USE_LIBJPEG
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
    char message[JMSG_LENGTH_MAX];
} gfx_host_jpeg_error_mgr_t;

static void gfx_host_jpeg_error_exit(j_common_ptr cinfo)
{
    gfx_host_jpeg_error_mgr_t *err = (gfx_host_jpeg_error_mgr_t *)cinfo->err;

    (*cinfo->err->format_message)(cinfo, err->message);
    longjmp(err->setjmp_buffer, 1);
}

static gfx_err_t gfx_host_jpeg_decode_rgb888(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size, uint32_t *out_width, uint32_t *out_height)
{
    struct jpeg_decompress_struct cinfo;
    gfx_host_jpeg_error_mgr_t jerr;
    gfx_err_t ret = GFX_FAIL;

    if (in_data == NULL || in_size == 0U) {
        return GFX_ERR_INVALID_ARG;
    }

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = gfx_host_jpeg_error_exit;
    jerr.message[0] = '\0';

    if (setjmp(jerr.setjmp_buffer) != 0) {
        jpeg_destroy_decompress(&cinfo);
        return GFX_FAIL;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, in_data, (unsigned long)in_size);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        goto cleanup;
    }

    if (out_data == NULL) {
        if (cinfo.image_width == 0U || cinfo.image_height == 0U) {
            ret = GFX_ERR_INVALID_SIZE;
            goto cleanup;
        }
        if (out_width != NULL) {
            *out_width = cinfo.image_width;
        }
        if (out_height != NULL) {
            *out_height = cinfo.image_height;
        }
        ret = GFX_OK;
        goto cleanup;
    }

    cinfo.out_color_space = JCS_RGB;
    if (!jpeg_start_decompress(&cinfo)) {
        goto cleanup;
    }

    if (cinfo.output_width == 0U || cinfo.output_height == 0U || cinfo.output_components != 3U) {
        ret = GFX_ERR_INVALID_SIZE;
        goto cleanup;
    }

    if (out_width != NULL) {
        *out_width = cinfo.output_width;
    }
    if (out_height != NULL) {
        *out_height = cinfo.output_height;
    }

    size_t row_stride = (size_t)cinfo.output_width * 3U;
    size_t required_size = row_stride * (size_t)cinfo.output_height;
    if (out_size == NULL || *out_size < required_size) {
        ret = GFX_ERR_INVALID_SIZE;
        goto cleanup;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = &out_data[(size_t)cinfo.output_scanline * row_stride];
        if (jpeg_read_scanlines(&cinfo, &row, 1) != 1U) {
            goto cleanup;
        }
    }
    *out_size = required_size;
    ret = GFX_OK;
    ret = jpeg_finish_decompress(&cinfo) ? ret : GFX_FAIL;

cleanup:
    jpeg_destroy_decompress(&cinfo);
    return ret;
}
#endif

gfx_err_t gfx_platform_jpeg_init(void)
{
    return GFX_OK;
}

void gfx_platform_jpeg_deinit(void)
{
}

bool gfx_platform_jpeg_is_available(void)
{
#if GFX_HOST_USE_LIBJPEG
    return true;
#else
    return false;
#endif
}

gfx_err_t gfx_platform_jpeg_get_info(const uint8_t *in_data, size_t in_size,
                                     uint32_t *out_width, uint32_t *out_height)
{
#if GFX_HOST_USE_LIBJPEG
    if (out_width == NULL || out_height == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    return gfx_host_jpeg_decode_rgb888(in_data, in_size, NULL, NULL, out_width, out_height);
#else
    (void)in_data;
    (void)in_size;
    (void)out_width;
    (void)out_height;
    return GFX_ERR_NOT_SUPPORTED;
#endif
}

gfx_err_t gfx_platform_jpeg_decode_rgb888(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
#if GFX_HOST_USE_LIBJPEG
    return gfx_host_jpeg_decode_rgb888(in_data, in_size, out_data, out_size, NULL, NULL);
#else
    (void)in_data;
    (void)in_size;
    (void)out_data;
    (void)out_size;
    return GFX_ERR_NOT_SUPPORTED;
#endif
}

gfx_err_t gfx_platform_jpeg_decode_rgb565_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size)
{
#if GFX_HOST_USE_LIBJPEG
    uint8_t *rgb888;
    size_t rgb888_size;
    uint32_t decoded_w = 0;
    uint32_t decoded_h = 0;
    gfx_err_t ret;

    (void)width;
    (void)height;
    if (out_data == NULL || out_size == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    ret = gfx_host_jpeg_decode_rgb888(in_data, in_size, NULL, NULL, &decoded_w, &decoded_h);
    if (ret != GFX_OK) {
        return ret;
    }
    rgb888_size = (size_t)decoded_w * (size_t)decoded_h * 3U;
    if (*out_size < (size_t)decoded_w * (size_t)decoded_h * 2U) {
        return GFX_ERR_INVALID_SIZE;
    }

    rgb888 = malloc(rgb888_size);
    if (rgb888 == NULL) {
        return GFX_ERR_NO_MEM;
    }
    ret = gfx_host_jpeg_decode_rgb888(in_data, in_size, rgb888, &rgb888_size, NULL, NULL);
    if (ret == GFX_OK) {
        for (size_t i = 0, j = 0; i < rgb888_size; i += 3U, j += 2U) {
            uint16_t rgb565 = (uint16_t)(((uint16_t)(rgb888[i] & 0xF8U) << 8) |
                                         ((uint16_t)(rgb888[i + 1U] & 0xFCU) << 3) |
                                         ((uint16_t)rgb888[i + 2U] >> 3));
            out_data[j] = (uint8_t)(rgb565 & 0xFFU);
            out_data[j + 1U] = (uint8_t)(rgb565 >> 8);
        }
        *out_size = (size_t)decoded_w * (size_t)decoded_h * 2U;
    }
    free(rgb888);
    return ret;
#else
    (void)in_data;
    (void)in_size;
    (void)width;
    (void)height;
    (void)out_data;
    (void)out_size;
    return GFX_ERR_NOT_SUPPORTED;
#endif
}

gfx_err_t gfx_platform_jpeg_decode_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
#if GFX_HOST_USE_LIBJPEG
    return gfx_platform_jpeg_decode_rgb565_with_hint(in_data, in_size, 0, 0, out_data, out_size);
#else
    (void)in_data;
    (void)in_size;
    (void)out_data;
    (void)out_size;
    return GFX_ERR_NOT_SUPPORTED;
#endif
}
