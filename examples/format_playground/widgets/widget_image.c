/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include <stdio.h>

#include "common/gfx_check.h"
#ifndef GFX_HOST_BUILD
#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_log_priv.h"
#include "esp_spiffs.h"
#endif

#define DEMO_SPIFFS_IMAGE_PARTITION "storage"
#define DEMO_SPIFFS_IMAGE_MOUNT     "/spiffs"
#ifdef GFX_HOST_BUILD
#define DEMO_FILE_IMAGE_MOUNT       "examples/esp/format_rgb565/spiffs_anim"
#else
#define DEMO_FILE_IMAGE_MOUNT       DEMO_SPIFFS_IMAGE_MOUNT
#endif

static const char *const TAG_IMAGE = "image";

const char *const gfx_format_demo_flow_image_names[DEMO_CARD_COUNT] = {
    "flow_misty_ridge.jpg",
    "flow_format_probe.jpg",
    "flow_warm_harbor.jpg",
    "flow_quiet_trail.jpg",
    "flow_night_lake.jpg",
};

typedef struct {
    const char *name;
    const char *path;  /**< Store name or absolute filesystem path. */
    const char *mode;
} demo_image_clip_t;

/*
 * Image clips for the preview. Plain names resolve through the default mmap
 * asset fs; "/spiffs/..." paths fall back to normal filesystem reads.
 * Switching index just updates the file source path.
 */
static const demo_image_clip_t s_image_clips[] = {
    { .name = "Format Probe", .path = "flow_format_probe.jpg", .mode = "mmap JPEG" },
    { .name = "Warm Harbor",  .path = "flow_warm_harbor.jpg", .mode = "mmap JPEG" },
    { .name = "Night Lake",   .path = "flow_night_lake.jpg", .mode = "mmap JPEG" },
    { .name = "File Trail",   .path = DEMO_FILE_IMAGE_MOUNT "/flow_quiet_trail.jpg", .mode = "file JPEG" },
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

static void demo_mount_spiffs_assets(void)
{
#ifndef GFX_HOST_BUILD
    /* The anim widget mounts the same partition too; skip if already mounted. */
    if (esp_spiffs_mounted(DEMO_SPIFFS_IMAGE_PARTITION)) {
        return;
    }
    const esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = DEMO_SPIFFS_IMAGE_MOUNT,
        .partition_label = DEMO_SPIFFS_IMAGE_PARTITION,
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    gfx_err_t err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != GFX_OK) {
        GFX_LOGW(TAG_IMAGE, "spiffs mount failed: %s; SPIFFS clip will be skipped",
                 "(err)");
    }
#endif
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

    demo_mount_spiffs_assets();

    obj = gfx_image_create(scene->disp);
    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create image demo failed");
    scene->image_demo = obj;
    (void)gfx_object_set_pos(obj, 286, 164);

    GFX_RETURN_ON_ERROR(demo_load_image_clip(scene, 0), "playground", "load image clip failed");
    (void)gfx_object_set_visible(obj, false);
    return GFX_OK;
}
