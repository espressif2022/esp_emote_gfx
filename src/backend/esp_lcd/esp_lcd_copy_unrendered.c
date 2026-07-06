/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "core/display/gfx_display_priv.h"
#include "backend/esp_lcd/esp_lcd_copy_unrendered.h"
#include "platform/esp_idf/esp_idf_async_fbcpy.h"

typedef struct {
    gfx_coord_t x1;
    gfx_coord_t y1;
    gfx_coord_t x2;
    gfx_coord_t y2;
} gfx_copy_rect_t;

#define GFX_COPY_RECT_MAX  (GFX_DISP_INV_BUF_SIZE * 4U + 4U)

static gfx_coord_t gfx_copy_min_coord(gfx_coord_t a, gfx_coord_t b)
{
    return a < b ? a : b;
}

static gfx_coord_t gfx_copy_max_coord(gfx_coord_t a, gfx_coord_t b)
{
    return a > b ? a : b;
}

static bool gfx_copy_rects_overlap(const gfx_copy_rect_t *a, const gfx_copy_rect_t *b)
{
    return !(a->x2 < b->x1 || a->x1 > b->x2 || a->y2 < b->y1 || a->y1 > b->y2);
}

static void gfx_copy_rects_subtract_dirty(gfx_copy_rect_t *unsync_rects, int *unsync_cnt, const gfx_copy_rect_t *dirty)
{
    int j = 0;

    while (j < *unsync_cnt) {
        gfx_copy_rect_t region = unsync_rects[j];

        if (!gfx_copy_rects_overlap(&region, dirty)) {
            j++;
            continue;
        }

        for (int k = j; k < *unsync_cnt - 1; k++) {
            unsync_rects[k] = unsync_rects[k + 1];
        }
        (*unsync_cnt)--;

        if (dirty->y1 > region.y1) {
            unsync_rects[(*unsync_cnt)++] = (gfx_copy_rect_t) {
                region.x1, region.y1, region.x2, (gfx_coord_t)(dirty->y1 - 1)
            };
        }
        if (dirty->y2 < region.y2) {
            unsync_rects[(*unsync_cnt)++] = (gfx_copy_rect_t) {
                region.x1, (gfx_coord_t)(dirty->y2 + 1), region.x2, region.y2
            };
        }

        gfx_coord_t overlap_y1 = gfx_copy_max_coord(region.y1, dirty->y1);
        gfx_coord_t overlap_y2 = gfx_copy_min_coord(region.y2, dirty->y2);

        if (dirty->x1 > region.x1) {
            unsync_rects[(*unsync_cnt)++] = (gfx_copy_rect_t) {
                region.x1, overlap_y1, (gfx_coord_t)(dirty->x1 - 1), overlap_y2
            };
        }
        if (dirty->x2 < region.x2) {
            unsync_rects[(*unsync_cnt)++] = (gfx_copy_rect_t) {
                (gfx_coord_t)(dirty->x2 + 1), overlap_y1, region.x2, overlap_y2
            };
        }
    }
}

static void gfx_copy_rects_merge(gfx_copy_rect_t *unsync_rects, int *unsync_cnt)
{
    bool merged;

    do {
        merged = false;
        for (int i = 0; i < *unsync_cnt; i++) {
            for (int j = i + 1; j < *unsync_cnt; j++) {
                gfx_copy_rect_t *a = &unsync_rects[i];
                gfx_copy_rect_t *b = &unsync_rects[j];

                if (a->y1 == b->y1 && a->y2 == b->y2 &&
                        b->x1 <= (gfx_coord_t)(a->x2 + 1) && b->x2 >= (gfx_coord_t)(a->x1 - 1)) {
                    a->x1 = gfx_copy_min_coord(a->x1, b->x1);
                    a->x2 = gfx_copy_max_coord(a->x2, b->x2);
                    for (int k = j; k < *unsync_cnt - 1; k++) {
                        unsync_rects[k] = unsync_rects[k + 1];
                    }
                    (*unsync_cnt)--;
                    merged = true;
                    goto merge_restart;
                }

                if (a->x1 == b->x1 && a->x2 == b->x2 &&
                        b->y1 <= (gfx_coord_t)(a->y2 + 1) && b->y2 >= (gfx_coord_t)(a->y1 - 1)) {
                    a->y1 = gfx_copy_min_coord(a->y1, b->y1);
                    a->y2 = gfx_copy_max_coord(a->y2, b->y2);
                    for (int k = j; k < *unsync_cnt - 1; k++) {
                        unsync_rects[k] = unsync_rects[k + 1];
                    }
                    (*unsync_cnt)--;
                    merged = true;
                    goto merge_restart;
                }
            }
        }
merge_restart:
        ;
    } while (merged);
}

void gfx_copy_unrendered_areas(const gfx_display_t *disp,
                               const void *src_fb,
                               void *dst_fb,
                               uint32_t hor_res,
                               uint32_t ver_res,
                               size_t pixel_size)
{
    gfx_copy_rect_t unsync_rects[GFX_COPY_RECT_MAX];
    int unsync_cnt = 1;

    if (disp == NULL || src_fb == NULL || dst_fb == NULL || hor_res == 0U || ver_res == 0U || pixel_size == 0U) {
        return;
    }

    unsync_rects[0] = (gfx_copy_rect_t) {
        0,
        0,
        (gfx_coord_t)(hor_res - 1U),
        (gfx_coord_t)(ver_res - 1U),
    };

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (disp->dirty.merged[i]) {
            continue;
        }

        gfx_copy_rect_t dirty = {
            disp->dirty.areas[i].x1,
            disp->dirty.areas[i].y1,
            disp->dirty.areas[i].x2,
            disp->dirty.areas[i].y2,
        };
        gfx_copy_rects_subtract_dirty(unsync_rects, &unsync_cnt, &dirty);
    }

    if (unsync_cnt <= 0) {
        return;
    }

    gfx_copy_rects_merge(unsync_rects, &unsync_cnt);

#if defined(CONFIG_SOC_DMA2D_SUPPORTED) && CONFIG_SOC_DMA2D_SUPPORTED
    if (unsync_cnt > 0) {
        gfx_platform_async_fbcpy_cache_msync(src_fb, (size_t)hor_res * ver_res * pixel_size);
    }
#endif

    for (int idx = 0; idx < unsync_cnt; idx++) {
        gfx_copy_rect_t region = unsync_rects[idx];
        uint32_t width_px = (uint32_t)(region.x2 - region.x1 + 1);
        uint32_t height_px = (uint32_t)(region.y2 - region.y1 + 1);

        (void)gfx_platform_async_fbcpy_region(src_fb,
                                              dst_fb,
                                              hor_res,
                                              ver_res,
                                              hor_res,
                                              (uint32_t)region.x1,
                                              (uint32_t)region.y1,
                                              width_px,
                                              height_px,
                                              pixel_size);
    }
}
