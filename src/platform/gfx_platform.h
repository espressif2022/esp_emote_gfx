/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/gfx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_PLATFORM_WAIT_FOREVER UINT32_MAX
#define GFX_PLATFORM_TICK_HZ      1000U

#define GFX_PLATFORM_HEAP_DEFAULT 0x00000001U
#define GFX_PLATFORM_HEAP_DMA     0x00000002U
#define GFX_PLATFORM_HEAP_SPIRAM  0x00000004U
#define GFX_PLATFORM_HEAP_INTERNAL 0x00000008U
#define GFX_PLATFORM_HEAP_8BIT    0x00000010U

typedef void *gfx_platform_event_t;
typedef void *gfx_platform_mutex_t;
typedef void *gfx_platform_task_t;
typedef uint32_t gfx_platform_event_bits_t;

typedef void (*gfx_platform_task_fn_t)(void *arg);

typedef struct {
    const char *name;
    uint32_t stack_size;
    uint32_t priority;
    int32_t affinity;
    uint32_t stack_caps;
} gfx_platform_task_config_t;

gfx_platform_event_t gfx_platform_event_create(void);
void gfx_platform_event_delete(gfx_platform_event_t event);
gfx_platform_event_bits_t gfx_platform_event_set(gfx_platform_event_t event, gfx_platform_event_bits_t bits);
gfx_platform_event_bits_t gfx_platform_event_set_from_isr(gfx_platform_event_t event, gfx_platform_event_bits_t bits,
        bool *need_yield);
gfx_platform_event_bits_t gfx_platform_event_clear(gfx_platform_event_t event, gfx_platform_event_bits_t bits);
gfx_platform_event_bits_t gfx_platform_event_wait(gfx_platform_event_t event, gfx_platform_event_bits_t bits,
        bool clear_on_exit, bool wait_all, uint32_t timeout_ms);
void gfx_platform_yield_from_isr(void);
bool gfx_platform_in_isr(void);

gfx_platform_mutex_t gfx_platform_mutex_create_recursive(void);
void gfx_platform_mutex_delete(gfx_platform_mutex_t mutex);
bool gfx_platform_mutex_lock(gfx_platform_mutex_t mutex, uint32_t timeout_ms);
bool gfx_platform_mutex_unlock(gfx_platform_mutex_t mutex);

gfx_err_t gfx_platform_task_create(const gfx_platform_task_config_t *cfg, gfx_platform_task_fn_t fn,
                                   void *arg, gfx_platform_task_t *out_task);
void gfx_platform_task_delete_current(void);
void gfx_platform_delay_ms(uint32_t ms);
uint32_t gfx_platform_min_delay_ms(void);

void *gfx_platform_malloc(size_t size, uint32_t caps);
void *gfx_platform_calloc(size_t count, size_t size, uint32_t caps);
void *gfx_platform_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void gfx_platform_free(void *ptr);

int64_t gfx_platform_time_us(void);

bool gfx_platform_psram_dma_capable(void);

#ifdef __cplusplus
}
#endif
