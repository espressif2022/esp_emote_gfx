/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "common/gfx_check.h"
#include "gfx/widgets/anim.h"
#include "codecs/anim/gfx_anim_decoder_priv.h"

#define GFX_ANIM_DECODER_MAX_COUNT 8

static const gfx_anim_decoder_t *s_decoders[GFX_ANIM_DECODER_MAX_COUNT];
static size_t s_decoder_count;
static bool s_registry_ready;

static gfx_err_t gfx_anim_decoder_register_internal(const gfx_anim_decoder_t *decoder)
{
    GFX_RETURN_ON_FALSE(decoder != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder is NULL");
    GFX_RETURN_ON_FALSE(decoder->name != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder name is NULL");
    GFX_RETURN_ON_FALSE(decoder->probe != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder probe is NULL");
    GFX_RETURN_ON_FALSE(decoder->open != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder open is NULL");
    GFX_RETURN_ON_FALSE(decoder->close != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder close is NULL");
    GFX_RETURN_ON_FALSE(decoder->get_info != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_info is NULL");
    GFX_RETURN_ON_FALSE(decoder->get_frame_count != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_count is NULL");
    GFX_RETURN_ON_FALSE(decoder->read_frame_desc != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder read_frame_desc is NULL");
    GFX_RETURN_ON_FALSE(decoder->free_frame_desc != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder free_frame_desc is NULL");
    GFX_RETURN_ON_FALSE(decoder->get_frame_payload != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_payload is NULL");
    GFX_RETURN_ON_FALSE(decoder->get_frame_payload_size != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder get_frame_payload_size is NULL");
    GFX_RETURN_ON_FALSE(decoder->decode_frame_block != NULL, GFX_ERR_INVALID_ARG, "gfx_anim_decoder", "decoder decode_frame_block is NULL");

    for (size_t i = 0; i < s_decoder_count; i++) {
        if (s_decoders[i] == decoder) {
            return GFX_OK;
        }
        if (strcmp(s_decoders[i]->name, decoder->name) == 0) {
            return GFX_ERR_INVALID_STATE;
        }
    }

    GFX_RETURN_ON_FALSE(s_decoder_count < GFX_ANIM_DECODER_MAX_COUNT, GFX_ERR_NO_MEM,
                        "gfx_anim_decoder", "decoder registry full");

    s_decoders[s_decoder_count++] = decoder;
    return GFX_OK;
}

gfx_err_t gfx_anim_decoder_registry_init(void)
{
    if (s_registry_ready) {
        return GFX_OK;
    }

    s_registry_ready = true;
    return gfx_anim_decoder_register_internal(gfx_anim_eaf_decoder_get());
}

const gfx_anim_decoder_t *gfx_anim_decoder_select(const gfx_anim_src_t *src)
{
    if (src == NULL) {
        return NULL;
    }

    if (gfx_anim_decoder_registry_init() != GFX_OK) {
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
