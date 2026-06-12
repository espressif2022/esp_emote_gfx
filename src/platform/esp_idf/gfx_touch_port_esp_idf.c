/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_lcd_touch.h"

#include "platform/gfx_touch_port.h"

typedef struct {
    gfx_touch_port_irq_cb_t cb;
    void *user_data;
    void *original_user_data;
    volatile bool unregistering;
} gfx_touch_port_irq_ctx_t;

static void IRAM_ATTR gfx_touch_port_esp_idf_isr(esp_lcd_touch_handle_t tp)
{
    if (tp == NULL || tp->config.user_data == NULL) {
        return;
    }

    gfx_touch_port_irq_ctx_t *ctx = (gfx_touch_port_irq_ctx_t *)tp->config.user_data;
    if (ctx->unregistering || ctx->cb == NULL) {
        return;
    }

    ctx->cb((void *)tp, ctx->user_data);
}

gfx_err_t gfx_touch_port_get_int_gpio(void *driver_handle, int *out_gpio)
{
    if (driver_handle == NULL || out_gpio == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)driver_handle;
    *out_gpio = tp->config.int_gpio_num;
    return GFX_OK;
}

bool gfx_touch_port_is_valid_gpio(int gpio_num)
{
    return gpio_num != GPIO_NUM_NC && GPIO_IS_VALID_GPIO(gpio_num);
}

gfx_err_t gfx_touch_port_disable_gpio_intr(int gpio_num)
{
    if (!gfx_touch_port_is_valid_gpio(gpio_num)) {
        return GFX_ERR_INVALID_ARG;
    }

    return gpio_intr_disable((gpio_num_t)gpio_num);
}

gfx_err_t gfx_touch_port_register_interrupt(void *driver_handle, gfx_touch_port_irq_cb_t cb,
        void *user_data, void **out_cookie)
{
    if (driver_handle == NULL || cb == NULL || out_cookie == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)driver_handle;
    gfx_touch_port_irq_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return GFX_ERR_NO_MEM;
    }

    ctx->cb = cb;
    ctx->user_data = user_data;
    ctx->original_user_data = tp->config.user_data;

    gfx_err_t ret = esp_lcd_touch_register_interrupt_callback_with_data(
                        tp, gfx_touch_port_esp_idf_isr, ctx);
    if (ret != GFX_OK) {
        free(ctx);
        return ret;
    }

    *out_cookie = ctx;
    return GFX_OK;
}

void gfx_touch_port_unregister_interrupt(void *driver_handle, void *cookie)
{
    if (driver_handle == NULL || cookie == NULL) {
        return;
    }

    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)driver_handle;
    gfx_touch_port_irq_ctx_t *ctx = (gfx_touch_port_irq_ctx_t *)cookie;
    ctx->unregistering = true;
    esp_lcd_touch_register_interrupt_callback(tp, NULL);
    if (tp->config.user_data != ctx->original_user_data) {
        tp->config.user_data = ctx->original_user_data;
    }
    free(ctx);
}

gfx_err_t gfx_touch_port_read(void *driver_handle)
{
    if (driver_handle == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return esp_lcd_touch_read_data((esp_lcd_touch_handle_t)driver_handle);
}

gfx_err_t gfx_touch_port_get_points(void *driver_handle, gfx_touch_port_point_t *points,
                                    uint8_t *count, uint8_t max_count)
{
    if (driver_handle == NULL || points == NULL || count == NULL || max_count == 0) {
        return GFX_ERR_INVALID_ARG;
    }

    esp_lcd_touch_point_data_t idf_points[1] = {0};
    uint8_t idf_count = 0;
    if (max_count > 1) {
        max_count = 1;
    }

    gfx_err_t ret = esp_lcd_touch_get_data((esp_lcd_touch_handle_t)driver_handle,
                                           idf_points, &idf_count, max_count);
    if (ret != GFX_OK) {
        return ret;
    }

    *count = idf_count;
    if (idf_count > 0) {
        points[0].x = idf_points[0].x;
        points[0].y = idf_points[0].y;
        points[0].strength = idf_points[0].strength;
        points[0].track_id = idf_points[0].track_id;
    }

    return GFX_OK;
}
