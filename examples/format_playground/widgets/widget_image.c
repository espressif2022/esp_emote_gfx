/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include <stdio.h>

#include "common/gfx_check.h"

const char *const gfx_format_demo_flow_image_names[DEMO_CARD_COUNT] = {
    DEMO_FLOW_IMAGE_C5_DEVKITC,
    DEMO_FLOW_IMAGE_C6_DEVKITC,
    DEMO_FLOW_IMAGE_S3_DEVKITC,
    DEMO_FLOW_IMAGE_P4_EYE_FRONT,
    DEMO_FLOW_IMAGE_P4_EYE_BACK,
};

typedef struct {
    const char *name;
    const char *path;  /**< Store name or absolute filesystem path. */
    const char *mode;
} demo_image_clip_t;

/*
 * Image clips for the preview. Plain names resolve through the default mmap
 * asset fs so image/pageflow/coverflow use the same fast asset path.
 */
static const demo_image_clip_t s_image_clips[] = {
    { .name = "ESP32-C5 DevKitC",   .path = DEMO_FLOW_IMAGE_C5_DEVKITC, .mode = "mmap JPEG" },
    { .name = "ESP32-C6 DevKitC",   .path = DEMO_FLOW_IMAGE_C6_DEVKITC, .mode = "mmap JPEG" },
    { .name = "ESP32-S3 DevKitC",   .path = DEMO_FLOW_IMAGE_S3_DEVKITC, .mode = "mmap JPEG" },
    { .name = "ESP32-P4 Eye Front", .path = DEMO_FLOW_IMAGE_P4_EYE_FRONT, .mode = "mmap JPEG" },
    { .name = "ESP32-P4 Eye Back",  .path = DEMO_FLOW_IMAGE_P4_EYE_BACK, .mode = "mmap JPEG" },
};

#define DEMO_IMAGE_CLIP_COUNT (sizeof(s_image_clips) / sizeof(s_image_clips[0]))

static size_t s_image_index;
static char s_image_note[128];

static void demo_update_image_note(size_t index)
{
    const demo_image_clip_t *clip = &s_image_clips[index];

    (void)snprintf(s_image_note, sizeof(s_image_note), "%s | %s",
                   clip->path, clip->mode);
}

size_t gfx_format_demo_image_clip_count(void)
{
    return DEMO_IMAGE_CLIP_COUNT;
}

size_t gfx_format_demo_image_clip_index(void)
{
    return s_image_index;
}

const char *gfx_format_demo_image_clip_note(void)
{
    if (s_image_note[0] == '\0') {
        demo_update_image_note(s_image_index);
    }
    return s_image_note;
}

static gfx_err_t demo_load_image_clip(format_playground_scene_t *scene, size_t index)
{
    const demo_image_clip_t *clip;
    gfx_image_src_t src = {0};

    GFX_RETURN_ON_FALSE(scene != NULL && scene->image_demo != NULL, GFX_ERR_INVALID_ARG,
                        "playground", "image object is not ready");
    GFX_RETURN_ON_FALSE(index < DEMO_IMAGE_CLIP_COUNT, GFX_ERR_INVALID_ARG,
                        "playground", "image index is invalid");

    clip = &s_image_clips[index];
    src.type = GFX_IMAGE_SRC_TYPE_FILE;
    src.data = clip->path;

    GFX_RETURN_ON_ERROR(gfx_image_set_source_desc(scene->image_demo, &src),
                        "playground", "set image src failed");

    s_image_index = index;
    demo_update_image_note(index);
    if (scene->preview_button != NULL && scene->widget_idx == DEMO_WIDGET_IMAGE) {
        (void)gfx_button_set_text_fmt(scene->preview_button, "Next Image  %u/%u",
                                      (unsigned)(index + 1U), (unsigned)DEMO_IMAGE_CLIP_COUNT);
    }
    if (scene->preview_note != NULL && scene->widget_idx == DEMO_WIDGET_IMAGE) {
        (void)gfx_label_set_text(scene->preview_note, gfx_format_demo_image_clip_note());
    }
    return GFX_OK;
}

gfx_err_t gfx_format_demo_next_image_clip(format_playground_scene_t *scene)
{
    return demo_load_image_clip(scene, (s_image_index + 1U) % DEMO_IMAGE_CLIP_COUNT);
}

gfx_err_t gfx_format_demo_build_image_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj;

    obj = gfx_image_create(scene->disp);
    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create image demo failed");
    scene->image_demo = obj;
    (void)gfx_object_set_pos(obj, 378, 164);

    GFX_RETURN_ON_ERROR(demo_load_image_clip(scene, 0), "playground", "load image clip failed");
    (void)gfx_object_set_visible(obj, false);
    return GFX_OK;
}
