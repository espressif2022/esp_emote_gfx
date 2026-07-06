/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write-back CPU cache for a buffer range before DMA2D read (no-op without DMA2D).
 */
void gfx_platform_async_fbcpy_cache_msync(const void *addr, size_t size);

/**
 * @brief Copy a rectangular region between two equal-sized framebuffers.
 *
 * Uses DMA2D (esp_async_fbcpy) when available, otherwise CPU memcpy.
 *
 * @param src_fb       Source framebuffer base
 * @param dst_fb       Destination framebuffer base
 * @param fb_w         Framebuffer width in pixels
 * @param fb_h         Framebuffer height in pixels
 * @param src_stride_px Source row stride in pixels (0 = fb_w)
 * @param x            Region left
 * @param y            Region top
 * @param copy_w       Region width in pixels
 * @param copy_h       Region height in pixels
 * @param pixel_size   Bytes per pixel (2 or 3 supported for DMA2D)
 * @return true on success
 */
bool gfx_platform_async_fbcpy_region(const void *src_fb,
                                     void *dst_fb,
                                     uint32_t fb_w,
                                     uint32_t fb_h,
                                     uint32_t src_stride_px,
                                     uint32_t x,
                                     uint32_t y,
                                     uint32_t copy_w,
                                     uint32_t copy_h,
                                     size_t pixel_size);

#ifdef __cplusplus
}
#endif
