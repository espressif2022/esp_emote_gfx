/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Shared coverflow math for object + arena shells.
 * Selection commit timing: tween to ±spacing first, then apply target index.
 * No gfx_object_t / arena types here.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_COVERFLOW_CORE_POS_Q            1024
#define GFX_COVERFLOW_CORE_TWEEN_MS         180U
#define GFX_COVERFLOW_CORE_DRAG_THRESHOLD   6U
#define GFX_COVERFLOW_CORE_PAGE_THRESHOLD   42U
#define GFX_COVERFLOW_CORE_CENTER_ZOOM      100U
#define GFX_COVERFLOW_CORE_SIDE_ZOOM        64U
#define GFX_COVERFLOW_CORE_SPACING_PCT      34U
#define GFX_COVERFLOW_CORE_SIDE_DIM_OPA     80U
#define GFX_COVERFLOW_CORE_VISIBLE_SIDE     2
#define GFX_COVERFLOW_CORE_BASE_W_PCT       58
#define GFX_COVERFLOW_CORE_BASE_H_PCT       76

typedef struct {
    uint16_t center_zoom;
    uint16_t side_zoom;
    uint16_t spacing_pct;
    uint8_t  side_dim_opa;
} gfx_coverflow_core_effect_t;

typedef struct {
    int32_t  pos_q;
    uint16_t zoom;
    uint8_t  dim_opa;
    bool     centerish;
} gfx_coverflow_core_effect_state_t;

typedef struct {
    int32_t target_index;
    int32_t start_offset;
    int32_t end_offset;
    bool    commit_pending;
} gfx_coverflow_core_tween_plan_t;

typedef struct {
    int32_t  index;
    int32_t  rel_slot;
    int32_t  pos_q;
    uint16_t zoom;
    uint8_t  dim_opa;
    bool     centerish;
    int32_t  x;
    int32_t  y;
    int32_t  w;
    int32_t  h;
} gfx_coverflow_core_card_t;

int32_t gfx_coverflow_core_clamp_i32(int32_t value, int32_t min_value, int32_t max_value);

int32_t gfx_coverflow_core_clamp_index(uint16_t item_count, int32_t index);

int32_t gfx_coverflow_core_spacing(int32_t width, uint16_t spacing_pct, int32_t fallback);

void gfx_coverflow_core_effect_calc(const gfx_coverflow_core_effect_t *effect,
                                    int32_t rel_slot, int32_t drag_offset, int32_t spacing,
                                    gfx_coverflow_core_effect_state_t *out);

/** RELEASE page decision: target index from drag vs page_threshold. */
int32_t gfx_coverflow_core_page_target(uint16_t item_count, int32_t selected,
                                       int32_t drag_offset, uint16_t page_threshold);

/**
 * Plan commit/cancel tween (gfx_coverflow_start_tween math).
 * commit_pending ⇒ end_offset = ±spacing; else end_offset = 0.
 */
void gfx_coverflow_core_plan_tween(uint16_t item_count, int32_t selected, int32_t target_index,
                                   int32_t spacing, int32_t start_offset,
                                   gfx_coverflow_core_tween_plan_t *out);

/** Fill card geometry for one slot; returns false if index OOB. */
bool gfx_coverflow_core_card_state(uint16_t item_count, int32_t selected, int32_t rel_slot,
                                   int32_t drag_offset, int32_t widget_x, int32_t widget_y,
                                   int32_t widget_w, int32_t widget_h,
                                   const gfx_coverflow_core_effect_t *effect,
                                   gfx_coverflow_core_card_t *out);

/** Collect selected±VISIBLE_SIDE cards; optional sort by ascending zoom. */
uint8_t gfx_coverflow_core_collect_cards(uint16_t item_count, int32_t selected,
                                         int32_t drag_offset, int32_t widget_x, int32_t widget_y,
                                         int32_t widget_w, int32_t widget_h,
                                         const gfx_coverflow_core_effect_t *effect,
                                         gfx_coverflow_core_card_t *cards, uint8_t max_cards,
                                         bool sort_by_zoom);

int32_t gfx_coverflow_core_ease_out_quad_i32(int32_t from, int32_t to,
                                             uint32_t elapsed_ms, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
