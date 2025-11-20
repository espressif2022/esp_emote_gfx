/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "qrcode.h"
#include "common/gfx_comm.h"
#include "core/gfx_blend_priv.h"
#include "core/gfx_core_priv.h"
#include "core/gfx_refr_priv.h"
#include "widget/gfx_qrcode_priv.h"

static const char *TAG = "gfx_qrcode";

/*********************
 *      DEFINES
 *********************/

/* Helper macro for type checking */
#define CHECK_OBJ_TYPE_QRCODE(obj) \
    do { \
        ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, TAG, "Object is NULL"); \
        ESP_RETURN_ON_FALSE(obj->type == GFX_OBJ_TYPE_QRCODE, ESP_ERR_INVALID_ARG, TAG, \
                           "Object is not a QRCODE type (type=%d). Cannot use qrcode API on non-qrcode objects.", obj->type); \
    } while(0)

/**********************
 *  STATIC PROTOTYPES
 **********************/
typedef struct {
    gfx_obj_t *obj;
    bool swap;
} gfx_qrcode_draw_data_t;

static void gfx_qrcode_generate_callback(esp_qrcode_handle_t qrcode, void *user_data);
static esp_err_t gfx_qrcode_generate(gfx_obj_t *obj, bool swap);
static void gfx_qrcode_blend_to_dest(gfx_obj_t *obj, gfx_qrcode_t *qrcode,
                                      int x1, int y1, int x2, int y2,
                                      const void *dest_buf, bool swap);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/


/**
 * @brief Callback function for esp_qrcode_generate
 * Generates scaled QR code image buffer (RGB565 format)
 */
static void gfx_qrcode_generate_callback(esp_qrcode_handle_t qrcode, void *user_data)
{
    gfx_qrcode_draw_data_t *draw_data = (gfx_qrcode_draw_data_t *)user_data;
    gfx_obj_t *obj = draw_data->obj;
    bool swap = draw_data->swap;

    gfx_qrcode_t *qrcode_obj = (gfx_qrcode_t *)obj->src;

    int qr_size = esp_qrcode_get_size(qrcode);
    int scale = qrcode_obj->display_size / qr_size;
    if (scale < 1) {
        scale = 1;
    }

    int scaled_size = qr_size * scale;
    
    ESP_LOGD(TAG, "Generating QR: qr_size=%d, display_size=%d, scale=%d, scaled_size=%d",
             qr_size, qrcode_obj->display_size, scale, scaled_size);

    /* Free old buffer if exists */
    if (qrcode_obj->qr_modules) {
        free(qrcode_obj->qr_modules);
        qrcode_obj->qr_modules = NULL;
    }

    /* Allocate buffer for scaled QR code image (RGB565: 2 bytes per pixel) */
    size_t buffer_size = scaled_size * scaled_size * sizeof(uint16_t);
    qrcode_obj->qr_modules = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!qrcode_obj->qr_modules) {
        ESP_LOGE(TAG, "Failed to allocate QR code buffer (%zu bytes)", buffer_size);
        return;
    }

    uint16_t *pixel_buf = (uint16_t *)qrcode_obj->qr_modules;
    
    /* Convert gfx_color_t to uint16_t */
    uint16_t fg_color = swap ? __builtin_bswap16(qrcode_obj->color.full) : qrcode_obj->color.full;
    uint16_t bg_color = swap ? __builtin_bswap16(qrcode_obj->bg_color.full) : qrcode_obj->bg_color.full;

    ESP_LOGI(TAG, "fg_color: 0x%04X, bg_color: 0x%04X", fg_color, bg_color);

    /* Generate scaled QR code image
     * scale it horizontally, then duplicate vertically */
    for (int qr_y = 0; qr_y < qr_size; qr_y++) {
        /* Process one QR module row */
        for (int qr_x = 0; qr_x < qr_size; qr_x++) {
            /* Get QR module value (true = black/foreground) */
            bool is_black = esp_qrcode_get_module(qrcode, qr_x, qr_y);
            uint16_t color = is_black ? fg_color : bg_color;
            
            /* Scale horizontally */
            for (int sx = 0; sx < scale; sx++) {
                int px = qr_x * scale + sx;
                int py = qr_y * scale;
                pixel_buf[py * scaled_size + px] = color;
            }
        }
        
        /* Duplicate row vertically for scaling */
        uint16_t *src_row = pixel_buf + (qr_y * scale) * scaled_size;
        for (int sy = 1; sy < scale; sy++) {
            uint16_t *dst_row = pixel_buf + (qr_y * scale + sy) * scaled_size;
            memcpy(dst_row, src_row, scaled_size * sizeof(uint16_t));
        }
    }

    /* Save QR code info */
    qrcode_obj->qr_size = qr_size;
    qrcode_obj->scaled_size = scaled_size;
    
    ESP_LOGD(TAG, "QR code buffer generated successfully");
}

/**
 * @brief Generate QR Code from text
 */
static esp_err_t gfx_qrcode_generate(gfx_obj_t *obj, bool swap)
{
    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    if (!qrcode->text || qrcode->text_len == 0) {
        ESP_LOGW(TAG, "No text to encode");
        return ESP_ERR_INVALID_ARG;
    }

    /* Map ECC level */
    int ecc_level = ESP_QRCODE_ECC_LOW;
    switch (qrcode->ecc) {
        case GFX_QRCODE_ECC_LOW:
            ecc_level = ESP_QRCODE_ECC_LOW;
            break;
        case GFX_QRCODE_ECC_MEDIUM:
            ecc_level = ESP_QRCODE_ECC_MED;
            break;
        case GFX_QRCODE_ECC_QUARTILE:
            ecc_level = ESP_QRCODE_ECC_QUART;
            break;
        case GFX_QRCODE_ECC_HIGH:
            ecc_level = ESP_QRCODE_ECC_HIGH;
            break;
    }

    gfx_qrcode_draw_data_t draw_data = {
        .obj = obj,
        .swap = swap
    };

    /* Generate QR Code */
    esp_qrcode_config_t cfg = {
        .display_func = gfx_qrcode_generate_callback,
        .max_qrcode_version = 5,
        .qrcode_ecc_level = ecc_level,
        .user_data = &draw_data
    };
    esp_qrcode_generate(&cfg, qrcode->text);

    ESP_LOGD(TAG, "Generated QR Code: size=%d", qrcode->qr_size);
    return ESP_OK;
}

/**
 * @brief Blend QR code image to destination buffer using hardware-accelerated blend
 * @param obj QR code object
 * @param qrcode QR code context
 * @param x1 Render area left
 * @param y1 Render area top
 * @param x2 Render area right
 * @param y2 Render area bottom
 * @param dest_buf Destination buffer
 * @param swap Whether to swap byte order
 */
static void gfx_qrcode_blend_to_dest(gfx_obj_t *obj, gfx_qrcode_t *qrcode,
                                      int x1, int y1, int x2, int y2,
                                      const void *dest_buf, bool swap)
{
    /* Get parent container dimensions for alignment calculation */
    uint32_t parent_w, parent_h;
    gfx_emote_get_screen_size(obj->parent_handle, &parent_w, &parent_h);

    gfx_coord_t *obj_x = &obj->x;
    gfx_coord_t *obj_y = &obj->y;
    gfx_obj_cal_aligned_pos(obj, parent_w, parent_h, obj_x, obj_y);

    /* Calculate clipping area */
    gfx_area_t render_area = {x1, y1, x2, y2};
    gfx_area_t obj_area = {*obj_x, *obj_y, *obj_x + qrcode->scaled_size, *obj_y + qrcode->scaled_size};
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        return;
    }

    /* Prepare buffer parameters for blend operation */
    gfx_coord_t dest_stride = (x2 - x1);
    gfx_coord_t src_stride = qrcode->scaled_size;

    /* Calculate source and destination buffer pointers with offset */
    gfx_color_t *src_pixels = (gfx_color_t *)qrcode->qr_modules + 
                               (clip_area.y1 - *obj_y) * src_stride + 
                               (clip_area.x1 - *obj_x);
    gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf + 
                                (clip_area.y1 - y1) * dest_stride + 
                                (clip_area.x1 - x1);

    gfx_sw_blend_img_draw(
        dest_pixels,
        dest_stride,
        src_pixels,
        src_stride,
        NULL,  /* No alpha mask - QR codes are opaque */
        0,     /* No mask stride */
        &clip_area,
        255,   /* Fully opaque */
        swap
    );
}

/**
 * @brief Draw QR Code to destination buffer
 */
void gfx_draw_qrcode(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    if (obj == NULL || obj->src == NULL) {
        ESP_LOGD(TAG, "Invalid object or source");
        return;
    }

    if (obj->type != GFX_OBJ_TYPE_QRCODE) {
        ESP_LOGW(TAG, "Object is not a QR Code type");
        return;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    /* Generate QR Code if needed */
    if (qrcode->needs_update) {
        esp_err_t ret = gfx_qrcode_generate(obj, swap);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to generate QR Code");
            return;
        }
        qrcode->needs_update = false;
    }

    if (!qrcode->qr_modules) {
        ESP_LOGW(TAG, "No QR Code data available");
        return;
    }

    /* Blend QR code to destination using hardware-accelerated function */
    gfx_qrcode_blend_to_dest(obj, qrcode, x1, y1, x2, y2, dest_buf, swap);
}

/*=====================
 * QR Code object creation and management
 *====================*/

gfx_obj_t *gfx_qrcode_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "No mem for QR Code object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->type = GFX_OBJ_TYPE_QRCODE;
    obj->parent_handle = handle;
    obj->is_visible = true;

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)malloc(sizeof(gfx_qrcode_t));
    if (qrcode == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for QR Code object");
        free(obj);
        return NULL;
    }
    memset(qrcode, 0, sizeof(gfx_qrcode_t));

    /* Set default values */
    qrcode->display_size = 100;  /* Default 100x100 pixels */
    qrcode->ecc = GFX_QRCODE_ECC_LOW;
    qrcode->color = (gfx_color_t) { .full = 0x0000 };     /* Black */
    qrcode->bg_color = (gfx_color_t) { .full = 0xFFFF };  /* White */
    qrcode->needs_update = true;

    obj->src = qrcode;
    obj->width = qrcode->display_size;
    obj->height = qrcode->display_size;

    gfx_obj_invalidate(obj);
    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_QRCODE, obj);
    ESP_LOGD(TAG, "Created QR Code object");
    return obj;
}

/*=====================
 * QR Code setter functions
 *====================*/

esp_err_t gfx_qrcode_set_data(gfx_obj_t *obj, const char *text)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    if (text == NULL) {
        ESP_LOGE(TAG, "Invalid text");
        return ESP_ERR_INVALID_ARG;
    }

    /* Calculate text length automatically */
    size_t text_len = strlen(text);
    if (text_len == 0) {
        ESP_LOGE(TAG, "Empty text");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    /* Free old text */
    if (qrcode->text) {
        free(qrcode->text);
    }

    /* Allocate and copy new text */
    qrcode->text = (char *)malloc(text_len + 1);
    if (!qrcode->text) {
        ESP_LOGE(TAG, "Failed to allocate text buffer");
        return ESP_ERR_NO_MEM;
    }

    memcpy(qrcode->text, text, text_len);
    qrcode->text[text_len] = '\0';
    qrcode->text_len = text_len;
    qrcode->needs_update = true;

    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set QR Code text: %s", qrcode->text);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_size(gfx_obj_t *obj, uint16_t size)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    if (size == 0) {
        ESP_LOGE(TAG, "Invalid size");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->display_size = size;
    qrcode->needs_update = true;  /* Size change requires buffer regeneration */
    
    obj->width = size;
    obj->height = size;

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set QR Code size: %d", size);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_ecc(gfx_obj_t *obj, gfx_qrcode_ecc_t ecc)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->ecc = ecc;
    qrcode->needs_update = true;

    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set QR Code ECC level: %d", ecc);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->color = color;
    qrcode->needs_update = true;  /* Color is encoded in buffer */

    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set QR Code color: 0x%04X", color.full);
    return ESP_OK;
}

esp_err_t gfx_qrcode_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color)
{
    CHECK_OBJ_TYPE_QRCODE(obj);

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;
    qrcode->bg_color = bg_color;
    qrcode->needs_update = true;  /* Color is encoded in buffer */

    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set QR Code background color: 0x%04X", bg_color.full);
    return ESP_OK;
}

/*=====================
 * QR Code object deletion
 *====================*/

esp_err_t gfx_qrcode_delete(gfx_obj_t *obj)
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
