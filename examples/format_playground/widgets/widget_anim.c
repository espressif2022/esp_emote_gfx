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

#define DEMO_SPIFFS_ANIM_PARTITION "storage"
#define DEMO_SPIFFS_ANIM_MOUNT     "/spiffs"
#ifdef GFX_HOST_BUILD
#define DEMO_FILE_ANIM_MOUNT       "examples/esp/format_rgb565/spiffs_anim"
#else
#define DEMO_FILE_ANIM_MOUNT       DEMO_SPIFFS_ANIM_MOUNT
#endif

static const char *const TAG_ANIM = "anim";

typedef struct {
    const char *name;
    const char *path;  /**< Store name or absolute filesystem path. */
    const char *mode;
    uint32_t flags;
} demo_anim_clip_t;

/*
 * Anim clips for the preview. Plain names resolve through the default mmap
 * asset fs; "/spiffs/..." paths fall back to normal filesystem reads.
 * The list intentionally covers AAF/EAF, compressed variants, transparent
 * frames, and mmap/SPIFFS loading through the same FILE source path.
 */
static const demo_anim_clip_t s_anim_clips[] = {
    { .name = "AAF 24-bit",      .path = "mi_1_eye_24bit.aaf", .mode = "mmap AAF" },
    // { .name = "AAF 4-bit",       .path = "mi_1_eye_4bit.aaf", .mode = "mmap AAF" },
    { .name = "EAF 8-bit",       .path = "mi_1_eye_8bit.eaf", .mode = "mmap EAF" },
    { .name = "AAF Huff 8-bit",  .path = "mi_1_eye_8bit_huff.aaf", .mode = "mmap AAF" },
    { .name = "EAF Huff 8-bit",  .path = "mi_1_eye_8bit_huff.eaf", .mode = "mmap EAF" },
    { .name = "Transparent EAF", .path = "transparent.eaf", .mode = "mmap EAF" },
    { .name = "File AAF",        .path = DEMO_FILE_ANIM_MOUNT "/mi_1_eye_24bit.aaf", .mode = "file AAF copy" },
    { .name = "File EAF",        .path = DEMO_FILE_ANIM_MOUNT "/mi_1_eye_8bit.eaf", .mode = "file EAF stream", .flags = GFX_ANIM_SRC_FLAG_STREAMING },
};

#define DEMO_ANIM_COUNT (sizeof(s_anim_clips) / sizeof(s_anim_clips[0]))

static size_t s_anim_index;
static char s_anim_note[128];

static void demo_update_anim_note(size_t index)
{
    const demo_anim_clip_t *clip = &s_anim_clips[index];

    (void)snprintf(s_anim_note, sizeof(s_anim_note), "%s | %s",
                   clip->path, clip->mode);
}

size_t gfx_format_demo_anim_clip_count(void)
{
    return DEMO_ANIM_COUNT;
}

size_t gfx_format_demo_anim_clip_index(void)
{
    return s_anim_index;
}

const char *gfx_format_demo_anim_clip_note(void)
{
    if (s_anim_note[0] == '\0') {
        demo_update_anim_note(s_anim_index);
    }
    return s_anim_note;
}

static void demo_mount_spiffs_assets(void)
{
#ifndef GFX_HOST_BUILD
    /* The image widget mounts the same partition too; skip if already mounted. */
    if (esp_spiffs_mounted(DEMO_SPIFFS_ANIM_PARTITION)) {
        return;
    }
    const esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = DEMO_SPIFFS_ANIM_MOUNT,
        .partition_label = DEMO_SPIFFS_ANIM_PARTITION,
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    gfx_err_t err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != GFX_OK) {
        GFX_LOGW(TAG_ANIM, "spiffs mount failed: %s; SPIFFS clips will be skipped",
                 "(err)");
    }
#endif
}

static gfx_err_t demo_load_anim_clip(format_playground_scene_t *scene, size_t index)
{
    const demo_anim_clip_t *clip;
    gfx_anim_src_t src = {0};

    GFX_RETURN_ON_FALSE(scene != NULL && scene->anim_demo != NULL, GFX_ERR_INVALID_ARG,
                        "playground", "anim object is not ready");
    GFX_RETURN_ON_FALSE(index < DEMO_ANIM_COUNT, GFX_ERR_INVALID_ARG,
                        "playground", "anim index is invalid");

    clip = &s_anim_clips[index];
    src.type = GFX_ANIM_SRC_TYPE_FILE;
    src.data = clip->path;
    src.flags = clip->flags;

    (void)gfx_anim_stop(scene->anim_demo);
    GFX_RETURN_ON_ERROR(gfx_anim_set_src_desc(scene->anim_demo, &src),
                        "playground", "set anim src failed");
    GFX_RETURN_ON_ERROR(gfx_anim_set_segment(scene->anim_demo, 0, 0xFFFFFFFFU, 50, true),
                        "playground", "set anim segment failed");
    GFX_RETURN_ON_ERROR(gfx_anim_start(scene->anim_demo), "playground", "start anim failed");

    s_anim_index = index;
    demo_update_anim_note(index);
    if (scene->preview_button != NULL && scene->widget_idx == DEMO_WIDGET_ANIM) {
        (void)gfx_button_set_text_fmt(scene->preview_button, "Next Anim  %u/%u",
                                      (unsigned)(index + 1U), (unsigned)DEMO_ANIM_COUNT);
    }
    if (scene->preview_note != NULL && scene->widget_idx == DEMO_WIDGET_ANIM) {
        (void)gfx_label_set_text(scene->preview_note, gfx_format_demo_anim_clip_note());
    }
    return GFX_OK;
}

gfx_err_t gfx_format_demo_next_anim_clip(format_playground_scene_t *scene)
{
    return demo_load_anim_clip(scene, (s_anim_index + 1U) % DEMO_ANIM_COUNT);
}

gfx_err_t gfx_format_demo_build_anim_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj;

    demo_mount_spiffs_assets();

    obj = gfx_anim_create(scene->disp);
    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create anim demo failed");

    scene->anim_demo = obj;
    (void)gfx_object_set_pos(obj, 286, 164);

    GFX_RETURN_ON_ERROR(demo_load_anim_clip(scene, 0), "playground", "load anim clip failed");
    (void)gfx_object_set_visible(obj, false);
    return GFX_OK;
}
