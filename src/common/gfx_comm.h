/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Common macro definitions for renderer modules
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/* Generic NULL-check utilities */
#ifndef GFX_IS_NULL
#define GFX_IS_NULL(p) ((p) == NULL)
#endif

#ifndef GFX_NOT_NULL
#define GFX_NOT_NULL(p) ((p) != NULL)
#endif

#ifndef GFX_RETURN_IF_NULL
#define GFX_RETURN_IF_NULL(p, retval) do { if ((p) == NULL) { return (retval); } } while (0)
#endif

#ifndef GFX_RETURN_IF_NULL_VOID
#define GFX_RETURN_IF_NULL_VOID(p) do { if ((p) == NULL) { return; } } while (0)
#endif

/* Generic object type checking macro */
#define CHECK_OBJ_TYPE(obj, expected_type, tag) \
    do { \
        ESP_RETURN_ON_FALSE(obj, ESP_ERR_INVALID_ARG, tag, "Object is NULL"); \
        ESP_RETURN_ON_FALSE((obj)->type == (expected_type), ESP_ERR_INVALID_ARG, tag, \
                           "Object type mismatch (expected=%d, actual=%d)", (expected_type), (obj)->type); \
    } while(0)

#ifdef __cplusplus
}
#endif
