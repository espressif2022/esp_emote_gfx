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
#include "lib/qrcode/qrcode_wrapper.h"
#include "common/gfx_comm.h"
#include "core/gfx_blend_priv.h"
#include "core/gfx_refr_priv.h"
#include "widget/gfx_qrcode.h"

/*********************
 *      DEFINES
 *********************/
/* Use generic type checking macro from gfx_obj_priv.h */
#define CHECK_OBJ_TYPE_QRCODE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_QRCODE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

/* QR Code context structure */
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

/* Draw callback data */
typedef struct {
    gfx_obj_t *obj;
    bool swap;
} gfx_qrcode_draw_data_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "gfx_qrcode";

/**********************
 *  STATIC PROTOTYPES
 **********************/

/* Virtual functions */
static esp_err_t gfx_draw_qrcode(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);
static esp_err_t gfx_qrcode_delete(gfx_obj_t *obj);

/* Helper functions */
static void gfx_qrcode_generate_callback(qrcode_wrapper_handle_t qrcode, void *user_data);
static esp_err_t gfx_qrcode_generate(gfx_obj_t *obj, bool swap);
static void gfx_qrcode_blend_to_dest(gfx_obj_t *obj, gfx_qrcode_t *qrcode,
                                     int x1, int y1, int x2, int y2,
                                     const void *dest_buf, bool swap);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief Callback function for qrcode_wrapper_generate
 * Generates scaled QR code image buffer (RGB565 format)
 */
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

    /* Calculate centering offset when scaled_size < display_size */
    int offset_x = (display_size - scaled_size) / 2;
    int offset_y = (display_size - scaled_size) / 2;

    ESP_LOGD(TAG, "Generating QR: qr_size=%d, display_size=%d, scale=%d, scaled_size=%d, offset=(%d,%d)",
             qr_size, display_size, scale, scaled_size, offset_x, offset_y);

    /* Free old buffer if exists */
    if (qrcode_obj->qr_modules) {
        free(qrcode_obj->qr_modules);
        qrcode_obj->qr_modules = NULL;
    }

    /* Allocate buffer for display size (RGB565: 2 bytes per pixel) */
    size_t buffer_size = display_size * display_size * sizeof(uint16_t);
    qrcode_obj->qr_modules = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!qrcode_obj->qr_modules) {
        ESP_LOGE(TAG, "Failed to allocate QR code buffer (%zu bytes)", buffer_size);
        return;
    }

    uint16_t *pixel_buf = (uint16_t *)qrcode_obj->qr_modules;

    /* Convert gfx_color_t to uint16_t */
    uint16_t fg_color = swap ? __builtin_bswap16(qrcode_obj->color.full) : qrcode_obj->color.full;
    uint16_t bg_color = swap ? __builtin_bswap16(qrcode_obj->bg_color.full) : qrcode_obj->bg_color.full;

    /* Fill entire buffer with background color */
    for (int y = 0; y < display_size; y++) {
        for (int x = 0; x < display_size; x++) {
            pixel_buf[y * display_size + x] = bg_color;
        }
    }

    /* Generate scaled QR code image and place it centered
     * Scale it horizontally, then duplicate vertically */
    for (int qr_y = 0; qr_y < qr_size; qr_y++) {
        /* Process one QR module row */
        for (int qr_x = 0; qr_x < qr_size; qr_x++) {
            /* Get QR module value (true = black/foreground) */
            bool is_black = qrcode_wrapper_get_module(qrcode, qr_x, qr_y);
            uint16_t color = is_black ? fg_color : bg_color;

            /* Scale horizontally with centering offset */
            for (int sx = 0; sx < scale; sx++) {
                int px = offset_x + qr_x * scale + sx;
                int py = offset_y + qr_y * scale;
                if (px >= 0 && px < display_size && py >= 0 && py < display_size) {
                    pixel_buf[py * display_size + px] = color;
                }
            }
        }

        /* Duplicate row vertically for scaling with centering offset */
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

    /* Save QR code info - use display_size as the actual buffer size */
    qrcode_obj->qr_size = qr_size;
    qrcode_obj->scaled_size = display_size;

    ESP_LOGD(TAG, "QR code buffer generated successfully");
}

/**
 * @brief Generate QR Code from text
 */
static esp_err_t gfx_qrcode_generate(gfx_obj_t *obj, bool swap)
{
    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    if (!qrcode->text || qrcode->text_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Map ECC level */
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

    /* Generate QR Code */
    qrcode_wrapper_config_t cfg = {
        .display_func = gfx_qrcode_generate_callback,
        .max_qrcode_version = 5,
        .qrcode_ecc_level = ecc_level,
        .user_data = &draw_data
    };
    qrcode_wrapper_generate(&cfg, qrcode->text);

    ESP_LOGD(TAG, "Generated QR Code: size=%d", qrcode->qr_size);
    return ESP_OK;
}

/**
 * @brief Blend QR code image to destination buffer
 */
static void gfx_qrcode_blend_to_dest(gfx_obj_t *obj, gfx_qrcode_t *qrcode,
                                     int x1, int y1, int x2, int y2,
                                     const void *dest_buf, bool swap)
{
    /* Get parent dimensions and calculate aligned position */
    gfx_obj_calc_pos_in_parent(obj);

    /* Calculate clipping area */
    gfx_area_t render_area = {x1, y1, x2, y2};
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y,
                           obj->geometry.x + qrcode->scaled_size,
                           obj->geometry.y + qrcode->scaled_size
                          };
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        return;
    }

    /* Prepare buffer parameters for blend operation */
    gfx_coord_t dest_stride = (x2 - x1);
    gfx_coord_t src_stride = qrcode->scaled_size;

    /* Calculate source and destination buffer pointers with offset */
    gfx_color_t *src_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(qrcode->qr_modules,
                              clip_area.y1 - obj->geometry.y,
                              src_stride,
                              clip_area.x1 - obj->geometry.x);
    gfx_color_t *dest_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(dest_buf,
                               clip_area.y1 - y1,
                               dest_stride,
                               clip_area.x1 - x1);

    /* Blend QR code to destination */
    gfx_sw_blend_img_draw(
        dest_pixels,
        dest_stride,
        src_pixels,
        src_stride,
        NULL,  /* No alpha mask - QR codes are opaque */
        0,     /* No mask stride */
        &clip_area,
        swap
    );
}

/**
 * @brief Virtual draw function for QR code widget
 */
static esp_err_t gfx_draw_qrcode(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    if (obj == NULL || obj->src == NULL) {
        ESP_LOGD(TAG, "Invalid object or source");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_QRCODE) {
        ESP_LOGW(TAG, "Object is not a QR Code type");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_qrcode_t *qrcode = (gfx_qrcode_t *)obj->src;

    /* Generate QR Code if needed */
    if (qrcode->needs_update) {
        esp_err_t ret = gfx_qrcode_generate(obj, swap);
        if (ret != ESP_OK) {
            return ret;
        }
        qrcode->needs_update = false;
    }

    if (!qrcode->qr_modules) {
        ESP_LOGW(TAG, "No QR Code data available");
        return ESP_ERR_INVALID_STATE;
    }

    /* Blend QR code to destination */
    gfx_qrcode_blend_to_dest(obj, qrcode, x1, y1, x2, y2, dest_buf, swap);
    return ESP_OK;
}

/**
 * @brief Virtual delete function for QR code widget
 */
static esp_err_t gfx_qrcode_delete(gfx_obj_t *obj)
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

/**
 * @brief Create a QR code object
 */
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
    obj->state.is_visible = true;
    obj->vfunc.draw = gfx_draw_qrcode;
    obj->vfunc.delete = gfx_qrcode_delete;

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
    qrcode->color = (gfx_color_t) {
        .full = 0xFFFF
    };     /* White */
    qrcode->bg_color = (gfx_color_t) {
        .full = 0x0000
    };  /* Black */
    qrcode->needs_update = true;

    obj->src = qrcode;
    obj->geometry.width = qrcode->display_size;
    obj->geometry.height = qrcode->display_size;

    gfx_obj_invalidate(obj);
    gfx_emote_add_child(handle, GFX_OBJ_TYPE_QRCODE, obj);

    ESP_LOGD(TAG, "Created QR Code object");
    return obj;
}

/**
 * @brief Set QR code data/text
 */
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

/**
 * @brief Set QR code display size
 */
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

    obj->geometry.width = size;
    obj->geometry.height = size;

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set QR Code size: %d", size);
    return ESP_OK;
}

/**
 * @brief Set QR code error correction level
 */
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

/**
 * @brief Set QR code foreground color
 */
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

/**
 * @brief Set QR code background color
 */
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
