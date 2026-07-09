/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena_scene.h"

#include <stdlib.h>
#include <string.h>

#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "fonts/gfx_font_priv.h"

int arena_scene_attach(gfx_display_t *disp, arena_t *arena, gfx_arena_scene_t *out)
{
    if (disp == NULL || arena == NULL || arena->base == NULL || out == NULL) {
        return -1;
    }
    if (disp->arena_scene != NULL) {
        return -2;
    }

    memset(out, 0, sizeof(*out));
    out->arena = *arena;
    out->disp = disp;
    out->pressed_off = ARENA_NO_NODE;
    arena->base = NULL;
    arena->size = 0;

    disp->arena_scene = out;
    return 0;
}

void arena_scene_detach(gfx_arena_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->disp != NULL && scene->disp->arena_scene == scene) {
        scene->disp->arena_scene = NULL;
    }
    arena_scene_set_font(scene, NULL);
    arena_free(&scene->arena);
    scene->disp = NULL;
    scene->actions = NULL;
    scene->action_count = 0;
    scene->pressed_off = ARENA_NO_NODE;
}

gfx_arena_scene_t *arena_scene_from_disp(gfx_display_t *disp)
{
    if (disp == NULL) {
        return NULL;
    }
    return (gfx_arena_scene_t *)disp->arena_scene;
}

void arena_scene_set_actions(gfx_arena_scene_t *scene,
                             const arena_action_entry_t *actions, size_t count)
{
    if (scene == NULL) {
        return;
    }
    scene->actions = actions;
    scene->action_count = count;
}

void arena_scene_set_font(gfx_arena_scene_t *scene, gfx_font_t font)
{
    if (scene == NULL) {
        return;
    }

    if (scene->font_adapter != NULL) {
        free(scene->font_adapter);
        scene->font_adapter = NULL;
    }
    scene->font = font;

    if (font == NULL) {
        return;
    }

    gfx_font_handle_t adapter = (gfx_font_handle_t)calloc(1, sizeof(gfx_font_adapter_t));
    if (adapter == NULL) {
        scene->font = NULL;
        return;
    }
    if (gfx_font_init_adapter(adapter, font) != GFX_OK) {
        free(adapter);
        scene->font = NULL;
        return;
    }
    scene->font_adapter = adapter;
}

int arena_scene_mark_dirty(gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || scene->disp == NULL || node_off == ARENA_NO_NODE) {
        return -1;
    }

    int16_t x1, y1, x2, y2;
    if (arena_node_abs_area(&scene->arena, node_off, &x1, &y1, &x2, &y2) != 0) {
        return -2;
    }

    gfx_area_t area = {
        .x1 = (gfx_coord_t)x1,
        .y1 = (gfx_coord_t)y1,
        .x2 = (gfx_coord_t)x2,
        .y2 = (gfx_coord_t)y2,
    };
    gfx_invalidate_area_disp(scene->disp, &area);
    return 0;
}

int arena_scene_mark_dirty_all(gfx_arena_scene_t *scene)
{
    if (scene == NULL || scene->disp == NULL) {
        return -1;
    }
    gfx_area_t full = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (gfx_coord_t)(scene->disp->res.h_res > 0 ? scene->disp->res.h_res - 1 : 0),
        .y2 = (gfx_coord_t)(scene->disp->res.v_res > 0 ? scene->disp->res.v_res - 1 : 0),
    };
    gfx_invalidate_area_disp(scene->disp, &full);
    return 0;
}

static uint32_t hit_rec(arena_t *a, uint32_t node_off, int ox, int oy, uint16_t x, uint16_t y)
{
    uint32_t hit = ARENA_NO_NODE;

    for (uint32_t off = node_off; off != ARENA_NO_NODE; ) {
        arena_node_t *n = arena_node(a, off);
        if (n == NULL) {
            break;
        }

        const int abs_x = ox + (int)n->x;
        const int abs_y = oy + (int)n->y;
        const int x2 = abs_x + (int)n->w;
        const int y2 = abs_y + (int)n->h;

        if ((n->flags & ARENA_F_VISIBLE) != 0) {
            uint32_t child_hit = ARENA_NO_NODE;
            if (n->first_child != ARENA_NO_NODE) {
                child_hit = hit_rec(a, n->first_child, abs_x, abs_y, x, y);
            }
            if (child_hit != ARENA_NO_NODE) {
                hit = child_hit;
            } else if ((n->flags & ARENA_F_CLICKABLE) != 0 &&
                       (int)x >= abs_x && (int)x < x2 &&
                       (int)y >= abs_y && (int)y < y2) {
                hit = off;
            }
        }

        off = n->next_sibling;
    }

    return hit;
}

uint32_t arena_scene_hit_test(gfx_arena_scene_t *scene, uint16_t x, uint16_t y)
{
    if (scene == NULL || scene->arena.base == NULL) {
        return ARENA_NO_NODE;
    }
    const arena_hdr_t *hdr = arena_hdr(&scene->arena);
    if (hdr->root_off == ARENA_NO_NODE) {
        return ARENA_NO_NODE;
    }
    return hit_rec(&scene->arena, hdr->root_off, 0, 0, x, y);
}

static void run_action(gfx_arena_scene_t *scene, arena_node_t *n, const gfx_touch_event_t *event)
{
    if (scene == NULL || n == NULL || scene->actions == NULL) {
        return;
    }
    /* LIST/WHEEL: reserved points at items blob, not action name. */
    if (n->type == ARENA_NODE_LIST || n->type == ARENA_NODE_WHEEL ||
            n->type == ARENA_NODE_IMAGE) {
        return;
    }
    const char *aname = arena_str(&scene->arena, n->reserved);
    if (aname == NULL) {
        return;
    }
    for (size_t i = 0; i < scene->action_count; i++) {
        if (scene->actions[i].name != NULL &&
                strcmp(scene->actions[i].name, aname) == 0 &&
                scene->actions[i].cb != NULL) {
            scene->actions[i].cb(&scene->arena, n, event, scene->actions[i].user_data);
            return;
        }
    }
}

static void select_items_at(gfx_arena_scene_t *scene, arena_node_t *n, uint32_t node_off,
                            uint16_t x, uint16_t y)
{
    if (scene == NULL || n == NULL ||
            (n->type != ARENA_NODE_LIST && n->type != ARENA_NODE_WHEEL)) {
        return;
    }
    arena_items_hdr_t *ih = arena_items_mut(&scene->arena, n->reserved);
    if (ih == NULL || ih->item_count == 0) {
        return;
    }

    int16_t x1, y1, x2, y2;
    if (arena_node_abs_area(&scene->arena, node_off, &x1, &y1, &x2, &y2) != 0) {
        return;
    }
    (void)x2;
    (void)x;

    const int row_h = (ih->item_height > 0) ? (int)ih->item_height : 24;
    if (row_h <= 0) {
        return;
    }
    const int visible = ((int)n->h) / row_h;
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

    const int local_y = (int)y - (int)y1;
    if (local_y < 0 || local_y >= (int)n->h) {
        return;
    }
    const int row = local_y / row_h;
    const int idx = start + row;
    if (idx < 0 || idx >= (int)ih->item_count) {
        return;
    }
    if (ih->selected != (uint16_t)idx) {
        ih->selected = (uint16_t)idx;
        (void)arena_scene_mark_dirty(scene, node_off);
    }
}

int arena_scene_handle_touch(gfx_display_t *disp, const gfx_touch_event_t *event)
{
    gfx_arena_scene_t *scene = arena_scene_from_disp(disp);
    if (scene == NULL || event == NULL) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        uint32_t hit = arena_scene_hit_test(scene, event->x, event->y);
        scene->pressed_off = hit;
        scene->pressed_track = event->track_id;
        if (hit != ARENA_NO_NODE) {
            arena_node_t *n = arena_node(&scene->arena, hit);
            if (n != NULL) {
                n->flags |= ARENA_F_PRESSED;
                (void)arena_scene_mark_dirty(scene, hit);
            }
            return 1;
        }
        return 0;
    }

    if (scene->pressed_off == ARENA_NO_NODE) {
        return 0;
    }

    if (event->track_id != scene->pressed_track) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        arena_node_t *n = arena_node(&scene->arena, scene->pressed_off);
        uint32_t off = scene->pressed_off;
        if (n != NULL) {
            n->flags = (uint16_t)(n->flags & ~ARENA_F_PRESSED);
            (void)arena_scene_mark_dirty(scene, off);
            uint32_t hit = arena_scene_hit_test(scene, event->x, event->y);
            if (hit == off) {
                if (n->type == ARENA_NODE_LIST || n->type == ARENA_NODE_WHEEL) {
                    select_items_at(scene, n, off, event->x, event->y);
                } else {
                    run_action(scene, n, event);
                }
            }
        }
        scene->pressed_off = ARENA_NO_NODE;
        return 1;
    }

    /* MOVE: keep capture; optional visual stay pressed */
    return 1;
}

uint8_t arena_scene_test_dirty_count(gfx_display_t *disp)
{
    if (disp == NULL) {
        return 0;
    }
    return disp->dirty.count;
}

int arena_scene_test_dirty_area(gfx_display_t *disp, uint8_t index, gfx_area_t *out)
{
    if (disp == NULL || out == NULL || index >= disp->dirty.count) {
        return -1;
    }
    *out = disp->dirty.areas[index];
    return 0;
}
