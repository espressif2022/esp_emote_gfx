/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core/gfx_area.h"

void gfx_area_copy(gfx_area_t *dest, const gfx_area_t *src)
{
    dest->x1 = src->x1;
    dest->y1 = src->y1;
    dest->x2 = src->x2;
    dest->y2 = src->y2;
}

bool gfx_area_is_in(const gfx_area_t *area_in, const gfx_area_t *area_parent)
{
    if (area_in->x1 >= area_parent->x1 &&
            area_in->y1 >= area_parent->y1 &&
            area_in->x2 <= area_parent->x2 &&
            area_in->y2 <= area_parent->y2) {
        return true;
    }
    return false;
}

bool gfx_area_intersect(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
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

uint32_t gfx_area_get_size(const gfx_area_t *area)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    return width * height;
}

bool gfx_area_is_on(const gfx_area_t *a1, const gfx_area_t *a2)
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

void gfx_area_join(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
{
    result->x1 = (a1->x1 < a2->x1) ? a1->x1 : a2->x1;
    result->y1 = (a1->y1 < a2->y1) ? a1->y1 : a2->y1;
    result->x2 = (a1->x2 > a2->x2) ? a1->x2 : a2->x2;
    result->y2 = (a1->y2 > a2->y2) ? a1->y2 : a2->y2;
}
