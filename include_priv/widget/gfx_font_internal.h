/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "core/gfx_types.h"
#include "core/gfx_core.h"
#include "core/gfx_obj.h"
#include "widget/gfx_label.h"
#include "ft2build.h"

#include FT_FREETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef void *gfx_ft_handle_t;
typedef void *gfx_ft_lib_handle_t;

typedef struct face_entry {
    void *face;
    const void *mem;
    struct face_entry *next;
} gfx_ft_face_entry_t;

typedef struct {
    gfx_ft_face_entry_t *ft_face_head;
    void *ft_library;
} gfx_ft_lib_t;

/* Default font configuration */
typedef struct {
    const char *name;      /*!< Font name */
    const void *mem;       /*!< Font data pointer */
    size_t mem_size;       /*!< Font data size */
    uint16_t default_size; /*!< Default font size */
    gfx_color_t bg_color; /*!< Default font color */
    gfx_opa_t default_opa; /*!< Default opacity */
} gfx_default_font_cfg_t;

// Internal function declarations
esp_err_t gfx_ft_lib_create(gfx_ft_lib_handle_t *ret_lib);
esp_err_t gfx_ft_lib_cleanup(gfx_ft_lib_handle_t lib_handle);

esp_err_t gfx_get_glphy_dsc(gfx_obj_t * obj);

/**
 * @brief Get default font handle (internal use)
 * @param handle Animation player handle
 * @param ret_font Pointer to store the default font handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_get_default_font(gfx_handle_t handle, gfx_font_t *ret_font);

/**
 * @brief Get default font configuration (internal use)
 * @param font Pointer to store default font handle
 * @param size Pointer to store default font size
 * @param color Pointer to store default font color
 * @param opa Pointer to store default font opacity
 */
void gfx_get_default_font_config(gfx_font_t *font, uint16_t *size, gfx_color_t *color, gfx_opa_t *opa);

#ifdef __cplusplus
}
#endif