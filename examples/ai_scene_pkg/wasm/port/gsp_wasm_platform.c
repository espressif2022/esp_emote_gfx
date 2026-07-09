/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "platform/gfx_platform.h"

#include <stdlib.h>
#include <string.h>

#include "core/fs/gfx_fs_priv.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

typedef struct {
    gfx_platform_event_bits_t bits;
} gsp_wasm_event_t;

typedef struct {
    uint32_t depth;
} gsp_wasm_mutex_t;

gfx_platform_event_t gfx_platform_event_create(void)
{
    return calloc(1, sizeof(gsp_wasm_event_t));
}

void gfx_platform_event_delete(gfx_platform_event_t event)
{
    free(event);
}

gfx_platform_event_bits_t gfx_platform_event_set(gfx_platform_event_t event,
                                                 gfx_platform_event_bits_t bits)
{
    gsp_wasm_event_t *e = (gsp_wasm_event_t *)event;
    if (e == NULL) {
        return 0;
    }
    e->bits |= bits;
    return e->bits;
}

gfx_platform_event_bits_t gfx_platform_event_set_from_isr(gfx_platform_event_t event,
                                                          gfx_platform_event_bits_t bits,
                                                          bool *need_yield)
{
    if (need_yield != NULL) {
        *need_yield = false;
    }
    return gfx_platform_event_set(event, bits);
}

gfx_platform_event_bits_t gfx_platform_event_clear(gfx_platform_event_t event,
                                                   gfx_platform_event_bits_t bits)
{
    gsp_wasm_event_t *e = (gsp_wasm_event_t *)event;
    if (e == NULL) {
        return 0;
    }
    gfx_platform_event_bits_t previous = e->bits;
    e->bits &= ~bits;
    return previous;
}

gfx_platform_event_bits_t gfx_platform_event_wait(gfx_platform_event_t event,
                                                  gfx_platform_event_bits_t bits,
                                                  bool clear_on_exit,
                                                  bool wait_all,
                                                  uint32_t timeout_ms)
{
    gsp_wasm_event_t *e = (gsp_wasm_event_t *)event;
    (void)timeout_ms;
    if (e == NULL) {
        return 0;
    }
    bool matched = wait_all ? ((e->bits & bits) == bits) : ((e->bits & bits) != 0);
    if (!matched) {
        return 0;
    }
    gfx_platform_event_bits_t ret = e->bits & bits;
    if (clear_on_exit) {
        e->bits &= ~ret;
    }
    return ret;
}

void gfx_platform_yield_from_isr(void)
{
}

bool gfx_platform_in_isr(void)
{
    return false;
}

gfx_platform_mutex_t gfx_platform_mutex_create_recursive(void)
{
    return calloc(1, sizeof(gsp_wasm_mutex_t));
}

void gfx_platform_mutex_delete(gfx_platform_mutex_t mutex)
{
    free(mutex);
}

bool gfx_platform_mutex_lock(gfx_platform_mutex_t mutex, uint32_t timeout_ms)
{
    gsp_wasm_mutex_t *m = (gsp_wasm_mutex_t *)mutex;
    (void)timeout_ms;
    if (m == NULL) {
        return false;
    }
    m->depth++;
    return true;
}

bool gfx_platform_mutex_unlock(gfx_platform_mutex_t mutex)
{
    gsp_wasm_mutex_t *m = (gsp_wasm_mutex_t *)mutex;
    if (m == NULL || m->depth == 0) {
        return false;
    }
    m->depth--;
    return true;
}

gfx_err_t gfx_platform_task_create(const gfx_platform_task_config_t *cfg,
                                   gfx_platform_task_fn_t fn,
                                   void *arg,
                                   gfx_platform_task_t *out_task)
{
    (void)cfg;
    (void)fn;
    (void)arg;
    if (out_task != NULL) {
        *out_task = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}

void gfx_platform_task_delete_current(void)
{
}

void gfx_platform_delay_ms(uint32_t ms)
{
    (void)ms;
}

uint32_t gfx_platform_min_delay_ms(void)
{
    return 1;
}

void *gfx_platform_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    return malloc(size);
}

void *gfx_platform_calloc(size_t count, size_t size, uint32_t caps)
{
    (void)caps;
    return calloc(count, size);
}

void *gfx_platform_aligned_alloc(size_t alignment, size_t size, uint32_t caps)
{
    (void)alignment;
    (void)caps;
    return malloc(size);
}

void gfx_platform_free(void *ptr)
{
    free(ptr);
}

int64_t gfx_platform_time_us(void)
{
#if defined(__EMSCRIPTEN__)
    return (int64_t)(emscripten_get_now() * 1000.0);
#else
    static int64_t fake_us;
    fake_us += 16000;
    return fake_us;
#endif
}

bool gfx_platform_psram_dma_capable(void)
{
    return true;
}

gfx_err_t gfx_fs_open_dir_port(const char *root_dir, gfx_asset_source_t **out_fs)
{
    (void)root_dir;
    if (out_fs != NULL) {
        *out_fs = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}

gfx_err_t gfx_fs_open_partition_port(const char *partition_label,
                                     gfx_fs_access_mode_t access_mode,
                                     gfx_asset_source_t **out_fs)
{
    (void)partition_label;
    (void)access_mode;
    if (out_fs != NULL) {
        *out_fs = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}

gfx_err_t gfx_fs_open_pack_file_port(const char *file_path, gfx_asset_source_t **out_fs)
{
    (void)file_path;
    if (out_fs != NULL) {
        *out_fs = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}
