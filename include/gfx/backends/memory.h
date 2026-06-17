/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_memory_backend gfx_memory_backend_t;

typedef struct {
    uint32_t h_res;          /**< Framebuffer width in pixels */
    uint32_t v_res;          /**< Framebuffer height in pixels */
    gfx_color_format_t color_format; /**< Backend framebuffer format (0 = RGB565) */
    void *buffer;            /**< Optional external framebuffer */
    size_t buffer_pixels;    /**< External framebuffer size in pixels */
} gfx_memory_backend_config_t;

/**
 * @brief Create a memory display backend.
 *
 * If cfg->buffer is NULL, the backend allocates a full-screen RGB565 buffer.
 * If the returned backend is passed to gfx_display_add() and display creation
 * succeeds, the display owns it and will delete it from gfx_display_delete().
 *
 * @param cfg Memory backend configuration.
 * @return Backend pointer on success, or NULL on failure.
 */
gfx_backend_t *gfx_memory_backend_create(const gfx_memory_backend_config_t *cfg);

/**
 * @brief Delete a memory backend that is not owned by a display.
 * @param backend Backend returned by gfx_memory_backend_create().
 */
void gfx_memory_backend_delete(gfx_backend_t *backend);

/**
 * @brief Get the semantic RGB565 framebuffer.
 * @param backend Memory backend.
 * @return Framebuffer pointer, or NULL if backend is invalid.
 */
const uint16_t *gfx_memory_backend_get_buffer(const gfx_backend_t *backend);

/**
 * @brief Get raw framebuffer bytes.
 * @param backend Memory backend.
 * @return Framebuffer pointer, or NULL if backend is invalid.
 */
const void *gfx_memory_backend_get_buffer_data(const gfx_backend_t *backend);

/**
 * @brief Get memory backend framebuffer color format.
 * @param backend Memory backend.
 * @return Framebuffer color format, or UNKNOWN if backend is invalid.
 */
gfx_color_format_t gfx_memory_backend_get_color_format(const gfx_backend_t *backend);

/**
 * @brief Get framebuffer size in pixels.
 * @param backend Memory backend.
 * @return Number of pixels, or 0 if backend is invalid.
 */
size_t gfx_memory_backend_get_buffer_pixels(const gfx_backend_t *backend);

/**
 * @brief Get framebuffer size in bytes.
 * @param backend Memory backend.
 * @return Number of bytes, or 0 if backend is invalid.
 */
size_t gfx_memory_backend_get_buffer_bytes(const gfx_backend_t *backend);

/**
 * @brief Fill the memory backend framebuffer with a semantic RGB565 color.
 * @param backend Memory backend.
 * @param color Semantic RGB565 color.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_memory_backend_clear(gfx_backend_t *backend, gfx_color_t color);

#ifdef __cplusplus
}
#endif
