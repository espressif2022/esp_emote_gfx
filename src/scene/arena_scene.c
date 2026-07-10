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
#include "gfx/ease.h"
#include "gfx/object.h"
#include "gfx/widgets/anim.h"
#include "gfx/widgets/coverflow_core.h"
#include "gfx/widgets/list_core.h"
#include "gfx/widgets/motion.h"
#include "gfx/widgets/pageflow_core.h"
#include "gfx/widgets/progress_core.h"
#include "gfx/widgets/wheel_core.h"
#include "platform/gfx_platform.h"
#include "widgets/img/gfx_image_resource_priv.h"

struct gfx_arena_img_rt {
    uint32_t             node_off;
    uint16_t             item_index; /* GFX_ARENA_IMG_ITEM_NONE or card index */
    uint8_t              face;       /* 0 normal, 1 pressed */
    uint8_t              used;
    gfx_image_resource_t resource;
};

static void img_rt_clear_all(gfx_arena_scene_t *scene)
{
    uint16_t i;
    if (scene == NULL || scene->img_rt == NULL) {
        return;
    }
    for (i = 0; i < scene->img_rt_count; i++) {
        if (scene->img_rt[i].used) {
            gfx_image_resource_close(&scene->img_rt[i].resource);
            scene->img_rt[i].used = 0;
        }
    }
    free(scene->img_rt);
    scene->img_rt = NULL;
    scene->img_rt_count = 0;
}

static int img_rt_ensure_table(gfx_arena_scene_t *scene)
{
    if (scene == NULL) {
        return -1;
    }
    if (scene->img_rt != NULL) {
        return 0;
    }
    scene->img_rt = (gfx_arena_img_rt_t *)calloc(GFX_ARENA_IMG_RT_MAX, sizeof(gfx_arena_img_rt_t));
    if (scene->img_rt == NULL) {
        return -1;
    }
    scene->img_rt_count = GFX_ARENA_IMG_RT_MAX;
    return 0;
}

static gfx_arena_img_rt_t *img_rt_find_mut(gfx_arena_scene_t *scene, uint32_t node_off,
                                       uint16_t item_index, uint8_t face)
{
    uint16_t i;
    if (scene == NULL || scene->img_rt == NULL) {
        return NULL;
    }
    for (i = 0; i < scene->img_rt_count; i++) {
        if (scene->img_rt[i].used &&
                scene->img_rt[i].node_off == node_off &&
                scene->img_rt[i].item_index == item_index &&
                scene->img_rt[i].face == face) {
            return &scene->img_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_img_rt_t *img_rt_alloc(gfx_arena_scene_t *scene, uint32_t node_off,
                                    uint16_t item_index, uint8_t face)
{
    uint16_t i;
    gfx_arena_img_rt_t *slot;
    if (img_rt_ensure_table(scene) != 0) {
        return NULL;
    }
    slot = img_rt_find_mut(scene, node_off, item_index, face);
    if (slot != NULL) {
        return slot;
    }
    for (i = 0; i < scene->img_rt_count; i++) {
        if (!scene->img_rt[i].used) {
            memset(&scene->img_rt[i], 0, sizeof(scene->img_rt[i]));
            scene->img_rt[i].used = 1;
            scene->img_rt[i].node_off = node_off;
            scene->img_rt[i].item_index = item_index;
            scene->img_rt[i].face = face;
            return &scene->img_rt[i];
        }
    }
    return NULL;
}

static uint32_t gfx_arena_now_ms(void)
{
    return (uint32_t)(gfx_platform_time_us() / 1000);
}

static int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static uint16_t wheel_row_h(const gfx_arena_items_hdr_t *ih)
{
    return (ih != NULL && ih->item_height > 0) ? ih->item_height : 24u;
}

static bool wheel_cyclic(const gfx_arena_items_hdr_t *ih)
{
    return ih != NULL && (ih->flags & GFX_ARENA_ITEMS_F_CYCLIC) != 0;
}

static int32_t coverflow_spacing(const gfx_arena_node_t *n)
{
    if (n == NULL) {
        return GFX_COVERFLOW_CORE_PAGE_THRESHOLD;
    }
    return gfx_coverflow_core_spacing((int32_t)n->w, GFX_COVERFLOW_CORE_SPACING_PCT,
                                      GFX_COVERFLOW_CORE_PAGE_THRESHOLD);
}

static void run_action(gfx_arena_scene_t *scene, gfx_arena_node_t *n, const gfx_touch_event_t *event);
static int handle_list_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                             const gfx_touch_event_t *event);
static int handle_wheel_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                              const gfx_touch_event_t *event);
static int handle_coverflow_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                  const gfx_touch_event_t *event);
static int handle_pageflow_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                 const gfx_touch_event_t *event);
static int handle_progress_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                 const gfx_touch_event_t *event);
static int handle_simple_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                               const gfx_touch_event_t *event);

typedef int (*gfx_arena_touch_fn_t)(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                const gfx_touch_event_t *event);

typedef struct {
    gfx_arena_touch_fn_t touch;
    uint8_t          action_by_name; /* 1: match action by node name; 0: by reserved */
} gfx_arena_type_ops_t;

/*
 * Indexed by GFX_ARENA_NODE_* (1-based). Register new interactive types here only —
 * do not grow if-chains in gfx_arena_scene_handle_touch / run_action.
 */
static const gfx_arena_type_ops_t s_arena_type_ops[] = {
    [GFX_ARENA_NODE_CONTAINER]    = { .touch = handle_simple_touch,    .action_by_name = 0 },
    [GFX_ARENA_NODE_LABEL]        = { .touch = NULL,                   .action_by_name = 0 },
    [GFX_ARENA_NODE_BUTTON]       = { .touch = handle_simple_touch,    .action_by_name = 0 },
    [GFX_ARENA_NODE_IMAGE]        = { .touch = handle_simple_touch,    .action_by_name = 1 },
    [GFX_ARENA_NODE_LIST]         = { .touch = handle_list_touch,      .action_by_name = 1 },
    [GFX_ARENA_NODE_WHEEL]        = { .touch = handle_wheel_touch,     .action_by_name = 1 },
    [GFX_ARENA_NODE_IMAGE_BUTTON] = { .touch = handle_simple_touch,    .action_by_name = 1 },
    [GFX_ARENA_NODE_PROGRESS]     = { .touch = handle_progress_touch,  .action_by_name = 1 },
    [GFX_ARENA_NODE_COVERFLOW]    = { .touch = handle_coverflow_touch, .action_by_name = 1 },
    [GFX_ARENA_NODE_PAGEFLOW]     = { .touch = handle_pageflow_touch,  .action_by_name = 1 },
    [GFX_ARENA_NODE_ANIM]         = { .touch = NULL,                   .action_by_name = 1 },
    [GFX_ARENA_NODE_MOTION]       = { .touch = NULL,                   .action_by_name = 1 },
};

static const gfx_arena_type_ops_t *gfx_arena_type_ops(uint16_t type)
{
    if (type == 0U || type >= (uint16_t)(sizeof(s_arena_type_ops) / sizeof(s_arena_type_ops[0]))) {
        return NULL;
    }
    return &s_arena_type_ops[type];
}

int gfx_arena_scene_attach(gfx_display_t *disp, gfx_arena_t *arena, gfx_arena_scene_t *out)
{
    if (disp == NULL || arena == NULL || arena->base == NULL || out == NULL) {
        return -1;
    }
    if (disp->gfx_arena_scene != NULL) {
        return -2;
    }

    memset(out, 0, sizeof(*out));
    out->arena = *arena;
    out->disp = disp;
    out->pressed_off = GFX_ARENA_NO_NODE;
    out->img_rt = NULL;
    out->img_rt_count = 0;
    arena->base = NULL;
    arena->size = 0;

    disp->gfx_arena_scene = out;
    return 0;
}

void gfx_arena_scene_detach(gfx_arena_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->disp != NULL && scene->disp->gfx_arena_scene == scene) {
        scene->disp->gfx_arena_scene = NULL;
    }
    gfx_arena_scene_set_font(scene, NULL);
    img_rt_clear_all(scene);
    for (int i = 0; i < GFX_ARENA_ANIM_RT_MAX; i++) {
        if (scene->anim_rt[i].used && scene->anim_rt[i].obj != NULL) {
            (void)gfx_object_delete(scene->anim_rt[i].obj);
            scene->anim_rt[i].obj = NULL;
        }
    }
    for (int i = 0; i < GFX_ARENA_MOTION_RT_MAX; i++) {
        if (scene->motion_rt[i].used && scene->motion_rt[i].player != NULL) {
            gfx_motion_player_delete(scene->motion_rt[i].player);
            scene->motion_rt[i].player = NULL;
        }
    }
    gfx_arena_free(&scene->arena);
    scene->disp = NULL;
    scene->actions = NULL;
    scene->action_count = 0;
    scene->pressed_off = GFX_ARENA_NO_NODE;
    memset(scene->list_rt, 0, sizeof(scene->list_rt));
    memset(scene->cover_rt, 0, sizeof(scene->cover_rt));
    memset(scene->page_rt, 0, sizeof(scene->page_rt));
    memset(scene->anim_rt, 0, sizeof(scene->anim_rt));
    memset(scene->motion_rt, 0, sizeof(scene->motion_rt));
}

gfx_arena_scene_t *gfx_arena_scene_from_disp(gfx_display_t *disp)
{
    if (disp == NULL) {
        return NULL;
    }
    return (gfx_arena_scene_t *)disp->gfx_arena_scene;
}

void gfx_arena_scene_set_actions(gfx_arena_scene_t *scene,
                             const gfx_arena_action_entry_t *actions, size_t count)
{
    if (scene == NULL) {
        return;
    }
    scene->actions = actions;
    scene->action_count = count;
}

void gfx_arena_scene_set_font(gfx_arena_scene_t *scene, gfx_font_t font)
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

int gfx_arena_scene_mark_dirty(gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || scene->disp == NULL || node_off == GFX_ARENA_NO_NODE) {
        return -1;
    }

    int16_t x1, y1, x2, y2;
    if (gfx_arena_node_abs_area(&scene->arena, node_off, &x1, &y1, &x2, &y2) != 0) {
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

int gfx_arena_scene_mark_dirty_all(gfx_arena_scene_t *scene)
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

static gfx_arena_list_rt_t *list_rt_find(gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || node_off == GFX_ARENA_NO_NODE) {
        return NULL;
    }
    for (int i = 0; i < GFX_ARENA_LIST_RT_MAX; i++) {
        if (scene->list_rt[i].used && scene->list_rt[i].node_off == node_off) {
            return &scene->list_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_list_rt_t *list_rt_get(gfx_arena_scene_t *scene, uint32_t node_off)
{
    gfx_arena_list_rt_t *rt = list_rt_find(scene, node_off);
    if (rt != NULL) {
        return rt;
    }
    for (int i = 0; i < GFX_ARENA_LIST_RT_MAX; i++) {
        if (!scene->list_rt[i].used) {
            memset(&scene->list_rt[i], 0, sizeof(scene->list_rt[i]));
            scene->list_rt[i].used = 1;
            scene->list_rt[i].node_off = node_off;
            return &scene->list_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_coverflow_rt_t *cover_rt_find(gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || node_off == GFX_ARENA_NO_NODE) {
        return NULL;
    }
    for (int i = 0; i < GFX_ARENA_COVERFLOW_RT_MAX; i++) {
        if (scene->cover_rt[i].used && scene->cover_rt[i].node_off == node_off) {
            return &scene->cover_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_coverflow_rt_t *cover_rt_get(gfx_arena_scene_t *scene, uint32_t node_off)
{
    gfx_arena_coverflow_rt_t *rt = cover_rt_find(scene, node_off);
    if (rt != NULL) {
        return rt;
    }
    for (int i = 0; i < GFX_ARENA_COVERFLOW_RT_MAX; i++) {
        if (!scene->cover_rt[i].used) {
            memset(&scene->cover_rt[i], 0, sizeof(scene->cover_rt[i]));
            scene->cover_rt[i].used = 1;
            scene->cover_rt[i].node_off = node_off;
            return &scene->cover_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_pageflow_rt_t *page_rt_find(gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || node_off == GFX_ARENA_NO_NODE) {
        return NULL;
    }
    for (int i = 0; i < GFX_ARENA_PAGEFLOW_RT_MAX; i++) {
        if (scene->page_rt[i].used && scene->page_rt[i].node_off == node_off) {
            return &scene->page_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_pageflow_rt_t *page_rt_get(gfx_arena_scene_t *scene, uint32_t node_off)
{
    gfx_arena_pageflow_rt_t *rt = page_rt_find(scene, node_off);
    if (rt != NULL) {
        return rt;
    }
    for (int i = 0; i < GFX_ARENA_PAGEFLOW_RT_MAX; i++) {
        if (!scene->page_rt[i].used) {
            memset(&scene->page_rt[i], 0, sizeof(scene->page_rt[i]));
            scene->page_rt[i].used = 1;
            scene->page_rt[i].node_off = node_off;
            return &scene->page_rt[i];
        }
    }
    return NULL;
}

int32_t gfx_arena_scene_list_scroll_y(const gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL) {
        return 0;
    }
    for (int i = 0; i < GFX_ARENA_LIST_RT_MAX; i++) {
        if (scene->list_rt[i].used && scene->list_rt[i].node_off == node_off) {
            return scene->list_rt[i].scroll_y;
        }
    }
    return 0;
}

int32_t gfx_arena_scene_coverflow_drag(const gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL) {
        return 0;
    }
    for (int i = 0; i < GFX_ARENA_COVERFLOW_RT_MAX; i++) {
        if (scene->cover_rt[i].used && scene->cover_rt[i].node_off == node_off) {
            return scene->cover_rt[i].drag_offset;
        }
    }
    return 0;
}

int32_t gfx_arena_scene_pageflow_drag(const gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL) {
        return 0;
    }
    for (int i = 0; i < GFX_ARENA_PAGEFLOW_RT_MAX; i++) {
        if (scene->page_rt[i].used && scene->page_rt[i].node_off == node_off) {
            return scene->page_rt[i].drag_offset;
        }
    }
    return 0;
}

static gfx_arena_anim_rt_t *anim_rt_find(gfx_arena_scene_t *scene, uint32_t node_off)
{
    int i;
    if (scene == NULL) {
        return NULL;
    }
    for (i = 0; i < GFX_ARENA_ANIM_RT_MAX; i++) {
        if (scene->anim_rt[i].used && scene->anim_rt[i].node_off == node_off) {
            return &scene->anim_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_anim_rt_t *anim_rt_alloc(gfx_arena_scene_t *scene, uint32_t node_off)
{
    int i;
    gfx_arena_anim_rt_t *slot = anim_rt_find(scene, node_off);
    if (slot != NULL) {
        return slot;
    }
    for (i = 0; i < GFX_ARENA_ANIM_RT_MAX; i++) {
        if (!scene->anim_rt[i].used) {
            memset(&scene->anim_rt[i], 0, sizeof(scene->anim_rt[i]));
            scene->anim_rt[i].used = 1;
            scene->anim_rt[i].node_off = node_off;
            return &scene->anim_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_motion_rt_t *motion_rt_find(gfx_arena_scene_t *scene, uint32_t node_off)
{
    int i;
    if (scene == NULL) {
        return NULL;
    }
    for (i = 0; i < GFX_ARENA_MOTION_RT_MAX; i++) {
        if (scene->motion_rt[i].used && scene->motion_rt[i].node_off == node_off) {
            return &scene->motion_rt[i];
        }
    }
    return NULL;
}

static gfx_arena_motion_rt_t *motion_rt_alloc(gfx_arena_scene_t *scene, uint32_t node_off)
{
    int i;
    gfx_arena_motion_rt_t *slot = motion_rt_find(scene, node_off);
    if (slot != NULL) {
        return slot;
    }
    for (i = 0; i < GFX_ARENA_MOTION_RT_MAX; i++) {
        if (!scene->motion_rt[i].used) {
            memset(&scene->motion_rt[i], 0, sizeof(scene->motion_rt[i]));
            scene->motion_rt[i].used = 1;
            scene->motion_rt[i].node_off = node_off;
            return &scene->motion_rt[i];
        }
    }
    return NULL;
}

static void anim_rt_sync_geom(gfx_arena_scene_t *scene, gfx_arena_anim_rt_t *rt)
{
    gfx_arena_node_t *n;
    int16_t x1, y1, x2, y2;
    bool visible;

    if (scene == NULL || rt == NULL || rt->obj == NULL) {
        return;
    }
    n = gfx_arena_node(&scene->arena, rt->node_off);
    if (n == NULL || n->type != GFX_ARENA_NODE_ANIM) {
        return;
    }
    if (gfx_arena_node_abs_area(&scene->arena, rt->node_off, &x1, &y1, &x2, &y2) != 0) {
        return;
    }
    visible = (n->flags & GFX_ARENA_F_VISIBLE) != 0;
    (void)gfx_object_set_pos(rt->obj, x1, y1);
    (void)gfx_object_set_size(rt->obj, (uint16_t)(x2 - x1), (uint16_t)(y2 - y1));
    (void)gfx_object_set_visible(rt->obj, visible);
    if (visible && rt->playing) {
        (void)gfx_anim_start(rt->obj);
    } else if (!visible) {
        (void)gfx_anim_stop(rt->obj);
    }
}

int gfx_arena_scene_bind_anim_src(gfx_arena_scene_t *scene, uint32_t node_off,
                              const gfx_anim_src_t *src)
{
    gfx_arena_anim_rt_t *rt;
    gfx_arena_node_t *n;
    const gfx_arena_anim_hdr_t *ah;
    gfx_anim_src_t local_src;
    const gfx_anim_src_t *use_src = src;
    uint16_t fps;
    bool loop;

    if (scene == NULL || scene->disp == NULL || node_off == GFX_ARENA_NO_NODE) {
        return -1;
    }
    n = gfx_arena_node(&scene->arena, node_off);
    if (n == NULL || n->type != GFX_ARENA_NODE_ANIM) {
        return -2;
    }
    ah = gfx_arena_anim(&scene->arena, n->reserved);
    if (ah == NULL) {
        return -3;
    }
    if (use_src == NULL) {
        const char *path = gfx_arena_anim_file_path(&scene->arena, n->reserved);
        if (path == NULL) {
            return -4;
        }
        local_src = (gfx_anim_src_t) {
            .type = GFX_ANIM_SRC_TYPE_FILE,
            .data = path,
        };
        use_src = &local_src;
    }

    rt = anim_rt_alloc(scene, node_off);
    if (rt == NULL) {
        return -5;
    }
    if (rt->obj == NULL) {
        rt->obj = gfx_anim_create(scene->disp);
        if (rt->obj == NULL) {
            rt->used = 0;
            return -6;
        }
        (void)gfx_object_set_visible(rt->obj, false);
    }

    fps = (ah->fps != 0u) ? ah->fps : 50u;
    loop = (ah->flags & GFX_ARENA_ANIM_F_LOOP) != 0;
    (void)gfx_anim_stop(rt->obj);
    if (gfx_anim_set_src_desc(rt->obj, use_src) != GFX_OK) {
        return -7;
    }
    if (gfx_anim_set_segment(rt->obj, ah->start_frame, ah->end_frame, fps, loop) != GFX_OK) {
        return -8;
    }
    rt->playing = 0;
    anim_rt_sync_geom(scene, rt);
    (void)gfx_arena_scene_mark_dirty(scene, node_off);
    return 0;
}

int gfx_arena_scene_ensure_pkg_anims(gfx_arena_scene_t *scene, uint32_t node_off)
{
    gfx_arena_anim_rt_t *rt = anim_rt_find(scene, node_off);
    if (rt != NULL && rt->obj != NULL) {
        return 0;
    }
    return gfx_arena_scene_bind_anim_src(scene, node_off, NULL);
}

int gfx_arena_scene_anim_set_playing(gfx_arena_scene_t *scene, uint32_t node_off, bool playing)
{
    gfx_arena_anim_rt_t *rt = anim_rt_find(scene, node_off);
    if (rt == NULL || rt->obj == NULL) {
        if (gfx_arena_scene_ensure_pkg_anims(scene, node_off) != 0) {
            return -1;
        }
        rt = anim_rt_find(scene, node_off);
    }
    if (rt == NULL || rt->obj == NULL) {
        return -2;
    }
    rt->playing = playing ? 1u : 0u;
    anim_rt_sync_geom(scene, rt);
    if (playing) {
        return (gfx_anim_start(rt->obj) == GFX_OK) ? 0 : -3;
    }
    (void)gfx_anim_stop(rt->obj);
    return 0;
}

int gfx_arena_scene_bind_motion_asset(gfx_arena_scene_t *scene, uint32_t node_off,
                                  const gfx_motion_asset_t *asset)
{
    gfx_arena_motion_rt_t *rt;
    gfx_arena_node_t *n;
    const gfx_arena_motion_hdr_t *mh;

    if (scene == NULL || scene->disp == NULL || node_off == GFX_ARENA_NO_NODE || asset == NULL) {
        return -1;
    }
    n = gfx_arena_node(&scene->arena, node_off);
    if (n == NULL || n->type != GFX_ARENA_NODE_MOTION) {
        return -2;
    }
    mh = gfx_arena_motion(&scene->arena, n->reserved);
    if (mh == NULL) {
        return -3;
    }

    rt = motion_rt_alloc(scene, node_off);
    if (rt == NULL) {
        return -4;
    }
    if (rt->player != NULL && rt->asset != asset) {
        gfx_motion_player_delete(rt->player);
        rt->player = NULL;
    }
    if (rt->player == NULL) {
        rt->player = gfx_motion_player_create(scene->disp, asset);
        if (rt->player == NULL) {
            rt->used = 0;
            return -5;
        }
        rt->asset = asset;
        if (mh->stroke_rgb != 0u) {
            (void)gfx_motion_player_set_color(rt->player, GFX_COLOR_HEX(mh->stroke_rgb));
        }
        (void)gfx_motion_player_set_action_loop(rt->player,
                                                (mh->flags & GFX_ARENA_MOTION_F_LOOP) != 0);
        (void)gfx_motion_player_set_visible(rt->player, false);
        (void)gfx_motion_player_set_action(rt->player, mh->action_idx, true);
    }
    return gfx_arena_scene_motion_sync(scene, node_off);
}

int gfx_arena_scene_motion_set_action(gfx_arena_scene_t *scene, uint32_t node_off,
                                  uint16_t action_idx, bool snap)
{
    gfx_arena_motion_rt_t *rt = motion_rt_find(scene, node_off);
    gfx_arena_node_t *n;
    gfx_arena_motion_hdr_t *mh;

    if (rt == NULL || rt->player == NULL) {
        return -1;
    }
    n = gfx_arena_node(&scene->arena, node_off);
    if (n == NULL) {
        return -2;
    }
    mh = gfx_arena_motion_mut(&scene->arena, n->reserved);
    if (mh != NULL) {
        mh->action_idx = action_idx;
    }
    if (gfx_motion_player_set_action(rt->player, action_idx, snap) != GFX_OK) {
        return -3;
    }
    (void)gfx_arena_scene_mark_dirty(scene, node_off);
    return 0;
}

int gfx_arena_scene_motion_sync(gfx_arena_scene_t *scene, uint32_t node_off)
{
    gfx_arena_motion_rt_t *rt = motion_rt_find(scene, node_off);
    gfx_arena_node_t *n;
    int16_t x1, y1, x2, y2;
    bool visible;

    if (scene == NULL || rt == NULL || rt->player == NULL) {
        return -1;
    }
    n = gfx_arena_node(&scene->arena, node_off);
    if (n == NULL || n->type != GFX_ARENA_NODE_MOTION) {
        return -2;
    }
    if (gfx_arena_node_abs_area(&scene->arena, node_off, &x1, &y1, &x2, &y2) != 0) {
        return -3;
    }
    visible = (n->flags & GFX_ARENA_F_VISIBLE) != 0;
    (void)gfx_motion_player_set_canvas(rt->player, x1, y1,
                                       (uint16_t)(x2 - x1), (uint16_t)(y2 - y1));
    (void)gfx_motion_player_set_visible(rt->player, visible);
    (void)gfx_motion_player_sync(rt->player);
    return 0;
}

static void gfx_arena_scene_sync_anim_motion(gfx_arena_scene_t *scene)
{
    int i;
    if (scene == NULL) {
        return;
    }
    for (i = 0; i < GFX_ARENA_ANIM_RT_MAX; i++) {
        if (scene->anim_rt[i].used) {
            anim_rt_sync_geom(scene, &scene->anim_rt[i]);
        }
    }
    for (i = 0; i < GFX_ARENA_MOTION_RT_MAX; i++) {
        if (scene->motion_rt[i].used) {
            (void)gfx_arena_scene_motion_sync(scene, scene->motion_rt[i].node_off);
        }
    }
}

int gfx_arena_scene_bind_image_src(gfx_arena_scene_t *scene, uint32_t node_off,
                               uint16_t item_index, uint8_t face,
                               const gfx_image_src_t *src)
{
    gfx_arena_img_rt_t *slot;
    gfx_err_t err;

    if (scene == NULL || src == NULL || node_off == GFX_ARENA_NO_NODE) {
        return -1;
    }
    slot = img_rt_alloc(scene, node_off, item_index, face);
    if (slot == NULL) {
        return -2;
    }
    err = gfx_image_resource_set_source(&slot->resource, src);
    if (err != GFX_OK) {
        slot->used = 0;
        return -3;
    }
    err = gfx_image_resource_open(&slot->resource);
    if (err != GFX_OK) {
        gfx_image_resource_close(&slot->resource);
        slot->used = 0;
        return -4;
    }
    (void)gfx_arena_scene_mark_dirty(scene, node_off);
    return 0;
}

void gfx_arena_scene_unbind_image(gfx_arena_scene_t *scene, uint32_t node_off,
                              uint16_t item_index, uint8_t face)
{
    uint16_t i;
    if (scene == NULL || scene->img_rt == NULL) {
        return;
    }
    for (i = 0; i < scene->img_rt_count; i++) {
        if (!scene->img_rt[i].used || scene->img_rt[i].node_off != node_off) {
            continue;
        }
        if (item_index != GFX_ARENA_IMG_ITEM_NONE &&
                scene->img_rt[i].item_index != item_index) {
            continue;
        }
        if (face != 0xFFU && scene->img_rt[i].face != face) {
            continue;
        }
        gfx_image_resource_close(&scene->img_rt[i].resource);
        scene->img_rt[i].used = 0;
    }
    (void)gfx_arena_scene_mark_dirty(scene, node_off);
}

int gfx_arena_scene_ensure_pkg_images(gfx_arena_scene_t *scene, uint32_t node_off)
{
    gfx_arena_node_t *n;
    const gfx_arena_img_hdr_t *ih;
    const char *path;
    gfx_image_src_t src;
    int rc;

    if (scene == NULL || node_off == GFX_ARENA_NO_NODE) {
        return -1;
    }
    n = gfx_arena_node(&scene->arena, node_off);
    if (n == NULL ||
            (n->type != GFX_ARENA_NODE_IMAGE && n->type != GFX_ARENA_NODE_IMAGE_BUTTON)) {
        return 0;
    }
    ih = gfx_arena_img(&scene->arena, n->reserved);
    if (ih == NULL || ih->format != GFX_ARENA_IMG_FMT_FILE_PATH) {
        return 0;
    }

    if (gfx_arena_scene_img_rt_find(scene, node_off, GFX_ARENA_IMG_ITEM_NONE, 0) == NULL) {
        path = gfx_arena_img_file_path(&scene->arena, n->reserved, 0);
        if (path == NULL || path[0] == '\0') {
            return -2;
        }
        memset(&src, 0, sizeof(src));
        src.type = GFX_IMAGE_SRC_TYPE_FILE;
        src.data = path;
        rc = gfx_arena_scene_bind_image_src(scene, node_off, GFX_ARENA_IMG_ITEM_NONE, 0, &src);
        if (rc != 0) {
            return rc;
        }
    }

    if (n->type == GFX_ARENA_NODE_IMAGE_BUTTON &&
            (ih->pad & GFX_ARENA_IMG_F_HAS_PRESSED) != 0 &&
            gfx_arena_scene_img_rt_find(scene, node_off, GFX_ARENA_IMG_ITEM_NONE, 1) == NULL) {
        path = gfx_arena_img_file_path(&scene->arena, n->reserved, 1);
        if (path != NULL && path[0] != '\0') {
            memset(&src, 0, sizeof(src));
            src.type = GFX_IMAGE_SRC_TYPE_FILE;
            src.data = path;
            (void)gfx_arena_scene_bind_image_src(scene, node_off, GFX_ARENA_IMG_ITEM_NONE, 1, &src);
        }
    }
    return 0;
}

const gfx_arena_img_rt_t *gfx_arena_scene_img_rt_find(const gfx_arena_scene_t *scene,
                                             uint32_t node_off,
                                             uint16_t item_index, uint8_t face)
{
    return img_rt_find_mut((gfx_arena_scene_t *)scene, node_off, item_index, face);
}

bool gfx_arena_img_rt_is_open(const gfx_arena_img_rt_t *rt)
{
    return rt != NULL && rt->used && gfx_image_resource_is_open(&rt->resource);
}

uint16_t gfx_arena_img_rt_width(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? (uint16_t)rt->resource.header.w : 0u;
}

uint16_t gfx_arena_img_rt_height(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? (uint16_t)rt->resource.header.h : 0u;
}

gfx_color_format_t gfx_arena_img_rt_format(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? gfx_image_resource_format(&rt->resource) : GFX_COLOR_FORMAT_NATIVE;
}

const uint8_t *gfx_arena_img_rt_pixels(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? gfx_image_resource_pixels(&rt->resource) : NULL;
}

gfx_coord_t gfx_arena_img_rt_stride_px(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? gfx_image_resource_stride_px(&rt->resource) : 0;
}

const gfx_opa_t *gfx_arena_img_rt_alpha(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? gfx_image_resource_alpha(&rt->resource) : NULL;
}

gfx_coord_t gfx_arena_img_rt_alpha_stride(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? gfx_image_resource_alpha_stride(&rt->resource) : 0;
}

uint8_t gfx_arena_img_rt_pixel_size(const gfx_arena_img_rt_t *rt)
{
    return (rt != NULL) ? gfx_image_resource_pixel_size(&rt->resource) : 0u;
}

static int32_t list_max_scroll(const gfx_arena_node_t *n, const gfx_arena_items_hdr_t *ih)
{
    if (n == NULL || ih == NULL || ih->item_count == 0) {
        return 0;
    }
    const uint16_t row_h = wheel_row_h(ih);
    if (n->type == GFX_ARENA_NODE_WHEEL) {
        return gfx_wheel_core_max_scroll_y((int32_t)n->h, row_h, ih->item_count);
    }
    return gfx_list_core_max_scroll_y((int32_t)n->h, row_h, ih->item_count);
}

static void list_set_scroll(gfx_arena_scene_t *scene, gfx_arena_list_rt_t *rt,
                            const gfx_arena_node_t *n, const gfx_arena_items_hdr_t *ih,
                            int32_t scroll_y, bool allow_overscroll)
{
    uint16_t row_h;
    if (rt == NULL || n == NULL || ih == NULL) {
        return;
    }
    row_h = wheel_row_h(ih);
    if (n->type == GFX_ARENA_NODE_WHEEL) {
        scroll_y = gfx_wheel_core_clamp_scroll_y((int32_t)n->h, row_h, ih->item_count,
                   wheel_cyclic(ih), scroll_y);
    } else {
        scroll_y = gfx_list_core_clamp_scroll_y((int32_t)n->h, row_h, ih->item_count,
                   scroll_y, allow_overscroll);
    }
    if (rt->scroll_y != scroll_y) {
        rt->scroll_y = scroll_y;
        (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
    }
}

static void list_snap(gfx_arena_scene_t *scene, gfx_arena_list_rt_t *rt,
                      const gfx_arena_node_t *n, gfx_arena_items_hdr_t *ih)
{
    if (rt == NULL || n == NULL || ih == NULL ||
            (ih->flags & GFX_ARENA_ITEMS_F_SNAP) == 0) {
        return;
    }
    list_set_scroll(scene, rt, n, ih,
                    gfx_list_core_snap_scroll_y(rt->scroll_y, wheel_row_h(ih)), false);
}

static bool list_anim_step(gfx_arena_scene_t *scene, gfx_arena_list_rt_t *rt)
{
    if (scene == NULL || rt == NULL || !rt->used) {
        return false;
    }
    gfx_arena_node_t *n = gfx_arena_node(&scene->arena, rt->node_off);
    gfx_arena_items_hdr_t *ih = (n != NULL) ? gfx_arena_items_mut(&scene->arena, n->reserved) : NULL;
    uint32_t now;
    bool changed = false;

    if (n == NULL || (n->type != GFX_ARENA_NODE_LIST && n->type != GFX_ARENA_NODE_WHEEL) || ih == NULL) {
        rt->inertia = 0;
        return false;
    }

    now = gfx_arena_now_ms();

    /* WHEEL snap uses dedicated ease-out tween (see wheel_start_snap_tween). */
    if (n->type == GFX_ARENA_NODE_WHEEL && rt->snap_tweening && !rt->pressed) {
        uint32_t elapsed = now - rt->tween_start_ms;
        int32_t next = gfx_ease_out_quad_i32(rt->tween_from, rt->tween_to,
                                             elapsed, GFX_WHEEL_CORE_TWEEN_MS);
        if (next != rt->scroll_y) {
            rt->scroll_y = next;
            (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
            changed = true;
        }
        if (elapsed >= GFX_WHEEL_CORE_TWEEN_MS || next == rt->tween_to) {
            rt->scroll_y = rt->tween_to;
            rt->snap_tweening = 0;
            (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
            changed = true;
        }
        return changed || rt->snap_tweening;
    }

    if (n->type == GFX_ARENA_NODE_WHEEL) {
        return changed;
    }

    {
        bool inertia = rt->inertia != 0;
        bool animating = gfx_list_core_anim_step((int32_t)n->h, wheel_row_h(ih), ih->item_count,
                           &rt->scroll_y, &rt->velocity_y, &rt->last_ms, &inertia,
                           rt->pressed != 0, (ih->flags & GFX_ARENA_ITEMS_F_SNAP) != 0,
                           now, &changed);
        rt->inertia = inertia ? 1 : 0;
        if (changed) {
            (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
        }
        return animating || changed;
    }
}

static void coverflow_finish_tween(gfx_arena_scene_t *scene, gfx_arena_coverflow_rt_t *rt,
                                   gfx_arena_node_t *n, gfx_arena_items_hdr_t *ih,
                                   const gfx_touch_event_t *event)
{
    bool selection_changed = false;
    if (rt->tween_commit_pending && ih != NULL) {
        rt->tween_commit_pending = 0;
        if (rt->tween_target_index >= 0 &&
                rt->tween_target_index < (int32_t)ih->item_count &&
                ih->selected != (uint16_t)rt->tween_target_index) {
            ih->selected = (uint16_t)rt->tween_target_index;
            selection_changed = true;
        }
    }
    rt->drag_offset = 0;
    rt->tweening = 0;
    (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
    if (selection_changed) {
        run_action(scene, n, event);
    }
}

static bool cover_anim_step(gfx_arena_scene_t *scene, gfx_arena_coverflow_rt_t *rt)
{
    if (scene == NULL || rt == NULL || !rt->used || !rt->tweening) {
        return false;
    }
    gfx_arena_node_t *n = gfx_arena_node(&scene->arena, rt->node_off);
    gfx_arena_items_hdr_t *ih = (n != NULL) ? gfx_arena_items_mut(&scene->arena, n->reserved) : NULL;
    if (n == NULL || n->type != GFX_ARENA_NODE_COVERFLOW || ih == NULL) {
        rt->tweening = 0;
        rt->tween_commit_pending = 0;
        rt->drag_offset = 0;
        return false;
    }

    uint32_t now = gfx_arena_now_ms();
    uint32_t elapsed = now - rt->tween_start_ms;
    int32_t next = gfx_ease_out_quad_i32(rt->tween_from, rt->tween_to,
                                         elapsed, GFX_COVERFLOW_CORE_TWEEN_MS);
    if (next != rt->drag_offset) {
        rt->drag_offset = next;
        (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
    }
    if (elapsed >= GFX_COVERFLOW_CORE_TWEEN_MS || next == rt->tween_to) {
        /* Commit selection after tween completes (gfx_coverflow_tween_done_cb). */
        coverflow_finish_tween(scene, rt, n, ih, NULL);
        return false;
    }
    return true;
}

static void coverflow_start_tween(gfx_arena_scene_t *scene, gfx_arena_coverflow_rt_t *rt,
                                  gfx_arena_node_t *n, gfx_arena_items_hdr_t *ih,
                                  int32_t target_index, int32_t start_offset)
{
    gfx_coverflow_core_tween_plan_t plan;
    uint16_t selected;

    if (scene == NULL || rt == NULL || n == NULL || ih == NULL || ih->item_count == 0) {
        return;
    }

    selected = ih->selected;
    if (selected == GFX_ARENA_ITEMS_SELECTED_NONE || selected >= ih->item_count) {
        selected = 0;
        ih->selected = selected;
    }

    gfx_coverflow_core_plan_tween(ih->item_count, (int32_t)selected, target_index,
                                  coverflow_spacing(n), start_offset, &plan);

    rt->tween_target_index = plan.target_index;
    rt->tween_commit_pending = plan.commit_pending ? 1 : 0;
    rt->tween_from = plan.start_offset;
    rt->tween_to = plan.end_offset;
    rt->drag_offset = plan.start_offset;
    rt->tween_start_ms = gfx_arena_now_ms();
    rt->tweening = 1;
    (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);

    if (plan.start_offset == plan.end_offset) {
        coverflow_finish_tween(scene, rt, n, ih, NULL);
    }
}

static bool page_anim_step(gfx_arena_scene_t *scene, gfx_arena_pageflow_rt_t *rt)
{
    if (scene == NULL || rt == NULL || !rt->used || !rt->tweening) {
        return false;
    }
    gfx_arena_node_t *n = gfx_arena_node(&scene->arena, rt->node_off);
    if (n == NULL || n->type != GFX_ARENA_NODE_PAGEFLOW) {
        rt->tweening = 0;
        rt->drag_offset = 0;
        return false;
    }

    uint32_t now = gfx_arena_now_ms();
    uint32_t elapsed = now - rt->tween_start_ms;
    int32_t next = gfx_ease_out_quad_i32(rt->tween_from, rt->tween_to,
                                         elapsed, GFX_PAGEFLOW_CORE_TWEEN_MS);
    if (next != rt->drag_offset) {
        rt->drag_offset = next;
        (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
    }
    if (elapsed >= GFX_PAGEFLOW_CORE_TWEEN_MS || next == rt->tween_to) {
        rt->drag_offset = rt->tween_to;
        rt->tweening = 0;
        (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
        return false;
    }
    return true;
}

int gfx_arena_scene_tick(gfx_arena_scene_t *scene)
{
    if (scene == NULL) {
        return 0;
    }
    int animating = 0;
    for (int i = 0; i < GFX_ARENA_LIST_RT_MAX; i++) {
        if (!scene->list_rt[i].used) {
            continue;
        }
        if (list_anim_step(scene, &scene->list_rt[i])) {
            animating = 1;
        }
    }
    for (int i = 0; i < GFX_ARENA_COVERFLOW_RT_MAX; i++) {
        if (!scene->cover_rt[i].used) {
            continue;
        }
        if (cover_anim_step(scene, &scene->cover_rt[i])) {
            animating = 1;
        }
    }
    for (int i = 0; i < GFX_ARENA_PAGEFLOW_RT_MAX; i++) {
        if (!scene->page_rt[i].used) {
            continue;
        }
        if (page_anim_step(scene, &scene->page_rt[i])) {
            animating = 1;
        }
    }
    gfx_arena_scene_sync_anim_motion(scene);
    for (int i = 0; i < GFX_ARENA_ANIM_RT_MAX; i++) {
        if (scene->anim_rt[i].used && scene->anim_rt[i].playing) {
            animating = 1;
        }
    }
    for (int i = 0; i < GFX_ARENA_MOTION_RT_MAX; i++) {
        if (scene->motion_rt[i].used && scene->motion_rt[i].player != NULL) {
            gfx_arena_node_t *n = gfx_arena_node(&scene->arena, scene->motion_rt[i].node_off);
            if (n != NULL && (n->flags & GFX_ARENA_F_VISIBLE) != 0) {
                animating = 1;
            }
        }
    }
    return animating;
}

static uint32_t hit_rec(gfx_arena_t *a, uint32_t node_off, int ox, int oy, uint16_t x, uint16_t y)
{
    uint32_t hit = GFX_ARENA_NO_NODE;

    for (uint32_t off = node_off; off != GFX_ARENA_NO_NODE; ) {
        gfx_arena_node_t *n = gfx_arena_node(a, off);
        if (n == NULL) {
            break;
        }

        const int abs_x = ox + (int)n->x;
        const int abs_y = oy + (int)n->y;
        const int x2 = abs_x + (int)n->w;
        const int y2 = abs_y + (int)n->h;

        if ((n->flags & GFX_ARENA_F_VISIBLE) != 0) {
            uint32_t child_hit = GFX_ARENA_NO_NODE;
            if (n->first_child != GFX_ARENA_NO_NODE) {
                child_hit = hit_rec(a, n->first_child, abs_x, abs_y, x, y);
            }
            if (child_hit != GFX_ARENA_NO_NODE) {
                hit = child_hit;
            } else if ((n->flags & GFX_ARENA_F_CLICKABLE) != 0 &&
                       (int)x >= abs_x && (int)x < x2 &&
                       (int)y >= abs_y && (int)y < y2) {
                hit = off;
            }
        }

        off = n->next_sibling;
    }

    return hit;
}

uint32_t gfx_arena_scene_hit_test(gfx_arena_scene_t *scene, uint16_t x, uint16_t y)
{
    if (scene == NULL || scene->arena.base == NULL) {
        return GFX_ARENA_NO_NODE;
    }
    const gfx_arena_hdr_t *hdr = gfx_arena_hdr(&scene->arena);
    if (hdr->root_off == GFX_ARENA_NO_NODE) {
        return GFX_ARENA_NO_NODE;
    }
    return hit_rec(&scene->arena, hdr->root_off, 0, 0, x, y);
}

static void run_action(gfx_arena_scene_t *scene, gfx_arena_node_t *n, const gfx_touch_event_t *event)
{
    const gfx_arena_type_ops_t *ops;
    const char *aname;

    if (scene == NULL || n == NULL || scene->actions == NULL) {
        return;
    }

    ops = gfx_arena_type_ops(n->type);
    if (ops != NULL && ops->action_by_name) {
        aname = gfx_arena_str(&scene->arena, n->name_off);
    } else {
        /* BUTTON / CONTAINER: reserved = action name string. */
        aname = gfx_arena_str(&scene->arena, n->reserved);
    }
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

static void select_list_at(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t node_off,
                           uint16_t y, int32_t scroll_y)
{
    gfx_arena_items_hdr_t *ih = gfx_arena_items_mut(&scene->arena, n->reserved);
    int16_t x1, y1, x2, y2;
    int32_t idx;

    if (ih == NULL || ih->item_count == 0) {
        return;
    }
    if (gfx_arena_node_abs_area(&scene->arena, node_off, &x1, &y1, &x2, &y2) != 0) {
        return;
    }
    (void)x1;
    (void)x2;
    (void)y2;
    idx = gfx_list_core_index_from_point((int32_t)y1, (int32_t)y, scroll_y,
                                         wheel_row_h(ih), ih->item_count);
    if (idx < 0) {
        return;
    }
    if (ih->selected != (uint16_t)idx) {
        ih->selected = (uint16_t)idx;
        (void)gfx_arena_scene_mark_dirty(scene, node_off);
    }
}

static void progress_set_at(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t node_off,
                            uint16_t x)
{
    gfx_arena_progress_hdr_t *ph = gfx_arena_progress_mut(&scene->arena, n->reserved);
    int16_t x1, y1, x2, y2;
    uint16_t v;
    const int32_t pad = 4; /* match gfx_arena_draw_progress / object fill_pad */

    if (ph == NULL || n->w == 0) {
        return;
    }
    if (gfx_arena_node_abs_area(&scene->arena, node_off, &x1, &y1, &x2, &y2) != 0) {
        return;
    }
    (void)y1;
    (void)y2;
    {
        int32_t inner_w = (int32_t)n->w - pad * 2;
        if (inner_w <= 0) {
            return;
        }
        v = gfx_progress_core_from_local_x((int32_t)x - (int32_t)x1 - pad, inner_w);
    }
    if (ph->value != v) {
        ph->value = v;
        (void)gfx_arena_scene_mark_dirty(scene, node_off);
    }
}

static int handle_progress_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                 const gfx_touch_event_t *event)
{
    if (scene == NULL || n == NULL || event == NULL) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        n->flags |= GFX_ARENA_F_PRESSED;
        (void)gfx_arena_scene_mark_dirty(scene, off);
        progress_set_at(scene, n, off, event->x);
        run_action(scene, n, event);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE) {
        progress_set_at(scene, n, off, event->x);
        run_action(scene, n, event);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_PRESSED);
        (void)gfx_arena_scene_mark_dirty(scene, off);
        if (gfx_arena_scene_hit_test(scene, event->x, event->y) == off) {
            progress_set_at(scene, n, off, event->x);
            run_action(scene, n, event);
        }
        return 1;
    }
    return 1;
}

static int handle_simple_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                               const gfx_touch_event_t *event)
{
    if (scene == NULL || n == NULL || event == NULL) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        n->flags |= GFX_ARENA_F_PRESSED;
        (void)gfx_arena_scene_mark_dirty(scene, off);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE) {
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_PRESSED);
        (void)gfx_arena_scene_mark_dirty(scene, off);
        if (gfx_arena_scene_hit_test(scene, event->x, event->y) == off) {
            run_action(scene, n, event);
        }
        return 1;
    }
    return 1;
}

static int handle_list_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                             const gfx_touch_event_t *event)
{
    gfx_arena_items_hdr_t *ih = gfx_arena_items_mut(&scene->arena, n->reserved);
    if (ih == NULL) {
        return 0;
    }
    gfx_arena_list_rt_t *rt = list_rt_get(scene, off);
    if (rt == NULL) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        rt->inertia = 0;
        rt->velocity_y = 0;
        rt->pressed = 1;
        rt->dragging = 0;
        rt->start_x = event->x;
        rt->start_y = event->y;
        rt->last_y = event->y;
        rt->start_scroll_y = rt->scroll_y;
        rt->last_ms = event->timestamp_ms != 0U ? event->timestamp_ms : gfx_arena_now_ms();
        n->flags |= GFX_ARENA_F_PRESSED;
        (void)gfx_arena_scene_mark_dirty(scene, off);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && rt->pressed) {
        int32_t abs_dx = (int32_t)event->x - (int32_t)rt->start_x;
        int32_t abs_dy = (int32_t)event->y - (int32_t)rt->start_y;
        if (abs_dx < 0) {
            abs_dx = -abs_dx;
        }
        if (abs_dy < 0) {
            abs_dy = -abs_dy;
        }
        if (!rt->dragging &&
                (abs_dx >= GFX_LIST_CORE_DRAG_THRESHOLD || abs_dy >= GFX_LIST_CORE_DRAG_THRESHOLD)) {
            rt->dragging = 1;
        }
        if (rt->dragging) {
            uint32_t ts = event->timestamp_ms != 0U ? event->timestamp_ms : gfx_arena_now_ms();
            uint32_t dt = ts - rt->last_ms;
            int32_t delta_y = (int32_t)rt->last_y - (int32_t)event->y;
            list_set_scroll(scene, rt, n, ih, rt->scroll_y + delta_y, true);
            if (dt > 0U) {
                rt->velocity_y = (delta_y * 1000) / (int32_t)dt;
            }
            rt->last_y = event->y;
            rt->last_ms = ts;
        }
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && rt->pressed) {
        const bool was_dragging = rt->dragging != 0;
        rt->pressed = 0;
        rt->dragging = 0;
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_PRESSED);
        (void)gfx_arena_scene_mark_dirty(scene, off);
        if (was_dragging) {
            if (iabs32(rt->velocity_y) >= GFX_LIST_CORE_INERTIA_MIN_V) {
                rt->inertia = 1;
                rt->last_ms = gfx_arena_now_ms();
                (void)gfx_arena_scene_mark_dirty(scene, off);
            } else {
                (void)list_anim_step(scene, rt);
            }
        } else {
            uint32_t hit = gfx_arena_scene_hit_test(scene, event->x, event->y);
            if (hit == off) {
                select_list_at(scene, n, off, event->y, rt->scroll_y);
                run_action(scene, n, event);
            }
        }
        return 1;
    }
    return 1;
}

static void wheel_start_snap_tween(gfx_arena_scene_t *scene, gfx_arena_list_rt_t *rt,
                                   gfx_arena_node_t *n, gfx_arena_items_hdr_t *ih,
                                   int32_t selected, bool emit_action,
                                   const gfx_touch_event_t *event)
{
    uint16_t row_h;
    int32_t target_scroll;
    bool changed;

    if (rt == NULL || n == NULL || ih == NULL || ih->item_count == 0) {
        return;
    }
    row_h = wheel_row_h(ih);
    selected = gfx_wheel_core_clamp_index(ih->item_count, selected, wheel_cyclic(ih));
    target_scroll = gfx_wheel_core_center_scroll_y((int32_t)n->h, row_h, selected);

    changed = (ih->selected != (uint16_t)selected);
    if (changed) {
        ih->selected = (uint16_t)selected;
    }

    if (rt->scroll_y == target_scroll) {
        rt->snap_tweening = 0;
        (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
        if (changed && emit_action) {
            run_action(scene, n, event);
        }
        return;
    }

    rt->tween_from = rt->scroll_y;
    rt->tween_to = target_scroll;
    rt->tween_start_ms = gfx_arena_now_ms();
    rt->snap_tweening = 1;
    (void)gfx_arena_scene_mark_dirty(scene, rt->node_off);
    if (changed && emit_action) {
        run_action(scene, n, event);
    }
}

/** Align with gfx_wheel_snap via shared core. */
static void wheel_snap(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                       gfx_arena_list_rt_t *rt, gfx_arena_items_hdr_t *ih,
                       const gfx_touch_event_t *event)
{
    int32_t selected;
    (void)off;
    if (scene == NULL || n == NULL || rt == NULL || ih == NULL || ih->item_count == 0) {
        return;
    }
    selected = gfx_wheel_core_index_from_scroll((int32_t)n->h, wheel_row_h(ih), ih->item_count,
               wheel_cyclic(ih), rt->scroll_y);
    if (selected < 0) {
        return;
    }
    wheel_start_snap_tween(scene, rt, n, ih, selected, true, event);
}

static void select_wheel_at_scroll(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                   gfx_arena_list_rt_t *rt, gfx_arena_items_hdr_t *ih, uint16_t y,
                                   const gfx_touch_event_t *event)
{
    int16_t x1, y1, x2, y2;
    int32_t selected;
    if (gfx_arena_node_abs_area(&scene->arena, off, &x1, &y1, &x2, &y2) != 0) {
        return;
    }
    (void)x1;
    (void)x2;
    (void)y2;
    selected = gfx_wheel_core_index_from_local_y((int32_t)n->h, wheel_row_h(ih), ih->item_count,
               wheel_cyclic(ih), rt->scroll_y, (int32_t)y - (int32_t)y1);
    if (selected < 0) {
        return;
    }
    wheel_start_snap_tween(scene, rt, n, ih, selected, true, event);
}

static int handle_wheel_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                              const gfx_touch_event_t *event)
{
    gfx_arena_items_hdr_t *ih = gfx_arena_items_mut(&scene->arena, n->reserved);
    const bool created = list_rt_find(scene, off) == NULL;
    gfx_arena_list_rt_t *rt;
    uint16_t row_h;
    bool cyclic;

    if (ih == NULL || ih->item_count == 0) {
        return 0;
    }
    rt = list_rt_get(scene, off);
    if (rt == NULL) {
        return 0;
    }
    row_h = wheel_row_h(ih);
    cyclic = wheel_cyclic(ih);
    if (created || (!rt->pressed && !rt->dragging && !rt->snap_tweening)) {
        uint16_t selected = ih->selected;
        if (selected == GFX_ARENA_ITEMS_SELECTED_NONE || selected >= ih->item_count) {
            selected = 0;
            ih->selected = selected;
        }
        if (!rt->snap_tweening) {
            rt->scroll_y = gfx_wheel_core_center_scroll_y((int32_t)n->h, row_h, (int32_t)selected);
        }
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        rt->inertia = 0;
        rt->velocity_y = 0;
        rt->snap_tweening = 0;
        rt->pressed = 1;
        rt->dragging = 0;
        rt->start_x = event->x;
        rt->start_y = event->y;
        rt->last_y = event->y;
        rt->start_scroll_y = rt->scroll_y;
        rt->last_ms = event->timestamp_ms != 0U ? event->timestamp_ms : gfx_arena_now_ms();
        n->flags |= GFX_ARENA_F_PRESSED;
        (void)gfx_arena_scene_mark_dirty(scene, off);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && rt->pressed) {
        int32_t abs_dx = (int32_t)event->x - (int32_t)rt->start_x;
        int32_t abs_dy = (int32_t)event->y - (int32_t)rt->start_y;
        if (abs_dx < 0) {
            abs_dx = -abs_dx;
        }
        if (abs_dy < 0) {
            abs_dy = -abs_dy;
        }
        if (!rt->dragging &&
                (abs_dx >= GFX_WHEEL_CORE_DRAG_THRESHOLD || abs_dy >= GFX_WHEEL_CORE_DRAG_THRESHOLD)) {
            rt->dragging = 1;
        }
        if (rt->dragging) {
            int32_t next = gfx_wheel_core_clamp_scroll_y((int32_t)n->h, row_h, ih->item_count,
                           cyclic, rt->scroll_y + ((int32_t)rt->last_y - (int32_t)event->y));
            if (next != rt->scroll_y) {
                rt->scroll_y = next;
                (void)gfx_arena_scene_mark_dirty(scene, off);
            }
            rt->last_y = event->y;
            rt->last_ms = event->timestamp_ms != 0U ? event->timestamp_ms : gfx_arena_now_ms();
        }
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && rt->pressed) {
        const bool was_dragging = rt->dragging != 0;
        rt->pressed = 0;
        rt->dragging = 0;
        rt->inertia = 0;
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_PRESSED);
        if (was_dragging) {
            wheel_snap(scene, n, off, rt, ih, event);
        } else {
            uint32_t hit = gfx_arena_scene_hit_test(scene, event->x, event->y);
            if (hit == off) {
                select_wheel_at_scroll(scene, n, off, rt, ih, event->y, event);
            } else {
                (void)gfx_arena_scene_mark_dirty(scene, off);
            }
        }
        return 1;
    }
    return 1;
}

static int handle_coverflow_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                  const gfx_touch_event_t *event)
{
    gfx_arena_items_hdr_t *ih = gfx_arena_items_mut(&scene->arena, n->reserved);
    if (ih == NULL || ih->item_count == 0) {
        return 0;
    }
    gfx_arena_coverflow_rt_t *rt = cover_rt_get(scene, off);
    if (rt == NULL) {
        return 0;
    }
    int32_t spacing = coverflow_spacing(n);
    bool was_pressed = rt->pressed != 0;

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        /* Cancel in-flight tween like gfx_coverflow_cancel_tween. */
        rt->pressed = 1;
        rt->dragging = 0;
        rt->tweening = 0;
        rt->tween_commit_pending = 0;
        rt->drag_offset = 0;
        rt->tween_to = 0;
        rt->start_x = event->x;
        rt->start_y = event->y;
        n->flags |= GFX_ARENA_F_PRESSED;
        (void)gfx_arena_scene_mark_dirty(scene, off);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && rt->pressed) {
        int32_t dx = (int32_t)event->x - (int32_t)rt->start_x;
        int32_t dy = (int32_t)event->y - (int32_t)rt->start_y;
        if (!rt->dragging &&
                (iabs32(dx) >= GFX_COVERFLOW_CORE_DRAG_THRESHOLD ||
                 iabs32(dy) >= GFX_COVERFLOW_CORE_DRAG_THRESHOLD)) {
            rt->dragging = 1;
        }
        if (rt->dragging) {
            dx = gfx_coverflow_core_clamp_i32(dx, -spacing, spacing);
            if (rt->drag_offset != dx) {
                rt->drag_offset = dx;
                (void)gfx_arena_scene_mark_dirty(scene, off);
            }
        }
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && was_pressed) {
        uint16_t selected;
        int32_t target;

        rt->pressed = 0;
        rt->dragging = 0;
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_PRESSED);

        selected = ih->selected;
        if (selected == GFX_ARENA_ITEMS_SELECTED_NONE || selected >= ih->item_count) {
            selected = 0;
            ih->selected = selected;
        }

        target = gfx_coverflow_core_page_target(ih->item_count, (int32_t)selected,
                                                rt->drag_offset, GFX_COVERFLOW_CORE_PAGE_THRESHOLD);
        if (rt->drag_offset != 0 || target != (int32_t)selected) {
            coverflow_start_tween(scene, rt, n, ih, target, rt->drag_offset);
        } else {
            uint32_t hit = gfx_arena_scene_hit_test(scene, event->x, event->y);
            if (hit == off) {
                run_action(scene, n, event);
            }
            (void)gfx_arena_scene_mark_dirty(scene, off);
        }
        return 1;
    }
    return 1;
}

static int handle_pageflow_touch(gfx_arena_scene_t *scene, gfx_arena_node_t *n, uint32_t off,
                                 const gfx_touch_event_t *event)
{
    gfx_arena_items_hdr_t *ih = gfx_arena_items_mut(&scene->arena, n->reserved);
    if (ih == NULL || ih->item_count == 0) {
        return 0;
    }
    gfx_arena_pageflow_rt_t *rt = page_rt_get(scene, off);
    if (rt == NULL) {
        return 0;
    }
    const int32_t span = (n->w > 0) ? (int32_t)n->w : (int32_t)GFX_PAGEFLOW_CORE_PAGE_THRESHOLD;
    const bool was_pressed = rt->pressed != 0;

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        rt->pressed = 1;
        rt->dragging = 0;
        rt->tweening = 0;
        rt->drag_offset = 0;
        rt->tween_to = 0;
        rt->start_x = event->x;
        rt->start_y = event->y;
        n->flags |= GFX_ARENA_F_PRESSED;
        (void)gfx_arena_scene_mark_dirty(scene, off);
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && rt->pressed) {
        int32_t dx = (int32_t)event->x - (int32_t)rt->start_x;
        int32_t dy = (int32_t)event->y - (int32_t)rt->start_y;
        if (!rt->dragging &&
                (iabs32(dx) >= GFX_PAGEFLOW_CORE_DRAG_THRESHOLD ||
                 iabs32(dy) >= GFX_PAGEFLOW_CORE_DRAG_THRESHOLD)) {
            rt->dragging = 1;
        }
        if (rt->dragging) {
            if (dx < -span) {
                dx = -span;
            } else if (dx > span) {
                dx = span;
            }
            if (rt->drag_offset != dx) {
                rt->drag_offset = dx;
                (void)gfx_arena_scene_mark_dirty(scene, off);
            }
        }
        return 1;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && was_pressed) {
        uint16_t selected = ih->selected;
        if (selected == GFX_ARENA_ITEMS_SELECTED_NONE || selected >= ih->item_count) {
            selected = 0;
            ih->selected = selected;
        }

        int32_t target = gfx_pageflow_core_page_target(ih->item_count, (int32_t)selected,
                         rt->drag_offset, GFX_PAGEFLOW_CORE_PAGE_THRESHOLD);
        gfx_pageflow_core_tween_plan_t plan;
        gfx_pageflow_core_plan_tween(ih->item_count, (int32_t)selected, target,
                                     span, rt->drag_offset, &plan);

        rt->pressed = 0;
        rt->dragging = 0;
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_PRESSED);

        if (plan.page_changed) {
            ih->selected = (uint16_t)plan.page_index;
        }

        rt->tween_from = plan.start_offset;
        rt->tween_to = plan.end_offset;
        rt->drag_offset = plan.start_offset;
        rt->tween_start_ms = gfx_arena_now_ms();
        rt->tweening = 1;
        (void)gfx_arena_scene_mark_dirty(scene, off);

        if (plan.page_changed) {
            run_action(scene, n, event);
        }

        if (plan.start_offset == plan.end_offset) {
            rt->tweening = 0;
        }
        return 1;
    }
    return 1;
}

int gfx_arena_scene_handle_touch(gfx_display_t *disp, const gfx_touch_event_t *event)
{
    gfx_arena_scene_t *scene = gfx_arena_scene_from_disp(disp);
    gfx_arena_node_t *n;
    const gfx_arena_type_ops_t *ops;
    gfx_arena_touch_fn_t touch;
    uint32_t off;
    int rc;

    if (scene == NULL || event == NULL) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        uint32_t hit = gfx_arena_scene_hit_test(scene, event->x, event->y);
        scene->pressed_off = hit;
        scene->pressed_track = event->track_id;
        if (hit == GFX_ARENA_NO_NODE) {
            return 0;
        }
        n = gfx_arena_node(&scene->arena, hit);
        if (n == NULL) {
            return 0;
        }
        ops = gfx_arena_type_ops(n->type);
        touch = (ops != NULL) ? ops->touch : NULL;
        if (touch == NULL) {
            return 0;
        }
        return touch(scene, n, hit, event);
    }

    if (scene->pressed_off == GFX_ARENA_NO_NODE || event->track_id != scene->pressed_track) {
        return 0;
    }

    off = scene->pressed_off;
    n = gfx_arena_node(&scene->arena, off);
    if (n == NULL) {
        if (event->type == GFX_TOUCH_EVENT_RELEASE) {
            scene->pressed_off = GFX_ARENA_NO_NODE;
        }
        return 0;
    }

    ops = gfx_arena_type_ops(n->type);
    touch = (ops != NULL) ? ops->touch : handle_simple_touch;
    if (touch == NULL) {
        touch = handle_simple_touch;
    }
    rc = touch(scene, n, off, event);
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->pressed_off = GFX_ARENA_NO_NODE;
    }
    return rc;
}

uint8_t gfx_arena_scene_test_dirty_count(gfx_display_t *disp)
{
    if (disp == NULL) {
        return 0;
    }
    return disp->dirty.count;
}

int gfx_arena_scene_test_dirty_area(gfx_display_t *disp, uint8_t index, gfx_area_t *out)
{
    if (disp == NULL || out == NULL || index >= disp->dirty.count) {
        return -1;
    }
    *out = disp->dirty.areas[index];
    return 0;
}
