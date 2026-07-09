/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arena_gfx_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/widgets/button.h"
#include "gfx/widgets/container.h"
#include "gfx/widgets/label.h"

static uint32_t node_off_at_index(const arena_t *a, uint16_t idx)
{
    const arena_hdr_t *hdr = (const arena_hdr_t *)a->base;
    return hdr->nodes_off + (uint32_t)idx * (uint32_t)sizeof(arena_node_t);
}

static int parent_index_of(arena_t *a, uint16_t child_idx)
{
    const arena_hdr_t *hdr = arena_hdr(a);
    const uint32_t child_off = node_off_at_index(a, child_idx);

    for (uint16_t i = 0; i < hdr->node_count; i++) {
        arena_node_t *n = arena_node(a, node_off_at_index(a, i));
        for (uint32_t c = n->first_child; c != ARENA_NO_NODE; ) {
            if (c == child_off) {
                return (int)i;
            }
            arena_node_t *cn = arena_node(a, c);
            c = cn->next_sibling;
        }
    }
    return -1;
}

int arena_gfx_sync_node(arena_gfx_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || scene->objs == NULL) {
        return -1;
    }

    const uint32_t idx = arena_node_index(&scene->arena, arena_node(&scene->arena, node_off));
    if (idx == ARENA_NO_NODE || idx >= scene->obj_count) {
        return -2;
    }

    arena_node_t *n = arena_node(&scene->arena, node_off);
    gfx_object_t *o = scene->objs[idx];
    if (n == NULL || o == NULL) {
        return -3;
    }

    (void)gfx_object_set_pos(o, n->x, n->y);
    (void)gfx_object_set_size(o, n->w, n->h);
    (void)gfx_object_set_visible(o, (n->flags & ARENA_F_VISIBLE) != 0);

    const char *name = arena_str(&scene->arena, n->name_off);

    switch (n->type) {
    case ARENA_NODE_CONTAINER:
        if (n->flags & ARENA_F_BG) {
            (void)gfx_container_set_bg_enable(o, true);
            (void)gfx_container_set_bg_color(o, GFX_COLOR_HEX(n->bg_rgb));
        }
        break;
    case ARENA_NODE_LABEL:
        if (scene->font != NULL) {
            (void)gfx_label_set_font(o, scene->font);
        }
        if (name != NULL) {
            /* Demo: label text = node name (arena has no separate text field yet). */
            (void)gfx_label_set_text(o, name);
        }
        (void)gfx_label_set_color(o, GFX_COLOR_HEX(0xF3F7FA));
        break;
    case ARENA_NODE_BUTTON:
        if (scene->font != NULL) {
            (void)gfx_button_set_font(o, scene->font);
        }
        if (name != NULL) {
            (void)gfx_button_set_text(o, name);
        }
        if (n->flags & ARENA_F_BG) {
            (void)gfx_button_set_bg_color(o, GFX_COLOR_HEX(n->bg_rgb));
        }
        (void)gfx_button_set_text_color(o, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_button_set_radius(o, 6);
        break;
    case ARENA_NODE_IMAGE:
        if (n->flags & ARENA_F_BG) {
            (void)gfx_container_set_bg_enable(o, true);
            (void)gfx_container_set_bg_color(o, GFX_COLOR_HEX(n->bg_rgb != 0 ? n->bg_rgb : 0x444444));
        }
        break;
    default:
        return -4;
    }

    return 0;
}

int arena_gfx_sync_all(arena_gfx_scene_t *scene)
{
    if (scene == NULL) {
        return -1;
    }
    const arena_hdr_t *hdr = arena_hdr(&scene->arena);
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        int rc = arena_gfx_sync_node(scene, node_off_at_index(&scene->arena, i));
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

int arena_gfx_bind(const uint8_t *pkg, size_t pkg_size, gfx_display_t *disp,
                   gfx_font_t font, arena_gfx_scene_t *out)
{
    if (pkg == NULL || disp == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->disp = disp;
    out->font = font;

    if (arena_load(pkg, pkg_size, &out->arena) != 0) {
        return -2;
    }

    const arena_hdr_t *hdr = arena_hdr(&out->arena);
    out->objs = (gfx_object_t **)calloc(hdr->node_count, sizeof(*out->objs));
    if (out->objs == NULL) {
        arena_free(&out->arena);
        return -3;
    }
    out->obj_count = hdr->node_count;

    /* Pass 1: create objects in index order (parents first by pack rule). */
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        arena_node_t *n = arena_node(&out->arena, node_off_at_index(&out->arena, i));
        gfx_object_t *o = NULL;
        switch (n->type) {
        case ARENA_NODE_CONTAINER:
            o = gfx_container_create(disp);
            break;
        case ARENA_NODE_LABEL:
            o = gfx_label_create(disp);
            break;
        case ARENA_NODE_BUTTON:
            o = gfx_button_create(disp);
            break;
        case ARENA_NODE_IMAGE:
            /* Bridge does not mirror images; placeholder container. */
            o = gfx_container_create(disp);
            break;
        default:
            arena_gfx_free(out);
            return -4;
        }
        if (o == NULL) {
            arena_gfx_free(out);
            return -5;
        }
        out->objs[i] = o;
    }

    /* Pass 2: parent links. */
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        const int p = parent_index_of(&out->arena, i);
        if (p >= 0) {
            if (gfx_object_add_child(out->objs[p], out->objs[i]) != GFX_OK) {
                arena_gfx_free(out);
                return -6;
            }
        }
    }

    if (arena_gfx_sync_all(out) != 0) {
        arena_gfx_free(out);
        return -7;
    }
    return 0;
}

gfx_object_t *arena_gfx_find_obj(arena_gfx_scene_t *scene, const char *name)
{
    if (scene == NULL || name == NULL) {
        return NULL;
    }
    arena_node_t *n = arena_find_by_name(&scene->arena, name);
    if (n == NULL) {
        return NULL;
    }
    const uint32_t idx = arena_node_index(&scene->arena, n);
    if (idx == ARENA_NO_NODE || idx >= scene->obj_count) {
        return NULL;
    }
    return scene->objs[idx];
}

void arena_gfx_free(arena_gfx_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->objs != NULL) {
        /* Delete roots only; children are owned by parents / display. */
        const arena_hdr_t *hdr = scene->arena.base != NULL ? arena_hdr(&scene->arena) : NULL;
        if (hdr != NULL) {
            for (uint16_t i = 0; i < scene->obj_count; i++) {
                if (scene->objs[i] == NULL) {
                    continue;
                }
                if (parent_index_of(&scene->arena, i) < 0) {
                    (void)gfx_object_delete(scene->objs[i]);
                }
                scene->objs[i] = NULL;
            }
        }
        free(scene->objs);
        scene->objs = NULL;
    }
    arena_free(&scene->arena);
    scene->obj_count = 0;
    scene->disp = NULL;
    scene->font = NULL;
}
