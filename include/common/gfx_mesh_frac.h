/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

/**
 * Fractional bits for mesh vertex coordinates (gfx_mesh_img_point_q8_t x_q8/y_q8)
 * and for the software triangle rasterizer.
 */
#define GFX_MESH_FRAC_SHIFT 8

#define GFX_MESH_FRAC_ONE  (1 << GFX_MESH_FRAC_SHIFT)
#define GFX_MESH_FRAC_HALF (1 << (GFX_MESH_FRAC_SHIFT - 1))
#define GFX_MESH_FRAC_MASK (GFX_MESH_FRAC_ONE - 1)

/**
 * Triangle outer-edge AA distance threshold, same units as vertex coordinates.
 * Kconfig 0 means one logical pixel (GFX_MESH_FRAC_ONE).
 */
#if defined(CONFIG_GFX_BLEND_TRI_EDGE_AA_RANGE) && (CONFIG_GFX_BLEND_TRI_EDGE_AA_RANGE > 0)
#define GFX_BLEND_TRI_EDGE_AA_RANGE (CONFIG_GFX_BLEND_TRI_EDGE_AA_RANGE)
#else
#define GFX_BLEND_TRI_EDGE_AA_RANGE (GFX_MESH_FRAC_ONE)
#endif
