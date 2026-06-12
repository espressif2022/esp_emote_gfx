/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int gpio_num_t;

#define GPIO_NUM_NC (-1)
#define GPIO_IS_VALID_GPIO(gpio_num) ((gpio_num) >= 0)

static inline int gpio_intr_disable(gpio_num_t gpio_num)
{
    (void)gpio_num;
    return 0;
}

#ifdef __cplusplus
}
#endif
