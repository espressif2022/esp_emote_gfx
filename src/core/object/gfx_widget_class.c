/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_OBJ
#include "common/gfx_log_priv.h"

#include "core/display/gfx_refr_priv.h"
#include "core/object/gfx_obj_priv.h"

static const char *TAG = "widget_class";

static const gfx_widget_class_t *s_widget_classes[UINT8_MAX + 1U];

esp_err_t gfx_widget_class_register(const gfx_widget_class_t *klass)
{
    const gfx_widget_class_t *existing;

    ESP_RETURN_ON_FALSE(klass != NULL, ESP_ERR_INVALID_ARG, TAG, "class is NULL");

    existing = s_widget_classes[klass->type];
    if (existing == NULL) {
        s_widget_classes[klass->type] = klass;
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(existing == klass, ESP_ERR_INVALID_STATE, TAG,
                        "class type %u already registered by %s",
                        (unsigned int)klass->type,
                        existing->name ? existing->name : "unknown");
    return ESP_OK;
}

const gfx_widget_class_t *gfx_widget_class_get(uint8_t type)
{
    return s_widget_classes[type];
}

esp_err_t gfx_obj_init_class_instance(gfx_obj_t *obj, gfx_disp_t *disp, const gfx_widget_class_t *klass, void *src)
{
    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_INVALID_ARG, TAG, "object is NULL");
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_INVALID_ARG, TAG, "display is NULL");
    ESP_RETURN_ON_FALSE(klass != NULL, ESP_ERR_INVALID_ARG, TAG, "class is NULL");

    ESP_RETURN_ON_ERROR(gfx_widget_class_register(klass), TAG, "register class failed");

    memset(obj, 0, sizeof(*obj));
    obj->type = klass->type;
    obj->klass = gfx_widget_class_get(klass->type);
    obj->disp = disp;
    obj->src = src;
    obj->state.is_visible = true;
    obj->vfunc.draw = obj->klass->draw;
    obj->vfunc.delete = obj->klass->delete;
    obj->vfunc.update = obj->klass->update;
    obj->vfunc.touch_event = obj->klass->touch_event;

    gfx_obj_invalidate(obj);
    return ESP_OK;
}
