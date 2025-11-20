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

#include "core/gfx_obj_priv.h"
#include "core/gfx_refr_priv.h"
#include "core/gfx_render_priv.h"
#include "core/gfx_timer_priv.h"

#include "widget/gfx_font_priv.h"
#include "decoder/gfx_img_dec_priv.h"

static const char *TAG = "gfx_core";

static void gfx_core_task(void *arg);
static bool gfx_event_handler(gfx_core_context_t *ctx);
static uint32_t gfx_cal_task_delay(uint32_t timer_delay);
static esp_err_t gfx_buf_init_frame(gfx_core_context_t *ctx, const gfx_core_config_t *cfg);
static esp_err_t gfx_buf_free_frame(gfx_core_context_t *ctx);

/* ============================================================================
 * Initialization and Cleanup Functions
 * ============================================================================ */

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
    gfx_timer_mgr_init(&disp_ctx->timer.timer_mgr, cfg->fps);

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
        ESP_LOGE(TAG, "Failed to initialize image decoder");
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
        xTaskCreateWithCaps(gfx_core_task, "gfx_core", cfg->task.task_stack,
                            disp_ctx, cfg->task.task_priority, NULL, stack_caps);
    } else {
        xTaskCreatePinnedToCoreWithCaps(gfx_core_task, "gfx_core", cfg->task.task_stack,
                                        disp_ctx, cfg->task.task_priority, NULL, cfg->task.task_affinity, stack_caps);
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
    gfx_timer_mgr_deinit(&ctx->timer.timer_mgr);

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

/* ============================================================================
 * Buffer Management Functions
 * ============================================================================ */

static esp_err_t gfx_buf_init_frame(gfx_core_context_t *ctx, const gfx_core_config_t *cfg)
{
    if (cfg->buffers.buf1 != NULL) {
        ctx->disp.buf1 = (uint16_t *)cfg->buffers.buf1;
        ctx->disp.buf2 = (uint16_t *)cfg->buffers.buf2;

        if (cfg->buffers.buf_pixels > 0) {
            ctx->disp.buf_pixels = cfg->buffers.buf_pixels;
        } else {
            ESP_LOGW(TAG, "buf_pixels=0, use default");
            ctx->disp.buf_pixels = ctx->display.h_res * ctx->display.v_res;
        }

        ctx->disp.ext_bufs = true;
    } else {
        // Allocate internal buffers
        uint32_t buff_caps = 0;
#if SOC_PSRAM_DMA_CAPABLE == 0
        if (cfg->flags.buff_dma && cfg->flags.buff_spiram) {
            ESP_LOGW(TAG, "DMA+SPIRAM not supported");
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

    ctx->disp.buf_act = ctx->disp.buf1;
    ctx->disp.bg_color.full = 0x0000;
    return ESP_OK;
}

static esp_err_t gfx_buf_free_frame(gfx_core_context_t *ctx)
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
    }
    ctx->disp.buf_pixels = 0;
    ctx->disp.ext_bufs = false;
    return ESP_OK;
}

/* ============================================================================
 * Task and Event Handling Functions
 * ============================================================================ */

static uint32_t gfx_cal_task_delay(uint32_t timer_delay)
{
    uint32_t min_delay_ms = (1000 / configTICK_RATE_HZ) + 1; // At least one tick + 1ms

    if (timer_delay == ANIM_NO_TIMER_READY) {
        return (min_delay_ms > 5) ? min_delay_ms : 5;
    } else {
        return (timer_delay < min_delay_ms) ? min_delay_ms : timer_delay;
    }
}

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

static void gfx_core_task(void *arg)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)arg;
    uint32_t timer_delay = 1; // Default delay

    gfx_emote_refresh_all((gfx_handle_t)ctx);

    while (1) {
        if (ctx->sync.lock_mutex && xSemaphoreTakeRecursive(ctx->sync.lock_mutex, portMAX_DELAY) == pdTRUE) {
            if (gfx_event_handler(ctx)) {
                xSemaphoreGiveRecursive(ctx->sync.lock_mutex);
                break;
            }

            timer_delay = gfx_timer_handler(&ctx->timer.timer_mgr);

            if (ctx->disp.child_list != NULL) {
                gfx_render_handler(ctx);
            }

            uint32_t task_delay = gfx_cal_task_delay(timer_delay);

            xSemaphoreGiveRecursive(ctx->sync.lock_mutex);
            vTaskDelay(pdMS_TO_TICKS(task_delay));
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex, retrying...");
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

/* ============================================================================
 * Synchronization and Locking Functions
 * ============================================================================ */

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

/* ============================================================================
 * Child Object Management Functions
 * ============================================================================ */

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

    if (ctx->disp.child_list == NULL) {
        ctx->disp.child_list = new_child;
    } else {
        gfx_core_child_t *current = ctx->disp.child_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_child;
    }

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
            return ESP_OK;
        }
        prev = current;
        current = current->next;
    }

    return ESP_ERR_NOT_FOUND;
}

/* ============================================================================
 * Display and Refresh Functions
 * ============================================================================ */

void gfx_emote_refresh_all(gfx_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is NULL");
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_area_t full_screen;
    full_screen.x1 = 0;
    full_screen.y1 = 0;
    full_screen.x2 = ctx->display.h_res - 1;
    full_screen.y2 = ctx->display.v_res - 1;
    gfx_invalidate_area(handle, &full_screen);
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

/* ============================================================================
 * Configuration and Status Functions
 * ============================================================================ */

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
    if (width == NULL || height == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || ctx->display.h_res == 0 || ctx->display.v_res == 0) {
        /* Use default screen dimensions if context is invalid */
        *width = DEFAULT_SCREEN_WIDTH;
        *height = DEFAULT_SCREEN_HEIGHT;
        if (ctx == NULL) {
            ESP_LOGW(TAG, "Invalid graphics context, using default screen size");
        }
        return ESP_OK;
    }

    *width = ctx->display.h_res;
    *height = ctx->display.v_res;

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
    ESP_LOGD(TAG, "BG color: 0x%04X", color.full);
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
