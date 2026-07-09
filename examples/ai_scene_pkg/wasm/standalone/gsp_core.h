/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GSP_CORE_MAGIC   0x31505347u
#define GSP_CORE_VERSION 4u

#define GSP_CORE_HEADER_SIZE 56u
#define GSP_CORE_OBJ_SIZE    64u
#define GSP_CORE_BLOB_SIZE   20u
#define GSP_CORE_ACTION_SIZE 24u

#define GSP_CORE_NO_PARENT     0xFFFFu
#define GSP_CORE_ACT_NO_TARGET 0xFFFFu

enum {
    GSP_CORE_OK = 0,
    GSP_CORE_ERR_ARG = -1,
    GSP_CORE_ERR_SIZE = -2,
    GSP_CORE_ERR_MAGIC = -3,
    GSP_CORE_ERR_VERSION = -4,
    GSP_CORE_ERR_BOUNDS = -5,
    GSP_CORE_ERR_CRC = -6,
    GSP_CORE_ERR_COUNT = -7,
    GSP_CORE_ERR_PARENT = -8,
    GSP_CORE_ERR_STRING = -9,
    GSP_CORE_ERR_TYPE = -10,
    GSP_CORE_ERR_BLOB = -11,
    GSP_CORE_ERR_CODEC = -12,
    GSP_CORE_ERR_ACTION = -13,
    GSP_CORE_ERR_ALLOC = -14,
};

enum {
    GSP_CORE_OBJ_CONTAINER = 1,
    GSP_CORE_OBJ_LABEL     = 2,
    GSP_CORE_OBJ_BUTTON    = 3,
    GSP_CORE_OBJ_IMAGE     = 4,
    GSP_CORE_OBJ_LIST      = 5,
    GSP_CORE_OBJ_WHEEL     = 6,
    GSP_CORE_OBJ_LAYER     = 7,
};

enum {
    GSP_CORE_CODEC_STORE = 0,
    GSP_CORE_CODEC_RLE16 = 1,
};

enum {
    GSP_CORE_EV_NONE    = 0,
    GSP_CORE_EV_CLICK   = 1,
    GSP_CORE_EV_PRESS   = 2,
    GSP_CORE_EV_RELEASE = 3,
    GSP_CORE_EV_LONG    = 4,
    GSP_CORE_EV_VALUE   = 5,
};

enum {
    GSP_CORE_ACT_NONE         = 0,
    GSP_CORE_ACT_SHOW         = 1,
    GSP_CORE_ACT_HIDE         = 2,
    GSP_CORE_ACT_TOGGLE       = 3,
    GSP_CORE_ACT_SET_TEXT     = 4,
    GSP_CORE_ACT_SET_BG_COLOR = 5,
    GSP_CORE_ACT_SET_OPACITY  = 6,
    GSP_CORE_ACT_CALL         = 7,
    GSP_CORE_ACT_GOTO         = 8,
    GSP_CORE_ACT_BACK         = 9,
};

#define GSP_CORE_F_TEXT      (1u << 0)
#define GSP_CORE_F_FG_COLOR  (1u << 1)
#define GSP_CORE_F_BG_COLOR  (1u << 2)
#define GSP_CORE_F_BORDER    (1u << 3)
#define GSP_CORE_F_RADIUS    (1u << 4)
#define GSP_CORE_F_CALLBACK  (1u << 5)
#define GSP_CORE_F_IMAGE     (1u << 6)
#define GSP_CORE_F_NAME      (1u << 7)
#define GSP_CORE_F_HIDDEN    (1u << 8)
#define GSP_CORE_F_OPACITY   (1u << 9)
#define GSP_CORE_F_ALIGN     (1u << 10)
#define GSP_CORE_F_PARAMS    (1u << 11)

#define GSP_CORE_ITEM_SELECTED_NONE 0xFFFFu

/* Local copy of the color format values used inside GSP blob entries. */
#define GSP_CORE_CF_RGB565          0x04u
#define GSP_CORE_CF_RGB565_SWAPPED  0x05u
#define GSP_CORE_CF_RGB888          0x0Fu
#define GSP_CORE_CF_BGR888          0x13u
#define GSP_CORE_CF_XRGB8888        0x11u
#define GSP_CORE_CF_ARGB8888        0x12u

typedef struct {
    uint32_t version;
    uint16_t screen_w;
    uint16_t screen_h;
    uint32_t screen_bg;
    uint32_t obj_count;
    uint32_t obj_table_off;
    uint32_t str_table_off;
    uint32_t blob_count;
    uint32_t blob_table_off;
    uint32_t total_size;
    uint32_t crc32;
    uint32_t action_count;
    uint32_t action_table_off;
} gsp_core_header_t;

typedef struct {
    uint16_t type;
    uint16_t parent_idx;
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t flags;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t border_color;
    uint16_t border_width;
    uint16_t radius;
    uint32_t text_off;
    uint32_t callback_off;
    uint32_t name_off;
    uint32_t blob_idx;
    uint32_t params_off;
    uint16_t params_len;
    uint8_t opacity;
    uint8_t text_align;
    uint16_t font_id;
    uint16_t bind_id;
} gsp_core_obj_t;

typedef struct {
    uint16_t w;
    uint16_t h;
    uint8_t cf;
    uint8_t codec;
    uint16_t stride;
    uint32_t raw_size;
    uint32_t comp_size;
    uint32_t data_off;
} gsp_core_blob_t;

typedef struct {
    uint16_t src_idx;
    uint16_t event;
    uint16_t action;
    uint16_t target_idx;
    uint32_t target_name_off;
    uint32_t param_off;
    uint16_t param_len;
    uint16_t flags;
    uint32_t arg;
} gsp_core_action_t;

typedef struct {
    uint16_t item_count;
    uint16_t selected;
    uint16_t item_height;
    uint16_t rows_or_page;
    uint16_t flags;
    const uint8_t *items;
    size_t items_len;
} gsp_core_item_params_t;

typedef struct gsp_core_runtime gsp_core_runtime_t;

uint16_t gsp_core_rd_u16(const uint8_t *p);
int16_t gsp_core_rd_i16(const uint8_t *p);
uint32_t gsp_core_rd_u32(const uint8_t *p);
uint32_t gsp_core_crc32(const uint8_t *buf, uint32_t total);

int gsp_core_parse_header(const uint8_t *buf, size_t size, gsp_core_header_t *out);
int gsp_core_get_object(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                        uint32_t index, gsp_core_obj_t *out);
int gsp_core_get_blob(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                      uint32_t index, gsp_core_blob_t *out);
int gsp_core_get_action(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                        uint32_t index, gsp_core_action_t *out);
const char *gsp_core_get_cstr(const uint8_t *buf, size_t size, uint32_t off);
int gsp_core_parse_item_params(const uint8_t *buf, size_t size, uint32_t off,
                               uint16_t len, gsp_core_item_params_t *out);
int gsp_core_validate(const uint8_t *buf, size_t size);
const char *gsp_core_err_name(int err);

int gsp_core_render_rgba(const uint8_t *buf, size_t size, uint8_t *rgba,
                         uint32_t width, uint32_t height, uint32_t stride);

gsp_core_runtime_t *gsp_core_runtime_create(const uint8_t *buf, size_t size);
void gsp_core_runtime_destroy(gsp_core_runtime_t *rt);
int gsp_core_runtime_render_rgba(gsp_core_runtime_t *rt, uint8_t *rgba,
                                 uint32_t width, uint32_t height, uint32_t stride);
int gsp_core_runtime_hit_test(gsp_core_runtime_t *rt, int32_t x, int32_t y);
int gsp_core_runtime_click(gsp_core_runtime_t *rt, int32_t x, int32_t y);
const char *gsp_core_runtime_last_call(const gsp_core_runtime_t *rt);
void gsp_core_runtime_clear_last_call(gsp_core_runtime_t *rt);

#ifdef __cplusplus
}
#endif
