/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "playground_scene_priv.h"

#include <stdio.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define DEMO_SPIFFS_ANIM_PARTITION "storage"
#define DEMO_SPIFFS_ANIM_MOUNT     "/spiffs"

static const char *const TAG_ANIM_JPG = "anim_jpg";

typedef struct {
    const char *name;
    gfx_anim_src_type_t type;
    const char *path;  /**< FILE: store name or absolute path. MEMORY: mmap asset name. */
    const char *mode;
} demo_anim_clip_t;

/*
 * Anim clips for the preview. FILE names resolve through the mmap asset store,
 * "/spiffs/..." paths are read from SPIFFS via fread, and the MEMORY clip plays
 * straight from the assets_test mmap mapping (direct-addressable flash).
 * Switching index just updates the src.
 */
static const demo_anim_clip_t s_anim_jpg_clips[] = {
    { .name = "Eye Anim 1",     .type = GFX_ANIM_SRC_TYPE_FILE,   .path = "mi_1_eye_24bit.aaf", .mode = "mmap FILE" },
    { .name = "Eye Anim 2",     .type = GFX_ANIM_SRC_TYPE_FILE,   .path = "mi_2_eye_24bit.aaf", .mode = "mmap FILE" },
    { .name = "SPIFFS Eye AAF", .type = GFX_ANIM_SRC_TYPE_FILE,   .path = DEMO_SPIFFS_ANIM_MOUNT "/mi_1_eye_24bit.aaf", .mode = "SPIFFS fread" },
    { .name = "SPIFFS Eye EAF", .type = GFX_ANIM_SRC_TYPE_FILE,   .path = DEMO_SPIFFS_ANIM_MOUNT "/mi_1_eye_8bit.eaf", .mode = "SPIFFS fread" },
    { .name = "MEM Eye AAF",    .type = GFX_ANIM_SRC_TYPE_MEMORY, .path = "mi_1_eye_24bit.aaf", .mode = "assets_test MEM" },
};

#define DEMO_ANIM_JPG_COUNT (sizeof(s_anim_jpg_clips) / sizeof(s_anim_jpg_clips[0]))

static size_t s_anim_jpg_index;
static char s_anim_jpg_note[96];

static void demo_update_anim_jpg_note(size_t index)
{
    const demo_anim_clip_t *clip = &s_anim_jpg_clips[index];

    (void)snprintf(s_anim_jpg_note, sizeof(s_anim_jpg_note), "%s | %s | %s",
                   clip->name, clip->mode, clip->path);
}

size_t gfx_format_demo_anim_asset_count(void)
{
    return DEMO_ANIM_JPG_COUNT;
}

size_t gfx_format_demo_anim_jpg_clip_index(void)
{
    return s_anim_jpg_index;
}

const char *gfx_format_demo_anim_jpg_clip_note(void)
{
    if (s_anim_jpg_note[0] == '\0') {
        demo_update_anim_jpg_note(s_anim_jpg_index);
    }
    return s_anim_jpg_note;
}

static void demo_mount_spiffs_assets(void)
{
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
    esp_err_t err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_ANIM_JPG, "spiffs mount failed: %s; SPIFFS clips will be skipped",
                 esp_err_to_name(err));
    }
}

static esp_err_t demo_load_anim_jpg_clip(format_playground_scene_t *scene, size_t index)
{
    const demo_anim_clip_t *clip;
    gfx_anim_src_t src = {0};

    ESP_RETURN_ON_FALSE(scene != NULL && scene->anim_jpg_demo != NULL, ESP_ERR_INVALID_STATE,
                        "playground", "anim object is not ready");
    ESP_RETURN_ON_FALSE(index < DEMO_ANIM_JPG_COUNT, ESP_ERR_INVALID_ARG,
                        "playground", "anim index is invalid");

    clip = &s_anim_jpg_clips[index];
    src.type = clip->type;
    if (clip->type == GFX_ANIM_SRC_TYPE_MEMORY) {
        /* Reuse the assets_test mmap mapping: the view is direct-addressable
         * and persistent for the store lifetime, so the pointer stays valid
         * after close. */
        gfx_asset_store_t *store = gfx_asset_get_default_store();
        gfx_asset_view_t view = {0};
        ESP_RETURN_ON_FALSE(store != NULL, ESP_ERR_INVALID_STATE, "playground",
                            "mem anim needs the default asset store");
        ESP_RETURN_ON_FALSE(gfx_asset_open_by_name(store, clip->path, &view) == GFX_OK,
                            ESP_ERR_NOT_FOUND, "playground", "mem anim asset not found");
        src.data = view.data;
        src.data_len = view.size;
        gfx_asset_view_close(&view);
    } else {
        src.data = clip->path;
    }

    (void)gfx_anim_stop(scene->anim_jpg_demo);
    ESP_RETURN_ON_ERROR(gfx_anim_set_src_desc(scene->anim_jpg_demo, &src),
                        "playground", "set anim src failed");
    ESP_RETURN_ON_ERROR(gfx_anim_set_segment(scene->anim_jpg_demo, 0, 0xFFFFFFFFU, 15, true),
                        "playground", "set anim segment failed");
    ESP_RETURN_ON_ERROR(gfx_anim_start(scene->anim_jpg_demo), "playground", "start anim failed");

    s_anim_jpg_index = index;
    demo_update_anim_jpg_note(index);
    if (scene->preview_button != NULL && scene->widget_idx == DEMO_WIDGET_ANIM_JPG) {
        (void)gfx_button_set_text_fmt(scene->preview_button, "Next Anim  %u/%u",
                                      (unsigned)(index + 1U), (unsigned)DEMO_ANIM_JPG_COUNT);
    }
    if (scene->preview_note != NULL && scene->widget_idx == DEMO_WIDGET_ANIM_JPG) {
        (void)gfx_label_set_text(scene->preview_note, gfx_format_demo_anim_jpg_clip_note());
    }
    return ESP_OK;
}

esp_err_t gfx_format_demo_next_anim_jpg_clip(format_playground_scene_t *scene)
{
    return demo_load_anim_jpg_clip(scene, (s_anim_jpg_index + 1U) % DEMO_ANIM_JPG_COUNT);
}

esp_err_t gfx_format_demo_build_anim_jpg_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj;

    demo_mount_spiffs_assets();

    obj = gfx_anim_create(scene->disp);
    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_NO_MEM, "playground", "create anim demo failed");

    scene->anim_jpg_demo = obj;
    (void)gfx_object_set_pos(obj, 286, 164);

    ESP_RETURN_ON_ERROR(demo_load_anim_jpg_clip(scene, 0), "playground", "load anim clip failed");
    (void)gfx_object_set_visible(obj, false);
    return ESP_OK;
}
