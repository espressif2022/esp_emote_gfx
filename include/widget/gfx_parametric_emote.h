/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_PARAMETRIC_EMOTE_SCHEMA_VERSION 1

typedef enum {
    GFX_PARAMETRIC_PART_BEZIER_FILL = 0,
    GFX_PARAMETRIC_PART_BEZIER_STROKE,
    GFX_PARAMETRIC_PART_MESH_POLYGON,
    GFX_PARAMETRIC_PART_EMBEDDED_IMAGE,
} gfx_parametric_part_kind_t;

typedef struct {
    uint32_t version;
    int32_t design_viewbox_x;
    int32_t design_viewbox_y;
    int32_t design_viewbox_w;
    int32_t design_viewbox_h;
    int32_t export_width;
    int32_t export_height;
    float export_scale;
    int32_t export_offset_x;
    int32_t export_offset_y;
} gfx_parametric_export_meta_t;

typedef struct {
    const char *name;
    gfx_parametric_part_kind_t kind;
    uint32_t layer_mask_bit;
    bool mirrored;
    bool closed;
    int32_t anchor_x;
    int32_t anchor_y;
    int32_t segs;
    float thickness;
} gfx_parametric_part_desc_t;

typedef struct {
    const gfx_parametric_export_meta_t *meta;
    const gfx_parametric_part_desc_t *parts;
    size_t part_count;
} gfx_parametric_emote_assets_t;

#ifdef __cplusplus
}
#endif
