/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_jpeg_dec.h"

#if defined(CONFIG_SOC_JPEG_CODEC_SUPPORTED) && CONFIG_SOC_JPEG_CODEC_SUPPORTED && __has_include("driver/jpeg_decode.h")
#include "driver/jpeg_decode.h"
#include "esp_jpeg_common.h"
#define GFX_PLATFORM_JPEG_HW_AVAILABLE 1
#else
#define GFX_PLATFORM_JPEG_HW_AVAILABLE 0
#endif

#define GFX_LOG_MODULE GFX_LOG_MODULE_EAF_DEC
#include "common/gfx_log_priv.h"

#include "platform/gfx_platform_jpeg_priv.h"

static const char *const TAG = "plat_jpeg";

typedef struct {
    bool inited;
    bool hw_ready;
#if GFX_PLATFORM_JPEG_HW_AVAILABLE
    jpeg_decoder_handle_t hw_handle;
#endif
} gfx_platform_jpeg_state_t;

static gfx_platform_jpeg_state_t s_jpeg;

static void gfx_platform_jpeg_log_decode_path_once(const char *path, const char *reason)
{
    static bool s_logged_hw;
    static bool s_logged_sw;

    if (path == NULL) {
        return;
    }

    if (strcmp(path, "hw") == 0) {
        if (s_logged_hw) {
            return;
        }
        s_logged_hw = true;
    } else if (strcmp(path, "sw") == 0) {
        if (s_logged_sw) {
            return;
        }
        s_logged_sw = true;
    } else {
        return;
    }

    GFX_LOGI(TAG, "JPEG decode path=%s reason=%s", path, reason ? reason : "none");
}

#if GFX_PLATFORM_JPEG_HW_AVAILABLE
static bool gfx_platform_jpeg_hw_can_decode(uint32_t w, uint32_t h)
{
    return s_jpeg.hw_ready &&
           (w % 16U == 0U) &&
           (h % 16U == 0U) &&
           (w >= 64U) &&
           (h >= 64U);
}

static esp_err_t gfx_platform_jpeg_try_decode_hw_rgb565(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size)
{
    size_t required_size = (size_t)width * (size_t)height * 2U;
    if (!gfx_platform_jpeg_hw_can_decode(width, height)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (*out_size < required_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    uint32_t used = 0;
    esp_err_t ret = jpeg_decoder_process(s_jpeg.hw_handle, &cfg, in_data, in_size, out_data, *out_size, &used);
    if (ret != ESP_OK) {
        return ret;
    }

    *out_size = required_size;
    gfx_platform_jpeg_log_decode_path_once("hw", "eligible");
    return ESP_OK;
}

static esp_err_t gfx_platform_jpeg_decode_hw_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    jpeg_decode_picture_info_t pic_info = {0};
    esp_err_t ret = jpeg_decoder_get_info(in_data, in_size, &pic_info);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!gfx_platform_jpeg_hw_can_decode(pic_info.width, pic_info.height)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return gfx_platform_jpeg_try_decode_hw_rgb565(in_data, in_size,
            pic_info.width, pic_info.height, out_data, out_size);
}
#endif

static esp_err_t gfx_platform_jpeg_decode_sw_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    esp_err_t ret = ESP_OK;
    uint32_t w;
    uint32_t h;
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    jpeg_dec_config_t config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    ESP_GOTO_ON_ERROR(jpeg_dec_open(&config, &jpeg_dec), err, TAG, "JPEG SW decoder open failed");

    jpeg_io = malloc(sizeof(*jpeg_io));
    ESP_GOTO_ON_FALSE(jpeg_io != NULL, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg io");

    out_info = malloc(sizeof(*out_info));
    ESP_GOTO_ON_FALSE(out_info != NULL, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg header");

    jpeg_io->inbuf = (unsigned char *)in_data;
    jpeg_io->inbuf_len = in_size;

    jpeg_error_t jpeg_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG SW header parse failed");

    w = out_info->width;
    h = out_info->height;

    size_t required_size = (size_t)w * (size_t)h * 2U;
    ESP_GOTO_ON_FALSE(*out_size >= required_size, ESP_ERR_INVALID_SIZE, err, TAG,
                      "JPEG output buffer too small: need %zu got %zu", required_size, *out_size);

    jpeg_io->outbuf = out_data;
    jpeg_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG SW decode failed: %d", jpeg_ret);

    *out_size = required_size;
    gfx_platform_jpeg_log_decode_path_once("sw", "software fallback");

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return ESP_OK;

err:
    if (jpeg_io != NULL) {
        free(jpeg_io);
    }
    if (out_info != NULL) {
        free(out_info);
    }
    if (jpeg_dec != NULL) {
        jpeg_dec_close(jpeg_dec);
    }
    return ret;
}

esp_err_t gfx_platform_jpeg_get_info(const uint8_t *in_data, size_t in_size,
                                     uint32_t *out_width, uint32_t *out_height)
{
    esp_err_t ret = ESP_OK;
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    if (!s_jpeg.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (in_data == NULL || in_size == 0U || out_width == NULL || out_height == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    jpeg_dec_config_t config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
        .rotate = JPEG_ROTATE_0D,
    };

    ESP_GOTO_ON_ERROR(jpeg_dec_open(&config, &jpeg_dec), err, TAG, "JPEG info decoder open failed");

    jpeg_io = malloc(sizeof(*jpeg_io));
    ESP_GOTO_ON_FALSE(jpeg_io != NULL, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg io");

    out_info = malloc(sizeof(*out_info));
    ESP_GOTO_ON_FALSE(out_info != NULL, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg header");

    jpeg_io->inbuf = (unsigned char *)in_data;
    jpeg_io->inbuf_len = in_size;

    jpeg_error_t jpeg_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG info header parse failed");

    *out_width = out_info->width;
    *out_height = out_info->height;

err:
    if (jpeg_io != NULL) {
        free(jpeg_io);
    }
    if (out_info != NULL) {
        free(out_info);
    }
    if (jpeg_dec != NULL) {
        jpeg_dec_close(jpeg_dec);
    }
    return ret;
}

esp_err_t gfx_platform_jpeg_decode_rgb888(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    esp_err_t ret = ESP_OK;
    uint32_t w;
    uint32_t h;
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    if (!s_jpeg.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (in_data == NULL || out_data == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    jpeg_dec_config_t config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
        .rotate = JPEG_ROTATE_0D,
    };

    ESP_GOTO_ON_ERROR(jpeg_dec_open(&config, &jpeg_dec), err, TAG, "JPEG RGB888 decoder open failed");

    jpeg_io = malloc(sizeof(*jpeg_io));
    ESP_GOTO_ON_FALSE(jpeg_io != NULL, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg io");

    out_info = malloc(sizeof(*out_info));
    ESP_GOTO_ON_FALSE(out_info != NULL, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg header");

    jpeg_io->inbuf = (unsigned char *)in_data;
    jpeg_io->inbuf_len = in_size;

    jpeg_error_t jpeg_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG RGB888 header parse failed");

    w = out_info->width;
    h = out_info->height;

    size_t required_size = (size_t)w * (size_t)h * 3U;
    ESP_GOTO_ON_FALSE(*out_size >= required_size, ESP_ERR_INVALID_SIZE, err, TAG,
                      "JPEG RGB888 output buffer too small: need %zu got %zu", required_size, *out_size);

    jpeg_io->outbuf = out_data;
    jpeg_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG RGB888 decode failed: %d", jpeg_ret);

    *out_size = required_size;
    gfx_platform_jpeg_log_decode_path_once("sw", "rgb888 software");

err:
    if (jpeg_io != NULL) {
        free(jpeg_io);
    }
    if (out_info != NULL) {
        free(out_info);
    }
    if (jpeg_dec != NULL) {
        jpeg_dec_close(jpeg_dec);
    }
    return ret;
}

esp_err_t gfx_platform_jpeg_init(void)
{
    if (s_jpeg.inited) {
        return ESP_OK;
    }

#if GFX_PLATFORM_JPEG_HW_AVAILABLE
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
    };
    esp_err_t hw_ret = jpeg_new_decoder_engine(&decode_eng_cfg, &s_jpeg.hw_handle);
    if (hw_ret == ESP_OK) {
        s_jpeg.hw_ready = true;
    } else {
        s_jpeg.hw_handle = NULL;
        s_jpeg.hw_ready = false;
        GFX_LOGI(TAG, "JPEG HW decoder unavailable: %s", esp_err_to_name(hw_ret));
    }
#endif

    s_jpeg.inited = true;
    GFX_LOGI(TAG, "JPEG decoder enabled: sw=1 hw_api=%d hw_engine=%d",
             GFX_PLATFORM_JPEG_HW_AVAILABLE,
             s_jpeg.hw_ready ? 1 : 0);
    return ESP_OK;
}

void gfx_platform_jpeg_deinit(void)
{
#if GFX_PLATFORM_JPEG_HW_AVAILABLE
    if (s_jpeg.hw_handle != NULL) {
        jpeg_del_decoder_engine(s_jpeg.hw_handle);
        s_jpeg.hw_handle = NULL;
    }
#endif
    s_jpeg.hw_ready = false;
    s_jpeg.inited = false;
}

bool gfx_platform_jpeg_is_available(void)
{
    return s_jpeg.inited;
}

esp_err_t gfx_platform_jpeg_decode_rgb565_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size)
{
    if (!s_jpeg.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (in_data == NULL || out_data == NULL || out_size == NULL || width == 0U || height == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

#if GFX_PLATFORM_JPEG_HW_AVAILABLE
    if (s_jpeg.hw_ready) {
        esp_err_t hw_ret = gfx_platform_jpeg_try_decode_hw_rgb565(in_data, in_size, width, height, out_data, out_size);
        if (hw_ret == ESP_OK) {
            return ESP_OK;
        }
        if (hw_ret != ESP_ERR_NOT_SUPPORTED) {
            GFX_LOGD(TAG, "JPEG HW decode fallback to SW: %s", esp_err_to_name(hw_ret));
        } else {
            gfx_platform_jpeg_log_decode_path_once("sw", "hw not eligible");
        }
    } else {
        gfx_platform_jpeg_log_decode_path_once("sw", "hw engine unavailable");
    }
#else
    gfx_platform_jpeg_log_decode_path_once("sw", "hw api unavailable");
#endif

    return gfx_platform_jpeg_decode_sw_rgb565(in_data, in_size, out_data, out_size);
}

esp_err_t gfx_platform_jpeg_decode_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    if (!s_jpeg.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (in_data == NULL || out_data == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if GFX_PLATFORM_JPEG_HW_AVAILABLE
    if (s_jpeg.hw_ready) {
        esp_err_t hw_ret = gfx_platform_jpeg_decode_hw_rgb565(in_data, in_size, out_data, out_size);
        if (hw_ret == ESP_OK) {
            return ESP_OK;
        }
        if (hw_ret != ESP_ERR_NOT_SUPPORTED) {
            GFX_LOGD(TAG, "JPEG HW decode fallback to SW: %s", esp_err_to_name(hw_ret));
        } else {
            gfx_platform_jpeg_log_decode_path_once("sw", "hw not eligible");
        }
    } else {
        gfx_platform_jpeg_log_decode_path_once("sw", "hw engine unavailable");
    }
#else
    gfx_platform_jpeg_log_decode_path_once("sw", "hw api unavailable");
#endif

    return gfx_platform_jpeg_decode_sw_rgb565(in_data, in_size, out_data, out_size);
}

#else

#include "platform/gfx_platform_jpeg_priv.h"

esp_err_t gfx_platform_jpeg_init(void)
{
    return ESP_OK;
}

void gfx_platform_jpeg_deinit(void)
{
}

bool gfx_platform_jpeg_is_available(void)
{
    return false;
}

esp_err_t gfx_platform_jpeg_get_info(const uint8_t *in_data, size_t in_size,
                                     uint32_t *out_width, uint32_t *out_height)
{
    (void)in_data;
    (void)in_size;
    (void)out_width;
    (void)out_height;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gfx_platform_jpeg_decode_rgb888(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    (void)in_data;
    (void)in_size;
    (void)out_data;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gfx_platform_jpeg_decode_rgb565_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size)
{
    (void)in_data;
    (void)in_size;
    (void)width;
    (void)height;
    (void)out_data;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gfx_platform_jpeg_decode_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    (void)in_data;
    (void)in_size;
    (void)out_data;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
