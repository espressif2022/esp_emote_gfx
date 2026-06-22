/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_OBJ
#include "common/gfx_log_priv.h"

#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/object/gfx_object_priv.h"

static const char *const TAG = "widget_class";

static const gfx_widget_class_t *s_widget_classes[UINT8_MAX + 1U];
static uint32_t s_obj_create_seq = 0U;

gfx_err_t gfx_widget_class_register(const gfx_widget_class_t *klass)
{
    const gfx_widget_class_t *existing;

    GFX_RETURN_ON_FALSE(klass != NULL, GFX_ERR_INVALID_ARG, TAG, "class is NULL");

    existing = s_widget_classes[klass->type];
    if (existing == NULL) {
        s_widget_classes[klass->type] = klass;
        return GFX_OK;
    }

    GFX_RETURN_ON_FALSE(existing == klass, GFX_ERR_INVALID_STATE, TAG,
                        "class type %u already registered by %s",
                        (unsigned int)klass->type,
                        existing->name ? existing->name : "unknown");
    return GFX_OK;
}

const gfx_widget_class_t *gfx_widget_class_get(uint8_t type)
{
    return s_widget_classes[type];
}

gfx_err_t gfx_object_init_class_instance(gfx_object_t *obj, gfx_display_t *disp, const gfx_widget_class_t *klass, void *src)
{
    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, TAG, "object is NULL");
    GFX_RETURN_ON_FALSE(disp != NULL, GFX_ERR_INVALID_ARG, TAG, "display is NULL");
    GFX_RETURN_ON_FALSE(klass != NULL, GFX_ERR_INVALID_ARG, TAG, "class is NULL");

    GFX_RETURN_ON_ERROR(gfx_widget_class_register(klass), TAG, "register class failed");

    memset(obj, 0, sizeof(*obj));
    obj->type = klass->type;
    obj->klass = gfx_widget_class_get(klass->type);
    obj->disp = disp;
    obj->src = src;
    obj->state.is_visible = true;
    obj->state.resource_dirty = true;
    obj->vfunc.draw = obj->klass->draw;
    obj->vfunc.delete = obj->klass->delete;
    obj->vfunc.load = obj->klass->load;
    obj->vfunc.release = obj->klass->release;
    obj->vfunc.update = obj->klass->update;
    obj->vfunc.touch_event = obj->klass->touch_event;
    obj->trace.create_seq = ++s_obj_create_seq;
    obj->trace.class_name = (obj->klass->name != NULL) ? obj->klass->name : "unknown";
    obj->trace.create_tag = obj->trace.class_name;

    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_object_create_class_instance(gfx_display_t *disp, const gfx_widget_class_t *klass,
        void *src, uint16_t width, uint16_t height,
        const char *create_tag, gfx_object_t **out_obj)
{
    gfx_object_t *obj;
    gfx_err_t ret;

    GFX_RETURN_ON_FALSE(disp != NULL, GFX_ERR_INVALID_ARG, TAG, "display is NULL");
    GFX_RETURN_ON_FALSE(klass != NULL, GFX_ERR_INVALID_ARG, TAG, "class is NULL");
    GFX_RETURN_ON_FALSE(out_obj != NULL, GFX_ERR_INVALID_ARG, TAG, "output object is NULL");

    *out_obj = NULL;
    obj = calloc(1, sizeof(*obj));
    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_NO_MEM, TAG, "no memory for object");

    ret = gfx_object_init_class_instance(obj, disp, klass, src);
    if (ret != GFX_OK) {
        free(obj);
        return ret;
    }

    obj->geometry.width = width;
    obj->geometry.height = height;
    obj->local_geometry.width = width;
    obj->local_geometry.height = height;
    gfx_object_invalidate_abs_area_cache_tree(obj);
    if (create_tag != NULL) {
        obj->trace.create_tag = create_tag;
    }

    ret = gfx_display_add_child(disp, obj);
    if (ret != GFX_OK) {
        free(obj);
        return ret;
    }

    GFX_LOGD(TAG, "created obj#%" PRIu32 " class=%s tag=%s",
             obj->trace.create_seq,
             obj->trace.class_name ? obj->trace.class_name : "unknown",
             obj->trace.create_tag ? obj->trace.create_tag : "unknown");
    *out_obj = obj;
    return GFX_OK;
}
