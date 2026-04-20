/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "widget/gfx_anim.h"
#include "widget/anim/gfx_anim_decoder_priv.h"

#define GFX_ANIM_DECODER_MAX_COUNT 8

static const gfx_anim_decoder_ops_t *s_anim_decoders[GFX_ANIM_DECODER_MAX_COUNT];
static size_t s_anim_decoder_count;
static bool s_anim_decoder_registry_ready;

static esp_err_t gfx_anim_decoder_register_internal(const gfx_anim_decoder_ops_t *ops)
{
    ESP_RETURN_ON_FALSE(ops != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder ops is NULL");
    ESP_RETURN_ON_FALSE(ops->name != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder name is NULL");
    ESP_RETURN_ON_FALSE(ops->open != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder open is NULL");
    ESP_RETURN_ON_FALSE(ops->close != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder close is NULL");
    ESP_RETURN_ON_FALSE(ops->get_total_frames != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_total_frames is NULL");
    ESP_RETURN_ON_FALSE(ops->get_frame_info != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_info is NULL");
    ESP_RETURN_ON_FALSE(ops->free_frame_info != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder free_frame_info is NULL");
    ESP_RETURN_ON_FALSE(ops->get_frame_data != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_data is NULL");
    ESP_RETURN_ON_FALSE(ops->get_frame_size != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_size is NULL");
    ESP_RETURN_ON_FALSE(ops->decode_block != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder decode_block is NULL");

    for (size_t i = 0; i < s_anim_decoder_count; i++) {
        if (s_anim_decoders[i] == ops) {
            return ESP_OK;
        }
        if (strcmp(s_anim_decoders[i]->name, ops->name) == 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    ESP_RETURN_ON_FALSE(s_anim_decoder_count < GFX_ANIM_DECODER_MAX_COUNT, ESP_ERR_NO_MEM,
                        "gfx_anim_decoder", "decoder registry full");

    s_anim_decoders[s_anim_decoder_count++] = ops;
    return ESP_OK;
}

esp_err_t gfx_anim_decoder_registry_init(void)
{
    if (s_anim_decoder_registry_ready) {
        return ESP_OK;
    }

    s_anim_decoder_registry_ready = true;
    return gfx_anim_decoder_register_internal(gfx_anim_decoder_get_eaf());
}

const gfx_anim_decoder_ops_t *gfx_anim_decoder_find_for_source(const gfx_anim_src_t *src_desc)
{
    if (src_desc == NULL) {
        return NULL;
    }

    if (gfx_anim_decoder_registry_init() != ESP_OK) {
        return NULL;
    }

    for (size_t i = 0; i < s_anim_decoder_count; i++) {
        const gfx_anim_decoder_ops_t *ops = s_anim_decoders[i];
        if (ops->can_open != NULL && ops->can_open(src_desc)) {
            return ops;
        }
    }

    return NULL;
}
