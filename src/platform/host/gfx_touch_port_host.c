/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "platform/gfx_touch_port.h"

gfx_err_t gfx_touch_port_get_int_gpio(void *driver_handle, int *out_gpio)
{
    (void)driver_handle;
    if (out_gpio == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_gpio = GFX_TOUCH_PORT_GPIO_NONE;
    return GFX_OK;
}

bool gfx_touch_port_is_valid_gpio(int gpio_num)
{
    (void)gpio_num;
    return false;
}

gfx_err_t gfx_touch_port_disable_gpio_intr(int gpio_num)
{
    (void)gpio_num;
    return GFX_ERR_NOT_SUPPORTED;
}

gfx_err_t gfx_touch_port_register_interrupt(void *driver_handle, gfx_touch_port_irq_cb_t cb,
        void *user_data, void **out_cookie)
{
    (void)driver_handle;
    (void)cb;
    (void)user_data;
    if (out_cookie != NULL) {
        *out_cookie = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}

void gfx_touch_port_unregister_interrupt(void *driver_handle, void *cookie)
{
    (void)driver_handle;
    (void)cookie;
}

gfx_err_t gfx_touch_port_read(void *driver_handle)
{
    (void)driver_handle;
    return GFX_ERR_NOT_SUPPORTED;
}

gfx_err_t gfx_touch_port_get_points(void *driver_handle, gfx_touch_port_point_t *points,
                                    uint8_t *count, uint8_t max_count)
{
    (void)driver_handle;
    (void)points;
    (void)max_count;
    if (count != NULL) {
        *count = 0;
    }
    return GFX_ERR_NOT_SUPPORTED;
}
