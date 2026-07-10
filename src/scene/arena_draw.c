/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena_draw.h"

#include <stdlib.h>
#include <string.h>

#include "common/gfx_types_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "fonts/gfx_font_priv.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/types.h"
#include "gfx/widgets/coverflow_core.h"
#include "gfx/widgets/pageflow_core.h"
#include "gfx/widgets/progress_core.h"
#include "gfx/widgets/wheel_core.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"

#define GFX_ARENA_BUTTON_RADIUS 6
#define GFX_ARENA_BUTTON_TEXT_RGB 0xFFFFFFu
#define GFX_ARENA_LABEL_DEFAULT_RGB 0xF3F7FAu

static void gfx_arena_blit_rt_image(gfx_display_t *disp, const gfx_arena_img_rt_t *rt,
                                int abs_x, int abs_y, int box_w, int box_h,
                                const gfx_render_surface_t *surf, bool scale_to_box);

static uint16_t rgb888_to_rgb565(uint32_t rgb)
{
    const uint32_t r = (rgb >> 16) & 0xFFu;
    const uint32_t g = (rgb >> 8) & 0xFFu;
    const uint32_t b = rgb & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static uint32_t rgb_scale(uint32_t rgb, uint8_t pct)
{
    uint32_t r = ((rgb >> 16) & 0xFFu) * pct / 100u;
    uint32_t g = ((rgb >> 8) & 0xFFu) * pct / 100u;
    uint32_t b = (rgb & 0xFFu) * pct / 100u;
    return (r << 16) | (g << 8) | b;
}

static uint32_t rgb_lighten(uint32_t rgb, uint8_t pct)
{
    uint32_t r = (rgb >> 16) & 0xFFu;
    uint32_t g = (rgb >> 8) & 0xFFu;
    uint32_t b = rgb & 0xFFu;
    r += (255u - r) * pct / 100u;
    g += (255u - g) * pct / 100u;
    b += (255u - b) * pct / 100u;
    return (r << 16) | (g << 8) | b;
}

static void fill_rect_fb(gfx_arena_fb_t *fb, int x, int y, int w, int h, uint16_t color)
{
    if (fb == NULL || fb->pixels == NULL || w <= 0 || h <= 0) {
        return;
    }

    int x1 = x;
    int y1 = y;
    int x2 = x + w;
    int y2 = y + h;

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 > (int)fb->width) {
        x2 = (int)fb->width;
    }
    if (y2 > (int)fb->height) {
        y2 = (int)fb->height;
    }
    if (x1 >= x2 || y1 >= y2) {
        return;
    }

    for (int row = y1; row < y2; row++) {
        uint16_t *dst = fb->pixels + (size_t)row * fb->width + (size_t)x1;
        for (int col = x1; col < x2; col++) {
            *dst++ = color;
        }
    }
}

static void draw_node_fb(gfx_arena_t *a, uint32_t node_off, int origin_x, int origin_y, gfx_arena_fb_t *fb)
{
    for (uint32_t off = node_off; off != GFX_ARENA_NO_NODE; ) {
        gfx_arena_node_t *n = gfx_arena_node(a, off);
        if (n == NULL) {
            break;
        }

        const int abs_x = origin_x + (int)n->x;
        const int abs_y = origin_y + (int)n->y;

        if ((n->flags & GFX_ARENA_F_VISIBLE) != 0) {
            if ((n->type == GFX_ARENA_NODE_CONTAINER || n->type == GFX_ARENA_NODE_BUTTON) &&
                    (n->flags & GFX_ARENA_F_BG) != 0) {
                uint32_t rgb = n->bg_rgb;
                if ((n->flags & GFX_ARENA_F_PRESSED) != 0) {
                    rgb = ((rgb >> 1) & 0x7F7F7Fu);
                }
                fill_rect_fb(fb, abs_x, abs_y, (int)n->w, (int)n->h, rgb888_to_rgb565(rgb));
            }
            /* LABEL skipped on bare FB path (no font). */

            if (n->first_child != GFX_ARENA_NO_NODE) {
                draw_node_fb(a, n->first_child, abs_x, abs_y, fb);
            }
        }

        off = n->next_sibling;
    }
}

int gfx_arena_draw(const gfx_arena_t *arena, gfx_arena_fb_t *fb)
{
    if (arena == NULL || arena->base == NULL || fb == NULL || fb->pixels == NULL ||
            fb->width == 0 || fb->height == 0) {
        return -1;
    }

    const size_t px = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < px; i++) {
        fb->pixels[i] = fb->clear_rgb565;
    }

    gfx_arena_t *a = (gfx_arena_t *)arena;
    const gfx_arena_hdr_t *hdr = gfx_arena_hdr(a);
    if (hdr->root_off == GFX_ARENA_NO_NODE) {
        return 0;
    }

    draw_node_fb(a, hdr->root_off, 0, 0, fb);
    return 0;
}

static int gfx_arena_utf8_to_unicode(const char **p, uint32_t *unicode)
{
    const uint8_t *ptr = (const uint8_t *)*p;
    uint8_t c = *ptr;
    if (c < 0x80u) {
        *unicode = c;
        *p += 1;
        return 1;
    }
    if ((c & 0xE0u) == 0xC0u && ptr[1]) {
        *unicode = ((uint32_t)(c & 0x1Fu) << 6) | (ptr[1] & 0x3Fu);
        *p += 2;
        return 2;
    }
    if ((c & 0xF0u) == 0xE0u && ptr[1] && ptr[2]) {
        *unicode = ((uint32_t)(c & 0x0Fu) << 12) |
                   ((uint32_t)(ptr[1] & 0x3Fu) << 6) | (ptr[2] & 0x3Fu);
        *p += 3;
        return 3;
    }
    if ((c & 0xF8u) == 0xF0u && ptr[1] && ptr[2] && ptr[3]) {
        *unicode = ((uint32_t)(c & 0x07u) << 18) |
                   ((uint32_t)(ptr[1] & 0x3Fu) << 12) |
                   ((uint32_t)(ptr[2] & 0x3Fu) << 6) | (ptr[3] & 0x3Fu);
        *p += 4;
        return 4;
    }
    *p += 1;
    return 0;
}

static int gfx_arena_text_width(gfx_font_handle_t font, const char *text)
{
    if (font == NULL || text == NULL || font->get_glyph_dsc == NULL ||
            font->get_advance_width == NULL) {
        return 0;
    }
    int w = 0;
    const char *p = text;
    while (*p) {
        uint32_t unicode = 0;
        if (gfx_arena_utf8_to_unicode(&p, &unicode) == 0) {
            continue;
        }
        gfx_glyph_dsc_t dsc;
        if (!font->get_glyph_dsc(font, &dsc, unicode, 0)) {
            continue;
        }
        w += font->get_advance_width(font, &dsc);
    }
    return w;
}

/** True if [abs_x,abs_y,w,h) overlaps both clip and buffer (exclusive areas). */
static bool gfx_arena_area_in_clip(const gfx_render_surface_t *surf,
                               int abs_x, int abs_y, int w, int h)
{
    if (surf == NULL || w <= 0 || h <= 0) {
        return false;
    }
    gfx_area_t area = {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + w),
        .y2 = (gfx_coord_t)(abs_y + h),
    };
    gfx_area_t clipped;
    return gfx_area_intersect_exclusive(&clipped, &area, &surf->clip_area) &&
           gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area);
}

/**
 * Tighten surf clip to widget box (like object clip_children).
 * Returns false if the box is fully outside the current dirty/buffer clip.
 */
static bool gfx_arena_surf_clip_to_box(const gfx_render_surface_t *surf,
                                   int abs_x, int abs_y, int w, int h,
                                   gfx_render_surface_t *out)
{
    gfx_area_t box;
    gfx_area_t clipped;

    if (surf == NULL || out == NULL || w <= 0 || h <= 0) {
        return false;
    }
    box = (gfx_area_t) {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + w),
        .y2 = (gfx_coord_t)(abs_y + h),
    };
    if (!gfx_area_intersect_exclusive(&clipped, &box, &surf->clip_area) ||
            !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
        return false;
    }
    *out = *surf;
    out->clip_area = clipped;
    return true;
}

static void gfx_arena_fill_bg(gfx_display_t *disp, const gfx_render_surface_t *surf,
                          int abs_x, int abs_y, int w, int h, uint32_t rgb, bool round)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    gfx_area_t area = {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + w),
        .y2 = (gfx_coord_t)(abs_y + h),
    };
    gfx_area_t clipped;
    if (!gfx_area_intersect_exclusive(&clipped, &area, &surf->clip_area) ||
            !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
        return;
    }
    if (round) {
        gfx_round_rect_fill_dsc_t dsc = {
            .color = GFX_COLOR_HEX(rgb),
            .opa = 0xFFU,
            .radius = GFX_ARENA_BUTTON_RADIUS,
        };
        /* Full geometry + surf->clip_area (fill_clipped) — do not shrink area. */
        gfx_render_surface_round_rect_fill(disp, surf, &area, &dsc);
    } else {
        gfx_render_surface_fill(disp, surf, &clipped, GFX_COLOR_HEX(rgb), 0xFFU);
    }
}

static void gfx_arena_fill_round_radius(gfx_display_t *disp, const gfx_render_surface_t *surf,
                                    int abs_x, int abs_y, int w, int h,
                                    uint32_t rgb, uint16_t radius)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    gfx_area_t area = {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + w),
        .y2 = (gfx_coord_t)(abs_y + h),
    };
    gfx_area_t clipped;
    if (!gfx_area_intersect_exclusive(&clipped, &area, &surf->clip_area) ||
            !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
        return;
    }
    gfx_round_rect_fill_dsc_t dsc = {
        .color = GFX_COLOR_HEX(rgb),
        .opa = 0xFFU,
        .radius = radius,
    };
    gfx_render_surface_round_rect_fill(disp, surf, &area, &dsc);
}

static void gfx_arena_fill_opa(gfx_display_t *disp, const gfx_render_surface_t *surf,
                           int abs_x, int abs_y, int w, int h, uint32_t rgb, uint8_t opa)
{
    if (w <= 0 || h <= 0 || opa == 0U) {
        return;
    }
    gfx_area_t area = {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + w),
        .y2 = (gfx_coord_t)(abs_y + h),
    };
    gfx_area_t clipped;
    if (!gfx_area_intersect_exclusive(&clipped, &area, &surf->clip_area) ||
            !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
        return;
    }
    gfx_render_surface_fill(disp, surf, &clipped, GFX_COLOR_HEX(rgb), opa);
}

static void gfx_arena_draw_text_n(gfx_display_t *disp, const gfx_render_surface_t *surf,
                              gfx_font_handle_t font, const char *text, size_t text_len,
                              int box_x, int box_y, int box_w, int box_h,
                              uint32_t rgb, bool center)
{
    if (disp == NULL || surf == NULL || font == NULL || text == NULL || text_len == 0 ||
            box_w <= 0 || box_h <= 0 ||
            font->get_glyph_dsc == NULL || font->get_glyph_bitmap == NULL ||
            font->get_pixel_value == NULL || font->get_line_height == NULL ||
            font->get_advance_width == NULL) {
        return;
    }

    /* Dirty clips often miss most widgets — bail before measure/glyph work. */
    if (!gfx_arena_area_in_clip(surf, box_x, box_y, box_w, box_h)) {
        return;
    }

    char stack[128];
    char *tmp = stack;
    if (text_len + 1u > sizeof(stack)) {
        tmp = (char *)malloc(text_len + 1u);
        if (tmp == NULL) {
            return;
        }
    }
    memcpy(tmp, text, text_len);
    tmp[text_len] = '\0';

    const int line_h = font->get_line_height(font);
    if (line_h <= 0) {
        if (tmp != stack) {
            free(tmp);
        }
        return;
    }

    const int text_w = gfx_arena_text_width(font, tmp);
    int pen_x = box_x;
    if (center && text_w < box_w) {
        pen_x = box_x + (box_w - text_w) / 2;
    }
    int pen_y = box_y;
    if (line_h < box_h) {
        pen_y = box_y + (box_h - line_h) / 2;
    }

    const char *p = tmp;
    while (*p) {
        uint32_t unicode = 0;
        if (gfx_arena_utf8_to_unicode(&p, &unicode) == 0) {
            continue;
        }

        gfx_glyph_dsc_t dsc;
        if (!font->get_glyph_dsc(font, &dsc, unicode, 0)) {
            continue;
        }
        const uint8_t *bitmap = font->get_glyph_bitmap(font, unicode, &dsc);
        const int adv = font->get_advance_width(font, &dsc);
        int ofs_y = dsc.ofs_y;
        if (font->adjust_baseline_offset != NULL) {
            ofs_y = font->adjust_baseline_offset(font, &dsc);
        }

        if (bitmap != NULL && dsc.box_w > 0 && dsc.box_h > 0) {
            const int gx0 = pen_x + dsc.ofs_x;
            const int gy0 = pen_y + ofs_y;
            gfx_area_t glyph_area = {
                .x1 = (gfx_coord_t)gx0,
                .y1 = (gfx_coord_t)gy0,
                .x2 = (gfx_coord_t)(gx0 + (int)dsc.box_w),
                .y2 = (gfx_coord_t)(gy0 + (int)dsc.box_h),
            };
            gfx_area_t clipped;
            if (gfx_area_intersect_exclusive(&clipped, &glyph_area, &surf->clip_area) &&
                    gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
                const size_t alpha_bytes = (size_t)dsc.box_w * (size_t)dsc.box_h;
                uint8_t *alpha = (uint8_t *)malloc(alpha_bytes);
                if (alpha != NULL) {
                    for (int gy = 0; gy < (int)dsc.box_h; gy++) {
                        for (int gx = 0; gx < (int)dsc.box_w; gx++) {
                            alpha[gy * dsc.box_w + gx] =
                                font->get_pixel_value(font, bitmap, gx, gy, dsc.box_w);
                        }
                    }
                    /* draw_mask expects area in screen space; mask origin at glyph_area.x1/y1 */
                    const gfx_coord_t mask_stride = (gfx_coord_t)dsc.box_w;
                    const int off_x = (int)clipped.x1 - gx0;
                    const int off_y = (int)clipped.y1 - gy0;
                    const gfx_opa_t *mask_ptr = alpha + off_y * dsc.box_w + off_x;
                    gfx_render_surface_draw_mask(disp, surf, &clipped, mask_ptr, mask_stride,
                                                 GFX_COLOR_HEX(rgb), 0xFFU);
                    free(alpha);
                }
            }
        }

        pen_x += adv;
        if (pen_x >= box_x + box_w) {
            break;
        }
    }

    if (tmp != stack) {
        free(tmp);
    }
}

static void gfx_arena_draw_text(gfx_display_t *disp, const gfx_render_surface_t *surf,
                            gfx_font_handle_t font, const char *text,
                            int box_x, int box_y, int box_w, int box_h,
                            uint32_t rgb, bool center)
{
    if (text == NULL) {
        return;
    }
    gfx_arena_draw_text_n(disp, surf, font, text, strlen(text),
                      box_x, box_y, box_w, box_h, rgb, center);
}

static void gfx_arena_draw_items(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n,
                             int abs_x, int abs_y, const gfx_render_surface_t *surf,
                             gfx_font_handle_t font, int32_t scroll_y)
{
    gfx_render_surface_t local;
    const gfx_render_surface_t *draw_surf = surf;
    const gfx_arena_items_hdr_t *ih = gfx_arena_items(a, n->reserved);

    if (surf == NULL || n == NULL || n->w == 0 || n->h == 0) {
        return;
    }
    /* Clip children to list/wheel box — prevents row/text overflow. */
    if (!gfx_arena_surf_clip_to_box(surf, abs_x, abs_y, (int)n->w, (int)n->h, &local)) {
        return;
    }
    draw_surf = &local;

    if (ih == NULL || ih->item_count == 0) {
        if ((n->flags & GFX_ARENA_F_BG) != 0) {
            gfx_arena_fill_bg(disp, draw_surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, false);
        }
        return;
    }

    if ((n->flags & GFX_ARENA_F_BG) != 0) {
        gfx_arena_fill_bg(disp, draw_surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, false);
    }

    const int row_h = (ih->item_height > 0) ? (int)ih->item_height : 24;
    if (row_h <= 0) {
        return;
    }

    const uint32_t text_rgb = (ih->text_rgb != 0u) ? ih->text_rgb : GFX_ARENA_LABEL_DEFAULT_RGB;
    /*
     * Match format_playground list chrome:
     * - nav list: selected/focus blue 0x2F8CFF, border 0x40515F
     * - demo list: focus yellow 0xF1C40F (object draws focus over selected), border 0x3F5163
     * Object list has row separators only (no outer rect stroke).
     */
    const char *list_name = gfx_arena_str(a, n->name_off);
    const bool is_nav = (list_name != NULL && strcmp(list_name, "nav") == 0);
    const uint32_t sel_bg = is_nav ? 0x2F8CFFu : 0xF1C40Fu;
    const uint32_t sel_text = is_nav ? 0xFFFFFFu : 0x101418u;
    const uint32_t row_border = is_nav ? 0x40515Fu : 0x3F5163u;
    const int text_pad_x = 14;

    if (n->type == GFX_ARENA_NODE_LIST) {
        /* Pixel scroll (drag/inertia via scene side table). */
        const int first = scroll_y / row_h;
        const int y_off = -(scroll_y % row_h);
        const int max_rows = ((int)n->h + row_h - 1) / row_h + 1;
        for (int row = 0; row < max_rows; row++) {
            const int idx = first + row;
            int y;
            int row_draw_h;
            bool selected;
            if (idx < 0) {
                continue;
            }
            if (idx >= (int)ih->item_count) {
                break;
            }
            y = abs_y + y_off + row * row_h;
            if (y + row_h <= abs_y || y >= abs_y + (int)n->h) {
                continue;
            }
            selected = (ih->selected != GFX_ARENA_ITEMS_SELECTED_NONE &&
                        (int)ih->selected == idx);
            row_draw_h = row_h;
            if (y < abs_y) {
                row_draw_h -= (abs_y - y);
                y = abs_y;
            }
            if (y + row_draw_h > abs_y + (int)n->h) {
                row_draw_h = abs_y + (int)n->h - y;
            }
            if (selected && row_draw_h > 0) {
                gfx_arena_fill_bg(disp, draw_surf, abs_x, y, (int)n->w, row_draw_h, sel_bg, false);
            }
            /* Row separator (object list border_width=2). */
            if (row_draw_h > 0) {
                const int line_y = abs_y + y_off + row * row_h + row_h - 1;
                if (line_y >= abs_y && line_y < abs_y + (int)n->h) {
                    gfx_arena_fill_bg(disp, draw_surf, abs_x, line_y, (int)n->w, 1, row_border, false);
                    if (line_y - 1 >= abs_y) {
                        gfx_arena_fill_bg(disp, draw_surf, abs_x, line_y - 1, (int)n->w, 1,
                                      row_border, false);
                    }
                }
            }
            {
                uint16_t len = 0;
                const char *txt = gfx_arena_item_text(a, n->reserved, (uint16_t)idx, &len);
                if (txt != NULL && len > 0) {
                    const uint32_t rgb = selected ? sel_text : text_rgb;
                    gfx_arena_draw_text_n(disp, draw_surf, font, txt, len,
                                      abs_x + text_pad_x, abs_y + y_off + row * row_h,
                                      (int)n->w - text_pad_x * 2, row_h, rgb, false);
                }
            }
        }
        return;
    }

    /* Wheel: object-canonical scroll_y + fixed center highlight band. */
    {
        const bool cyclic = (ih->flags & GFX_ARENA_ITEMS_F_CYCLIC) != 0;
        int32_t selected_idx = (ih->selected != GFX_ARENA_ITEMS_SELECTED_NONE &&
                                ih->selected < ih->item_count) ? (int32_t)ih->selected : 0;
        if (scroll_y == 0) {
            scroll_y = gfx_wheel_core_center_scroll_y((int32_t)n->h, (uint16_t)row_h, selected_idx);
        }
        /* Match format_playground wheel: center band yellow + dark text. */
        const uint32_t wheel_center_bg = 0xF1C40Fu;
        const uint32_t wheel_center_text = 0x101418u;
        const int center_y = abs_y + ((int)n->h - row_h) / 2;
        gfx_arena_fill_bg(disp, draw_surf, abs_x, center_y, (int)n->w, row_h, wheel_center_bg, false);

        const int32_t center_raw = gfx_wheel_core_raw_index_from_scroll((int32_t)n->h,
                                   (uint16_t)row_h, scroll_y);
        const int half_rows = ((int)n->h / row_h) / 2 + 2;
        for (int slot = -half_rows; slot <= half_rows; slot++) {
            int32_t idx = center_raw + slot;
            if (cyclic && ih->item_count > 0) {
                idx = gfx_wheel_core_clamp_index(ih->item_count, idx, true);
            } else if (idx < 0 || idx >= (int32_t)ih->item_count) {
                continue;
            }
            const int y = abs_y + (int)((center_raw + slot) * row_h - scroll_y);
            if (y + row_h <= abs_y || y >= abs_y + (int)n->h) {
                continue;
            }
            const bool selected = (ih->selected != GFX_ARENA_ITEMS_SELECTED_NONE &&
                                   (int)ih->selected == (int)idx);
            uint16_t len = 0;
            const char *txt = gfx_arena_item_text(a, n->reserved, (uint16_t)idx, &len);
            if (txt != NULL && len > 0) {
                const uint32_t rgb = selected ? wheel_center_text : text_rgb;
                gfx_arena_draw_text_n(disp, draw_surf, font, txt, len,
                                  abs_x + 6, y, (int)n->w - 12, row_h, rgb, true);
            }
        }
    }
}

static void gfx_arena_draw_coverflow(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n,
                                 int abs_x, int abs_y, const gfx_render_surface_t *surf,
                                 gfx_font_handle_t font, int32_t drag_offset)
{
    gfx_render_surface_t local;
    const gfx_render_surface_t *draw_surf;
    const gfx_arena_items_hdr_t *ih = gfx_arena_items(a, n->reserved);
    gfx_coverflow_core_effect_t effect = {
        .center_zoom = GFX_COVERFLOW_CORE_CENTER_ZOOM,
        .side_zoom = GFX_COVERFLOW_CORE_SIDE_ZOOM,
        .spacing_pct = GFX_COVERFLOW_CORE_SPACING_PCT,
        .side_dim_opa = GFX_COVERFLOW_CORE_SIDE_DIM_OPA,
    };
    gfx_coverflow_core_card_t cards[GFX_COVERFLOW_CORE_VISIBLE_SIDE * 2 + 1];
    uint8_t card_count;
    uint16_t selected;

    if (surf == NULL || n == NULL || n->w == 0 || n->h == 0) {
        return;
    }
    /* Side cards extend past the widget box; clip like object coverflow. */
    if (!gfx_arena_surf_clip_to_box(surf, abs_x, abs_y, (int)n->w, (int)n->h, &local)) {
        return;
    }
    draw_surf = &local;

    if ((n->flags & GFX_ARENA_F_BG) != 0) {
        gfx_arena_fill_bg(disp, draw_surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, false);
    }
    if (ih == NULL || ih->item_count == 0) {
        return;
    }

    selected = ih->selected;
    if (selected == GFX_ARENA_ITEMS_SELECTED_NONE || selected >= ih->item_count) {
        selected = 0;
    }

    card_count = gfx_coverflow_core_collect_cards(ih->item_count, (int32_t)selected, drag_offset,
                 abs_x, abs_y, (int32_t)n->w, (int32_t)n->h, &effect,
                 cards, (uint8_t)(sizeof(cards) / sizeof(cards[0])), true);

    {
        gfx_arena_scene_t *sc = gfx_arena_scene_from_disp(disp);
        uint32_t node_off = (sc != NULL) ? gfx_arena_node_offset(a, n) : GFX_ARENA_NO_NODE;

        for (uint8_t i = 0; i < card_count; i++) {
            const gfx_coverflow_core_card_t *c = &cards[i];
            const uint32_t fill = c->centerish ? 0x2f8cffu : 0x2a3142u;
            const uint32_t text_rgb = c->centerish ? 0xFFFFFFu : 0xA8B3C7u;
            const gfx_arena_img_rt_t *rt = NULL;
            bool drew_img = false;

            gfx_arena_fill_bg(disp, draw_surf, (int)c->x, (int)c->y, (int)c->w, (int)c->h, fill, true);
            if (sc != NULL && node_off != GFX_ARENA_NO_NODE) {
                rt = gfx_arena_scene_img_rt_find(sc, node_off, (uint16_t)c->index, 0);
                if (rt != NULL && gfx_arena_img_rt_is_open(rt)) {
                    gfx_arena_blit_rt_image(disp, rt, (int)c->x, (int)c->y,
                                        (int)c->w, (int)c->h, draw_surf, true);
                    drew_img = true;
                }
            }
            if (!drew_img) {
                uint16_t len = 0;
                const char *txt = gfx_arena_item_text(a, n->reserved, (uint16_t)c->index, &len);
                if (txt != NULL && len > 0) {
                    gfx_arena_draw_text_n(disp, draw_surf, font, txt, len,
                                      (int)c->x + 8, (int)c->y, (int)c->w - 16, (int)c->h,
                                      text_rgb, true);
                }
            } else {
                /* Caption strip at bottom when image is bound. */
                uint16_t len = 0;
                const char *txt = gfx_arena_item_text(a, n->reserved, (uint16_t)c->index, &len);
                if (txt != NULL && len > 0 && (int)c->h > 28) {
                    gfx_arena_fill_opa(disp, draw_surf, (int)c->x, (int)c->y + (int)c->h - 28,
                                   (int)c->w, 28, 0x000000u, 140);
                    gfx_arena_draw_text_n(disp, draw_surf, font, txt, len,
                                      (int)c->x + 8, (int)c->y + (int)c->h - 28,
                                      (int)c->w - 16, 28, 0xFFFFFFu, true);
                }
            }
            if (c->dim_opa > 0U) {
                gfx_arena_fill_opa(disp, draw_surf, (int)c->x, (int)c->y, (int)c->w, (int)c->h,
                               0x000000u, c->dim_opa);
            }
        }
    }
}

static void gfx_arena_draw_pageflow(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n,
                                int abs_x, int abs_y, const gfx_render_surface_t *surf,
                                gfx_font_handle_t font, int32_t drag_offset)
{
    gfx_render_surface_t local;
    const gfx_render_surface_t *draw_surf;
    const gfx_arena_items_hdr_t *ih = gfx_arena_items(a, n->reserved);

    if (surf == NULL || n == NULL || n->w == 0 || n->h == 0) {
        return;
    }
    if (!gfx_arena_surf_clip_to_box(surf, abs_x, abs_y, (int)n->w, (int)n->h, &local)) {
        return;
    }
    draw_surf = &local;

    if ((n->flags & GFX_ARENA_F_BG) != 0) {
        gfx_arena_fill_bg(disp, draw_surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, false);
    }
    if (ih == NULL || ih->item_count == 0) {
        return;
    }

    uint16_t selected = ih->selected;
    if (selected == GFX_ARENA_ITEMS_SELECTED_NONE || selected >= ih->item_count) {
        selected = 0;
    }

    {
        gfx_arena_scene_t *sc = gfx_arena_scene_from_disp(disp);
        uint32_t node_off = (sc != NULL) ? gfx_arena_node_offset(a, n) : GFX_ARENA_NO_NODE;
        const int32_t span = (int32_t)n->w;

        for (int slot = -1; slot <= 1; slot++) {
            int32_t idx = (int32_t)selected + slot;
            int32_t x_ofs;
            int page_x;
            const uint32_t fill = (slot == 0) ? 0x1F2A35u : 0x19222Cu;
            const uint32_t text_rgb = (slot == 0) ? 0xF3F7FAu : 0xA8B3C7u;
            const gfx_arena_img_rt_t *rt = NULL;
            bool drew_img = false;
            gfx_area_t border;

            if (idx < 0 || idx >= (int32_t)ih->item_count) {
                continue;
            }
            x_ofs = gfx_pageflow_core_slot_offset(slot, span, drag_offset);
            page_x = abs_x + (int)x_ofs;
            if (page_x + (int)n->w <= abs_x || page_x >= abs_x + (int)n->w) {
                continue;
            }

            gfx_arena_fill_bg(disp, draw_surf, page_x, abs_y, (int)n->w, (int)n->h, fill, false);
            if (sc != NULL && node_off != GFX_ARENA_NO_NODE) {
                rt = gfx_arena_scene_img_rt_find(sc, node_off, (uint16_t)idx, 0);
                if (rt != NULL && gfx_arena_img_rt_is_open(rt)) {
                    gfx_arena_blit_rt_image(disp, rt, page_x, abs_y,
                                        (int)n->w, (int)n->h, draw_surf, true);
                    drew_img = true;
                }
            }
            border = (gfx_area_t) {
                .x1 = (gfx_coord_t)page_x,
                .y1 = (gfx_coord_t)abs_y,
                .x2 = (gfx_coord_t)(page_x + (int)n->w),
                .y2 = (gfx_coord_t)(abs_y + (int)n->h),
            };
            gfx_render_surface_rect_stroke(disp, draw_surf, &border, 1,
                                           GFX_COLOR_HEX(0x3F5163u), 0xFFU);

            {
                uint16_t len = 0;
                const char *txt = gfx_arena_item_text(a, n->reserved, (uint16_t)idx, &len);
                if (txt != NULL && len > 0) {
                    if (drew_img) {
                        gfx_arena_fill_opa(disp, draw_surf, page_x, abs_y + (int)n->h - 36,
                                       (int)n->w, 36, 0x000000u, 140);
                        gfx_arena_draw_text_n(disp, draw_surf, font, txt, len,
                                          page_x + 12, abs_y + (int)n->h - 36,
                                          (int)n->w - 24, 36, 0xFFFFFFu, true);
                    } else {
                        gfx_arena_draw_text_n(disp, draw_surf, font, txt, len,
                                          page_x + 12, abs_y + 8, (int)n->w - 24, (int)n->h - 16,
                                          text_rgb, true);
                    }
                }
            }
        }
    }
}

static void gfx_arena_draw_progress(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n,
                                int abs_x, int abs_y, const gfx_render_surface_t *surf)
{
    const gfx_arena_progress_hdr_t *ph = gfx_arena_progress(a, n->reserved);
    uint16_t radius;
    uint16_t fill_radius;
    uint16_t v;
    const int pad = 4; /* match format_playground fill_pad */
    int inner_w;
    int inner_h;
    int fill_w;
    int thumb;
    int center_x;
    int min_x;
    int max_x;
    uint32_t track;
    uint32_t fill;
    bool pressed;
    gfx_area_t track_area;
    gfx_area_t fill_area;
    gfx_area_t thumb_area;
    gfx_round_rect_fill_dsc_t round_dsc;
    gfx_round_rect_stroke_dsc_t stroke_dsc;
    uint16_t stroke_w;

    if (ph == NULL || n->w == 0 || n->h == 0) {
        return;
    }
    track = (ph->track_rgb != 0u) ? ph->track_rgb : 0x26313Bu;
    fill = (ph->fill_rgb != 0u) ? ph->fill_rgb : 0x2F8CFFu;
    radius = ph->pad != 0u ? ph->pad : 6u;
    pressed = (n->flags & GFX_ARENA_F_PRESSED) != 0;

    track_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + (int)n->w),
        .y2 = (gfx_coord_t)(abs_y + (int)n->h),
    };
    round_dsc = (gfx_round_rect_fill_dsc_t) {
        .color = GFX_COLOR_HEX(track),
        .opa = 0xFFU,
        .radius = radius,
    };
    gfx_render_surface_round_rect_fill(disp, surf, &track_area, &round_dsc);

    v = gfx_progress_core_clamp(ph->value);
    inner_w = (int)n->w - pad * 2;
    inner_h = (int)n->h - pad * 2;
    if (inner_w <= 0 || inner_h <= 0) {
        return;
    }
    fill_w = (int)gfx_progress_core_fill_extent(inner_w, v);
    thumb = inner_h;
    if (thumb > inner_w) {
        thumb = inner_w;
    }
    center_x = abs_x + pad + fill_w;
    min_x = abs_x + pad + thumb / 2;
    max_x = abs_x + pad + inner_w - (thumb + 1) / 2;
    if (center_x < min_x) {
        center_x = min_x;
    } else if (center_x > max_x) {
        center_x = max_x;
    }

    /* Interactive fill ends at thumb center (object progress_bar). */
    fill_radius = radius > (uint16_t)pad ? (uint16_t)(radius - (uint16_t)pad) : radius;
    fill_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)(abs_x + pad),
        .y1 = (gfx_coord_t)(abs_y + pad),
        .x2 = (gfx_coord_t)center_x,
        .y2 = (gfx_coord_t)(abs_y + pad + inner_h),
    };
    if (fill_area.x2 > fill_area.x1) {
        round_dsc.color = GFX_COLOR_HEX(fill);
        round_dsc.radius = fill_radius;
        gfx_render_surface_round_rect_fill(disp, surf, &fill_area, &round_dsc);
    }

    thumb_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)(center_x - thumb / 2),
        .y1 = (gfx_coord_t)(abs_y + pad),
        .x2 = (gfx_coord_t)(center_x - thumb / 2 + thumb),
        .y2 = (gfx_coord_t)(abs_y + pad + inner_h),
    };
    round_dsc.color = GFX_COLOR_HEX(0xF7FBFFu);
    round_dsc.radius = (uint16_t)(thumb / 2);
    gfx_render_surface_round_rect_fill(disp, surf, &thumb_area, &round_dsc);

    stroke_w = 2u;
    if (pressed) {
        stroke_w++;
    }
    stroke_dsc = (gfx_round_rect_stroke_dsc_t) {
        .color = GFX_COLOR_HEX(pressed ? fill : 0x86C8FFu),
        .opa = 0xFFU,
        .radius = (uint16_t)(thumb / 2),
        .width = stroke_w,
    };
    gfx_render_surface_round_rect_stroke(disp, surf, &thumb_area, &stroke_dsc);
}

static void gfx_arena_blit_rt_image(gfx_display_t *disp, const gfx_arena_img_rt_t *rt,
                                int abs_x, int abs_y, int box_w, int box_h,
                                const gfx_render_surface_t *surf, bool scale_to_box)
{
    uint16_t img_w;
    uint16_t img_h;
    gfx_color_format_t cf;
    const uint8_t *pixels;
    uint8_t px_size;
    gfx_coord_t stride;
    const gfx_opa_t *alpha;
    gfx_coord_t alpha_stride;
    int draw_w;
    int draw_h;

    if (disp == NULL || rt == NULL || surf == NULL || !gfx_arena_img_rt_is_open(rt) ||
            box_w <= 0 || box_h <= 0) {
        return;
    }
    img_w = gfx_arena_img_rt_width(rt);
    img_h = gfx_arena_img_rt_height(rt);
    if (img_w == 0U || img_h == 0U) {
        return;
    }
    cf = gfx_arena_img_rt_format(rt);
    if (!gfx_color_format_is_image_supported(cf)) {
        return;
    }
    pixels = gfx_arena_img_rt_pixels(rt);
    px_size = gfx_arena_img_rt_pixel_size(rt);
    stride = gfx_arena_img_rt_stride_px(rt);
    if (pixels == NULL || px_size == 0U || stride <= 0) {
        return;
    }
    alpha = NULL;
    alpha_stride = 0;
    if (gfx_color_format_has_plane_alpha(cf)) {
        alpha = gfx_arena_img_rt_alpha(rt);
        alpha_stride = gfx_arena_img_rt_alpha_stride(rt);
        if (alpha == NULL) {
            return;
        }
    }

    if (scale_to_box) {
        gfx_area_t card = {
            .x1 = (gfx_coord_t)abs_x,
            .y1 = (gfx_coord_t)abs_y,
            .x2 = (gfx_coord_t)(abs_x + box_w),
            .y2 = (gfx_coord_t)(abs_y + box_h),
        };
        gfx_area_t clipped;
        int32_t src_w = (int32_t)img_w;
        int32_t src_h = (int32_t)img_h;
        int32_t src_draw_w;
        int32_t src_draw_h;
        int32_t src_x0;
        int32_t src_y0;
        gfx_area_t src_area;
        gfx_area_t dst_area;
        gfx_area_t dst_clip;
        gfx_render_image_t render_src;

        if (!gfx_area_intersect_exclusive(&clipped, &card, &surf->clip_area) ||
                !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
            return;
        }
        if ((int64_t)src_w * box_h > (int64_t)src_h * box_w) {
            src_draw_h = src_h;
            src_draw_w = (box_w * src_h) / box_h;
        } else {
            src_draw_w = src_w;
            src_draw_h = (box_h * src_w) / box_w;
        }
        if (src_draw_w <= 0) {
            src_draw_w = 1;
        }
        if (src_draw_h <= 0) {
            src_draw_h = 1;
        }
        src_x0 = (src_w - src_draw_w) / 2;
        src_y0 = (src_h - src_draw_h) / 2;
        src_area.x1 = (gfx_coord_t)src_x0;
        src_area.y1 = (gfx_coord_t)src_y0;
        src_area.x2 = (gfx_coord_t)(src_x0 + src_draw_w);
        src_area.y2 = (gfx_coord_t)(src_y0 + src_draw_h);
        render_src = (gfx_render_image_t) {
            .pixels = pixels,
            .stride = stride,
            .format = cf,
            .alpha = alpha,
            .alpha_stride = alpha_stride,
        };
        if (gfx_render_surface_scale_image(disp, surf, &card, &render_src, &src_area, 0xFFU)) {
            return;
        }
        dst_area.x1 = (gfx_coord_t)(card.x1 - surf->buf_area.x1);
        dst_area.y1 = (gfx_coord_t)(card.y1 - surf->buf_area.y1);
        dst_area.x2 = (gfx_coord_t)(card.x2 - surf->buf_area.x1);
        dst_area.y2 = (gfx_coord_t)(card.y2 - surf->buf_area.y1);
        dst_clip.x1 = (gfx_coord_t)(clipped.x1 - surf->buf_area.x1);
        dst_clip.y1 = (gfx_coord_t)(clipped.y1 - surf->buf_area.y1);
        dst_clip.x2 = (gfx_coord_t)(clipped.x2 - surf->buf_area.x1);
        dst_clip.y2 = (gfx_coord_t)(clipped.y2 - surf->buf_area.y1);
        gfx_sw_blend_img_scale_draw_fmt(surf->buf, surf->stride, surf->format,
                                        pixels, stride, alpha, alpha_stride,
                                        &dst_area, &dst_clip, &src_area, cf, 0xFFU);
        return;
    }

    draw_w = (box_w < (int)img_w) ? box_w : (int)img_w;
    draw_h = (box_h < (int)img_h) ? box_h : (int)img_h;
    if (draw_w <= 0 || draw_h <= 0) {
        return;
    }
    {
        gfx_area_t dst_area = {
            .x1 = (gfx_coord_t)abs_x,
            .y1 = (gfx_coord_t)abs_y,
            .x2 = (gfx_coord_t)(abs_x + draw_w),
            .y2 = (gfx_coord_t)(abs_y + draw_h),
        };
        gfx_area_t clipped;
        gfx_coord_t src_x;
        gfx_coord_t src_y;
        const uint8_t *src_pixels;
        gfx_opa_t *alpha_mask = NULL;
        gfx_render_image_t backend_src;
        gfx_area_t local;

        if (!gfx_area_intersect_exclusive(&clipped, &dst_area, &surf->clip_area) ||
                !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
            return;
        }
        src_x = (gfx_coord_t)(clipped.x1 - abs_x);
        src_y = (gfx_coord_t)(clipped.y1 - abs_y);
        src_pixels = pixels + ((size_t)src_y * (size_t)stride + (size_t)src_x) * px_size;
        if (alpha != NULL) {
            alpha_mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP((const uint8_t *)alpha,
                         src_y, img_w, src_x);
        }
        backend_src = (gfx_render_image_t) {
            .pixels = pixels,
            .stride = stride,
            .format = cf,
            .alpha = alpha,
            .alpha_stride = alpha_stride,
        };
        if (gfx_render_surface_blit_image(disp, surf, &clipped, &backend_src,
                                          src_x, src_y, 0xFFU)) {
            return;
        }
        local = (gfx_area_t) {
            .x1 = (gfx_coord_t)(clipped.x1 - surf->buf_area.x1),
            .y1 = (gfx_coord_t)(clipped.y1 - surf->buf_area.y1),
            .x2 = (gfx_coord_t)(clipped.x2 - surf->buf_area.x1),
            .y2 = (gfx_coord_t)(clipped.y2 - surf->buf_area.y1),
        };
        gfx_sw_blend_img_draw_fmt(surf->buf, surf->stride, surf->format,
                                  src_pixels, stride, alpha_mask, alpha_stride,
                                  &local, cf, 0xFFU);
    }
}

static void gfx_arena_draw_image(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n,
                             int abs_x, int abs_y, const gfx_render_surface_t *surf)
{
    gfx_arena_scene_t *sc;
    uint32_t node_off;
    uint8_t face = 0;
    const gfx_arena_img_rt_t *rt;
    const gfx_arena_img_hdr_t *ih;
    const uint16_t *px;

    if (n == NULL || n->w == 0 || n->h == 0) {
        return;
    }

    sc = gfx_arena_scene_from_disp(disp);
    node_off = (sc != NULL) ? gfx_arena_node_offset(&sc->arena, n) : GFX_ARENA_NO_NODE;
    if (sc != NULL && node_off != GFX_ARENA_NO_NODE) {
        (void)gfx_arena_scene_ensure_pkg_images(sc, node_off);
        if (n->type == GFX_ARENA_NODE_IMAGE_BUTTON && (n->flags & GFX_ARENA_F_PRESSED) != 0) {
            face = 1;
        }
        rt = gfx_arena_scene_img_rt_find(sc, node_off, GFX_ARENA_IMG_ITEM_NONE, face);
        if (rt == NULL && face == 1) {
            rt = gfx_arena_scene_img_rt_find(sc, node_off, GFX_ARENA_IMG_ITEM_NONE, 0);
        }
        if (rt != NULL && gfx_arena_img_rt_is_open(rt)) {
            gfx_arena_blit_rt_image(disp, rt, abs_x, abs_y, (int)n->w, (int)n->h, surf, false);
            return;
        }
    }

    ih = gfx_arena_img(a, n->reserved);
    px = gfx_arena_img_pixels(a, n->reserved);
    if (ih == NULL || px == NULL) {
        return;
    }

    if (n->type == GFX_ARENA_NODE_IMAGE_BUTTON &&
            (n->flags & GFX_ARENA_F_PRESSED) != 0 &&
            (ih->pad & GFX_ARENA_IMG_F_HAS_PRESSED) != 0) {
        const size_t px_count = (size_t)ih->w * (size_t)ih->h;
        const size_t need = (size_t)n->reserved + sizeof(gfx_arena_img_hdr_t) +
                            px_count * sizeof(uint16_t) * 2u;
        if (need <= a->size) {
            px += px_count;
        }
    }

    {
        const int draw_w = (n->w < ih->w) ? (int)n->w : (int)ih->w;
        const int draw_h = (n->h < ih->h) ? (int)n->h : (int)ih->h;
        gfx_area_t dst_area;
        gfx_area_t clipped;
        gfx_render_image_t src;
        gfx_coord_t src_x;
        gfx_coord_t src_y;
        gfx_area_t local;
        const uint16_t *src_ptr;

        if (draw_w <= 0 || draw_h <= 0) {
            return;
        }
        dst_area = (gfx_area_t) {
            .x1 = (gfx_coord_t)abs_x,
            .y1 = (gfx_coord_t)abs_y,
            .x2 = (gfx_coord_t)(abs_x + draw_w),
            .y2 = (gfx_coord_t)(abs_y + draw_h),
        };
        if (!gfx_area_intersect_exclusive(&clipped, &dst_area, &surf->clip_area) ||
                !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
            return;
        }
        src = (gfx_render_image_t) {
            .pixels = px,
            .stride = (gfx_coord_t)ih->w,
            .format = GFX_COLOR_FORMAT_RGB565,
            .alpha = NULL,
            .alpha_stride = 0,
        };
        src_x = (gfx_coord_t)(clipped.x1 - abs_x);
        src_y = (gfx_coord_t)(clipped.y1 - abs_y);
        if (gfx_render_surface_blit_image(disp, surf, &clipped, &src, src_x, src_y, 0xFFU)) {
            return;
        }
        local = (gfx_area_t) {
            .x1 = (gfx_coord_t)(clipped.x1 - surf->buf_area.x1),
            .y1 = (gfx_coord_t)(clipped.y1 - surf->buf_area.y1),
            .x2 = (gfx_coord_t)(clipped.x2 - surf->buf_area.x1),
            .y2 = (gfx_coord_t)(clipped.y2 - surf->buf_area.y1),
        };
        src_ptr = px + (size_t)src_y * (size_t)ih->w + (size_t)src_x;
        gfx_sw_blend_img_draw_fmt(surf->buf, surf->stride, surf->format,
                                  src_ptr, (gfx_coord_t)ih->w,
                                  NULL, 0, &local, GFX_COLOR_FORMAT_RGB565, 0xFFU);
    }
}

typedef void (*gfx_arena_draw_fn_t)(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                                int abs_x, int abs_y, const gfx_render_surface_t *surf,
                                gfx_font_handle_t font);

static void draw_type_container(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                                int abs_x, int abs_y, const gfx_render_surface_t *surf,
                                gfx_font_handle_t font)
{
    (void)a;
    (void)off;
    (void)font;
    if ((n->flags & GFX_ARENA_F_BG) != 0) {
        gfx_arena_fill_bg(disp, surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, false);
    }
}

static void draw_type_button(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                             int abs_x, int abs_y, const gfx_render_surface_t *surf,
                             gfx_font_handle_t font)
{
    uint32_t rgb;
    const char *caption;
    (void)off;
    if ((n->flags & GFX_ARENA_F_BG) == 0) {
        return;
    }
    rgb = n->bg_rgb;
    if ((n->flags & GFX_ARENA_F_PRESSED) != 0) {
        rgb = rgb_scale(rgb, 55);
    }
    gfx_arena_fill_bg(disp, surf, abs_x, abs_y, (int)n->w, (int)n->h, rgb, true);
    {
        /* Match format_playground button: border 0x76B7E8 width 2. */
        gfx_area_t border = {
            .x1 = (gfx_coord_t)abs_x,
            .y1 = (gfx_coord_t)abs_y,
            .x2 = (gfx_coord_t)(abs_x + (int)n->w),
            .y2 = (gfx_coord_t)(abs_y + (int)n->h),
        };
        gfx_render_surface_rect_stroke(disp, surf, &border, 2,
                                       GFX_COLOR_HEX(0x76B7E8u), 0xFFU);
    }
    caption = gfx_arena_str(a, n->name_off);
    if (caption != NULL) {
        gfx_arena_draw_text(disp, surf, font, caption, abs_x, abs_y,
                        (int)n->w, (int)n->h, GFX_ARENA_BUTTON_TEXT_RGB, true);
    }
}

static void draw_type_label(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                            int abs_x, int abs_y, const gfx_render_surface_t *surf,
                            gfx_font_handle_t font)
{
    const char *text;
    uint32_t rgb;
    (void)off;
    text = gfx_arena_str(a, n->name_off);
    rgb = (n->bg_rgb != 0u) ? n->bg_rgb : GFX_ARENA_LABEL_DEFAULT_RGB;
    if (text != NULL) {
        gfx_arena_draw_text(disp, surf, font, text, abs_x, abs_y,
                        (int)n->w, (int)n->h, rgb, false);
    }
}

static void draw_type_image(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                            int abs_x, int abs_y, const gfx_render_surface_t *surf,
                            gfx_font_handle_t font)
{
    (void)off;
    gfx_arena_draw_image(disp, a, n, abs_x, abs_y, surf);
    if (n->type == GFX_ARENA_NODE_IMAGE_BUTTON) {
        const char *caption = gfx_arena_str(a, n->name_off);
        if (caption != NULL) {
            uint32_t rgb = GFX_ARENA_BUTTON_TEXT_RGB;
            if ((n->flags & GFX_ARENA_F_PRESSED) != 0) {
                rgb = 0xD0D8E0u;
            }
            gfx_arena_draw_text(disp, surf, font, caption, abs_x, abs_y,
                            (int)n->w, (int)n->h, rgb, true);
        }
    }
}

static void draw_type_progress(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                               int abs_x, int abs_y, const gfx_render_surface_t *surf,
                               gfx_font_handle_t font)
{
    (void)off;
    (void)font;
    gfx_arena_draw_progress(disp, a, n, abs_x, abs_y, surf);
}

static void draw_type_items(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                            int abs_x, int abs_y, const gfx_render_surface_t *surf,
                            gfx_font_handle_t font)
{
    int32_t scroll_y = 0;
    gfx_arena_scene_t *sc = gfx_arena_scene_from_disp(disp);
    if (sc != NULL) {
        scroll_y = gfx_arena_scene_list_scroll_y(sc, off);
    }
    gfx_arena_draw_items(disp, a, n, abs_x, abs_y, surf, font, scroll_y);
}

static void draw_type_coverflow(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                                int abs_x, int abs_y, const gfx_render_surface_t *surf,
                                gfx_font_handle_t font)
{
    int32_t drag_offset = 0;
    gfx_arena_scene_t *sc = gfx_arena_scene_from_disp(disp);
    if (sc != NULL) {
        drag_offset = gfx_arena_scene_coverflow_drag(sc, off);
    }
    gfx_arena_draw_coverflow(disp, a, n, abs_x, abs_y, surf, font, drag_offset);
}

static void draw_type_pageflow(gfx_display_t *disp, gfx_arena_t *a, gfx_arena_node_t *n, uint32_t off,
                               int abs_x, int abs_y, const gfx_render_surface_t *surf,
                               gfx_font_handle_t font)
{
    int32_t drag_offset = 0;
    gfx_arena_scene_t *sc = gfx_arena_scene_from_disp(disp);
    if (sc != NULL) {
        drag_offset = gfx_arena_scene_pageflow_drag(sc, off);
    }
    gfx_arena_draw_pageflow(disp, a, n, abs_x, abs_y, surf, font, drag_offset);
}

/*
 * Indexed by GFX_ARENA_NODE_* (1-based). Register new drawable types here only —
 * do not grow if-chains in draw_node_disp.
 */
static const gfx_arena_draw_fn_t s_arena_draw_ops[] = {
    [GFX_ARENA_NODE_CONTAINER]    = draw_type_container,
    [GFX_ARENA_NODE_LABEL]        = draw_type_label,
    [GFX_ARENA_NODE_BUTTON]       = draw_type_button,
    [GFX_ARENA_NODE_IMAGE]        = draw_type_image,
    [GFX_ARENA_NODE_LIST]         = draw_type_items,
    [GFX_ARENA_NODE_WHEEL]        = draw_type_items,
    [GFX_ARENA_NODE_IMAGE_BUTTON] = draw_type_image,
    [GFX_ARENA_NODE_PROGRESS]     = draw_type_progress,
    [GFX_ARENA_NODE_COVERFLOW]    = draw_type_coverflow,
    [GFX_ARENA_NODE_PAGEFLOW]     = draw_type_pageflow,
    /* ANIM/MOTION: playback hosts draw via composite object tree; no ARN fill. */
    [GFX_ARENA_NODE_ANIM]         = NULL,
    [GFX_ARENA_NODE_MOTION]       = NULL,
};

static void draw_node_disp(gfx_display_t *disp, gfx_arena_t *a, uint32_t node_off,
                           int origin_x, int origin_y, const gfx_render_surface_t *surf,
                           gfx_font_handle_t font)
{
    for (uint32_t off = node_off; off != GFX_ARENA_NO_NODE; ) {
        gfx_arena_node_t *n = gfx_arena_node(a, off);
        if (n == NULL) {
            break;
        }

        const int abs_x = origin_x + (int)n->x;
        const int abs_y = origin_y + (int)n->y;

        if ((n->flags & GFX_ARENA_F_VISIBLE) != 0) {
            /*
             * Cull by node box vs dirty clip. Children that extend outside the
             * parent box are also skipped — keep descendants within parent bounds
             * (same assumption as typical object clip_children layouts).
             */
            if (!gfx_arena_area_in_clip(surf, abs_x, abs_y, (int)n->w, (int)n->h)) {
                off = n->next_sibling;
                continue;
            }

            if (n->type > 0U &&
                    n->type < (uint16_t)(sizeof(s_arena_draw_ops) / sizeof(s_arena_draw_ops[0])) &&
                    s_arena_draw_ops[n->type] != NULL) {
                s_arena_draw_ops[n->type](disp, a, n, off, abs_x, abs_y, surf, font);
            }

            if (n->first_child != GFX_ARENA_NO_NODE) {
                draw_node_disp(disp, a, n->first_child, abs_x, abs_y, surf, font);
            }
        }

        off = n->next_sibling;
    }
}

int gfx_arena_draw_clipped(gfx_display_t *disp, const gfx_arena_t *arena, const void *render_surface)
{
    if (disp == NULL || arena == NULL || arena->base == NULL || render_surface == NULL) {
        return -1;
    }

    const gfx_render_surface_t *surf = (const gfx_render_surface_t *)render_surface;
    gfx_arena_t *a = (gfx_arena_t *)arena;
    const gfx_arena_hdr_t *hdr = gfx_arena_hdr(a);
    if (hdr->root_off == GFX_ARENA_NO_NODE) {
        return 0;
    }

    gfx_font_handle_t font = NULL;
    gfx_arena_scene_t *scene = gfx_arena_scene_from_disp(disp);
    if (scene != NULL) {
        font = (gfx_font_handle_t)scene->font_adapter;
    }

    draw_node_disp(disp, a, hdr->root_off, 0, 0, surf, font);
    return 0;
}
