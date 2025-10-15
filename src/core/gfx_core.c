/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"

#include "core/gfx_core_internal.h"
#include "core/gfx_timer_internal.h"
#include "widget/gfx_img_internal.h"
#include "widget/gfx_label_internal.h"
#include "widget/gfx_anim_internal.h"

static const char *TAG = "gfx_core";

static bool gfx_refr_handler(gfx_core_context_t *ctx);
static bool gfx_event_handler(gfx_core_context_t *ctx);
static bool gfx_object_handler(gfx_core_context_t *ctx);
static esp_err_t gfx_buf_init_frame(gfx_core_context_t *ctx, const gfx_core_config_t *cfg);
static void gfx_buf_free_frame(gfx_core_context_t *ctx);
static uint32_t gfx_calculate_task_delay(uint32_t timer_delay);

/* Dirty area helper functions */
static void area_copy(gfx_area_t *dest, const gfx_area_t *src);
static bool area_is_in(const gfx_area_t *area_in, const gfx_area_t *area_parent);
static bool area_intersect(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2);
static uint32_t area_get_size(const gfx_area_t *area);
static bool area_is_on(const gfx_area_t *a1, const gfx_area_t *a2);
static void area_join(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2);
static void gfx_refr_join_areas(gfx_core_context_t *ctx);

/**
 * @brief Calculate task delay based on timer delay and system tick rate
 * @param timer_delay Timer delay in milliseconds
 * @return Calculated task delay in milliseconds
 */
static uint32_t gfx_calculate_task_delay(uint32_t timer_delay)
{
    uint32_t min_delay_ms = (1000 / configTICK_RATE_HZ) + 1; // At least one tick + 1ms

    if (timer_delay == ANIM_NO_TIMER_READY) {
        return (min_delay_ms > 5) ? min_delay_ms : 5;
    } else {
        return (timer_delay < min_delay_ms) ? min_delay_ms : timer_delay;
    }
}

/**
 * @brief Handle system events and user requests
 * @param ctx Player context
 * @return true if event was handled, false otherwise
 */
static bool gfx_event_handler(gfx_core_context_t *ctx)
{
    EventBits_t event_bits = xEventGroupWaitBits(ctx->sync.event_group,
                             NEED_DELETE, pdTRUE, pdFALSE, pdMS_TO_TICKS(0));

    if (event_bits & NEED_DELETE) {
        xEventGroupSetBits(ctx->sync.event_group, DELETE_DONE);
        vTaskDeleteWithCaps(NULL);
        return true;
    }

    return false;
}

/**
 * @brief Handle object updates and preprocessing
 * @param ctx Player context
 * @return true if objects need rendering, false otherwise
 */
static bool gfx_object_handler(gfx_core_context_t *ctx)
{
    if (ctx->disp.child_list == NULL) {
        return false;
    }

    gfx_core_child_t *child_node = ctx->disp.child_list;

    /* Preprocess animation frames */
    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

        if (obj->type == GFX_OBJ_TYPE_ANIMATION) {
            gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
            if (anim && anim->file_desc) {
                if (ESP_OK != gfx_anim_preprocess_frame(anim)) {
                    child_node = child_node->next;
                    continue;
                }
            }
        }
        child_node = child_node->next;
    }

    return true;
}

/**
 * @brief Initialize frame buffers (internal or external)
 * @param ctx Player context
 * @param cfg Graphics configuration (includes buffer configuration)
 * @return esp_err_t ESP_OK on success, otherwise error code
 */
static esp_err_t gfx_buf_init_frame(gfx_core_context_t *ctx, const gfx_core_config_t *cfg)
{
    ESP_LOGD(TAG, "cfg.buffers.buf1=%p, cfg.buffers.buf2=%p", cfg->buffers.buf1, cfg->buffers.buf2);
    if (cfg->buffers.buf1 != NULL) {
        ctx->disp.buf1 = (uint16_t *)cfg->buffers.buf1;
        ctx->disp.buf2 = (uint16_t *)cfg->buffers.buf2;

        if (cfg->buffers.buf_pixels > 0) {
            ctx->disp.buf_pixels = cfg->buffers.buf_pixels;
        } else {
            ESP_LOGW(TAG, "cfg.buffers.buf_pixels is 0, use default size");
            ctx->disp.buf_pixels = ctx->display.h_res * ctx->display.v_res;
        }

        ctx->disp.ext_bufs = true;
    } else {
        // Allocate internal buffers
        uint32_t buff_caps = 0;
#if SOC_PSRAM_DMA_CAPABLE == 0
        if (cfg->flags.buff_dma && cfg->flags.buff_spiram) {
            ESP_LOGW(TAG, "Alloc DMA capable buffer in SPIRAM is not supported!");
            return ESP_ERR_NOT_SUPPORTED;
        }
#endif
        if (cfg->flags.buff_dma) {
            buff_caps |= MALLOC_CAP_DMA;
        }
        if (cfg->flags.buff_spiram) {
            buff_caps |= MALLOC_CAP_SPIRAM;
        }
        if (buff_caps == 0) {
            buff_caps |= MALLOC_CAP_DEFAULT;
        }

        size_t buf_pixels = cfg->buffers.buf_pixels > 0 ? cfg->buffers.buf_pixels : ctx->display.h_res * ctx->display.v_res;

        ctx->disp.buf1 = (uint16_t *)heap_caps_malloc(buf_pixels * sizeof(uint16_t), buff_caps);
        if (!ctx->disp.buf1) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer 1");
            return ESP_ERR_NO_MEM;
        }

        if (cfg->flags.double_buffer) {
            ctx->disp.buf2 = (uint16_t *)heap_caps_malloc(buf_pixels * sizeof(uint16_t), buff_caps);
            if (!ctx->disp.buf2) {
                ESP_LOGE(TAG, "Failed to allocate frame buffer 2");
                free(ctx->disp.buf1);
                ctx->disp.buf1 = NULL;
                return ESP_ERR_NO_MEM;
            }
        }

        ctx->disp.buf_pixels = buf_pixels;
        ctx->disp.ext_bufs = false;
    }
    ESP_LOGD(TAG, "Use frame buffers: buf1=%p, buf2=%p, size=%zu, ext_bufs=%d",
             ctx->disp.buf1, ctx->disp.buf2, ctx->disp.buf_pixels, ctx->disp.ext_bufs);

    ctx->disp.buf_act = ctx->disp.buf1;
    ctx->disp.bg_color.full = 0x0000;
    return ESP_OK;
}

/**
 * @brief Free frame buffers (only internal buffers)
 * @param ctx Player context
 */
static void gfx_buf_free_frame(gfx_core_context_t *ctx)
{
    // Only free buffers if they were internally allocated
    if (!ctx->disp.ext_bufs) {
        if (ctx->disp.buf1) {
            free(ctx->disp.buf1);
            ctx->disp.buf1 = NULL;
        }
        if (ctx->disp.buf2) {
            free(ctx->disp.buf2);
            ctx->disp.buf2 = NULL;
        }
        ESP_LOGI(TAG, "Freed internal frame buffers");
    } else {
        ESP_LOGI(TAG, "External buffers provided by user, not freeing");
    }
    ctx->disp.buf_pixels = 0;
    ctx->disp.ext_bufs = false;
}

void gfx_draw_child(gfx_core_context_t *ctx, int x1, int y1, int x2, int y2, const void *dest_buf)
{
    if (ctx->disp.child_list == NULL) {
        ESP_LOGD(TAG, "no child objects");
        return;
    }

    gfx_core_child_t *child_node = ctx->disp.child_list;
    bool swap = ctx->display.flags.swap;

    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

        // Skip rendering if object is not visible
        if (!obj->is_visible) {
            child_node = child_node->next;
            continue;
        }

        if (obj->type == GFX_OBJ_TYPE_LABEL) {
            gfx_draw_label(obj, x1, y1, x2, y2, dest_buf, swap);
        } else if (obj->type == GFX_OBJ_TYPE_IMAGE) {
            gfx_draw_img(obj, x1, y1, x2, y2, dest_buf, swap);
        } else if (obj->type == GFX_OBJ_TYPE_ANIMATION) {
            gfx_draw_animation(obj, x1, y1, x2, y2, dest_buf, swap);
        }

        child_node = child_node->next;
    }
}

static void gfx_core_task(void *arg)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)arg;
    uint32_t timer_delay = 1; // Default delay

    gfx_obj_t obj;
    obj.x = 0;
    obj.y = 0;
    obj.width = ctx->display.h_res;
    obj.height = ctx->display.v_res;
    obj.is_visible = true;
    obj.is_dirty = true;
    obj.parent_handle = ctx;
    gfx_obj_invalidate(&obj);

    while (1) {
        if (ctx->sync.lock_mutex && xSemaphoreTakeRecursive(ctx->sync.lock_mutex, portMAX_DELAY) == pdTRUE) {
            if (gfx_event_handler(ctx)) {
                xSemaphoreGiveRecursive(ctx->sync.lock_mutex);
                break;
            }

            timer_delay = gfx_timer_handler(&ctx->timer.timer_mgr);

            if (ctx->disp.child_list != NULL) {
                gfx_refr_handler(ctx);
            }

            uint32_t task_delay = gfx_calculate_task_delay(timer_delay);

            xSemaphoreGiveRecursive(ctx->sync.lock_mutex);
            vTaskDelay(pdMS_TO_TICKS(task_delay));
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex, retrying...");
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

bool gfx_emote_flush_ready(gfx_handle_t handle, bool swap_act_buf)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        return false;
    }

    if (xPortInIsrContext()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        ctx->disp.swap_act_buf = swap_act_buf;
        bool result = xEventGroupSetBitsFromISR(ctx->sync.event_group, WAIT_FLUSH_DONE, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        return result;
    } else {
        ctx->disp.swap_act_buf = swap_act_buf;
        return xEventGroupSetBits(ctx->sync.event_group, WAIT_FLUSH_DONE);
    }
}

void *gfx_emote_get_user_data(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context");
        return NULL;
    }

    return ctx->callbacks.user_data;
}

esp_err_t gfx_emote_get_screen_size(gfx_handle_t handle, uint32_t *width, uint32_t *height)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context");
        return ESP_ERR_INVALID_ARG;
    }

    if (width == NULL || height == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    *width = ctx->display.h_res;
    *height = ctx->display.v_res;

    return ESP_OK;
}

gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg)
{
    if (!cfg) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }

    gfx_core_context_t *disp_ctx = malloc(sizeof(gfx_core_context_t));
    if (!disp_ctx) {
        ESP_LOGE(TAG, "Failed to allocate player context");
        return NULL;
    }

    // Initialize all fields to zero/NULL
    memset(disp_ctx, 0, sizeof(gfx_core_context_t));

    disp_ctx->display.v_res = cfg->v_res;
    disp_ctx->display.h_res = cfg->h_res;
    disp_ctx->display.flags.swap = cfg->flags.swap;

    disp_ctx->callbacks.flush_cb = cfg->flush_cb;
    disp_ctx->callbacks.update_cb = cfg->update_cb;
    disp_ctx->callbacks.user_data = cfg->user_data;

    disp_ctx->sync.event_group = xEventGroupCreate();

    disp_ctx->disp.child_list = NULL;

    // Initialize frame buffers (internal or external)
    esp_err_t buffer_ret = gfx_buf_init_frame(disp_ctx, cfg);
    if (buffer_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize frame buffers");
        vEventGroupDelete(disp_ctx->sync.event_group);
        free(disp_ctx);
        return NULL;
    }

    // Initialize timer manager
    gfx_timer_manager_init(&disp_ctx->timer.timer_mgr, cfg->fps);

    // Create recursive render mutex for protecting rendering operations
    disp_ctx->sync.lock_mutex = xSemaphoreCreateRecursiveMutex();
    if (disp_ctx->sync.lock_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create recursive render mutex");
        gfx_buf_free_frame(disp_ctx);
        vEventGroupDelete(disp_ctx->sync.event_group);
        free(disp_ctx);
        return NULL;
    }

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    esp_err_t font_ret = gfx_ft_lib_create();
    if (font_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create font library");
        gfx_buf_free_frame(disp_ctx);
        vSemaphoreDelete(disp_ctx->sync.lock_mutex);
        vEventGroupDelete(disp_ctx->sync.event_group);
        free(disp_ctx);
        return NULL;
    }
#endif

    // Initialize image decoder system
    esp_err_t decoder_ret = gfx_image_decoder_init();
    if (decoder_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize image decoder system");
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
        gfx_ft_lib_cleanup();
#endif
        gfx_buf_free_frame(disp_ctx);
        vSemaphoreDelete(disp_ctx->sync.lock_mutex);
        vEventGroupDelete(disp_ctx->sync.event_group);
        free(disp_ctx);
        return NULL;
    }

    const uint32_t stack_caps = cfg->task.task_stack_caps ? cfg->task.task_stack_caps : MALLOC_CAP_DEFAULT; // caps cannot be zero
    if (cfg->task.task_affinity < 0) {
        xTaskCreateWithCaps(gfx_core_task, "gfx_core", cfg->task.task_stack, disp_ctx, cfg->task.task_priority, NULL, stack_caps);
    } else {
        xTaskCreatePinnedToCoreWithCaps(gfx_core_task, "gfx_core", cfg->task.task_stack, disp_ctx, cfg->task.task_priority, NULL, cfg->task.task_affinity, stack_caps);
    }

    return (gfx_handle_t)disp_ctx;
}

void gfx_emote_deinit(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context");
        return;
    }

    xEventGroupSetBits(ctx->sync.event_group, NEED_DELETE);
    xEventGroupWaitBits(ctx->sync.event_group, DELETE_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

    // Free all child nodes
    gfx_core_child_t *child_node = ctx->disp.child_list;
    while (child_node != NULL) {
        gfx_core_child_t *next_child = child_node->next;
        free(child_node);
        child_node = next_child;
    }
    ctx->disp.child_list = NULL;

    // Clean up timers
    gfx_timer_manager_deinit(&ctx->timer.timer_mgr);

    // Free frame buffers
    gfx_buf_free_frame(ctx);

    // Delete font library
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_ft_lib_cleanup();
#endif

    // Delete mutex
    if (ctx->sync.lock_mutex) {
        vSemaphoreDelete(ctx->sync.lock_mutex);
        ctx->sync.lock_mutex = NULL;
    }

    // Delete event group
    if (ctx->sync.event_group) {
        vEventGroupDelete(ctx->sync.event_group);
        ctx->sync.event_group = NULL;
    }

    // Deinitialize image decoder system
    gfx_image_decoder_deinit();

    // Free context
    free(ctx);
}

esp_err_t gfx_emote_add_chlid(gfx_handle_t handle, int type, void *src)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || src == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_core_child_t *new_child = (gfx_core_child_t *)malloc(sizeof(gfx_core_child_t));
    if (new_child == NULL) {
        ESP_LOGE(TAG, "Failed to allocate child node");
        return ESP_ERR_NO_MEM;
    }

    new_child->type = type;
    new_child->src = src;
    new_child->next = NULL;

    // Add to child list
    if (ctx->disp.child_list == NULL) {
        ctx->disp.child_list = new_child;
    } else {
        gfx_core_child_t *current = ctx->disp.child_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_child;
    }

    ESP_LOGD(TAG, "Added child object of type %d", type);
    return ESP_OK;
}

esp_err_t gfx_emote_remove_child(gfx_handle_t handle, void *src)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || src == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_core_child_t *current = ctx->disp.child_list;
    gfx_core_child_t *prev = NULL;

    while (current != NULL) {
        if (current->src == src) {
            if (prev == NULL) {
                ctx->disp.child_list = current->next;
            } else {
                prev->next = current->next;
            }

            free(current);
            ESP_LOGD(TAG, "Removed child object from list");
            return ESP_OK;
        }
        prev = current;
        current = current->next;
    }

    ESP_LOGW(TAG, "Child object not found in list");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_emote_lock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || ctx->sync.lock_mutex == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context or mutex");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(ctx->sync.lock_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire graphics lock");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t gfx_emote_unlock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || ctx->sync.lock_mutex == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context or mutex");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreGiveRecursive(ctx->sync.lock_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to release graphics lock");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

esp_err_t gfx_emote_set_bg_color(gfx_handle_t handle, gfx_color_t color)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context");
        return ESP_ERR_INVALID_ARG;
    }

    ctx->disp.bg_color = color;
    ESP_LOGD(TAG, "Set background color to 0x%04X", color.full);
    return ESP_OK;
}

bool gfx_emote_is_flushing_last(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context");
        return false;
    }

    return ctx->disp.flushing_last;
}

/**
 * @brief Fast fill buffer with background color
 * @param buf Pointer to uint16_t buffer
 * @param color 16-bit color value
 * @param pixels Number of pixels to fill
 */
static inline void gfx_fill_color(uint16_t *buf, uint16_t color, size_t pixels)
{
    if ((color & 0xFF) == (color >> 8)) {
        memset(buf, color & 0xFF, pixels * sizeof(uint16_t));
    } else {
        uint32_t color32 = (color << 16) | color;
        uint32_t *buf32 = (uint32_t *)buf;
        size_t pixels_half = pixels / 2;

        for (size_t i = 0; i < pixels_half; i++) {
            buf32[i] = color32;
        }

        if (pixels & 1) {
            buf[pixels - 1] = color;
        }
    }
}

/**
 * @brief Handle rendering of all objects in the scene
 * @param ctx Player context
 * @return true if rendering was performed, false otherwise
 */
static bool gfx_refr_handler(gfx_core_context_t *ctx)
{
    bool updated = gfx_object_handler(ctx);


    /* Join overlapping/adjacent dirty areas */
    if (ctx->disp.inv_p > 1) {
        gfx_refr_join_areas(ctx);
    }

    /* If no dirty areas, skip rendering */
    if (ctx->disp.inv_p == 0) {
        return false;
    }

    /* Print dirty areas summary */
    ESP_LOGI(TAG, "╔════════════════ Dirty Areas: %d ════════════════╗", ctx->disp.inv_p);
    uint32_t total_dirty_pixels = 0;
    for (uint8_t i = 0; i < ctx->disp.inv_p; i++) {
        if (ctx->disp.inv_area_joined[i]) {
            continue;    /* Skip joined areas */
        }
        gfx_area_t *area = &ctx->disp.inv_areas[i];
        uint32_t area_size = area_get_size(area);
        total_dirty_pixels += area_size;
        ESP_LOGI(TAG, "║ [%d] Area: (%3d,%3d)->(%3d,%3d)  Size: %3dx%-3d ║",
                 i, area->x1, area->y1, area->x2, area->y2,
                 area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    }
    ESP_LOGI(TAG, "╠═══════════════════════════════════════════════╣");

    uint32_t rendered_blocks = 0;

    for (uint8_t i = 0; i < ctx->disp.inv_p; i++) {
        if (ctx->disp.inv_area_joined[i]) {
            continue;
        }

        gfx_area_t *area = &ctx->disp.inv_areas[i];

        uint32_t area_width = area->x2 - area->x1 + 1;
        uint32_t area_height = area->y2 - area->y1 + 1;
        uint32_t area_pixels = area_width * area_height;

        uint32_t per_flush = ctx->disp.buf_pixels / area_width;
        if (per_flush == 0) {
            ESP_LOGE(TAG, "Area[%d] width %lu exceeds buffer width, skipping", i, area_width);
            continue;
        }

        uint32_t total_flushes = (area_height + per_flush - 1) / per_flush;

        ESP_LOGI(TAG, "Area[%d]: %lu px > %lu px, split into %lu flushes (%lu px each)",
            i, area_pixels, ctx->disp.buf_pixels, total_flushes, per_flush);

        int current_y = area->y1;
        uint32_t flush_idx = 0;

        while (current_y <= area->y2) {
            rendered_blocks++;
            flush_idx++;

            int x1 = area->x1;
            int y1 = current_y;
            int x2 = area->x2 + 1;  /* Convert to exclusive */
            int y2 = current_y + per_flush;
            if (y2 > area->y2 + 1) {
                y2 = area->y2 + 1;
            }

            uint32_t chunk_pixels = area_width * (y2 - y1);
            uint16_t *buf_act = ctx->disp.buf_act;

            gfx_fill_color(buf_act, ctx->disp.bg_color.full, ctx->disp.buf_pixels);

            gfx_draw_child(ctx, x1, y1, x2, y2, buf_act);

            if (ctx->callbacks.flush_cb) {
                xEventGroupClearBits(ctx->sync.event_group, WAIT_FLUSH_DONE);

                if (total_flushes == 1) {
                    ESP_LOGI(TAG, "Flush[%lu]: Area[%d] (%3d,%3d)->(%3d,%3d) [%lupx/%lupx]",
                             rendered_blocks, i, x1, y1, x2 - 1, y2 - 1,
                             chunk_pixels, ctx->disp.buf_pixels);
                } else {
                    ESP_LOGI(TAG, "Flush[%lu]: Area[%d] (%3d,%3d)->(%3d,%3d) [Chunk %lu/%lu: %lupx]",
                             rendered_blocks, i, x1, y1, x2 - 1, y2 - 1,
                             flush_idx, total_flushes, chunk_pixels);
                }

                ctx->callbacks.flush_cb(ctx, x1, y1, x2, y2, buf_act);
                xEventGroupWaitBits(ctx->sync.event_group, WAIT_FLUSH_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(20));
            }

            current_y = y2;
        }
    }

    /* Set flushing_last and swap buffers after all blocks are done */
    ctx->disp.flushing_last = true;
    if (ctx->disp.buf2 != NULL) {
        if (ctx->disp.buf_act == ctx->disp.buf1) {
            ctx->disp.buf_act = ctx->disp.buf2;
        } else {
            ctx->disp.buf_act = ctx->disp.buf1;
        }
    }

    uint32_t screen_pixels = ctx->display.h_res * ctx->display.v_res;
    float dirty_percentage = (total_dirty_pixels * 100.0f) / screen_pixels;
    ESP_LOGI(TAG, "║ Rendered: %lu blocks, %lu pixels (%.1f%% screen)║",
             rendered_blocks, total_dirty_pixels, dirty_percentage);
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════╝");

    /* Clear dirty areas after rendering is complete */
    if (ctx->disp.inv_p > 0) {
        ESP_LOGI(TAG, "Clearing %d dirty areas after render", ctx->disp.inv_p);
        gfx_inv_area(ctx, NULL);
    }

    /* Clear all object dirty flags */
    gfx_core_child_t *child_node = ctx->disp.child_list;
    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;
        obj->is_dirty = false;
        child_node = child_node->next;
    }

    return true;
}

/*=====================
 * Dirty area helper function implementations
 *====================*/

/**
 * @brief Copy area from src to dest
 */
static void area_copy(gfx_area_t *dest, const gfx_area_t *src)
{
    dest->x1 = src->x1;
    dest->y1 = src->y1;
    dest->x2 = src->x2;
    dest->y2 = src->y2;
}

/**
 * @brief Check if area_in is fully contained within area_parent
 * @return true if area_in is completely inside area_parent
 */
static bool area_is_in(const gfx_area_t *area_in, const gfx_area_t *area_parent)
{
    if (area_in->x1 >= area_parent->x1 &&
            area_in->y1 >= area_parent->y1 &&
            area_in->x2 <= area_parent->x2 &&
            area_in->y2 <= area_parent->y2) {
        return true;
    }
    return false;
}

/**
 * @brief Get intersection of two areas
 * @return true if areas intersect, false otherwise
 */
static bool area_intersect(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
{
    gfx_coord_t x1 = (a1->x1 > a2->x1) ? a1->x1 : a2->x1;
    gfx_coord_t y1 = (a1->y1 > a2->y1) ? a1->y1 : a2->y1;
    gfx_coord_t x2 = (a1->x2 < a2->x2) ? a1->x2 : a2->x2;
    gfx_coord_t y2 = (a1->y2 < a2->y2) ? a1->y2 : a2->y2;

    if (x1 <= x2 && y1 <= y2) {
        result->x1 = x1;
        result->y1 = y1;
        result->x2 = x2;
        result->y2 = y2;
        return true;
    }
    return false;
}


/**
 * @brief Get the size (area) of a rectangular region
 * @param area Area to calculate size for
 * @return Size in pixels (width * height)
 */
static uint32_t area_get_size(const gfx_area_t *area)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    return width * height;
}

/**
 * @brief Check if two areas are on each other (overlap or touch)
 * Similar to LVGL's _lv_area_is_on
 * @param a1 First area
 * @param a2 Second area
 * @return true if areas overlap or are adjacent (touch)
 */
static bool area_is_on(const gfx_area_t *a1, const gfx_area_t *a2)
{
    /* Check if areas are completely separate */
    if ((a1->x1 > a2->x2) ||
            (a2->x1 > a1->x2) ||
            (a1->y1 > a2->y2) ||
            (a2->y1 > a1->y2)) {
        return false;
    }
    return true;
}

/**
 * @brief Join two areas into a larger area (bounding box)
 * Similar to LVGL's _lv_area_join
 * @param result Result area (bounding box of a1 and a2)
 * @param a1 First area
 * @param a2 Second area
 */
static void area_join(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
{
    result->x1 = (a1->x1 < a2->x1) ? a1->x1 : a2->x1;
    result->y1 = (a1->y1 < a2->y1) ? a1->y1 : a2->y1;
    result->x2 = (a1->x2 > a2->x2) ? a1->x2 : a2->x2;
    result->y2 = (a1->y2 > a2->y2) ? a1->y2 : a2->y2;
}

/**
 * @brief Join overlapping/adjacent dirty areas to minimize redraw regions
 * Similar to LVGL's lv_refr_join_area
 * This function merges dirty areas that overlap or touch if the resulting
 * area is smaller than the sum of the two separate areas.
 * @param ctx Graphics context containing dirty areas
 */
static void gfx_refr_join_areas(gfx_core_context_t *ctx)
{
    uint32_t join_from;
    uint32_t join_in;
    gfx_area_t joined_area;

    /* Clear joined flags */
    memset(ctx->disp.inv_area_joined, 0, sizeof(ctx->disp.inv_area_joined));

    for (join_in = 0; join_in < ctx->disp.inv_p; join_in++) {
        if (ctx->disp.inv_area_joined[join_in] != 0) {
            continue;
        }

        /* Check all areas to join them into 'join_in' */
        for (join_from = 0; join_from < ctx->disp.inv_p; join_from++) {
            /* Handle only unjoined areas and ignore itself */
            if (ctx->disp.inv_area_joined[join_from] != 0 || join_in == join_from) {
                continue;
            }

            /* Check if the areas are on each other (overlap or adjacent) */
            if (!area_is_on(&ctx->disp.inv_areas[join_in], &ctx->disp.inv_areas[join_from])) {
                continue;
            }

            /* Create joined area */
            area_join(&joined_area, &ctx->disp.inv_areas[join_in], &ctx->disp.inv_areas[join_from]);

            /* Join two areas only if the joined area size is smaller than the sum
             * This prevents unnecessary joining of areas that would waste rendering */
            uint32_t joined_size = area_get_size(&joined_area);
            uint32_t separate_size = area_get_size(&ctx->disp.inv_areas[join_in]) +
                                     area_get_size(&ctx->disp.inv_areas[join_from]);

            if (joined_size < separate_size) {
                area_copy(&ctx->disp.inv_areas[join_in], &joined_area);

                /* Mark 'join_from' as joined into 'join_in' */
                ctx->disp.inv_area_joined[join_from] = 1;

                ESP_LOGD(TAG, "Joined area [%d] into [%d], saved %lu pixels",
                         join_from, join_in, separate_size - joined_size);
            }
        }
    }
}

void gfx_inv_area(gfx_handle_t handle, const gfx_area_t *area_p)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is NULL");
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;

    if (area_p == NULL) {
        ctx->disp.inv_p = 0;
        memset(ctx->disp.inv_area_joined, 0, sizeof(ctx->disp.inv_area_joined));
        ESP_LOGD(TAG, "Cleared all invalid areas");
        return;
    }

    if (ctx->disp.rendering_in_progress) {
        ESP_LOGE(TAG, "Detected modifying dirty areas in render");
        return;
    }

    gfx_area_t scr_area;
    scr_area.x1 = 0;
    scr_area.y1 = 0;
    scr_area.x2 = ctx->display.h_res - 1;
    scr_area.y2 = ctx->display.v_res - 1;

    /* Intersect with screen bounds */
    gfx_area_t com_area;
    bool suc = area_intersect(&com_area, area_p, &scr_area);
    if (!suc) {
        ESP_LOGD(TAG, "Area out of screen bounds");
        return;  /* Out of the screen */
    }

    /* Check if this area is already covered by existing invalid areas */
    for (uint8_t i = 0; i < ctx->disp.inv_p; i++) {
        if (area_is_in(&com_area, &ctx->disp.inv_areas[i])) {
            ESP_LOGD(TAG, "Area already covered by existing invalid area %d", i);
            return;
        }
    }

    /* Add as new invalid area if there's space */
    if (ctx->disp.inv_p < GFX_INV_BUF_SIZE) {
        area_copy(&ctx->disp.inv_areas[ctx->disp.inv_p], &com_area);
        ctx->disp.inv_p++;
        ESP_LOGI(TAG, "Added invalid area [%d,%d,%d,%d], total: %d",
                 com_area.x1, com_area.y1, com_area.x2, com_area.y2, ctx->disp.inv_p);
    } else {
        /* No space left, mark entire screen as invalid */
        ctx->disp.inv_p = 1;
        area_copy(&ctx->disp.inv_areas[0], &scr_area);
        ESP_LOGW(TAG, "Invalid area buffer full, marking entire screen as dirty");
    }
}

void gfx_obj_invalidate(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->parent_handle == NULL) {
        ESP_LOGE(TAG, "Object has no parent handle");
        return;
    }

    obj->is_dirty = true;

    gfx_area_t obj_area;
    obj_area.x1 = obj->x;
    obj_area.y1 = obj->y;
    obj_area.x2 = obj->x + obj->width - 1;
    obj_area.y2 = obj->y + obj->height - 1;

    gfx_inv_area(obj->parent_handle, &obj_area);

    // ESP_LOGI(TAG, "[%p] type:%d, Invalidated object area [%d,%d,%d,%d]",
    //          obj, obj->type, obj_area.x1, obj_area.y1, obj_area.x2, obj_area.y2);
}

bool gfx_is_invalidation_enabled(gfx_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }
    /* For now, always enabled. Can add a flag later if needed */
    return true;
}
