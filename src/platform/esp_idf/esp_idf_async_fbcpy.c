/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "platform/esp_idf/esp_idf_async_fbcpy.h"

#if defined(CONFIG_SOC_DMA2D_SUPPORTED) && CONFIG_SOC_DMA2D_SUPPORTED && __has_include("esp_async_fbcpy.h")
#define GFX_PLATFORM_ASYNC_FBCPY_DMA2D 1
#include "esp_async_fbcpy.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "hal/color_types.h"

#if defined(ESP_COLOR_FOURCC_RGB16) && defined(ESP_COLOR_FOURCC_RGB24)
#define GFX_ASYNC_FBCPY_DMA2D_PIXEL_FORMAT(_px_bytes) \
    .pixel_format_fourcc_id = ((_px_bytes) == 2U ? ESP_COLOR_FOURCC_RGB16 : ESP_COLOR_FOURCC_RGB24)
#else
#define GFX_ASYNC_FBCPY_DMA2D_PIXEL_FORMAT(_px_bytes) \
    .pixel_format_unique_id = { \
        .color_type_id = ((_px_bytes) == 2U) ? \
            COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565) : \
            COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888) \
    }
#endif
#else
#define GFX_PLATFORM_ASYNC_FBCPY_DMA2D 0
#endif

typedef struct {
#if GFX_PLATFORM_ASYNC_FBCPY_DMA2D
    esp_async_fbcpy_handle_t handle;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t done_sem;
    bool installed;
#else
    uint8_t unused;
#endif
} gfx_platform_async_fbcpy_state_t;

#if GFX_PLATFORM_ASYNC_FBCPY_DMA2D
static gfx_platform_async_fbcpy_state_t s_fbcpy;
#endif

void gfx_platform_async_fbcpy_cache_msync(const void *addr, size_t size)
{
#if GFX_PLATFORM_ASYNC_FBCPY_DMA2D
    size_t align = 0U;

    if (addr == NULL || size == 0U) {
        return;
    }

    if (esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &align) != ESP_OK || align == 0U) {
        if (esp_cache_get_alignment(MALLOC_CAP_INTERNAL, &align) != ESP_OK || align == 0U) {
            return;
        }
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t aligned_start = start & ~((uintptr_t)align - 1U);
    size_t padding = (size_t)(start - aligned_start);
    size_t aligned_size = ((size + padding + align - 1U) / align) * align;

    (void)esp_cache_msync((void *)aligned_start, aligned_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
#else
    (void)addr;
    (void)size;
#endif
}

#if GFX_PLATFORM_ASYNC_FBCPY_DMA2D

static bool IRAM_ATTR gfx_platform_async_fbcpy_done_cb(esp_async_fbcpy_handle_t mcp,
        esp_async_fbcpy_event_data_t *event_data,
        void *cb_args)
{
    BaseType_t high_task_woken = pdFALSE;

    (void)mcp;
    (void)event_data;
    (void)cb_args;

    if (s_fbcpy.done_sem != NULL) {
        xSemaphoreGiveFromISR(s_fbcpy.done_sem, &high_task_woken);
    }

    return high_task_woken == pdTRUE;
}

static bool gfx_platform_async_fbcpy_ensure_installed(void)
{
    if (s_fbcpy.installed) {
        return true;
    }

    esp_async_fbcpy_config_t cfg = { };
    if (esp_async_fbcpy_install(&cfg, &s_fbcpy.handle) != ESP_OK) {
        return false;
    }

    s_fbcpy.mutex = xSemaphoreCreateMutex();
    s_fbcpy.done_sem = xSemaphoreCreateBinary();
    if (s_fbcpy.mutex == NULL || s_fbcpy.done_sem == NULL) {
        esp_async_fbcpy_uninstall(s_fbcpy.handle);
        s_fbcpy.handle = NULL;
        if (s_fbcpy.mutex != NULL) {
            vSemaphoreDelete(s_fbcpy.mutex);
            s_fbcpy.mutex = NULL;
        }
        if (s_fbcpy.done_sem != NULL) {
            vSemaphoreDelete(s_fbcpy.done_sem);
            s_fbcpy.done_sem = NULL;
        }
        return false;
    }

    s_fbcpy.installed = true;
    return true;
}

static bool gfx_platform_async_fbcpy_region_dma2d(const void *src_fb,
        void *dst_fb,
        uint32_t fb_w,
        uint32_t fb_h,
        uint32_t src_stride_px,
        uint32_t x,
        uint32_t y,
        uint32_t copy_w,
        uint32_t copy_h,
        size_t pixel_size)
{
    bool ok;

    if (pixel_size != 2U && pixel_size != 3U) {
        return false;
    }

    if (!gfx_platform_async_fbcpy_ensure_installed()) {
        return false;
    }

    if (xSemaphoreTake(s_fbcpy.mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    xSemaphoreTake(s_fbcpy.done_sem, 0);
    gfx_platform_async_fbcpy_cache_msync(src_fb, (size_t)src_stride_px * copy_h * pixel_size);

    esp_async_fbcpy_trans_desc_t blit = {
        .src_buffer = src_fb,
        .dst_buffer = dst_fb,
        .src_buffer_size_x = src_stride_px,
        .src_buffer_size_y = fb_h,
        .src_offset_x = x,
        .src_offset_y = y,
        .dst_buffer_size_x = fb_w,
        .dst_buffer_size_y = fb_h,
        .dst_offset_x = x,
        .dst_offset_y = y,
        .copy_size_x = copy_w,
        .copy_size_y = copy_h,
        GFX_ASYNC_FBCPY_DMA2D_PIXEL_FORMAT(pixel_size),
    };

    ok = esp_async_fbcpy(s_fbcpy.handle, &blit, gfx_platform_async_fbcpy_done_cb, NULL) == ESP_OK &&
         xSemaphoreTake(s_fbcpy.done_sem, portMAX_DELAY) == pdTRUE;

    xSemaphoreGive(s_fbcpy.mutex);
    return ok;
}

#endif /* GFX_PLATFORM_ASYNC_FBCPY_DMA2D */

static bool gfx_platform_async_fbcpy_region_cpu(const void *src_fb,
        void *dst_fb,
        uint32_t fb_w,
        uint32_t src_stride_px,
        uint32_t x,
        uint32_t y,
        uint32_t copy_w,
        uint32_t copy_h,
        size_t pixel_size)
{
    const uint8_t *src_base = (const uint8_t *)src_fb;
    uint8_t *dst_base = (uint8_t *)dst_fb;
    size_t row_bytes = (size_t)copy_w * pixel_size;
    size_t src_line_bytes = (size_t)src_stride_px * pixel_size;
    size_t dst_line_bytes = (size_t)fb_w * pixel_size;

    src_base += ((size_t)y * src_stride_px + (size_t)x) * pixel_size;
    dst_base += ((size_t)y * fb_w + (size_t)x) * pixel_size;

    for (uint32_t row = 0; row < copy_h; row++) {
        memcpy(dst_base, src_base, row_bytes);
        src_base += src_line_bytes;
        dst_base += dst_line_bytes;
    }

    return true;
}

bool gfx_platform_async_fbcpy_region(const void *src_fb,
                                     void *dst_fb,
                                     uint32_t fb_w,
                                     uint32_t fb_h,
                                     uint32_t src_stride_px,
                                     uint32_t x,
                                     uint32_t y,
                                     uint32_t copy_w,
                                     uint32_t copy_h,
                                     size_t pixel_size)
{
    if (src_fb == NULL || dst_fb == NULL || fb_w == 0U || fb_h == 0U || pixel_size == 0U ||
            copy_w == 0U || copy_h == 0U) {
        return false;
    }

    if (src_stride_px == 0U) {
        src_stride_px = fb_w;
    }

    if (x + copy_w > fb_w || y + copy_h > fb_h) {
        return false;
    }

#if GFX_PLATFORM_ASYNC_FBCPY_DMA2D
    if (src_stride_px == fb_w &&
            gfx_platform_async_fbcpy_region_dma2d(src_fb, dst_fb, fb_w, fb_h, src_stride_px,
                    x, y, copy_w, copy_h, pixel_size)) {
        return true;
    }
#endif

    return gfx_platform_async_fbcpy_region_cpu(src_fb, dst_fb, fb_w, src_stride_px,
            x, y, copy_w, copy_h, pixel_size);
}
