/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_QRCODE
#include "common/gfx_log_priv.h"
#include "lib/qrcode/qrcode_wrapper.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/gfx_qrcode.h"

/*********************
 *      DEFINES
 *********************/
#define CHECK_OBJ_TYPE_QRCODE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_QRCODE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    char *text;                 /**< QR Code text/data */
    size_t text_len;            /**< Length of text */
    uint8_t *qr_modules;        /**< Scaled QR Code image buffer (RGB565 format) */
    int qr_size;                /**< QR Code modules size (from qrcode_wrapper) */
    int scaled_size;            /**< Scaled image size in pixels (qr_size * scale) */
    uint16_t display_size;      /**< Display size in pixels */
    gfx_qrcode_ecc_t ecc;       /**< Error correction level */
    gfx_color_t color;          /**< Foreground color (modules) */
    gfx_color_t bg_color;       /**< Background color */
    bool needs_update;          /**< Flag to indicate QR code needs regeneration */
} gfx_qrcode_t;

typedef struct {
    gfx_obj_t *obj;
    bool swap;
} gfx_qrcode_draw_data_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "qrcode";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_qrcode_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_qrcode_delete_impl(gfx_obj_t *obj);
static void gfx_qrcode_generate_callback(qrcode_wrapper_handle_t qrcode, void *user_data);
static esp_err_t gfx_qrcode_generate(gfx_obj_t *obj, bool swap);
static void gfx_qrcode_blend(gfx_obj_t *obj, gfx_qrcode_t *qrcode, const gfx_draw_ctx_t *ctx);
static void gfx_qrcode_init_default_state(gfx_qrcode_t *qrcode);

static const gfx_widget_class_t s_gfx_qrcode_widget_class = {
    .type = GFX_OBJ_TYPE_QRCODE,
    .name = "qrcode",
    .draw = gfx_qrcode_draw,
    .delete = gfx_qrcode_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_qrcode_init_default_state(gfx_qrcode_t *qrcode)
{
    memset(qrcode, 0, sizeof(*qrcode));
    qrcode->display_size = 100;
    qrcode->ecc = GFX_QRCODE_ECC_LOW;
    qrcode->color = (gfx_color_t) {
        .full = 0xFFFF
    };
    qrcode->bg_color = (gfx_color_t) {
        .full = 0x0000
    };
    qrcode->needs_update = true;
}

static void gfx_qrcode_generate_callback(qrcode_wrapper_handle_t qrcode, void *user_data)
{
    gfx_qrcode_draw_data_t *draw_data = (gfx_qrcode_draw_data_t *)user_data;
    gfx_obj_t *obj = draw_data->obj;
    bool swap = draw_data->swap;

    gfx_qrcode_t *qrcode_obj = (gfx_qrcode_t *)obj->src;

    int qr_size = qrcode_wrapper_get_size(qrcode);
    int scale = qrcode_obj->display_size / qr_size;
    if (scale < 1) {
        scale = 1;
    }

    int scaled_size = qr_size * scale;
    int display_size = qrcode_obj->display_size;

    int offset_x = (display_size - scaled_size) / 2;
    int offset_y = (display_size - scaled_size) / 2;

    GFX_LOGD(TAG, "generate qrcode: qr_size=%d display_size=%d scale=%d scaled_size=%d offset=(%d,%d)",
             qr_size, display_size, scale, scaled_size, offset_x, offset_y);

    if (qrcode_obj->qr_modules) {
        free(qrcode_obj->qr_modules);
        qrcode_obj->qr_modules = NULL;
    }

    size_t buffer_size = display_size * display_size * sizeof(uint16_t);
    qrcode_obj->qr_modules = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!qrcode_obj->qr_modules) {
        GFX_LOGE(TAG, "generate qrcode: allocate buffer failed (%zu bytes)", buffer_size);
        return;
    }

    uint16_t *pixel_buf = (uint16_t *)qrcode_obj->qr_modules;

    uint16_t fg_color = gfx_color_to_native_u16(qrcode_obj->color, swap);
    uint16_t bg_color = gfx_color_to_native_u16(qrcode_obj->bg_color, swap);

    for (int y = 0; y < display_size; y++) {
        for (int x = 0; x < display_size; x++) {
            pixel_buf[y * display_size + x] = bg_color;
        }
    }

    for (int qr_y = 0; qr_y < qr_size; qr_y++) {
        for (int qr_x = 0; qr_x < qr_size; qr_x++) {
            bool is_black = qrcode_wrapper_get_module(qrcode, qr_x, qr_y);
            uint16_t color = is_black ? fg_color : bg_color;

            for (int sx = 0; sx < scale; sx++) {
                int px = offset_x + qr_x * scale + sx;
                int py = offset_y + qr_y * scale;
                if (px >= 0 && px < display_size && py >= 0 && py < display_size) {
                    pixel_buf[py * display_size + px] = color;
                }
            }
        }

        for (int sy = 1; sy < scale; sy++) {
            int src_y = offset_y + qr_y * scale;
            int dst_y = offset_y + qr_y * scale + sy;
            if (src_y >= 0 && src_y < display_size && dst_y >= 0 && dst_y < display_size) {
                uint16_t *src_row = pixel_buf + src_y * display_size + offset_x;
                uint16_t *dst_row = pixel_buf + dst_y * display_size + offset_x;
                memcpy(dst_row, src_row, scaled_size * sizeof(uint16_t));
            }
        }
    }

    qrcode_obj->qr_size = qr_size;
    qrcode_obj->scaled_size = display_size;

    GFX_LOGD(TAG, "generate qrcode: buffer generated");
}

static esp_err_t gfx_qrcode_generate(gfx_obj_t *obj, bool swap)
{
    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    if (!qrcode->text || qrcode->text_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int ecc_level = QRCODE_WRAPPER_ECC_LOW;
    switch (qrcode->ecc) {
    case GFX_QRCODE_ECC_LOW:
        ecc_level = QRCODE_WRAPPER_ECC_LOW;
        break;
    case GFX_QRCODE_ECC_MEDIUM:
        ecc_level = QRCODE_WRAPPER_ECC_MED;
        break;
    case GFX_QRCODE_ECC_QUARTILE:
        ecc_level = QRCODE_WRAPPER_ECC_QUART;
        break;
    case GFX_QRCODE_ECC_HIGH:
        ecc_level = QRCODE_WRAPPER_ECC_HIGH;
        break;
    }

    gfx_qrcode_draw_data_t draw_data = {
        .obj = obj,
        .swap = swap
    };

    qrcode_wrapper_config_t cfg = {
        .display_func = gfx_qrcode_generate_callback,
        .max_qrcode_version = 5,
        .qrcode_ecc_level = ecc_level,
        .user_data = &draw_data
    };
    qrcode_wrapper_generate(&cfg, qrcode->text);

    GFX_LOGD(TAG, "generate qrcode: size=%d", qrcode->qr_size);
    return ESP_OK;
}

static void gfx_qrcode_blend(gfx_obj_t *obj, gfx_qrcode_t *qrcode, const gfx_draw_ctx_t *ctx)
{
    gfx_obj_calc_pos_in_parent(obj);

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y,
                           obj->geometry.x + qrcode->scaled_size,
                           obj->geometry.y + qrcode->scaled_size
                          };
    gfx_area_t clip_area;

    if (!gfx_area_intersect_exclusive(&clip_area, &render_area, &obj_area)) {
        return;
    }

    gfx_coord_t src_stride = qrcode->scaled_size;
    gfx_color_t *src_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(qrcode->qr_modules,
                              clip_area.y1 - obj->geometry.y,
                              src_stride,
                              clip_area.x1 - obj->geometry.x);
    gfx_color_t *dest_pixels = GFX_DRAW_CTX_DEST_PTR(ctx, clip_area.x1, clip_area.y1);

    gfx_sw_blend_img_draw(
        dest_pixels,
        ctx->stride,
        src_pixels,
        src_stride,
        NULL,
        0,
        &clip_area,
        ctx->swap
    );
}

static esp_err_t gfx_qrcode_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGD(TAG, "draw qrcode: object, state, or draw context is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_QRCODE) {
        GFX_LOGW(TAG, "draw qrcode: object type is not qrcode");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    if (qrcode->needs_update) {
        esp_err_t ret = gfx_qrcode_generate(obj, ctx->swap);
        if (ret != ESP_OK) {
            return ret;
        }
        qrcode->needs_update = false;
    }

    if (!qrcode->qr_modules) {
        GFX_LOGW(TAG, "draw qrcode: no generated data available");
        return ESP_ERR_INVALID_STATE;
    }

    gfx_qrcode_blend(obj, qrcode, ctx);
    return ESP_OK;
}

static esp_err_t gfx_qrcode_delete_impl(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    if (qrcode) {
        if (qrcode->text) {
            free(qrcode->text);
        }
        if (qrcode->qr_modules) {
            free(qrcode->qr_modules);
        }
        free(qrcode);
    }

    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_obj_t *gfx_qrcode_create(gfx_disp_t *disp)
{
    gfx_obj_t *obj = NULL;
    gfx_qrcode_t *qrcode;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create qrcode: display is NULL");
        return NULL;
    }

    qrcode = (gfx_qrcode_t *)malloc(sizeof(gfx_qrcode_t));
    if (qrcode == NULL) {
        GFX_LOGE(TAG, "create qrcode: no mem for state");
        return NULL;
    }
    gfx_qrcode_init_default_state(qrcode);
    if (gfx_obj_create_class_instance(disp, &s_gfx_qrcode_widget_class,
                                      qrcode, qrcode->display_size, qrcode->display_size,
                                      "gfx_qrcode_create", &obj) != ESP_OK) {
        free(qrcode);
        GFX_LOGE(TAG, "create qrcode: no mem for object");
        return NULL;
    }

    GFX_LOGD(TAG, "create qrcode: object created");
    return obj;
}

esp_err_t gfx_qrcode_set_data(gfx_obj_t *obj, const char *text)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    if (text == NULL) {
        GFX_LOGE(TAG, "set qrcode data: text is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    size_t text_len = strlen(text);
    if (text_len == 0) {
        GFX_LOGE(TAG, "set qrcode data: text is empty");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    if (qrcode->text) {
        free(qrcode->text);
    }

    qrcode->text = (char *)malloc(text_len + 1);
    if (!qrcode->text) {
        GFX_LOGE(TAG, "set qrcode data: allocate text buffer failed");
        return ESP_ERR_NO_MEM;
    }

    memcpy(qrcode->text, text, text_len);
    qrcode->text[text_len] = '\0';
    qrcode->text_len = text_len;
    qrcode->needs_update = true;

    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set qrcode data: text length=%zu", qrcode->text_len);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_size(gfx_obj_t *obj, uint16_t size)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    if (size == 0) {
        GFX_LOGE(TAG, "set qrcode size: size is zero");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->display_size = size;
    qrcode->needs_update = true;

    obj->geometry.width = size;
    obj->geometry.height = size;

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set qrcode size: %u", size);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_ecc(gfx_obj_t *obj, gfx_qrcode_ecc_t ecc)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->ecc = ecc;
    qrcode->needs_update = true;

    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set qrcode ecc: %d", ecc);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->color = color;
    qrcode->needs_update = true;

    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set qrcode color: 0x%04X", color.full);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->bg_color = bg_color;
    qrcode->needs_update = true;

    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set qrcode background color: 0x%04X", bg_color.full);
    return ESP_OK;
}
