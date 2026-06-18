/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "playground_scene_priv.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define DEMO_SPIFFS_IMAGE_PARTITION "storage"
#define DEMO_SPIFFS_IMAGE_MOUNT     "/spiffs"

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
    gfx_image_src_type_t type;
    const char *path;  /**< FILE: store name or absolute path. MEMORY: mmap asset name. */
} demo_image_clip_t;

/*
 * Image clips for the preview. FILE names resolve through the mmap asset store,
 * the "/spiffs/..." path is read from a real SPIFFS filesystem via fread, and the
 * MEMORY clip is decoded straight from the assets_test mmap mapping
 * (direct-addressable flash). Switching index just updates the image src.
 */
static const demo_image_clip_t s_image_clips[] = {
    { .name = "Warm Harbor",   .type = GFX_IMAGE_SRC_TYPE_FILE,   .path = "flow_warm_harbor.jpg" },
    { .name = "Misty Ridge",   .type = GFX_IMAGE_SRC_TYPE_FILE,   .path = "flow_misty_ridge.jpg" },
    { .name = "SPIFFS Trail",  .type = GFX_IMAGE_SRC_TYPE_FILE,   .path = DEMO_SPIFFS_IMAGE_MOUNT "/flow_quiet_trail.jpg" },
    { .name = "MEM Probe JPG", .type = GFX_IMAGE_SRC_TYPE_MEMORY, .path = "flow_format_probe.jpg" },
};

#define DEMO_IMAGE_CLIP_COUNT (sizeof(s_image_clips) / sizeof(s_image_clips[0]))

static size_t s_image_index;

static void demo_mount_spiffs_assets(void)
{
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
    esp_err_t err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_IMAGE, "spiffs mount failed: %s; SPIFFS clip will be skipped",
                 esp_err_to_name(err));
    }
}

static esp_err_t demo_load_image_clip(format_playground_scene_t *scene, size_t index)
{
    const demo_image_clip_t *clip;
    gfx_image_src_t src = {0};

    ESP_RETURN_ON_FALSE(scene != NULL && scene->image_demo != NULL, ESP_ERR_INVALID_STATE,
                        "playground", "image object is not ready");
    ESP_RETURN_ON_FALSE(index < DEMO_IMAGE_CLIP_COUNT, ESP_ERR_INVALID_ARG,
                        "playground", "image index is invalid");

    clip = &s_image_clips[index];
    src.type = clip->type;
    if (clip->type == GFX_IMAGE_SRC_TYPE_MEMORY) {
        /* Reuse the assets_test mmap mapping: the view is direct-addressable
         * and persistent for the store lifetime, so the pointer stays valid
         * after close. */
        gfx_asset_store_t *store = gfx_asset_get_default_store();
        gfx_asset_view_t view = {0};
        ESP_RETURN_ON_FALSE(store != NULL, ESP_ERR_INVALID_STATE, "playground",
                            "mem image needs the default asset store");
        ESP_RETURN_ON_FALSE(gfx_asset_open_by_name(store, clip->path, &view) == GFX_OK,
                            ESP_ERR_NOT_FOUND, "playground", "mem image asset not found");
        src.data = view.data;
        src.data_len = view.size;
        gfx_asset_view_close(&view);
    } else {
        src.data = clip->path;
    }

    ESP_RETURN_ON_ERROR(gfx_image_set_source_desc(scene->image_demo, &src),
                        "playground", "set image src failed");

    s_image_index = index;
    if (scene->image_button != NULL) {
        (void)gfx_button_set_text_fmt(scene->image_button, "Next Image  %u/%u",
                                      (unsigned)(index + 1U), (unsigned)DEMO_IMAGE_CLIP_COUNT);
    }
    if (scene->status != NULL && scene->widget_idx == DEMO_WIDGET_IMAGE) {
        (void)gfx_label_set_text_fmt(scene->status, "Image: %s   Output: %s",
                                     clip->name,
                                     scene->format_tag != NULL ? scene->format_tag : "Unknown");
    }
    return ESP_OK;
}

static void demo_image_button_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    (void)demo_load_image_clip(scene, (s_image_index + 1U) % DEMO_IMAGE_CLIP_COUNT);
}

esp_err_t gfx_format_demo_build_image_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj;
    gfx_object_t *button;

    demo_mount_spiffs_assets();

    obj = gfx_image_create(scene->disp);
    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_NO_MEM, "playground", "create image demo failed");
    scene->image_demo = obj;
    (void)gfx_object_set_pos(obj, 286, 164);

    button = gfx_button_create(scene->disp);
    ESP_RETURN_ON_FALSE(button != NULL, ESP_ERR_NO_MEM, "playground", "create image switch button failed");
    scene->image_button = button;
    (void)gfx_object_set_pos(button, 330, 370);
    (void)gfx_object_set_size(button, 190, 42);
    (void)gfx_button_set_font(button, scene->font);
    (void)gfx_button_set_bg_color(button, GFX_COLOR_HEX(0x245C8F));
    (void)gfx_button_set_bg_color_pressed(button, GFX_COLOR_HEX(0x2E7D32));
    (void)gfx_button_set_border_color(button, GFX_COLOR_HEX(0x76B7E8));
    (void)gfx_button_set_border_width(button, 2);
    (void)gfx_button_set_text_color(button, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_button_set_text_align(button, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_object_set_touch_cb(button, demo_image_button_cb, scene);

    ESP_RETURN_ON_ERROR(demo_load_image_clip(scene, 0), "playground", "load image clip failed");
    (void)gfx_object_set_visible(obj, false);
    (void)gfx_object_set_visible(button, false);
    return ESP_OK;
}
