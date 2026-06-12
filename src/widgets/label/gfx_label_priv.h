/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Widget-internal label lifecycle hooks and shared private label state. */
#include "widgets/label/gfx_label_types_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gfx_label_delete_impl(gfx_object_t *obj);
esp_err_t gfx_label_update_impl(gfx_object_t *obj);

#ifdef __cplusplus
}
#endif
