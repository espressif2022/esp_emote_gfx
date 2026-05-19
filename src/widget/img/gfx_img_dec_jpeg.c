/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG_DEC
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "widget/img/gfx_img_dec_priv.h"

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"

/*********************
 *      DEFINES
 *********************/

#define GFX_JPEG_HEADER_SOI_MSB 0xFF
#define GFX_JPEG_HEADER_SOI_LSB 0xD8

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint8_t *input_buf;
    size_t input_buf_size;
    uint8_t *output_buf;
    size_t output_buf_size;
    uint32_t decoded_size;
} gfx_img_dec_jpeg_ctx_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "img_dec_jpeg";
static jpeg_decoder_handle_t s_jpeg_decoder_engine = NULL;
static bool s_jpeg_decoder_registered = false;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_img_dec_jpeg_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
static esp_err_t gfx_img_dec_jpeg_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static void gfx_img_dec_jpeg_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static esp_err_t gfx_img_dec_jpeg_get_source(const gfx_image_decoder_dsc_t *dsc, const gfx_jpeg_dsc_t **out_src);
static esp_err_t gfx_img_dec_jpeg_parse_header(const gfx_jpeg_dsc_t *jpeg_src, gfx_image_header_t *out_header,
                                               uint32_t *out_process_w, uint32_t *out_process_h);
static esp_err_t gfx_img_dec_jpeg_get_mcu_size(jpeg_down_sampling_type_t sample_method, uint32_t *out_mcu_w, uint32_t *out_mcu_h);
static jpeg_dec_rgb_element_order_t gfx_img_dec_jpeg_get_rgb_order(bool swap);

static gfx_image_decoder_t s_gfx_img_decoder_jpeg = {
    .name = "jpeg_hw",
    .info_cb = gfx_img_dec_jpeg_info_cb,
    .open_cb = gfx_img_dec_jpeg_open_cb,
    .close_cb = gfx_img_dec_jpeg_close_cb,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_img_dec_jpeg_get_source(const gfx_image_decoder_dsc_t *dsc, const gfx_jpeg_dsc_t **out_src)
{
    ESP_RETURN_ON_FALSE(dsc != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg source: decoder desc is NULL");
    ESP_RETURN_ON_FALSE(out_src != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg source: output is NULL");
    ESP_RETURN_ON_FALSE(dsc->src.type == GFX_IMG_SRC_TYPE_JPEG, ESP_ERR_INVALID_ARG, TAG, "jpeg source: src type mismatch");
    ESP_RETURN_ON_FALSE(dsc->src.data != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg source: payload is NULL");

    const gfx_jpeg_dsc_t *jpeg_src = (const gfx_jpeg_dsc_t *)dsc->src.data;
    ESP_RETURN_ON_FALSE(jpeg_src->data != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg source: bitstream is NULL");
    ESP_RETURN_ON_FALSE(jpeg_src->data_size >= 2U, ESP_ERR_INVALID_ARG, TAG, "jpeg source: bitstream is too small");
    ESP_RETURN_ON_FALSE(jpeg_src->data[0] == GFX_JPEG_HEADER_SOI_MSB && jpeg_src->data[1] == GFX_JPEG_HEADER_SOI_LSB,
                        ESP_ERR_INVALID_ARG, TAG, "jpeg source: invalid SOI marker");

    *out_src = jpeg_src;
    return ESP_OK;
}

static esp_err_t gfx_img_dec_jpeg_get_mcu_size(jpeg_down_sampling_type_t sample_method, uint32_t *out_mcu_w, uint32_t *out_mcu_h)
{
    ESP_RETURN_ON_FALSE(out_mcu_w != NULL && out_mcu_h != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg mcu: output is NULL");

    switch (sample_method) {
    case JPEG_DOWN_SAMPLING_YUV444:
        *out_mcu_w = 8U;
        *out_mcu_h = 8U;
        return ESP_OK;
    case JPEG_DOWN_SAMPLING_YUV422:
        *out_mcu_w = 16U;
        *out_mcu_h = 8U;
        return ESP_OK;
    case JPEG_DOWN_SAMPLING_YUV420:
        *out_mcu_w = 16U;
        *out_mcu_h = 16U;
        return ESP_OK;
    case JPEG_DOWN_SAMPLING_GRAY:
        return ESP_ERR_NOT_SUPPORTED;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t gfx_img_dec_jpeg_parse_header(const gfx_jpeg_dsc_t *jpeg_src, gfx_image_header_t *out_header,
                                               uint32_t *out_process_w, uint32_t *out_process_h)
{
    jpeg_decode_picture_info_t picture_info = {0};
    uint32_t mcu_w = 0;
    uint32_t mcu_h = 0;

    ESP_RETURN_ON_FALSE(jpeg_src != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg header: source is NULL");
    ESP_RETURN_ON_FALSE(out_header != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg header: output header is NULL");
    ESP_RETURN_ON_FALSE(out_process_w != NULL && out_process_h != NULL, ESP_ERR_INVALID_ARG, TAG, "jpeg header: process size output is NULL");

    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(jpeg_src->data, jpeg_src->data_size, &picture_info), TAG, "jpeg header: get info failed");
    ESP_RETURN_ON_ERROR(gfx_img_dec_jpeg_get_mcu_size(picture_info.sample_method, &mcu_w, &mcu_h), TAG,
                        "jpeg header: unsupported sample method %d", (int)picture_info.sample_method);
    ESP_RETURN_ON_FALSE(picture_info.width > 0U && picture_info.height > 0U, ESP_ERR_INVALID_SIZE, TAG,
                        "jpeg header: invalid picture size %" PRIu32 "x%" PRIu32, picture_info.width, picture_info.height);
    ESP_RETURN_ON_FALSE(picture_info.width <= UINT16_MAX && picture_info.height <= UINT16_MAX, ESP_ERR_NOT_SUPPORTED, TAG,
                        "jpeg header: picture size exceeds gfx header range");

    *out_process_w = ((picture_info.width + mcu_w - 1U) / mcu_w) * mcu_w;
    *out_process_h = ((picture_info.height + mcu_h - 1U) / mcu_h) * mcu_h;
    ESP_RETURN_ON_FALSE((*out_process_w * GFX_PIXEL_SIZE_16BPP) <= UINT16_MAX, ESP_ERR_NOT_SUPPORTED, TAG,
                        "jpeg header: stride exceeds gfx header range");

    memset(out_header, 0, sizeof(*out_header));
    out_header->cf = GFX_COLOR_FORMAT_RGB565;
    out_header->w = (uint16_t)picture_info.width;
    out_header->h = (uint16_t)picture_info.height;
    out_header->stride = (uint16_t)(*out_process_w * GFX_PIXEL_SIZE_16BPP);

    return ESP_OK;
}

static jpeg_dec_rgb_element_order_t gfx_img_dec_jpeg_get_rgb_order(bool swap)
{
    /* Match software/native semantics:
     * swap=false -> RGB565 little-endian
     * swap=true  -> RGB565 big-endian */
    return swap ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
}

static esp_err_t gfx_img_dec_jpeg_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header)
{
    (void)decoder;

    const gfx_jpeg_dsc_t *jpeg_src = NULL;
    uint32_t process_w = 0;
    uint32_t process_h = 0;

    ESP_RETURN_ON_ERROR(gfx_img_dec_jpeg_get_source(dsc, &jpeg_src), TAG, "jpeg info: invalid source");
    ESP_RETURN_ON_ERROR(gfx_img_dec_jpeg_parse_header(jpeg_src, header, &process_w, &process_h), TAG, "jpeg info: parse header failed");
    (void)process_w;
    (void)process_h;
    return ESP_OK;
}

static esp_err_t gfx_img_dec_jpeg_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;

    const gfx_jpeg_dsc_t *jpeg_src = NULL;
    gfx_img_dec_jpeg_ctx_t *ctx = NULL;
    gfx_image_header_t header = {0};
    jpeg_decode_memory_alloc_cfg_t input_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
    };
    jpeg_decode_memory_alloc_cfg_t output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };
    jpeg_decode_cfg_t decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = gfx_img_dec_jpeg_get_rgb_order(dsc->swap),
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t process_w = 0;
    uint32_t process_h = 0;
    uint32_t decoded_size = 0;
    size_t requested_output_size = 0;
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(s_jpeg_decoder_engine != NULL, ESP_ERR_INVALID_STATE, TAG, "jpeg open: decoder engine is not ready");
    ESP_RETURN_ON_ERROR(gfx_img_dec_jpeg_get_source(dsc, &jpeg_src), TAG, "jpeg open: invalid source");
    ESP_RETURN_ON_ERROR(gfx_img_dec_jpeg_parse_header(jpeg_src, &header, &process_w, &process_h), TAG, "jpeg open: parse header failed");

    ctx = calloc(1, sizeof(*ctx));
    ESP_GOTO_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, err, TAG, "jpeg open: no mem for context");

    ctx->input_buf = jpeg_alloc_decoder_mem(jpeg_src->data_size, &input_mem_cfg, &ctx->input_buf_size);
    ESP_GOTO_ON_FALSE(ctx->input_buf != NULL, ESP_ERR_NO_MEM, err, TAG, "jpeg open: no mem for input buffer");
    memcpy(ctx->input_buf, jpeg_src->data, jpeg_src->data_size);

    requested_output_size = (size_t)process_w * (size_t)process_h * GFX_PIXEL_SIZE_16BPP;
    ctx->output_buf = jpeg_alloc_decoder_mem(requested_output_size, &output_mem_cfg, &ctx->output_buf_size);
    ESP_GOTO_ON_FALSE(ctx->output_buf != NULL, ESP_ERR_NO_MEM, err, TAG, "jpeg open: no mem for output buffer");

    ret = jpeg_decoder_process(s_jpeg_decoder_engine, &decode_cfg, ctx->input_buf, jpeg_src->data_size,
                               ctx->output_buf, ctx->output_buf_size, &decoded_size);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "jpeg open: hardware decode failed");

    ctx->decoded_size = decoded_size;
    dsc->header = header;
    dsc->data = ctx->output_buf;
    dsc->data_size = ctx->decoded_size;
    dsc->user_data = ctx;

    GFX_LOGD(TAG, "jpeg open: decoded %ux%u stride=%u swap=%d",
             dsc->header.w, dsc->header.h, dsc->header.stride, dsc->swap);
    return ESP_OK;

err:
    if (ctx != NULL) {
        free(ctx->output_buf);
        free(ctx->input_buf);
        free(ctx);
    }
    return ret;
}

static void gfx_img_dec_jpeg_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;

    if (dsc == NULL || dsc->src.type != GFX_IMG_SRC_TYPE_JPEG || dsc->user_data == NULL) {
        return;
    }

    gfx_img_dec_jpeg_ctx_t *ctx = (gfx_img_dec_jpeg_ctx_t *)dsc->user_data;
    free(ctx->output_buf);
    free(ctx->input_buf);
    free(ctx);

    dsc->data = NULL;
    dsc->data_size = 0;
    dsc->user_data = NULL;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

esp_err_t gfx_image_decoder_register_jpeg(void)
{
    esp_err_t ret = ESP_OK;
    jpeg_decode_engine_cfg_t engine_cfg = {
        .intr_priority = 0,
        .timeout_ms = -1,
    };

    if (s_jpeg_decoder_registered) {
        return ESP_OK;
    }

    ret = jpeg_new_decoder_engine(&engine_cfg, &s_jpeg_decoder_engine);
    ESP_RETURN_ON_ERROR(ret, TAG, "jpeg decoder register: create engine failed");

    ret = gfx_image_decoder_register(&s_gfx_img_decoder_jpeg);
    if (ret != ESP_OK) {
        jpeg_del_decoder_engine(s_jpeg_decoder_engine);
        s_jpeg_decoder_engine = NULL;
        return ret;
    }

    s_jpeg_decoder_registered = true;
    GFX_LOGD(TAG, "jpeg decoder register: hardware decoder ready");
    return ESP_OK;
}

esp_err_t gfx_image_decoder_unregister_jpeg(void)
{
    if (s_jpeg_decoder_engine != NULL) {
        jpeg_del_decoder_engine(s_jpeg_decoder_engine);
        s_jpeg_decoder_engine = NULL;
    }

    s_jpeg_decoder_registered = false;
    return ESP_OK;
}

#else

esp_err_t gfx_image_decoder_register_jpeg(void)
{
    return ESP_OK;
}

esp_err_t gfx_image_decoder_unregister_jpeg(void)
{
    return ESP_OK;
}

#endif
