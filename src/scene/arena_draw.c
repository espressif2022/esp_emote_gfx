/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena_draw.h"

#include <stdlib.h>
#include <string.h>

#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "fonts/gfx_font_priv.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/types.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"

#define ARENA_BUTTON_RADIUS 6
#define ARENA_BUTTON_TEXT_RGB 0xFFFFFFu
#define ARENA_LABEL_DEFAULT_RGB 0xF3F7FAu

static uint16_t rgb888_to_rgb565(uint32_t rgb)
{
    const uint32_t r = (rgb >> 16) & 0xFFu;
    const uint32_t g = (rgb >> 8) & 0xFFu;
    const uint32_t b = rgb & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void fill_rect_fb(arena_fb_t *fb, int x, int y, int w, int h, uint16_t color)
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

static void draw_node_fb(arena_t *a, uint32_t node_off, int origin_x, int origin_y, arena_fb_t *fb)
{
    for (uint32_t off = node_off; off != ARENA_NO_NODE; ) {
        arena_node_t *n = arena_node(a, off);
        if (n == NULL) {
            break;
        }

        const int abs_x = origin_x + (int)n->x;
        const int abs_y = origin_y + (int)n->y;

        if ((n->flags & ARENA_F_VISIBLE) != 0) {
            if ((n->type == ARENA_NODE_CONTAINER || n->type == ARENA_NODE_BUTTON) &&
                    (n->flags & ARENA_F_BG) != 0) {
                uint32_t rgb = n->bg_rgb;
                if ((n->flags & ARENA_F_PRESSED) != 0) {
                    rgb = ((rgb >> 1) & 0x7F7F7Fu);
                }
                fill_rect_fb(fb, abs_x, abs_y, (int)n->w, (int)n->h, rgb888_to_rgb565(rgb));
            }
            /* LABEL skipped on bare FB path (no font). */

            if (n->first_child != ARENA_NO_NODE) {
                draw_node_fb(a, n->first_child, abs_x, abs_y, fb);
            }
        }

        off = n->next_sibling;
    }
}

int arena_draw(const arena_t *arena, arena_fb_t *fb)
{
    if (arena == NULL || arena->base == NULL || fb == NULL || fb->pixels == NULL ||
            fb->width == 0 || fb->height == 0) {
        return -1;
    }

    const size_t px = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < px; i++) {
        fb->pixels[i] = fb->clear_rgb565;
    }

    arena_t *a = (arena_t *)arena;
    const arena_hdr_t *hdr = arena_hdr(a);
    if (hdr->root_off == ARENA_NO_NODE) {
        return 0;
    }

    draw_node_fb(a, hdr->root_off, 0, 0, fb);
    return 0;
}

static int arena_utf8_to_unicode(const char **p, uint32_t *unicode)
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

static int arena_text_width(gfx_font_handle_t font, const char *text)
{
    if (font == NULL || text == NULL || font->get_glyph_dsc == NULL ||
            font->get_advance_width == NULL) {
        return 0;
    }
    int w = 0;
    const char *p = text;
    while (*p) {
        uint32_t unicode = 0;
        if (arena_utf8_to_unicode(&p, &unicode) == 0) {
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

/** Reusable glyph alpha buffer owned by the scene; NULL = caller mallocs. */
static uint8_t *arena_glyph_scratch_get(gfx_arena_scene_t *scene, size_t bytes)
{
    if (scene == NULL) {
        return NULL;
    }
    if (scene->glyph_scratch_cap < bytes) {
        uint8_t *grown = (uint8_t *)realloc(scene->glyph_scratch, bytes);
        if (grown == NULL) {
            return NULL;
        }
        scene->glyph_scratch = grown;
        scene->glyph_scratch_cap = bytes;
    }
    return scene->glyph_scratch;
}

/** True if [abs_x,abs_y,w,h) overlaps both clip and buffer (exclusive areas). */
static bool arena_area_in_clip(const gfx_render_surface_t *surf,
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

static void arena_fill_bg(gfx_display_t *disp, const gfx_render_surface_t *surf,
                          int abs_x, int abs_y, int w, int h, uint32_t rgb, uint16_t radius)
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
    if (radius > 0) {
        gfx_round_rect_fill_dsc_t dsc = {
            .color = GFX_COLOR_HEX(rgb),
            .opa = 0xFFU,
            .radius = radius,
        };
        gfx_render_surface_round_rect_fill(disp, surf, &area, &dsc);
    } else {
        gfx_render_surface_fill(disp, surf, &clipped, GFX_COLOR_HEX(rgb), 0xFFU);
    }
}

static void arena_draw_text_n(gfx_display_t *disp, const gfx_render_surface_t *surf,
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
    if (!arena_area_in_clip(surf, box_x, box_y, box_w, box_h)) {
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

    /* Left-aligned text never needs the measuring pass. */
    int pen_x = box_x;
    if (center) {
        const int text_w = arena_text_width(font, tmp);
        if (text_w < box_w) {
            pen_x = box_x + (box_w - text_w) / 2;
        }
    }
    int pen_y = box_y;
    if (line_h < box_h) {
        pen_y = box_y + (box_h - line_h) / 2;
    }

    gfx_arena_scene_t *scene = arena_scene_from_disp(disp);

    const char *p = tmp;
    while (*p) {
        uint32_t unicode = 0;
        if (arena_utf8_to_unicode(&p, &unicode) == 0) {
            continue;
        }

        gfx_glyph_dsc_t dsc;
        if (!font->get_glyph_dsc(font, &dsc, unicode, 0)) {
            continue;
        }
        const int adv = font->get_advance_width(font, &dsc);
        int ofs_y = dsc.ofs_y;
        if (font->adjust_baseline_offset != NULL) {
            ofs_y = font->adjust_baseline_offset(font, &dsc);
        }

        if (dsc.box_w > 0 && dsc.box_h > 0) {
            const int gx0 = pen_x + dsc.ofs_x;
            const int gy0 = pen_y + ofs_y;
            gfx_area_t glyph_area = {
                .x1 = (gfx_coord_t)gx0,
                .y1 = (gfx_coord_t)gy0,
                .x2 = (gfx_coord_t)(gx0 + (int)dsc.box_w),
                .y2 = (gfx_coord_t)(gy0 + (int)dsc.box_h),
            };
            gfx_area_t clipped;
            /* Clip first: fetching/converting the bitmap is the expensive part. */
            if (gfx_area_intersect_exclusive(&clipped, &glyph_area, &surf->clip_area) &&
                    gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
                const uint8_t *bitmap = font->get_glyph_bitmap(font, unicode, &dsc);
                if (bitmap != NULL) {
                    const size_t alpha_bytes = (size_t)dsc.box_w * (size_t)dsc.box_h;
                    bool alpha_owned = false;
                    uint8_t *alpha = arena_glyph_scratch_get(scene, alpha_bytes);
                    if (alpha == NULL) {
                        alpha = (uint8_t *)malloc(alpha_bytes);
                        alpha_owned = true;
                    }
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
                        if (alpha_owned) {
                            free(alpha);
                        }
                    }
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

static void arena_draw_text(gfx_display_t *disp, const gfx_render_surface_t *surf,
                            gfx_font_handle_t font, const char *text,
                            int box_x, int box_y, int box_w, int box_h,
                            uint32_t rgb, bool center)
{
    if (text == NULL) {
        return;
    }
    arena_draw_text_n(disp, surf, font, text, strlen(text),
                      box_x, box_y, box_w, box_h, rgb, center);
}

static void arena_draw_items(gfx_display_t *disp, arena_t *a, arena_node_t *n,
                             int abs_x, int abs_y, const gfx_render_surface_t *surf,
                             gfx_font_handle_t font)
{
    const arena_items_hdr_t *ih = arena_items(a, n->reserved);
    if (ih == NULL || ih->item_count == 0) {
        if ((n->flags & ARENA_F_BG) != 0) {
            arena_fill_bg(disp, surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, 0);
        }
        return;
    }

    if ((n->flags & ARENA_F_BG) != 0) {
        arena_fill_bg(disp, surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb, 0);
    }

    const int row_h = (ih->item_height > 0) ? (int)ih->item_height : 24;
    if (row_h <= 0 || n->h == 0) {
        return;
    }
    const int visible = (int)n->h / row_h;
    if (visible <= 0) {
        return;
    }

    int start = 0;
    if (n->type == ARENA_NODE_WHEEL && ih->selected != ARENA_ITEMS_SELECTED_NONE &&
            ih->selected < ih->item_count) {
        start = (int)ih->selected - visible / 2;
        if (start < 0) {
            start = 0;
        }
        if (start + visible > (int)ih->item_count) {
            start = (int)ih->item_count - visible;
            if (start < 0) {
                start = 0;
            }
        }
    }

    const uint32_t text_rgb = (ih->text_rgb != 0u) ? ih->text_rgb : ARENA_LABEL_DEFAULT_RGB;
    const uint32_t sel_bg = 0x2f8cffu;

    for (int row = 0; row < visible; row++) {
        const int idx = start + row;
        if (idx < 0 || idx >= (int)ih->item_count) {
            break;
        }
        const int y = abs_y + row * row_h;
        const bool selected = (ih->selected != ARENA_ITEMS_SELECTED_NONE &&
                               (int)ih->selected == idx);
        if (selected) {
            arena_fill_bg(disp, surf, abs_x, y, (int)n->w, row_h, sel_bg, 0);
        }
        uint16_t len = 0;
        const char *txt = arena_item_text(a, n->reserved, (uint16_t)idx, &len);
        if (txt != NULL && len > 0) {
            const uint32_t rgb = selected ? 0xFFFFFFu : text_rgb;
            arena_draw_text_n(disp, surf, font, txt, len,
                              abs_x + 6, y, (int)n->w - 12, row_h, rgb, false);
        }
    }
}

static void arena_draw_image(gfx_display_t *disp, arena_t *a, arena_node_t *n,
                             int abs_x, int abs_y, const gfx_render_surface_t *surf)
{
    const arena_img_hdr_t *ih = arena_img(a, n->reserved);
    const uint16_t *px = arena_img_pixels(a, n->reserved);
    if (ih == NULL || px == NULL || n->w == 0 || n->h == 0) {
        return;
    }

    const int draw_w = (n->w < ih->w) ? (int)n->w : (int)ih->w;
    const int draw_h = (n->h < ih->h) ? (int)n->h : (int)ih->h;
    if (draw_w <= 0 || draw_h <= 0) {
        return;
    }

    gfx_area_t dst_area = {
        .x1 = (gfx_coord_t)abs_x,
        .y1 = (gfx_coord_t)abs_y,
        .x2 = (gfx_coord_t)(abs_x + draw_w),
        .y2 = (gfx_coord_t)(abs_y + draw_h),
    };
    gfx_area_t clipped;
    if (!gfx_area_intersect_exclusive(&clipped, &dst_area, &surf->clip_area) ||
            !gfx_area_intersect_exclusive(&clipped, &clipped, &surf->buf_area)) {
        return;
    }

    gfx_render_image_t src = {
        .pixels = px,
        .stride = (gfx_coord_t)ih->w,
        .format = GFX_COLOR_FORMAT_RGB565,
        .alpha = NULL,
        .alpha_stride = 0,
    };
    const gfx_coord_t src_x = (gfx_coord_t)(clipped.x1 - abs_x);
    const gfx_coord_t src_y = (gfx_coord_t)(clipped.y1 - abs_y);

    if (gfx_render_surface_blit_image(disp, surf, &clipped, &src, src_x, src_y, 0xFFU)) {
        return;
    }

    /* Software fallback: clip_area is destination-local for blend helper. */
    gfx_area_t local = {
        .x1 = (gfx_coord_t)(clipped.x1 - surf->buf_area.x1),
        .y1 = (gfx_coord_t)(clipped.y1 - surf->buf_area.y1),
        .x2 = (gfx_coord_t)(clipped.x2 - surf->buf_area.x1),
        .y2 = (gfx_coord_t)(clipped.y2 - surf->buf_area.y1),
    };
    const uint16_t *src_ptr = px + (size_t)src_y * (size_t)ih->w + (size_t)src_x;
    gfx_sw_blend_img_draw_fmt(surf->buf, surf->stride, surf->format,
                              src_ptr, (gfx_coord_t)ih->w,
                              NULL, 0, &local, GFX_COLOR_FORMAT_RGB565, 0xFFU);
}

static void draw_node_disp(gfx_display_t *disp, arena_t *a, uint32_t node_off,
                           int origin_x, int origin_y, const gfx_render_surface_t *surf,
                           gfx_font_handle_t font)
{
    for (uint32_t off = node_off; off != ARENA_NO_NODE; ) {
        arena_node_t *n = arena_node(a, off);
        if (n == NULL) {
            break;
        }

        const int abs_x = origin_x + (int)n->x;
        const int abs_y = origin_y + (int)n->y;

        if ((n->flags & ARENA_F_VISIBLE) != 0) {
            /*
             * Cull by node box vs dirty clip. Children that extend outside the
             * parent box are also skipped — keep descendants within parent bounds
             * (same assumption as typical object clip_children layouts).
             */
            if (!arena_area_in_clip(surf, abs_x, abs_y, (int)n->w, (int)n->h)) {
                off = n->next_sibling;
                continue;
            }

            if (n->type == ARENA_NODE_CONTAINER && (n->flags & ARENA_F_BG) != 0) {
                arena_fill_bg(disp, surf, abs_x, abs_y, (int)n->w, (int)n->h, n->bg_rgb,
                              arena_node_radius(n));
            } else if (n->type == ARENA_NODE_BUTTON && (n->flags & ARENA_F_BG) != 0) {
                uint32_t rgb = n->bg_rgb;
                if ((n->flags & ARENA_F_PRESSED) != 0) {
                    rgb = ((rgb >> 1) & 0x7F7F7Fu);
                }
                uint16_t radius = arena_node_radius(n);
                if (radius == 0U) {
                    radius = ARENA_BUTTON_RADIUS;
                }
                arena_fill_bg(disp, surf, abs_x, abs_y, (int)n->w, (int)n->h, rgb, radius);
                const char *caption = arena_str(a, n->name_off);
                if (caption != NULL) {
                    arena_draw_text(disp, surf, font, caption, abs_x, abs_y,
                                    (int)n->w, (int)n->h, ARENA_BUTTON_TEXT_RGB, true);
                }
            } else if (n->type == ARENA_NODE_LABEL) {
                const char *text = arena_str(a, n->name_off);
                uint32_t rgb = (n->bg_rgb != 0u) ? n->bg_rgb : ARENA_LABEL_DEFAULT_RGB;
                if (text != NULL) {
                    arena_draw_text(disp, surf, font, text, abs_x, abs_y,
                                    (int)n->w, (int)n->h, rgb, false);
                }
            } else if (n->type == ARENA_NODE_IMAGE) {
                arena_draw_image(disp, a, n, abs_x, abs_y, surf);
            } else if (n->type == ARENA_NODE_LIST || n->type == ARENA_NODE_WHEEL) {
                arena_draw_items(disp, a, n, abs_x, abs_y, surf, font);
            }

            if (n->first_child != ARENA_NO_NODE) {
                draw_node_disp(disp, a, n->first_child, abs_x, abs_y, surf, font);
            }
        }

        off = n->next_sibling;
    }
}

/*
 * Pager compose: copy shifted rows from the two page snapshots instead of
 * redrawing the node tree. Snapshots share the surface's 16bpp render
 * format, so rows move with raw memcpy at PSRAM bandwidth.
 */
static int arena_draw_pager_compose(gfx_arena_scene_t *scene, const gfx_render_surface_t *surf)
{
    const int32_t w = (int32_t)scene->pager.w;
    const int32_t h = (int32_t)scene->pager.h;
    const uint16_t *cur = scene->pager.snap[scene->pager.current];
    const uint16_t *nb = scene->pager.snap[scene->pager.current ^ 1U];
    const int32_t dx = scene->pager.drag_dx;

    if (cur == NULL || nb == NULL || w <= 0 || h <= 0) {
        return -1;
    }

    gfx_area_t lim;
    if (!gfx_area_intersect_exclusive(&lim, &surf->clip_area, &surf->buf_area)) {
        return 0;
    }

    /* Current page occupies screen x in [max(0,dx), min(w,w+dx)); the
     * neighbor fills the remaining strip on the opposite side. */
    const int32_t cur_x1 = (dx > 0) ? dx : 0;
    const int32_t cur_x2 = (dx < 0) ? (w + dx) : w;
    const int32_t nb_x1 = (dx < 0) ? (w + dx) : 0;
    const int32_t nb_x2 = (dx < 0) ? w : dx;
    const int32_t nb_src_shift = (dx < 0) ? -(w + dx) : (w - dx);

    uint16_t *dst_base = (uint16_t *)surf->buf;

    for (int32_t y = lim.y1; y < lim.y2 && y < h; y++) {
        uint16_t *dst_row = dst_base +
                            (size_t)(y - surf->buf_area.y1) * (size_t)surf->stride;
        const uint16_t *cur_row = cur + (size_t)y * (size_t)w;
        const uint16_t *nb_row = nb + (size_t)y * (size_t)w;

        int32_t x1 = (cur_x1 > lim.x1) ? cur_x1 : lim.x1;
        int32_t x2 = (cur_x2 < lim.x2) ? cur_x2 : lim.x2;
        if (x2 > x1) {
            memcpy(dst_row + (x1 - surf->buf_area.x1), cur_row + (x1 - dx),
                   (size_t)(x2 - x1) * sizeof(uint16_t));
        }

        x1 = (nb_x1 > lim.x1) ? nb_x1 : lim.x1;
        x2 = (nb_x2 < lim.x2) ? nb_x2 : lim.x2;
        if (x2 > x1) {
            memcpy(dst_row + (x1 - surf->buf_area.x1), nb_row + (x1 + nb_src_shift),
                   (size_t)(x2 - x1) * sizeof(uint16_t));
        }
    }
    return 0;
}

bool arena_draw_covers_clip(gfx_display_t *disp, const gfx_area_t *clip)
{
    gfx_arena_scene_t *scene = arena_scene_from_disp(disp);

    if (scene == NULL || clip == NULL || clip->x2 <= clip->x1 || clip->y2 <= clip->y1) {
        return false;
    }
    /* Compose copies snapshot pixels over the whole clip. */
    if (arena_scene_pager_composing(scene)) {
        return true;
    }
    if (scene->arena.base == NULL) {
        return false;
    }

    const arena_hdr_t *hdr = arena_hdr(&scene->arena);
    arena_node_t *root = arena_node(&scene->arena, hdr->root_off);
    if (root == NULL || root->type != ARENA_NODE_CONTAINER ||
            (root->flags & ARENA_F_VISIBLE) == 0 || (root->flags & ARENA_F_BG) == 0 ||
            arena_node_radius(root) != 0) {
        return false;
    }
    /* Bottom-most root; anything above only adds pixels. */
    return (int)root->x <= (int)clip->x1 && (int)root->y <= (int)clip->y1 &&
           (int)root->x + (int)root->w >= (int)clip->x2 &&
           (int)root->y + (int)root->h >= (int)clip->y2;
}

int arena_draw_clipped(gfx_display_t *disp, const arena_t *arena, const void *render_surface)
{
    if (disp == NULL || arena == NULL || arena->base == NULL || render_surface == NULL) {
        return -1;
    }

    const gfx_render_surface_t *surf = (const gfx_render_surface_t *)render_surface;
    arena_t *a = (arena_t *)arena;

    gfx_font_handle_t font = NULL;
    gfx_arena_scene_t *scene = arena_scene_from_disp(disp);
    if (scene != NULL) {
        font = (gfx_font_handle_t)scene->font_adapter;
        if (arena_scene_pager_composing(scene)) {
            return arena_draw_pager_compose(scene, surf);
        }
    }

    const arena_hdr_t *hdr = arena_hdr(a);
    if (hdr->root_off == ARENA_NO_NODE) {
        return 0;
    }

    draw_node_disp(disp, a, hdr->root_off, 0, 0, surf, font);
    return 0;
}
