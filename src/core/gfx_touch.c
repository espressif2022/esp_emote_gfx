/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "core/gfx_core_priv.h"
#include "core/gfx_touch_priv.h"
#include "core/gfx_obj_priv.h"

static const char *TAG = "gfx_touch";
static const uint32_t DEFAULT_POLL_MS = 15;
static const uint32_t DEFAULT_IRQ_POLL_MS = 5;

typedef struct {
    gfx_touch_t *touch;
    void *original_user_data;
    volatile bool unregistering;
} gfx_touch_isr_ctx_t;

static void gfx_touch_poll_cb(void *user_data);

/** Return topmost visible object on disp that contains (x, y), or NULL (same order as render = last in list = front) */
static gfx_obj_t *gfx_touch_hit_test(gfx_disp_t *disp, uint16_t x, uint16_t y)
{
    gfx_obj_t *hit = NULL;
    for (gfx_obj_child_t *n = disp->child_list; n != NULL; n = n->next) {
        gfx_obj_t *obj = (gfx_obj_t *)n->src;
        if (!obj->state.is_visible) {
            continue;
        }
        int32_t ox = obj->geometry.x;
        int32_t oy = obj->geometry.y;
        uint32_t w = obj->geometry.width;
        uint32_t h = obj->geometry.height;
        if ((int32_t)x >= ox && (int32_t)x < ox + (int32_t)w && (int32_t)y >= oy && (int32_t)y < oy + (int32_t)h) {
            hit = obj;
        }
    }
    return hit;
}

static uint32_t gfx_touch_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void gfx_touch_dispatch(gfx_touch_t *touch, gfx_touch_event_type_t type, const esp_lcd_touch_point_data_t *pt)
{
    void *hit_obj = NULL;

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

    if (touch->disp) {
        if (type == GFX_TOUCH_EVENT_PRESS) {
            hit_obj = gfx_touch_hit_test(touch->disp, evt.x, evt.y);
            if (hit_obj != NULL) {
                touch->pressed_obj = (gfx_obj_t *)hit_obj;
                touch->pressed_id = evt.track_id;
            } else {
                touch->pressed_obj = NULL;
            }
        } else {
            /* MOVE / RELEASE: keep delivering to the object that got PRESS (drag support) */
            if (touch->pressed_obj != NULL && evt.track_id == touch->pressed_id) {
                hit_obj = (gfx_obj_t *)touch->pressed_obj;
            } else {
                hit_obj = NULL;
            }
            if (type == GFX_TOUCH_EVENT_RELEASE) {
                touch->pressed_obj = NULL;
            }
        }
        if (hit_obj != NULL) {
            gfx_obj_t *obj = (gfx_obj_t *)hit_obj;
            if (obj->vfunc.touch_event) {
                obj->vfunc.touch_event(obj, &evt);
            }
            if (obj->user_touch_cb) {
                obj->user_touch_cb(obj, &evt, obj->user_touch_data);
            }
        }
    }

    if (touch->event_cb) {
        touch->event_cb((gfx_touch_t *)touch, &evt, touch->user_data);
    }
}

static void IRAM_ATTR gfx_touch_isr(esp_lcd_touch_handle_t tp)
{

    if (!tp || !tp->config.user_data) {
        return;
    }

    gfx_touch_isr_ctx_t *isr_ctx = (gfx_touch_isr_ctx_t *)tp->config.user_data;
    if (!isr_ctx || isr_ctx->unregistering || !isr_ctx->touch) {
        return;
    }

    isr_ctx->touch->irq_pending = true;
}

static esp_err_t gfx_touch_enable_interrupt(gfx_touch_t *touch)
{
    if (!touch || !touch->handle || touch->int_gpio_num == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_touch_isr_ctx_t *isr_ctx = calloc(1, sizeof(gfx_touch_isr_ctx_t));
    if (!isr_ctx) {
        return ESP_ERR_NO_MEM;
    }

    isr_ctx->touch = touch;
    isr_ctx->original_user_data = touch->handle->config.user_data;
    touch->isr_ctx = isr_ctx;

    esp_err_t ret = esp_lcd_touch_register_interrupt_callback_with_data(touch->handle, gfx_touch_isr, isr_ctx);
    if (ret != ESP_OK) {
        touch->isr_ctx = NULL;
        free(isr_ctx);
        return ret;
    }

    touch->irq_enabled = true;
    touch->irq_pending = false;
    ESP_LOGI(TAG, "Touch interrupt enabled on GPIO %d", touch->int_gpio_num);
    return ESP_OK;
}

static void gfx_touch_disable_interrupt(gfx_touch_t *touch)
{
    if (!touch) {
        return;
    }

    if (touch->irq_enabled && touch->int_gpio_num != GPIO_NUM_NC && GPIO_IS_VALID_GPIO(touch->int_gpio_num)) {
        esp_err_t gpio_ret = gpio_intr_disable(touch->int_gpio_num);
        if (gpio_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to disable GPIO interrupt on pin %d (%d)", touch->int_gpio_num, gpio_ret);
        }
    }

    if (touch->isr_ctx) {
        gfx_touch_isr_ctx_t *isr_ctx = (gfx_touch_isr_ctx_t *)touch->isr_ctx;
        isr_ctx->unregistering = true;
        esp_lcd_touch_register_interrupt_callback(touch->handle, NULL);
        if (touch->handle && touch->handle->config.user_data != isr_ctx->original_user_data) {
            touch->handle->config.user_data = isr_ctx->original_user_data;
        }
        free(isr_ctx);
        touch->isr_ctx = NULL;
    }

    touch->irq_enabled = false;
    touch->irq_pending = false;
}

esp_err_t gfx_touch_start(gfx_touch_t *touch, const gfx_touch_config_t *cfg)
{
    if (!touch || !touch->ctx || !cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!cfg->handle) {
        return ESP_OK;
    }

    touch->handle = cfg->handle;
    touch->disp = cfg->disp;
    touch->event_cb = cfg->event_cb;
    touch->user_data = cfg->user_data;
    touch->int_gpio_num = GPIO_NUM_NC;
    touch->irq_enabled = false;
    touch->irq_pending = false;
    touch->isr_ctx = NULL;

    bool irq_requested = false;
    gpio_num_t selected_gpio = GPIO_NUM_NC;

    if (touch->handle->config.int_gpio_num != GPIO_NUM_NC &&
            GPIO_IS_VALID_GPIO(touch->handle->config.int_gpio_num)) {
        selected_gpio = touch->handle->config.int_gpio_num;
    }

    if (selected_gpio != GPIO_NUM_NC) {
        touch->int_gpio_num = selected_gpio;
        irq_requested = true;
    } else {
        touch->int_gpio_num = GPIO_NUM_NC;
    }

    uint32_t default_poll = irq_requested ? DEFAULT_IRQ_POLL_MS : DEFAULT_POLL_MS;
    touch->poll_ms = cfg->poll_ms ? cfg->poll_ms : default_poll;
    touch->pressed = false;
    touch->last_x = 0;
    touch->last_y = 0;
    touch->last_strength = 0;
    touch->last_id = 0;
    touch->pressed_obj = NULL;

    if (irq_requested) {
        esp_err_t irq_ret = gfx_touch_enable_interrupt(touch);
        if (irq_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to enable touch interrupt on GPIO %d (%d), using polling mode", touch->int_gpio_num, irq_ret);
            touch->int_gpio_num = GPIO_NUM_NC;
            touch->irq_enabled = false;
            touch->irq_pending = false;
            if (!cfg->poll_ms) {
                touch->poll_ms = DEFAULT_POLL_MS;
            }
        }
    }

    touch->poll_timer = gfx_timer_create(touch->ctx, gfx_touch_poll_cb, touch->poll_ms, touch);
    if (!touch->poll_timer) {
        ESP_LOGE(TAG, "Failed to create touch timer");
        if (touch->irq_enabled || touch->isr_ctx) {
            gfx_touch_disable_interrupt(touch);
        }
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Touch polling started (%"PRIu32" ms)", touch->poll_ms);
    return ESP_OK;
}

static void gfx_touch_poll_cb(void *user_data)
{
    gfx_touch_t *touch = (gfx_touch_t *)user_data;
    if (!touch || !touch->handle) {
        return;
    }

    if (touch->irq_enabled) {
        if (!touch->irq_pending) {
            return;
        }
        touch->irq_pending = false;
    }

    esp_err_t ret = esp_lcd_touch_read_data(touch->handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch read failed: %d", ret);
        return;
    }

    esp_lcd_touch_point_data_t points[1] = {0};
    uint8_t count = 0;

    ret = esp_lcd_touch_get_data(touch->handle, points, &count, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch get data failed: %d", ret);
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

void gfx_touch_del(gfx_touch_t *touch)
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

    if (touch->irq_enabled || touch->isr_ctx) {
        gfx_touch_disable_interrupt(touch);
    }

    if (touch->poll_timer && touch->ctx) {
        gfx_timer_delete(touch->ctx, touch->poll_timer);
        touch->poll_timer = NULL;
    }

    touch->ctx = NULL;
    touch->next = NULL;
    touch->handle = NULL;
    touch->event_cb = NULL;
    touch->user_data = NULL;
    touch->pressed = false;
    touch->pressed_obj = NULL;
    touch->int_gpio_num = GPIO_NUM_NC;
}

gfx_touch_t *gfx_touch_add(gfx_handle_t handle, const gfx_touch_config_t *cfg)
{
    if (!handle || !cfg || !cfg->handle) {
        return NULL;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;

    gfx_touch_t *new_touch = (gfx_touch_t *)calloc(1, sizeof(gfx_touch_t));
    if (!new_touch) {
        return NULL;
    }
    memset(new_touch, 0, sizeof(gfx_touch_t));
    new_touch->ctx = ctx;

    if (gfx_touch_start(new_touch, cfg) != ESP_OK) {
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

esp_err_t gfx_touch_set_disp(gfx_touch_t *touch, gfx_disp_t *disp)
{
    if (!touch || !disp) {
        return ESP_ERR_INVALID_ARG;
    }

    touch->disp = disp;
    return ESP_OK;
}
