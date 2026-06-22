/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "platform/gfx_platform.h"

#ifndef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    gfx_platform_event_bits_t bits;
} gfx_linux_event_t;

typedef struct {
    pthread_mutex_t mutex;
} gfx_linux_mutex_t;

typedef struct {
    gfx_platform_task_fn_t fn;
    void *arg;
} gfx_linux_task_start_t;

static void gfx_linux_abs_deadline(uint32_t timeout_ms, struct timespec *out_ts)
{
    clock_gettime(CLOCK_REALTIME, out_ts);
    out_ts->tv_sec += timeout_ms / 1000U;
    out_ts->tv_nsec += (long)(timeout_ms % 1000U) * 1000000L;
    if (out_ts->tv_nsec >= 1000000000L) {
        out_ts->tv_sec++;
        out_ts->tv_nsec -= 1000000000L;
    }
}

static bool gfx_linux_event_condition_met(gfx_platform_event_bits_t current,
        gfx_platform_event_bits_t bits, bool wait_all)
{
    if (wait_all) {
        return (current & bits) == bits;
    }
    return (current & bits) != 0;
}

gfx_platform_event_t gfx_platform_event_create(void)
{
    gfx_linux_event_t *event = calloc(1, sizeof(*event));
    if (event == NULL) {
        return NULL;
    }

    if (pthread_mutex_init(&event->mutex, NULL) != 0) {
        free(event);
        return NULL;
    }
    if (pthread_cond_init(&event->cond, NULL) != 0) {
        pthread_mutex_destroy(&event->mutex);
        free(event);
        return NULL;
    }

    return event;
}

void gfx_platform_event_delete(gfx_platform_event_t event_handle)
{
    gfx_linux_event_t *event = (gfx_linux_event_t *)event_handle;
    if (event == NULL) {
        return;
    }

    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
    free(event);
}

gfx_platform_event_bits_t gfx_platform_event_set(gfx_platform_event_t event_handle, gfx_platform_event_bits_t bits)
{
    gfx_linux_event_t *event = (gfx_linux_event_t *)event_handle;
    if (event == NULL) {
        return 0;
    }

    pthread_mutex_lock(&event->mutex);
    event->bits |= bits;
    gfx_platform_event_bits_t current = event->bits;
    pthread_cond_broadcast(&event->cond);
    pthread_mutex_unlock(&event->mutex);
    return current;
}

gfx_platform_event_bits_t gfx_platform_event_set_from_isr(gfx_platform_event_t event, gfx_platform_event_bits_t bits,
        bool *need_yield)
{
    if (need_yield != NULL) {
        *need_yield = false;
    }
    return gfx_platform_event_set(event, bits);
}

gfx_platform_event_bits_t gfx_platform_event_clear(gfx_platform_event_t event_handle, gfx_platform_event_bits_t bits)
{
    gfx_linux_event_t *event = (gfx_linux_event_t *)event_handle;
    if (event == NULL) {
        return 0;
    }

    pthread_mutex_lock(&event->mutex);
    gfx_platform_event_bits_t previous = event->bits;
    event->bits &= ~bits;
    pthread_mutex_unlock(&event->mutex);
    return previous;
}

gfx_platform_event_bits_t gfx_platform_event_wait(gfx_platform_event_t event_handle, gfx_platform_event_bits_t bits,
        bool clear_on_exit, bool wait_all, uint32_t timeout_ms)
{
    gfx_linux_event_t *event = (gfx_linux_event_t *)event_handle;
    if (event == NULL) {
        return 0;
    }

    pthread_mutex_lock(&event->mutex);

    if (!gfx_linux_event_condition_met(event->bits, bits, wait_all)) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&event->mutex);
            return 0;
        }

        if (timeout_ms == GFX_PLATFORM_WAIT_FOREVER) {
            while (!gfx_linux_event_condition_met(event->bits, bits, wait_all)) {
                pthread_cond_wait(&event->cond, &event->mutex);
            }
        } else {
            struct timespec deadline;
            gfx_linux_abs_deadline(timeout_ms, &deadline);
            while (!gfx_linux_event_condition_met(event->bits, bits, wait_all)) {
                int ret = pthread_cond_timedwait(&event->cond, &event->mutex, &deadline);
                if (ret == ETIMEDOUT) {
                    pthread_mutex_unlock(&event->mutex);
                    return 0;
                }
            }
        }
    }

    gfx_platform_event_bits_t matched = event->bits & bits;
    if (clear_on_exit) {
        event->bits &= ~matched;
    }
    pthread_mutex_unlock(&event->mutex);
    return matched;
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
    gfx_linux_mutex_t *mutex = calloc(1, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }

    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        free(mutex);
        return NULL;
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int ret = pthread_mutex_init(&mutex->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (ret != 0) {
        free(mutex);
        return NULL;
    }

    return mutex;
}

void gfx_platform_mutex_delete(gfx_platform_mutex_t mutex_handle)
{
    gfx_linux_mutex_t *mutex = (gfx_linux_mutex_t *)mutex_handle;
    if (mutex == NULL) {
        return;
    }

    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
}

bool gfx_platform_mutex_lock(gfx_platform_mutex_t mutex_handle, uint32_t timeout_ms)
{
    gfx_linux_mutex_t *mutex = (gfx_linux_mutex_t *)mutex_handle;
    if (mutex == NULL) {
        return false;
    }

    if (timeout_ms == 0) {
        return pthread_mutex_trylock(&mutex->mutex) == 0;
    }

    if (timeout_ms == GFX_PLATFORM_WAIT_FOREVER) {
        return pthread_mutex_lock(&mutex->mutex) == 0;
    }

#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L)
    struct timespec deadline;
    gfx_linux_abs_deadline(timeout_ms, &deadline);
    return pthread_mutex_timedlock(&mutex->mutex, &deadline) == 0;
#else
    uint32_t waited_ms = 0;
    while (waited_ms < timeout_ms) {
        if (pthread_mutex_trylock(&mutex->mutex) == 0) {
            return true;
        }
        gfx_platform_delay_ms(1);
        waited_ms++;
    }
    return false;
#endif
}

bool gfx_platform_mutex_unlock(gfx_platform_mutex_t mutex_handle)
{
    gfx_linux_mutex_t *mutex = (gfx_linux_mutex_t *)mutex_handle;
    if (mutex == NULL) {
        return false;
    }
    return pthread_mutex_unlock(&mutex->mutex) == 0;
}

static void *gfx_linux_task_entry(void *arg)
{
    gfx_linux_task_start_t *start = (gfx_linux_task_start_t *)arg;
    gfx_platform_task_fn_t fn = start->fn;
    void *fn_arg = start->arg;

    free(start);
    fn(fn_arg);
    return NULL;
}

gfx_err_t gfx_platform_task_create(const gfx_platform_task_config_t *cfg, gfx_platform_task_fn_t fn,
                                   void *arg, gfx_platform_task_t *out_task)
{
    if (cfg == NULL || fn == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_linux_task_start_t *start = calloc(1, sizeof(*start));
    if (start == NULL) {
        return GFX_ERR_NO_MEM;
    }
    start->fn = fn;
    start->arg = arg;

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        free(start);
        return GFX_ERR_NO_MEM;
    }
    if (cfg->stack_size > 0) {
        pthread_attr_setstacksize(&attr, cfg->stack_size);
    }

    pthread_t thread;
    int ret = pthread_create(&thread, &attr, gfx_linux_task_entry, start);
    pthread_attr_destroy(&attr);
    if (ret != 0) {
        free(start);
        return GFX_ERR_NO_MEM;
    }

    if (out_task != NULL) {
        pthread_t *task = malloc(sizeof(*task));
        if (task == NULL) {
            pthread_detach(thread);
            return GFX_ERR_NO_MEM;
        }
        *task = thread;
        *out_task = task;
    } else {
        pthread_detach(thread);
    }

    return GFX_OK;
}

void gfx_platform_task_delete_current(void)
{
    pthread_exit(NULL);
}

void gfx_platform_delay_ms(uint32_t ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };

    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
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
    (void)caps;
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

void gfx_platform_free(void *ptr)
{
    free(ptr);
}

int64_t gfx_platform_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + (int64_t)(ts.tv_nsec / 1000L);
}

bool gfx_platform_psram_dma_capable(void)
{
    return true;
}
