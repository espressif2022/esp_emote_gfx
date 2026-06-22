/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "core/gfx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline gfx_err_t gfx_err_from_esp(esp_err_t err)
{
    switch (err) {
    case ESP_OK:                return GFX_OK;
    case ESP_FAIL:              return GFX_FAIL;
    case ESP_ERR_NO_MEM:        return GFX_ERR_NO_MEM;
    case ESP_ERR_INVALID_ARG:   return GFX_ERR_INVALID_ARG;
    case ESP_ERR_INVALID_STATE: return GFX_ERR_INVALID_STATE;
    case ESP_ERR_INVALID_SIZE:  return GFX_ERR_INVALID_SIZE;
    case ESP_ERR_NOT_FOUND:     return GFX_ERR_NOT_FOUND;
    case ESP_ERR_NOT_SUPPORTED: return GFX_ERR_NOT_SUPPORTED;
    case ESP_ERR_TIMEOUT:       return GFX_ERR_TIMEOUT;
    case ESP_ERR_INVALID_RESPONSE: return GFX_ERR_INVALID_RESPONSE;
    case ESP_ERR_INVALID_CRC:   return GFX_ERR_INVALID_CRC;
    default:                    return GFX_FAIL;
    }
}

static inline esp_err_t gfx_err_to_esp(gfx_err_t err)
{
    switch (err) {
    case GFX_OK:                return ESP_OK;
    case GFX_FAIL:              return ESP_FAIL;
    case GFX_ERR_NO_MEM:        return ESP_ERR_NO_MEM;
    case GFX_ERR_INVALID_ARG:   return ESP_ERR_INVALID_ARG;
    case GFX_ERR_INVALID_STATE: return ESP_ERR_INVALID_STATE;
    case GFX_ERR_INVALID_SIZE:  return ESP_ERR_INVALID_SIZE;
    case GFX_ERR_NOT_FOUND:     return ESP_ERR_NOT_FOUND;
    case GFX_ERR_NOT_SUPPORTED: return ESP_ERR_NOT_SUPPORTED;
    case GFX_ERR_TIMEOUT:       return ESP_ERR_TIMEOUT;
    case GFX_ERR_INVALID_RESPONSE: return ESP_ERR_INVALID_RESPONSE;
    case GFX_ERR_INVALID_CRC:   return ESP_ERR_INVALID_CRC;
    default:                    return ESP_FAIL;
    }
}

#ifdef __cplusplus
}
#endif
