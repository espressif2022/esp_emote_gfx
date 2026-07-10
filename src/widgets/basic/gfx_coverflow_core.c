/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/widgets/coverflow_core.h"

#include <stddef.h>

#include "gfx/ease.h"

static int32_t abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static uint8_t dim_from_zoom(const gfx_coverflow_core_effect_t *effect, uint16_t zoom)
{
    if (effect == NULL) {
        return 0;
    }
    if (effect->center_zoom <= effect->side_zoom || zoom >= effect->center_zoom) {
        return 0;
    }
    if (zoom <= effect->side_zoom) {
        return effect->side_dim_opa;
    }
    return (uint8_t)(((uint32_t)(effect->center_zoom - zoom) * effect->side_dim_opa) /
                     (uint32_t)(effect->center_zoom - effect->side_zoom));
}

int32_t gfx_coverflow_core_clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

int32_t gfx_coverflow_core_clamp_index(uint16_t item_count, int32_t index)
{
    if (item_count == 0U) {
        return 0;
    }
    if (index < 0) {
        return 0;
    }
    if (index >= (int32_t)item_count) {
        return (int32_t)item_count - 1;
    }
    return index;
}

int32_t gfx_coverflow_core_spacing(int32_t width, uint16_t spacing_pct, int32_t fallback)
{
    int32_t spacing;

    if (width <= 0 || spacing_pct == 0U) {
        return fallback > 0 ? fallback : 1;
    }
    spacing = (width * (int32_t)spacing_pct) / 100;
    if (spacing <= 0) {
        return fallback > 0 ? fallback : 1;
    }
    return spacing;
}

void gfx_coverflow_core_effect_calc(const gfx_coverflow_core_effect_t *effect,
                                    int32_t rel_slot, int32_t drag_offset, int32_t spacing,
                                    gfx_coverflow_core_effect_state_t *out)
{
    int32_t pos_q;
    int32_t progress_q;
    int32_t abs_pos_q;
    int32_t zoom_delta;
    int32_t zoom;

    if (effect == NULL || out == NULL) {
        return;
    }

    pos_q = rel_slot * GFX_COVERFLOW_CORE_POS_Q;
    progress_q = spacing > 0 ? (drag_offset * GFX_COVERFLOW_CORE_POS_Q) / spacing : 0;
    pos_q += progress_q;
    abs_pos_q = abs_i32(pos_q);
    if (abs_pos_q > GFX_COVERFLOW_CORE_POS_Q) {
        abs_pos_q = GFX_COVERFLOW_CORE_POS_Q;
    }

    zoom_delta = (int32_t)effect->center_zoom - (int32_t)effect->side_zoom;
    zoom = (int32_t)effect->center_zoom -
           (zoom_delta * abs_pos_q + GFX_COVERFLOW_CORE_POS_Q / 2) / GFX_COVERFLOW_CORE_POS_Q;
    zoom = gfx_coverflow_core_clamp_i32(zoom, (int32_t)effect->side_zoom, (int32_t)effect->center_zoom);

    out->pos_q = pos_q;
    out->zoom = (uint16_t)zoom;
    out->dim_opa = dim_from_zoom(effect, (uint16_t)zoom);
    out->centerish = abs_pos_q < (GFX_COVERFLOW_CORE_POS_Q / 2);
}

int32_t gfx_coverflow_core_page_target(uint16_t item_count, int32_t selected,
                                       int32_t drag_offset, uint16_t page_threshold)
{
    int32_t target = gfx_coverflow_core_clamp_index(item_count, selected);

    if (item_count == 0U) {
        return 0;
    }
    if (drag_offset <= -(int32_t)page_threshold) {
        target = selected + 1;
    } else if (drag_offset >= (int32_t)page_threshold) {
        target = selected - 1;
    }
    return gfx_coverflow_core_clamp_index(item_count, target);
}

void gfx_coverflow_core_plan_tween(uint16_t item_count, int32_t selected, int32_t target_index,
                                   int32_t spacing, int32_t start_offset,
                                   gfx_coverflow_core_tween_plan_t *out)
{
    if (out == NULL) {
        return;
    }

    selected = gfx_coverflow_core_clamp_index(item_count, selected);
    target_index = gfx_coverflow_core_clamp_index(item_count, target_index);
    if (spacing <= 0) {
        spacing = 1;
    }

    out->target_index = target_index;
    out->commit_pending = (target_index != selected);
    out->start_offset = start_offset;
    out->end_offset = 0;

    if (out->commit_pending) {
        out->end_offset = (target_index > selected) ? -spacing : spacing;
        if (out->start_offset == out->end_offset) {
            out->start_offset += (out->end_offset < 0) ? 1 : -1;
        }
    }
}

bool gfx_coverflow_core_card_state(uint16_t item_count, int32_t selected, int32_t rel_slot,
                                   int32_t drag_offset, int32_t widget_x, int32_t widget_y,
                                   int32_t widget_w, int32_t widget_h,
                                   const gfx_coverflow_core_effect_t *effect,
                                   gfx_coverflow_core_card_t *out)
{
    gfx_coverflow_core_effect_t def;
    gfx_coverflow_core_effect_state_t st = {0};
    int32_t spacing;
    int32_t base_w;
    int32_t base_h;
    int32_t card_w;
    int32_t card_h;
    int32_t card_cx;
    int32_t index;

    if (out == NULL || item_count == 0U || widget_w <= 0 || widget_h <= 0) {
        return false;
    }

    index = selected + rel_slot;
    if (index < 0 || index >= (int32_t)item_count) {
        return false;
    }

    if (effect == NULL) {
        def.center_zoom = GFX_COVERFLOW_CORE_CENTER_ZOOM;
        def.side_zoom = GFX_COVERFLOW_CORE_SIDE_ZOOM;
        def.spacing_pct = GFX_COVERFLOW_CORE_SPACING_PCT;
        def.side_dim_opa = GFX_COVERFLOW_CORE_SIDE_DIM_OPA;
        effect = &def;
    }

    spacing = gfx_coverflow_core_spacing(widget_w, effect->spacing_pct,
                                         GFX_COVERFLOW_CORE_PAGE_THRESHOLD);
    gfx_coverflow_core_effect_calc(effect, rel_slot, drag_offset, spacing, &st);

    base_w = (widget_w * GFX_COVERFLOW_CORE_BASE_W_PCT) / 100;
    base_h = (widget_h * GFX_COVERFLOW_CORE_BASE_H_PCT) / 100;
    card_w = (base_w * (int32_t)st.zoom) / 100;
    card_h = (base_h * (int32_t)st.zoom) / 100;
    if (card_w < 1) {
        card_w = 1;
    }
    if (card_h < 1) {
        card_h = 1;
    }
    card_cx = widget_x + widget_w / 2 + (st.pos_q * spacing) / GFX_COVERFLOW_CORE_POS_Q;

    out->index = index;
    out->rel_slot = rel_slot;
    out->pos_q = st.pos_q;
    out->zoom = st.zoom;
    out->dim_opa = st.dim_opa;
    out->centerish = st.centerish;
    out->w = card_w;
    out->h = card_h;
    out->x = card_cx - card_w / 2;
    out->y = widget_y + widget_h / 2 - card_h / 2;
    return true;
}

uint8_t gfx_coverflow_core_collect_cards(uint16_t item_count, int32_t selected,
                                         int32_t drag_offset, int32_t widget_x, int32_t widget_y,
                                         int32_t widget_w, int32_t widget_h,
                                         const gfx_coverflow_core_effect_t *effect,
                                         gfx_coverflow_core_card_t *cards, uint8_t max_cards,
                                         bool sort_by_zoom)
{
    uint8_t count = 0;

    if (cards == NULL || max_cards == 0U || item_count == 0U) {
        return 0;
    }

    for (int32_t slot = -GFX_COVERFLOW_CORE_VISIBLE_SIDE; slot <= GFX_COVERFLOW_CORE_VISIBLE_SIDE;
            slot++) {
        if (count >= max_cards) {
            break;
        }
        if (gfx_coverflow_core_card_state(item_count, selected, slot, drag_offset,
                                          widget_x, widget_y, widget_w, widget_h,
                                          effect, &cards[count])) {
            count++;
        }
    }

    if (sort_by_zoom) {
        for (uint8_t i = 0; i < count; i++) {
            for (uint8_t j = (uint8_t)(i + 1U); j < count; j++) {
                if (cards[i].zoom > cards[j].zoom) {
                    gfx_coverflow_core_card_t tmp = cards[i];
                    cards[i] = cards[j];
                    cards[j] = tmp;
                }
            }
        }
    }
    return count;
}

int32_t gfx_coverflow_core_ease_out_quad_i32(int32_t from, int32_t to,
                                             uint32_t elapsed_ms, uint32_t duration_ms)
{
    return gfx_ease_out_quad_i32(from, to, elapsed_ms, duration_ms);
}
