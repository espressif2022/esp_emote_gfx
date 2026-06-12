/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "gfx/widgets/anim.h"
#include "codecs/anim/gfx_anim_decoder_priv.h"

#define GFX_ANIM_DECODER_MAX_COUNT 8

static const gfx_anim_decoder_t *s_decoders[GFX_ANIM_DECODER_MAX_COUNT];
static size_t s_decoder_count;
static bool s_registry_ready;

static esp_err_t gfx_anim_decoder_register_internal(const gfx_anim_decoder_t *decoder)
{
    ESP_RETURN_ON_FALSE(decoder != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder is NULL");
    ESP_RETURN_ON_FALSE(decoder->name != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder name is NULL");
    ESP_RETURN_ON_FALSE(decoder->probe != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder probe is NULL");
    ESP_RETURN_ON_FALSE(decoder->open != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder open is NULL");
    ESP_RETURN_ON_FALSE(decoder->close != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder close is NULL");
    ESP_RETURN_ON_FALSE(decoder->get_frame_count != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_count is NULL");
    ESP_RETURN_ON_FALSE(decoder->read_frame_desc != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder read_frame_desc is NULL");
    ESP_RETURN_ON_FALSE(decoder->free_frame_desc != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder free_frame_desc is NULL");
    ESP_RETURN_ON_FALSE(decoder->get_frame_payload != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_payload is NULL");
    ESP_RETURN_ON_FALSE(decoder->get_frame_payload_size != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_payload_size is NULL");
    ESP_RETURN_ON_FALSE(decoder->decode_frame_block != NULL, ESP_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder decode_frame_block is NULL");

    for (size_t i = 0; i < s_decoder_count; i++) {
        if (s_decoders[i] == decoder) {
            return ESP_OK;
        }
        if (strcmp(s_decoders[i]->name, decoder->name) == 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    ESP_RETURN_ON_FALSE(s_decoder_count < GFX_ANIM_DECODER_MAX_COUNT, ESP_ERR_NO_MEM,
                        "gfx_anim_decoder", "decoder registry full");

    s_decoders[s_decoder_count++] = decoder;
    return ESP_OK;
}

esp_err_t gfx_anim_decoder_registry_init(void)
{
    if (s_registry_ready) {
        return ESP_OK;
    }

    s_registry_ready = true;
    return gfx_anim_decoder_register_internal(gfx_anim_eaf_decoder_get());
}

const gfx_anim_decoder_t *gfx_anim_decoder_select(const gfx_anim_src_t *src)
{
    if (src == NULL) {
        return NULL;
    }

    if (gfx_anim_decoder_registry_init() != ESP_OK) {
        return NULL;
    }

    for (size_t i = 0; i < s_decoder_count; i++) {
        const gfx_anim_decoder_t *decoder = s_decoders[i];
        if (decoder->probe(src)) {
            return decoder;
        }
    }

    return NULL;
}
