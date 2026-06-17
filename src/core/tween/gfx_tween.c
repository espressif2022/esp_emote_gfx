/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_log_priv.h"
#include "gfx/tween.h"
#include "platform/gfx_platform.h"
#include "core/tween/gfx_tween_priv.h"

#define GFX_TWEEN_Q       1024U
#define GFX_TWEEN_MAX_DT  60000U

struct gfx_tween {
    gfx_object_t *obj;
    struct gfx_tween *next;
    bool active;
    struct {
        int32_t from;
        int32_t to;
        int32_t last_value;
        uint32_t start_ms;
        uint32_t duration_ms;
        gfx_tween_ease_t ease;
        gfx_tween_i32_cb_t value_cb;
        gfx_tween_done_cb_t done_cb;
        void *user_data;
    } i32;
};

static const char *const TAG = "tween";
static gfx_tween_t *s_tween_list;

static uint32_t gfx_tween_now_ms(void)
{
    return (uint32_t)(gfx_platform_time_us() / 1000);
}

static uint32_t gfx_tween_ease_apply(gfx_tween_ease_t ease, uint32_t t)
{
    uint32_t inv;

    if (t >= GFX_TWEEN_Q) {
        return GFX_TWEEN_Q;
    }

    switch (ease) {
    case GFX_TWEEN_EASE_LINEAR:
        return t;
    case GFX_TWEEN_EASE_OUT_QUAD:
        inv = GFX_TWEEN_Q - t;
        return GFX_TWEEN_Q - (inv * inv) / GFX_TWEEN_Q;
    case GFX_TWEEN_EASE_OUT_CUBIC:
        inv = GFX_TWEEN_Q - t;
        return GFX_TWEEN_Q - (uint32_t)(((uint64_t)inv * inv * inv) /
                                        ((uint64_t)GFX_TWEEN_Q * GFX_TWEEN_Q));
    case GFX_TWEEN_EASE_IN_OUT_CUBIC:
        if (t < GFX_TWEEN_Q / 2U) {
            return (uint32_t)(((uint64_t)4U * t * t * t) /
                              ((uint64_t)GFX_TWEEN_Q * GFX_TWEEN_Q));
        }
        inv = GFX_TWEEN_Q - t;
        return GFX_TWEEN_Q - (uint32_t)(((uint64_t)4U * inv * inv * inv) /
                                        ((uint64_t)GFX_TWEEN_Q * GFX_TWEEN_Q));
    default:
        return t;
    }
}

static int32_t gfx_tween_lerp_i32(int32_t from, int32_t to, uint32_t progress)
{
    return from + (int32_t)(((int64_t)(to - from) * (int64_t)progress) / (int64_t)GFX_TWEEN_Q);
}

static void gfx_tween_list_add(gfx_tween_t *tween)
{
    if (tween == NULL) {
        return;
    }

    tween->next = s_tween_list;
    s_tween_list = tween;
}

static void gfx_tween_list_remove(gfx_tween_t *tween)
{
    gfx_tween_t *prev = NULL;
    gfx_tween_t *cur = s_tween_list;

    while (cur != NULL) {
        if (cur == tween) {
            if (prev == NULL) {
                s_tween_list = cur->next;
            } else {
                prev->next = cur->next;
            }
            cur->next = NULL;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

gfx_tween_t *gfx_tween_create(gfx_object_t *obj)
{
    gfx_tween_t *tween;

    if (obj == NULL) {
        return NULL;
    }

    tween = calloc(1, sizeof(*tween));
    if (tween == NULL) {
        return NULL;
    }

    tween->obj = obj;
    gfx_tween_list_add(tween);
    return tween;
}

void gfx_tween_delete(gfx_tween_t *tween)
{
    if (tween == NULL) {
        return;
    }

    gfx_tween_list_remove(tween);
    free(tween);
}

gfx_err_t gfx_tween_start_i32(gfx_tween_t *tween,
                              int32_t from,
                              int32_t to,
                              uint32_t duration_ms,
                              gfx_tween_ease_t ease,
                              gfx_tween_i32_cb_t value_cb,
                              gfx_tween_done_cb_t done_cb,
                              void *user_data)
{
    ESP_RETURN_ON_FALSE(tween != NULL, ESP_ERR_INVALID_ARG, TAG, "tween is NULL");
    ESP_RETURN_ON_FALSE(value_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "value callback is NULL");

    tween->active = false;
    tween->i32.from = from;
    tween->i32.to = to;
    tween->i32.last_value = from;
    tween->i32.start_ms = gfx_tween_now_ms();
    tween->i32.duration_ms = duration_ms;
    tween->i32.ease = ease;
    tween->i32.value_cb = value_cb;
    tween->i32.done_cb = done_cb;
    tween->i32.user_data = user_data;

    value_cb(tween, tween->obj, from, user_data);
    if (duration_ms == 0U || from == to) {
        value_cb(tween, tween->obj, to, user_data);
        if (done_cb != NULL) {
            done_cb(tween, tween->obj, user_data);
        }
        return ESP_OK;
    }

    tween->active = true;
    return ESP_OK;
}

void gfx_tween_stop(gfx_tween_t *tween, bool complete)
{
    if (tween == NULL || !tween->active) {
        return;
    }

    tween->active = false;
    if (complete && tween->i32.value_cb != NULL) {
        tween->i32.last_value = tween->i32.to;
        tween->i32.value_cb(tween, tween->obj, tween->i32.to, tween->i32.user_data);
        if (tween->i32.done_cb != NULL) {
            tween->i32.done_cb(tween, tween->obj, tween->i32.user_data);
        }
    }
}

bool gfx_tween_is_active(const gfx_tween_t *tween)
{
    return tween != NULL && tween->active;
}

bool gfx_tween_core_tick(void)
{
    bool did_update = false;
    uint32_t now_ms = gfx_tween_now_ms();

    for (gfx_tween_t *tween = s_tween_list; tween != NULL; tween = tween->next) {
        uint32_t elapsed;
        uint32_t t;
        uint32_t eased;
        int32_t value;

        if (!tween->active || tween->i32.value_cb == NULL) {
            continue;
        }

        elapsed = now_ms - tween->i32.start_ms;
        if (elapsed > GFX_TWEEN_MAX_DT) {
            elapsed = tween->i32.duration_ms;
        }

        if (elapsed >= tween->i32.duration_ms) {
            tween->active = false;
            value = tween->i32.to;
        } else {
            t = (elapsed * GFX_TWEEN_Q) / tween->i32.duration_ms;
            eased = gfx_tween_ease_apply(tween->i32.ease, t);
            value = gfx_tween_lerp_i32(tween->i32.from, tween->i32.to, eased);
        }

        if (value != tween->i32.last_value || !tween->active) {
            tween->i32.last_value = value;
            tween->i32.value_cb(tween, tween->obj, value, tween->i32.user_data);
            did_update = true;
        }

        if (!tween->active && tween->i32.done_cb != NULL) {
            tween->i32.done_cb(tween, tween->obj, tween->i32.user_data);
        }
    }

    return did_update;
}

void gfx_tween_core_deinit(void)
{
    while (s_tween_list != NULL) {
        gfx_tween_t *next = s_tween_list->next;
        free(s_tween_list);
        s_tween_list = next;
    }
}
