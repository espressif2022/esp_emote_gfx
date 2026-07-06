/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_RETURN_ON_FALSE(cond, err_code, tag, format, ...) \
    do {                                                      \
        if (0) {                                              \
            (void)(tag);                                      \
        }                                                     \
        if (!(cond)) {                                        \
            return (err_code);                                \
        }                                                     \
    } while (0)

#define GFX_RETURN_ON_ERROR(expr, tag, format, ...) \
    do {                                            \
        if (0) {                                    \
            (void)(tag);                            \
        }                                             \
        gfx_err_t __gfx_err_rc = (expr);              \
        if (__gfx_err_rc != GFX_OK) {                 \
            return __gfx_err_rc;                      \
        }                                             \
    } while (0)

#define GFX_GOTO_ON_FALSE(cond, err_code, goto_tag, log_tag, format, ...) \
    do {                                                                 \
        if (0) {                                                         \
            (void)(log_tag);                                             \
        }                                                                \
        if (!(cond)) {                                                   \
            ret = (err_code);                                            \
            goto goto_tag;                                               \
        }                                                                \
    } while (0)

#define GFX_GOTO_ON_ERROR(expr, goto_tag, log_tag, format, ...) \
    do {                                                        \
        if (0) {                                                \
            (void)(log_tag);                                      \
        }                                                       \
        ret = (expr);                                           \
        if (ret != GFX_OK) {                                    \
            goto goto_tag;                                      \
        }                                                       \
    } while (0)

#ifdef __cplusplus
}
#endif
