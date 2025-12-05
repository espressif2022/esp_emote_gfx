/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "core/gfx_core_priv.h"

static const char *TAG = "gfx_touch";
static const uint32_t DEFAULT_POLL_MS = 15;
static const uint32_t DEFAULT_IRQ_POLL_MS = 5;

typedef struct {
    gfx_core_context_t *ctx;
    void *original_user_data;
    volatile bool unregistering;
} gfx_touch_isr_ctx_t;

static void gfx_touch_poll_cb(void *user_data);

static uint32_t gfx_touch_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void gfx_touch_dispatch(gfx_core_context_t *ctx, gfx_touch_event_type_t type, const esp_lcd_touch_point_data_t *pt)
{
    gfx_touch_event_t evt = {
        .type = type,
        .x = ctx->touch.last_x,
        .y = ctx->touch.last_y,
        .strength = ctx->touch.last_strength,
        .track_id = ctx->touch.last_id,
        .timestamp_ms = gfx_touch_now_ms(),
    };

    if (pt) {
        evt.x = pt->x;
        evt.y = pt->y;
        evt.strength = pt->strength;
        evt.track_id = pt->track_id;
    }

    /* Push into ring buffer */
    if (ctx->touch.q_count < sizeof(ctx->touch.queue) / sizeof(ctx->touch.queue[0])) {
        ctx->touch.queue[ctx->touch.q_tail] = evt;
        ctx->touch.q_tail = (ctx->touch.q_tail + 1) % (uint8_t)(sizeof(ctx->touch.queue) / sizeof(ctx->touch.queue[0]));
        ctx->touch.q_count++;
    } else {
        ESP_LOGD(TAG, "Touch event queue full, dropping event");
    }

    /* Optional callback */
    if (ctx->touch.event_cb) {
        ctx->touch.event_cb((gfx_handle_t)ctx, &evt, ctx->touch.user_data);
    }
}

static void IRAM_ATTR gfx_touch_isr(esp_lcd_touch_handle_t tp)
{
    if (!tp || !tp->config.user_data) {
        return;
    }

    gfx_touch_isr_ctx_t *isr_ctx = (gfx_touch_isr_ctx_t *)tp->config.user_data;
    if (!isr_ctx || isr_ctx->unregistering || !isr_ctx->ctx) {
        return;
    }

    isr_ctx->ctx->touch.irq_pending = true;
}

static esp_err_t gfx_touch_enable_interrupt(gfx_core_context_t *ctx)
{
    if (!ctx || !ctx->touch.handle || ctx->touch.int_gpio_num == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_touch_isr_ctx_t *isr_ctx = calloc(1, sizeof(gfx_touch_isr_ctx_t));
    if (!isr_ctx) {
        return ESP_ERR_NO_MEM;
    }

    isr_ctx->ctx = ctx;
    isr_ctx->original_user_data = ctx->touch.handle->config.user_data;
    ctx->touch.isr_ctx = isr_ctx;

    esp_err_t ret = esp_lcd_touch_register_interrupt_callback_with_data(ctx->touch.handle, gfx_touch_isr, isr_ctx);
    if (ret != ESP_OK) {
        ctx->touch.isr_ctx = NULL;
        free(isr_ctx);
        return ret;
    }

    ctx->touch.irq_enabled = true;
    ctx->touch.irq_pending = false;
    ESP_LOGI(TAG, "Touch interrupt enabled on GPIO %d", ctx->touch.int_gpio_num);
    return ESP_OK;
}

static void gfx_touch_disable_interrupt(gfx_core_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->touch.irq_enabled && ctx->touch.int_gpio_num != GPIO_NUM_NC && GPIO_IS_VALID_GPIO(ctx->touch.int_gpio_num)) {
        esp_err_t gpio_ret = gpio_intr_disable(ctx->touch.int_gpio_num);
        if (gpio_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to disable GPIO interrupt on pin %d (%d)", ctx->touch.int_gpio_num, gpio_ret);
        }
    }

    if (ctx->touch.isr_ctx) {
        gfx_touch_isr_ctx_t *isr_ctx = (gfx_touch_isr_ctx_t *)ctx->touch.isr_ctx;
        isr_ctx->unregistering = true;
        esp_lcd_touch_register_interrupt_callback(ctx->touch.handle, NULL);
        if (ctx->touch.handle && ctx->touch.handle->config.user_data != isr_ctx->original_user_data) {
            ctx->touch.handle->config.user_data = isr_ctx->original_user_data;
        }
        free(isr_ctx);
        ctx->touch.isr_ctx = NULL;
    }

    ctx->touch.irq_enabled = false;
    ctx->touch.irq_pending = false;
}

static esp_err_t gfx_touch_start(gfx_core_context_t *ctx, const gfx_touch_config_t *cfg)
{
    if (!ctx || !cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!cfg->handle) {
        return ESP_OK;
    }

    ctx->touch.handle = cfg->handle;
    ctx->touch.event_cb = cfg->event_cb;
    ctx->touch.user_data = cfg->user_data;
    ctx->touch.int_gpio_num = GPIO_NUM_NC;
    ctx->touch.irq_enabled = false;
    ctx->touch.irq_pending = false;
    ctx->touch.isr_ctx = NULL;

    bool irq_requested = false;
    gpio_num_t selected_gpio = ctx->touch.handle->config.int_gpio_num;

    if (selected_gpio != GPIO_NUM_NC) {
        ctx->touch.int_gpio_num = selected_gpio;
        irq_requested = true;
    } else {
        ctx->touch.int_gpio_num = GPIO_NUM_NC;
    }

    uint32_t default_poll = irq_requested ? DEFAULT_IRQ_POLL_MS : DEFAULT_POLL_MS;
    ctx->touch.poll_ms = cfg->poll_ms ? cfg->poll_ms : default_poll;
    ctx->touch.pressed = false;
    ctx->touch.last_x = 0;
    ctx->touch.last_y = 0;
    ctx->touch.last_strength = 0;
    ctx->touch.last_id = 0;
    ctx->touch.q_head = 0;
    ctx->touch.q_tail = 0;
    ctx->touch.q_count = 0;

    if (irq_requested) {
        esp_err_t irq_ret = gfx_touch_enable_interrupt(ctx);
        if (irq_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to enable touch interrupt on GPIO %d (%d), using polling mode", ctx->touch.int_gpio_num, irq_ret);
            ctx->touch.int_gpio_num = GPIO_NUM_NC;
            ctx->touch.irq_enabled = false;
            ctx->touch.irq_pending = false;
            if (!cfg->poll_ms) {
                ctx->touch.poll_ms = DEFAULT_POLL_MS;
            }
        }
    }

    ctx->touch.poll_timer = gfx_timer_create(ctx, gfx_touch_poll_cb, ctx->touch.poll_ms, ctx);
    if (!ctx->touch.poll_timer) {
        ESP_LOGE(TAG, "Failed to create touch timer");
        if (ctx->touch.irq_enabled || ctx->touch.isr_ctx) {
            gfx_touch_disable_interrupt(ctx);
        }
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Touch polling started (%"PRIu32" ms)", ctx->touch.poll_ms);
    return ESP_OK;
}

static void gfx_touch_poll_cb(void *user_data)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)user_data;
    if (!ctx || !ctx->touch.handle) {
        return;
    }

    if (ctx->touch.irq_enabled) {
        if (!ctx->touch.irq_pending) {
            return;
        }
        ctx->touch.irq_pending = false;
    }

    esp_err_t ret = esp_lcd_touch_read_data(ctx->touch.handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch read failed: %d", ret);
        return;
    }

    esp_lcd_touch_point_data_t points[1] = {0};
    uint8_t count = 0;

    ret = esp_lcd_touch_get_data(ctx->touch.handle, points, &count, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch get data failed: %d", ret);
        return;
    }

    bool pressed_now = (count > 0);

    if (pressed_now) {
        ctx->touch.last_x = points[0].x;
        ctx->touch.last_y = points[0].y;
        ctx->touch.last_strength = points[0].strength;
        ctx->touch.last_id = points[0].track_id;
    }

    if (pressed_now && !ctx->touch.pressed) {
        gfx_touch_dispatch(ctx, GFX_TOUCH_EVENT_PRESS, pressed_now ? &points[0] : NULL);
    } else if (!pressed_now && ctx->touch.pressed) {
        gfx_touch_dispatch(ctx, GFX_TOUCH_EVENT_RELEASE, NULL);
    }

    ctx->touch.pressed = pressed_now;
}

esp_err_t gfx_touch_init(gfx_core_context_t *ctx, const gfx_core_config_t *cfg)
{
    if (!ctx || !cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    return gfx_touch_start(ctx, &cfg->touch);
}

void gfx_touch_deinit(gfx_core_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->touch.irq_enabled || ctx->touch.isr_ctx) {
        gfx_touch_disable_interrupt(ctx);
    }

    if (ctx->touch.poll_timer) {
        gfx_timer_delete(ctx, ctx->touch.poll_timer);
        ctx->touch.poll_timer = NULL;
    }

    ctx->touch.handle = NULL;
    ctx->touch.event_cb = NULL;
    ctx->touch.user_data = NULL;
    ctx->touch.pressed = false;
    ctx->touch.q_head = 0;
    ctx->touch.q_tail = 0;
    ctx->touch.q_count = 0;
    ctx->touch.int_gpio_num = GPIO_NUM_NC;
}

esp_err_t gfx_touch_configure(gfx_handle_t handle, const gfx_touch_config_t *config)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    esp_err_t ret = ESP_OK;

    if (ctx->sync.lock_mutex && xSemaphoreTakeRecursive(ctx->sync.lock_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    gfx_touch_deinit(ctx);

    if (config) {
        ret = gfx_touch_start(ctx, config);
    }

    if (ctx->sync.lock_mutex) {
        xSemaphoreGiveRecursive(ctx->sync.lock_mutex);
    }

    return ret;
}

bool gfx_touch_pop_event(gfx_handle_t handle, gfx_touch_event_t *out_event)
{
    if (!handle || !out_event) {
        return false;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    bool popped = false;

    if (ctx->sync.lock_mutex && xSemaphoreTakeRecursive(ctx->sync.lock_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    if (ctx->touch.q_count > 0) {
        *out_event = ctx->touch.queue[ctx->touch.q_head];
        ctx->touch.q_head = (ctx->touch.q_head + 1) % (uint8_t)(sizeof(ctx->touch.queue) / sizeof(ctx->touch.queue[0]));
        ctx->touch.q_count--;
        popped = true;
    }

    if (ctx->sync.lock_mutex) {
        xSemaphoreGiveRecursive(ctx->sync.lock_mutex);
    }

    return popped;
}
