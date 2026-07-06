/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_TOUCH_PORT_GPIO_NONE (-1)

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t strength;
    uint8_t track_id;
} gfx_touch_port_point_t;

typedef void (*gfx_touch_port_irq_cb_t)(void *driver_handle, void *user_data);

gfx_err_t gfx_touch_port_get_int_gpio(void *driver_handle, int *out_gpio);
bool gfx_touch_port_is_valid_gpio(int gpio_num);
gfx_err_t gfx_touch_port_disable_gpio_intr(int gpio_num);
gfx_err_t gfx_touch_port_register_interrupt(void *driver_handle, gfx_touch_port_irq_cb_t cb,
        void *user_data, void **out_cookie);
void gfx_touch_port_unregister_interrupt(void *driver_handle, void *cookie);
gfx_err_t gfx_touch_port_read(void *driver_handle);
gfx_err_t gfx_touch_port_get_points(void *driver_handle, gfx_touch_port_point_t *points,
                                    uint8_t *count, uint8_t max_count);

#ifdef __cplusplus
}
#endif
