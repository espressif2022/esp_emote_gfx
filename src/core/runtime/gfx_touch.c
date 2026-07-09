/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define GFX_LOG_MODULE GFX_LOG_MODULE_TOUCH
#include "common/gfx_log_priv.h"

#include "core/display/gfx_display_priv.h"
#include "core/object/gfx_object_priv.h"
#include "core/runtime/gfx_core_priv.h"
#include "core/runtime/gfx_touch_priv.h"
#include "gfx/scene/arena_scene.h"
#include "platform/gfx_platform.h"
#include "platform/gfx_touch_port.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "touch";
static const uint32_t s_default_poll_ms = 15;
static const uint32_t s_default_irq_poll_ms = 5;

/**********************
 *      TYPEDEFS
 **********************/

/** Touch node (one per device); list chained by next; public API uses opaque gfx_touch_t */
struct gfx_touch {
    struct gfx_touch *next;
    struct gfx_core_context *ctx;
    void *driver_handle;
    gfx_display_t *disp;
    gfx_timer_handle_t poll_timer;
    gfx_touch_event_cb_t event_cb;
    void *user_data;
    uint32_t poll_ms;

    bool pressed;
    uint16_t last_x;
    uint16_t last_y;
    uint16_t last_strength;
    uint8_t last_id;

    /** Object that received PRESS; gets MOVE/RELEASE for same track until RELEASE (for drag) */
    struct gfx_object *pressed_obj;
    uint32_t pressed_obj_seq;
    uint8_t pressed_id;

    int int_gpio_num;
    bool irq_enabled;
    volatile bool irq_pending;
    void *irq_cookie;
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_touch_poll_cb(void *user_data);
static gfx_err_t gfx_touch_start(gfx_touch_t *touch, const gfx_touch_config_t *cfg);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool gfx_touch_obj_is_active(gfx_display_t *disp, gfx_object_t *target, uint32_t create_seq)
{
    if (!disp || !target) {
        return false;
    }

    return gfx_object_child_list_contains(disp->child_list, target, create_seq);
}

static uint32_t gfx_touch_now_ms(void)
{
    return (uint32_t)(gfx_platform_time_us() / 1000);
}

static void gfx_touch_update_capture(gfx_touch_t *touch, gfx_display_t *disp, const gfx_touch_event_t *evt, gfx_object_t **out_hit_obj)
{
    gfx_object_t *hit_obj = NULL;

    if (disp == NULL || evt == NULL) {
        if (out_hit_obj != NULL) {
            *out_hit_obj = NULL;
        }
        return;
    }

    if (evt->type == GFX_TOUCH_EVENT_PRESS) {
        hit_obj = gfx_display_hit_test(disp, evt->x, evt->y);
        if (touch != NULL) {
            touch->pressed_obj = hit_obj;
            touch->pressed_obj_seq = hit_obj != NULL ? hit_obj->trace.create_seq : 0;
            touch->pressed_id = evt->track_id;
        }
    } else if (touch != NULL) {
        if (touch->pressed_obj != NULL && evt->track_id == touch->pressed_id &&
                gfx_touch_obj_is_active(disp, touch->pressed_obj, touch->pressed_obj_seq)) {
            hit_obj = touch->pressed_obj;
        } else {
            touch->pressed_obj = NULL;
            touch->pressed_obj_seq = 0;
        }

        if (evt->type == GFX_TOUCH_EVENT_RELEASE) {
            touch->pressed_obj = NULL;
            touch->pressed_obj_seq = 0;
        }
    } else {
        hit_obj = gfx_display_hit_test(disp, evt->x, evt->y);
    }

    if (out_hit_obj != NULL) {
        *out_hit_obj = hit_obj;
    }
}

static void gfx_touch_dispatch_event(gfx_touch_t *touch, gfx_display_t *disp, const gfx_touch_event_t *evt)
{
    gfx_object_t *hit_obj = NULL;

    if (evt == NULL) {
        return;
    }

    if (disp != NULL) {
        if (disp->arena_scene != NULL && arena_scene_handle_touch(disp, evt) != 0) {
            /* Arena package path handled press/release; skip object hit-test. */
        } else {
            gfx_touch_update_capture(touch, disp, evt, &hit_obj);
            if (hit_obj != NULL) {
                if (hit_obj->vfunc.touch_event) {
                    hit_obj->vfunc.touch_event(hit_obj, evt);
                }
                if (hit_obj->user_touch_cb) {
                    hit_obj->user_touch_cb(hit_obj, evt, hit_obj->user_touch_data);
                }
            }
        }
    }

    if (touch != NULL && touch->event_cb) {
        touch->event_cb((gfx_touch_t *)touch, evt, touch->user_data);
    }
}

static void gfx_touch_dispatch_injected_event(gfx_display_t *disp, const gfx_touch_event_t *evt)
{
    gfx_object_t *hit_obj = NULL;

    if (disp == NULL || evt == NULL) {
        return;
    }

    if (disp->arena_scene != NULL && arena_scene_handle_touch(disp, evt) != 0) {
        return;
    }

    if (evt->type == GFX_TOUCH_EVENT_PRESS) {
        hit_obj = gfx_display_hit_test(disp, evt->x, evt->y);
        disp->injected_touch.pressed_obj = hit_obj;
        disp->injected_touch.pressed_obj_seq = hit_obj != NULL ? hit_obj->trace.create_seq : 0;
        disp->injected_touch.pressed_id = evt->track_id;
    } else {
        if (disp->injected_touch.pressed_obj != NULL &&
                evt->track_id == disp->injected_touch.pressed_id &&
                gfx_touch_obj_is_active(disp, disp->injected_touch.pressed_obj,
                                        disp->injected_touch.pressed_obj_seq)) {
            hit_obj = disp->injected_touch.pressed_obj;
        } else {
            disp->injected_touch.pressed_obj = NULL;
            disp->injected_touch.pressed_obj_seq = 0;
        }

        if (evt->type == GFX_TOUCH_EVENT_RELEASE) {
            disp->injected_touch.pressed_obj = NULL;
            disp->injected_touch.pressed_obj_seq = 0;
        }
    }

    if (hit_obj != NULL) {
        if (hit_obj->vfunc.touch_event) {
            hit_obj->vfunc.touch_event(hit_obj, evt);
        }
        if (hit_obj->user_touch_cb) {
            hit_obj->user_touch_cb(hit_obj, evt, hit_obj->user_touch_data);
        }
    }
}

static void gfx_touch_dispatch(gfx_touch_t *touch, gfx_touch_event_type_t type, const gfx_touch_port_point_t *pt)
{
    gfx_touch_event_t evt = {
        .type = type,
        .x = touch->last_x,
        .y = touch->last_y,
        .strength = touch->last_strength,
        .track_id = touch->last_id,
        .timestamp_ms = gfx_touch_now_ms(),
    };

    if (pt) {
        evt.x = pt->x;
        evt.y = pt->y;
        evt.strength = pt->strength;
        evt.track_id = pt->track_id;
    }

    gfx_touch_dispatch_event(touch, touch->disp, &evt);
}

static void gfx_touch_irq_cb(void *driver_handle, void *user_data)
{
    (void)driver_handle;
    gfx_touch_t *touch = (gfx_touch_t *)user_data;
    if (touch == NULL) {
        return;
    }

    touch->irq_pending = true;
}

static gfx_err_t gfx_touch_enable_interrupt(gfx_touch_t *touch)
{
    if (!touch || !touch->driver_handle || touch->int_gpio_num == GFX_TOUCH_PORT_GPIO_NONE) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_err_t ret = gfx_touch_port_register_interrupt(touch->driver_handle, gfx_touch_irq_cb,
                    touch, &touch->irq_cookie);
    if (ret != GFX_OK) {
        touch->irq_cookie = NULL;
        return ret;
    }

    touch->irq_enabled = true;
    touch->irq_pending = false;
    GFX_LOGI(TAG, "init touch: interrupt enabled on gpio %d", touch->int_gpio_num);
    return GFX_OK;
}

static void gfx_touch_disable_interrupt(gfx_touch_t *touch)
{
    if (!touch) {
        return;
    }

    if (touch->irq_enabled && gfx_touch_port_is_valid_gpio(touch->int_gpio_num)) {
        gfx_err_t gpio_ret = gfx_touch_port_disable_gpio_intr(touch->int_gpio_num);
        if (gpio_ret != GFX_OK) {
            GFX_LOGW(TAG, "delete touch: disable gpio interrupt failed on pin %d (%d)", touch->int_gpio_num, gpio_ret);
        }
    }

    if (touch->irq_cookie != NULL) {
        gfx_touch_port_unregister_interrupt(touch->driver_handle, touch->irq_cookie);
        touch->irq_cookie = NULL;
    }

    touch->irq_enabled = false;
    touch->irq_pending = false;
}

static void gfx_touch_poll_cb(void *user_data)
{
    gfx_touch_t *touch = (gfx_touch_t *)user_data;
    if (!touch || !touch->driver_handle) {
        return;
    }

    if (touch->irq_enabled) {
        if (!touch->irq_pending) {
            return;
        }
        touch->irq_pending = false;
    }

    gfx_err_t ret = gfx_touch_port_read(touch->driver_handle);
    if (ret != GFX_OK) {
        GFX_LOGW(TAG, "poll touch: read failed (%d)", ret);
        return;
    }

    gfx_touch_port_point_t points[1] = {0};
    uint8_t count = 0;

    ret = gfx_touch_port_get_points(touch->driver_handle, points, &count, 1);
    if (ret != GFX_OK) {
        GFX_LOGW(TAG, "poll touch: get data failed (%d)", ret);
        return;
    }

    bool pressed_now = (count > 0);

    if (pressed_now) {
        uint16_t new_x = points[0].x;
        uint16_t new_y = points[0].y;

        if (pressed_now && !touch->pressed) {
            gfx_touch_dispatch(touch, GFX_TOUCH_EVENT_PRESS, &points[0]);
        } else if (touch->pressed && (new_x != touch->last_x || new_y != touch->last_y)) {
            gfx_touch_dispatch(touch, GFX_TOUCH_EVENT_MOVE, &points[0]);
        }

        touch->last_x = new_x;
        touch->last_y = new_y;
        touch->last_strength = points[0].strength;
        touch->last_id = points[0].track_id;
    } else {
        if (touch->pressed) {
            gfx_touch_dispatch(touch, GFX_TOUCH_EVENT_RELEASE, NULL);
        }
    }

    touch->pressed = pressed_now;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

static gfx_err_t gfx_touch_start(gfx_touch_t *touch, const gfx_touch_config_t *cfg)
{
    if (!touch || !touch->ctx || !cfg) {
        return GFX_ERR_INVALID_ARG;
    }

    if (!cfg->driver_handle) {
        return GFX_OK;
    }

    touch->driver_handle = cfg->driver_handle;
    touch->disp = cfg->disp;
    touch->event_cb = cfg->event_cb;
    touch->user_data = cfg->user_data;
    touch->int_gpio_num = GFX_TOUCH_PORT_GPIO_NONE;
    touch->irq_enabled = false;
    touch->irq_pending = false;
    touch->irq_cookie = NULL;

    bool irq_requested = false;
    int selected_gpio = GFX_TOUCH_PORT_GPIO_NONE;

    int int_gpio = GFX_TOUCH_PORT_GPIO_NONE;
    if (gfx_touch_port_get_int_gpio(touch->driver_handle, &int_gpio) == GFX_OK &&
            gfx_touch_port_is_valid_gpio(int_gpio)) {
        selected_gpio = int_gpio;
    }

    if (selected_gpio != GFX_TOUCH_PORT_GPIO_NONE) {
        touch->int_gpio_num = selected_gpio;
        irq_requested = true;
    } else {
        touch->int_gpio_num = GFX_TOUCH_PORT_GPIO_NONE;
    }

    uint32_t default_poll = irq_requested ? s_default_irq_poll_ms : s_default_poll_ms;
    touch->poll_ms = cfg->poll_ms ? cfg->poll_ms : default_poll;
    touch->pressed = false;
    touch->pressed_obj = NULL;
    touch->pressed_obj_seq = 0;
    touch->last_x = 0;
    touch->last_y = 0;
    touch->last_strength = 0;
    touch->last_id = 0;
    touch->pressed_id = 0;

    if (irq_requested) {
        gfx_err_t irq_ret = gfx_touch_enable_interrupt(touch);
        if (irq_ret != GFX_OK) {
            GFX_LOGW(TAG, "init touch: enable gpio interrupt failed on %d (%d), using polling mode", touch->int_gpio_num, irq_ret);
            touch->int_gpio_num = GFX_TOUCH_PORT_GPIO_NONE;
            touch->irq_enabled = false;
            touch->irq_pending = false;
            if (!cfg->poll_ms) {
                touch->poll_ms = s_default_poll_ms;
            }
        }
    }

    touch->poll_timer = gfx_timer_create(touch->ctx, gfx_touch_poll_cb, touch->poll_ms, touch);
    if (!touch->poll_timer) {
        GFX_LOGE(TAG, "init touch: create polling timer failed");
        if (touch->irq_enabled || touch->irq_cookie != NULL) {
            gfx_touch_disable_interrupt(touch);
        }
        return GFX_ERR_NO_MEM;
    }

    GFX_LOGD(TAG, "init touch: polling started (%"PRIu32" ms)", touch->poll_ms);
    return GFX_OK;
}

void gfx_touch_delete_all(gfx_core_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    while (ctx->touch != NULL) {
        gfx_touch_delete(ctx->touch);
    }
}

void gfx_touch_delete(gfx_touch_t *touch)
{
    if (!touch) {
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)touch->ctx;
    if (ctx != NULL) {
        if (ctx->touch == touch) {
            ctx->touch = touch->next;
        } else {
            gfx_touch_t *prev = ctx->touch;
            while (prev != NULL && prev->next != touch) {
                prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = touch->next;
            }
        }
    }

    if (touch->irq_enabled || touch->irq_cookie != NULL) {
        gfx_touch_disable_interrupt(touch);
    }

    if (touch->poll_timer && touch->ctx) {
        gfx_timer_delete(touch->ctx, touch->poll_timer);
        touch->poll_timer = NULL;
    }

    touch->ctx = NULL;
    touch->next = NULL;
    touch->driver_handle = NULL;
    touch->event_cb = NULL;
    touch->user_data = NULL;
    touch->pressed = false;
    touch->pressed_obj = NULL;
    touch->int_gpio_num = GFX_TOUCH_PORT_GPIO_NONE;
    free(touch);
}

gfx_touch_t *gfx_touch_add(gfx_handle_t handle, const gfx_touch_config_t *cfg)
{
    if (!handle || !cfg || !cfg->driver_handle) {
        return NULL;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;

    gfx_touch_t *new_touch = (gfx_touch_t *)calloc(1, sizeof(gfx_touch_t));
    if (!new_touch) {
        return NULL;
    }
    memset(new_touch, 0, sizeof(gfx_touch_t));
    new_touch->ctx = ctx;

    if (gfx_touch_start(new_touch, cfg) != GFX_OK) {
        free(new_touch);
        return NULL;
    }

    if (ctx->touch == NULL) {
        ctx->touch = new_touch;
    } else {
        gfx_touch_t *tail = ctx->touch;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = new_touch;
    }

    return new_touch;
}

gfx_err_t gfx_touch_set_disp(gfx_touch_t *touch, gfx_display_t *disp)
{
    if (!touch || !disp) {
        return GFX_ERR_INVALID_ARG;
    }

    touch->disp = disp;
    return GFX_OK;
}

gfx_err_t gfx_touch_inject(gfx_display_t *disp, const gfx_touch_event_t *event)
{
    if (disp == NULL || event == NULL || disp->ctx == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)disp->ctx;
    if (ctx->sync.render_mutex == NULL) {
        return GFX_ERR_INVALID_STATE;
    }

    if (!gfx_platform_mutex_lock(ctx->sync.render_mutex, GFX_PLATFORM_WAIT_FOREVER)) {
        return GFX_ERR_TIMEOUT;
    }

    gfx_touch_dispatch_injected_event(disp, event);

    if (!gfx_platform_mutex_unlock(ctx->sync.render_mutex)) {
        return GFX_ERR_INVALID_STATE;
    }

    return GFX_OK;
}
