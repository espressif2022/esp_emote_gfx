/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_RETURN_ON_FALSE(a, err_code, tag, format, ...) \
    do {                                                   \
        (void)(tag);                                       \
        if (!(a)) {                                        \
            return (err_code);                             \
        }                                                  \
    } while (0)

#define ESP_RETURN_ON_ERROR(x, tag, format, ...) \
    do {                                         \
        (void)(tag);                             \
        esp_err_t __err_rc = (x);                \
        if (__err_rc != ESP_OK) {                \
            return __err_rc;                     \
        }                                        \
    } while (0)

#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) \
    do {                                                              \
        (void)(log_tag);                                              \
        if (!(a)) {                                                   \
            ret = (err_code);                                         \
            goto goto_tag;                                            \
        }                                                             \
    } while (0)

#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...) \
    do {                                                     \
        (void)(log_tag);                                     \
        ret = (x);                                           \
        if (ret != ESP_OK) {                                 \
            goto goto_tag;                                   \
        }                                                    \
    } while (0)

#ifdef __cplusplus
}
#endif
