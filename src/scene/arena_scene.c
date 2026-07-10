/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena_scene.h"

#include <stdlib.h>
#include <string.h>

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "fonts/gfx_font_priv.h"
#include "gfx/scene/arena_draw.h"
#include "gfx/scene/arena_snapshot.h"
#include "platform/gfx_platform.h"
#include "render/gfx_render_priv.h"

static const char *TAG = "arena_scene";

#define ARENA_PAGER_DEFAULT_THRESHOLD 8

#define ARENA_PAGER_DEFAULT_SNAP_MS   200
#define ARENA_PAGER_TIMER_PERIOD_MS   16
#define ARENA_PAGER_SNAP_ALIGN        64 /* PSRAM cache-line burst writes */

/** 64B-aligned PSRAM-first snapshot buffer (RLE decode / compose bandwidth). */
static uint16_t *arena_pager_snap_alloc(size_t bytes)
{
    void *buf = gfx_platform_aligned_alloc(ARENA_PAGER_SNAP_ALIGN, bytes,
                                           GFX_PLATFORM_HEAP_SPIRAM | GFX_PLATFORM_HEAP_8BIT);
    if (buf == NULL) {
        buf = gfx_platform_aligned_alloc(ARENA_PAGER_SNAP_ALIGN, bytes,
                                         GFX_PLATFORM_HEAP_DEFAULT);
    }
    return (uint16_t *)buf;
}

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
    arena_scene_pager_disable(scene);
    arena_scene_set_font(scene, NULL);
    arena_free(&scene->arena);
    free(scene->glyph_scratch);
    scene->glyph_scratch = NULL;
    scene->glyph_scratch_cap = 0;
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

/* Content changed outside pager composing -> current page snapshot is stale. */
static void arena_pager_note_content_dirty(gfx_arena_scene_t *scene)
{
    if (scene->pager.enabled && !arena_scene_pager_composing(scene)) {
        scene->pager.snap_stale = true;
    }
}

int arena_scene_mark_dirty(gfx_arena_scene_t *scene, uint32_t node_off)
{
    if (scene == NULL || scene->disp == NULL || node_off == ARENA_NO_NODE) {
        return -1;
    }
    arena_pager_note_content_dirty(scene);

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
    arena_pager_note_content_dirty(scene);
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

int arena_scene_replace_arena(gfx_arena_scene_t *scene, arena_t *arena)
{
    if (scene == NULL || arena == NULL || arena->base == NULL) {
        return -1;
    }
    arena_free(&scene->arena);
    scene->arena = *arena;
    arena->base = NULL;
    arena->size = 0;
    scene->pressed_off = ARENA_NO_NODE;
    return 0;
}

/* ---- pager ---- */

static int32_t arena_pager_clamp_dx(const gfx_arena_scene_t *scene, int32_t dx)
{
    const int32_t w = (int32_t)scene->pager.w;

    /* dx < 0 slides in the next page, dx > 0 the previous one. */
    if (scene->pager.current == 0U && dx > 0) {
        dx = 0;
    }
    if ((uint16_t)scene->pager.current + 1U >= scene->pager.page_count && dx < 0) {
        dx = 0;
    }
    if (dx < -w) {
        dx = -w;
    }
    if (dx > w) {
        dx = w;
    }
    return dx;
}

/** Slot index holding `page`, or -1. */
static int arena_pager_slot_find(const gfx_arena_scene_t *scene, int page)
{
    for (uint8_t i = 0; i < scene->pager.snap_slots; i++) {
        if ((int)scene->pager.snap_page[i] == page) {
            return (int)i;
        }
    }
    return -1;
}

/** Slot to reuse: empty first, else one outside the current +/-1 window. */
static int arena_pager_slot_take(const gfx_arena_scene_t *scene)
{
    for (uint8_t i = 0; i < scene->pager.snap_slots; i++) {
        if (scene->pager.snap_page[i] < 0) {
            return (int)i;
        }
    }
    for (uint8_t i = 0; i < scene->pager.snap_slots; i++) {
        const int p = (int)scene->pager.snap_page[i];
        if (p < (int)scene->pager.current - 1 || p > (int)scene->pager.current + 1) {
            return (int)i;
        }
    }
    return -1;
}

/** Render the arena's current content into a full-frame snapshot buffer. */
static int arena_pager_render_snapshot(gfx_arena_scene_t *scene, uint16_t *buf)
{
    gfx_display_t *disp = scene->disp;
    const gfx_coord_t w = (gfx_coord_t)scene->pager.w;
    const gfx_coord_t h = (gfx_coord_t)scene->pager.h;
    gfx_render_surface_t surf = {
        .buf = buf,
        .buf_area = { 0, 0, w, h },
        .clip_area = { 0, 0, w, h },
        .stride = w,
        .format = disp->format.render_format,
    };
    gfx_area_t full = { 0, 0, w, h };
    gfx_color_t bg = disp->style.bg_enable ? disp->style.bg_color : GFX_COLOR_HEX(0x000000);

    gfx_render_surface_fill(disp, &surf, &full, bg, 0xFFU);
    return arena_draw_clipped(disp, &scene->arena, &surf);
}

/**
 * Generate a snapshot of `page` into buf: baked path first, then fallback
 * load_page + node-tree render. The arena holds pager.current on return.
 */
static int arena_pager_generate_snapshot(gfx_arena_scene_t *scene, uint8_t page,
                                         uint16_t *buf)
{
    if (scene->pager.snapshot_page != NULL &&
            scene->pager.snapshot_page(page, buf, scene->pager.w, scene->pager.h,
                                       scene->pager.user_data) == 0) {
        return 0;
    }
    if (page == scene->pager.current) {
        return arena_pager_render_snapshot(scene, buf);
    }
    if (scene->pager.load_page == NULL ||
            scene->pager.load_page(page, scene->pager.user_data) != 0) {
        return -1;
    }
    int rc = arena_pager_render_snapshot(scene, buf);
    if (scene->pager.load_page(scene->pager.current, scene->pager.user_data) != 0) {
        rc = -1;
    }
    return rc;
}

/** Make `page` resident in the snapshot window (no-op if out of range). */
static int arena_pager_ensure_snapshot(gfx_arena_scene_t *scene, int page)
{
    if (page < 0 || page >= (int)scene->pager.page_count) {
        return 0;
    }
    if (arena_pager_slot_find(scene, page) >= 0) {
        return 0;
    }
    const int slot = arena_pager_slot_take(scene);
    if (slot < 0) {
        return -1;
    }
    scene->pager.snap_page[slot] = -1;
    if (arena_pager_generate_snapshot(scene, (uint8_t)page, scene->pager.snap[slot]) != 0) {
        return -1;
    }
    scene->pager.snap_page[slot] = (int16_t)page;
    return 0;
}

static void arena_pager_ensure_neighbors(gfx_arena_scene_t *scene)
{
    (void)arena_pager_ensure_snapshot(scene, (int)scene->pager.current - 1);
    (void)arena_pager_ensure_snapshot(scene, (int)scene->pager.current + 1);
}

/** Re-render the current page snapshot if content changed since capture. */
static void arena_pager_refresh_stale(gfx_arena_scene_t *scene)
{
    uint16_t *buf = arena_scene_pager_snap_of(scene, (int)scene->pager.current);
    if (scene->pager.snap_stale && buf != NULL) {
        (void)arena_pager_render_snapshot(scene, buf);
        scene->pager.snap_stale = false;
    }
}

static void arena_pager_snap_finish(gfx_arena_scene_t *scene)
{
    scene->pager.animating = false;
    if (scene->pager.snap_to != 0) {
        scene->pager.current = (scene->pager.snap_to < 0) ?
                               (uint8_t)(scene->pager.current + 1U) :
                               (uint8_t)(scene->pager.current - 1U);
        if (scene->pager.load_page != NULL) {
            (void)scene->pager.load_page(scene->pager.current, scene->pager.user_data);
        }
        /* Refill the vacated far slot on the next tick, off the drag path. */
        scene->pager.prefetch_pending = true;
    }
    scene->pager.drag_dx = 0;
    (void)arena_scene_mark_dirty_all(scene);
    /* Rebuilt page matches its enable-time snapshot; the dirty above is ours. */
    scene->pager.snap_stale = false;

    if (scene->pager.snap_timer != NULL) {
        if (scene->pager.prefetch_pending) {
            gfx_timer_reset(scene->pager.snap_timer);
            gfx_timer_resume(scene->pager.snap_timer);
        } else {
            gfx_timer_pause(scene->pager.snap_timer);
        }
    } else if (scene->pager.prefetch_pending) {
        scene->pager.prefetch_pending = false;
        arena_pager_ensure_neighbors(scene);
    }
}

static void arena_pager_snap_tick(void *user_data)
{
    gfx_arena_scene_t *scene = (gfx_arena_scene_t *)user_data;

    if (scene == NULL) {
        return;
    }
    if (!scene->pager.enabled) {
        if (scene->pager.snap_timer != NULL) {
            gfx_timer_pause(scene->pager.snap_timer);
        }
        return;
    }

    if (scene->pager.animating) {
        const uint32_t elapsed = gfx_timer_tick_get() - scene->pager.snap_start;
        if (elapsed >= scene->pager.snap_ms) {
            arena_pager_snap_finish(scene);
            return;
        }

        /* Ease-out cubic: dx = to + (from - to) * (1 - t)^3, fixed-point /1000. */
        const int64_t f = 1000 - ((int64_t)elapsed * 1000) / scene->pager.snap_ms;
        const int64_t cube = ((f * f) / 1000) * f / 1000;
        scene->pager.drag_dx = scene->pager.snap_to +
                               (int32_t)(((int64_t)(scene->pager.snap_from - scene->pager.snap_to) * cube) / 1000);
        (void)arena_scene_mark_dirty_all(scene);
        return;
    }

    if (scene->pager.prefetch_pending) {
        scene->pager.prefetch_pending = false;
        arena_pager_ensure_neighbors(scene);
    }
    if (scene->pager.snap_timer != NULL) {
        gfx_timer_pause(scene->pager.snap_timer);
    }
}

static void arena_pager_snap_start(gfx_arena_scene_t *scene)
{
    const int32_t w = (int32_t)scene->pager.w;
    const int32_t dx = scene->pager.drag_dx;

    scene->pager.snap_from = dx;
    if (dx <= -w / 3) {
        scene->pager.snap_to = -w;
    } else if (dx >= w / 3) {
        scene->pager.snap_to = w;
    } else {
        scene->pager.snap_to = 0;
    }

    if (scene->pager.snap_timer == NULL || scene->pager.snap_ms == 0U ||
            scene->pager.snap_from == scene->pager.snap_to) {
        arena_pager_snap_finish(scene);
        return;
    }

    scene->pager.animating = true;
    scene->pager.snap_start = gfx_timer_tick_get();
    gfx_timer_reset(scene->pager.snap_timer);
    gfx_timer_resume(scene->pager.snap_timer);
}

int arena_scene_pager_enable(gfx_arena_scene_t *scene, const arena_pager_config_t *cfg)
{
    if (scene == NULL || scene->disp == NULL || cfg == NULL ||
            cfg->page_count < 2U || cfg->page_count > ARENA_PAGER_MAX_PAGES ||
            cfg->current >= cfg->page_count ||
            cfg->load_page == NULL || cfg->gfx == NULL) {
        return -1;
    }
    if (scene->pager.enabled) {
        return -2;
    }

    gfx_display_t *disp = scene->disp;
    /* Compose is a raw 16bpp row copy; other render formats are not wired. */
    if (disp->format.render_pixel_size != 2U ||
            disp->res.h_res == 0U || disp->res.v_res == 0U) {
        return -3;
    }

    const uint16_t w = (uint16_t)disp->res.h_res;
    const uint16_t h = (uint16_t)disp->res.v_res;
    const size_t snap_bytes = (size_t)w * (size_t)h * sizeof(uint16_t);
    const uint8_t slots = (cfg->page_count < ARENA_PAGER_SNAP_SLOTS) ?
                          cfg->page_count : (uint8_t)ARENA_PAGER_SNAP_SLOTS;

    memset(&scene->pager, 0, sizeof(scene->pager));
    for (uint8_t i = 0; i < ARENA_PAGER_SNAP_SLOTS; i++) {
        scene->pager.snap_page[i] = -1;
    }

    gfx_timer_handle_t timer = gfx_timer_create(cfg->gfx, arena_pager_snap_tick,
                               ARENA_PAGER_TIMER_PERIOD_MS, scene);
    bool alloc_ok = (timer != NULL);
    for (uint8_t i = 0; i < slots; i++) {
        scene->pager.snap[i] = arena_pager_snap_alloc(snap_bytes);
        alloc_ok = alloc_ok && (scene->pager.snap[i] != NULL);
    }
    if (!alloc_ok) {
        for (uint8_t i = 0; i < slots; i++) {
            gfx_platform_free(scene->pager.snap[i]);
            scene->pager.snap[i] = NULL;
        }
        if (timer != NULL) {
            gfx_timer_delete(cfg->gfx, timer);
        }
        return -4;
    }
    gfx_timer_pause(timer);

    scene->pager.gfx = cfg->gfx;
    scene->pager.current = cfg->current;
    scene->pager.page_count = cfg->page_count;
    scene->pager.snap_slots = slots;
    scene->pager.drag_threshold = (cfg->drag_threshold != 0U) ?
                                  cfg->drag_threshold : ARENA_PAGER_DEFAULT_THRESHOLD;
    scene->pager.snap_ms = (cfg->snap_ms != 0U) ? cfg->snap_ms : ARENA_PAGER_DEFAULT_SNAP_MS;
    scene->pager.w = w;
    scene->pager.h = h;
    scene->pager.snap_timer = timer;
    scene->pager.load_page = cfg->load_page;
    scene->pager.snapshot_page = cfg->snapshot_page;
    scene->pager.user_data = cfg->user_data;

    /* Current page first (arena already holds it), then valid neighbors. */
    if (arena_pager_ensure_snapshot(scene, (int)cfg->current) != 0) {
        arena_scene_pager_disable(scene);
        return -5;
    }
    arena_pager_ensure_neighbors(scene);
    if (arena_pager_slot_find(scene, (int)cfg->current) < 0) {
        arena_scene_pager_disable(scene);
        return -5;
    }

    scene->pager.enabled = true;
    return 0;
}

void arena_scene_pager_disable(gfx_arena_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->pager.snap_timer != NULL && scene->pager.gfx != NULL) {
        gfx_timer_delete(scene->pager.gfx, scene->pager.snap_timer);
    }
    for (uint8_t i = 0; i < ARENA_PAGER_SNAP_SLOTS; i++) {
        gfx_platform_free(scene->pager.snap[i]);
    }
    memset(&scene->pager, 0, sizeof(scene->pager));
}

int arena_scene_pager_snapshot_baked(gfx_arena_scene_t *scene,
                                     const uint8_t *rle, size_t rle_size,
                                     const uint8_t *pkg, size_t pkg_size,
                                     uint16_t *buf)
{
    if (scene == NULL || scene->disp == NULL || rle == NULL || buf == NULL ||
            scene->pager.w == 0U || scene->pager.h == 0U) {
        return -1;
    }
    const uint16_t w = scene->pager.w;
    const uint16_t h = scene->pager.h;

    /* Staleness guard: a layer baked from a different ARN blob would paint
     * outdated content silently — fall back to node-tree render instead. */
    if (pkg != NULL && rle_size >= sizeof(arena_snapshot_hdr_t)) {
        arena_snapshot_hdr_t hdr;
        memcpy(&hdr, rle, sizeof(hdr));
        if (hdr.arn_crc32 != 0u &&
                hdr.arn_crc32 != arena_snapshot_crc32(pkg, pkg_size)) {
            GFX_LOGW(TAG, "baked layer stale (ARN crc mismatch), re-run uic pages");
            return -4;
        }
    }

    if (arena_snapshot_rle_decode(rle, rle_size, buf, w, h) != 0) {
        return -2;
    }
    if (pkg == NULL) {
        return 0; /* fully static page: no text overlay */
    }

    arena_t tmp = {0};
    if (arena_load(pkg, pkg_size, &tmp) != 0) {
        return -3;
    }
    gfx_render_surface_t surf = {
        .buf = buf,
        .buf_area = { 0, 0, (gfx_coord_t)w, (gfx_coord_t)h },
        .clip_area = { 0, 0, (gfx_coord_t)w, (gfx_coord_t)h },
        .stride = (gfx_coord_t)w,
        .format = scene->disp->format.render_format,
    };
    const int rc = arena_draw_clipped_mode(scene->disp, &tmp, &surf,
                                           ARENA_DRAW_MODE_OVERLAY);
    arena_free(&tmp);
    return rc;
}

int arena_scene_pager_set_offset(gfx_arena_scene_t *scene, int32_t dx)
{
    if (scene == NULL || !scene->pager.enabled || scene->pager.animating) {
        return -1;
    }
    if (!scene->pager.dragging) {
        arena_pager_refresh_stale(scene);
        arena_pager_ensure_neighbors(scene);
        scene->pager.dragging = true;
    }
    scene->pager.drag_dx = arena_pager_clamp_dx(scene, dx);
    return arena_scene_mark_dirty_all(scene);
}

int arena_scene_pager_cancel(gfx_arena_scene_t *scene)
{
    if (scene == NULL || !scene->pager.enabled) {
        return -1;
    }
    scene->pager.dragging = false;
    scene->pager.animating = false;
    scene->pager.touch_active = false;
    scene->pager.prefetch_pending = false;
    scene->pager.drag_dx = 0;
    if (scene->pager.snap_timer != NULL) {
        gfx_timer_pause(scene->pager.snap_timer);
    }
    int rc = arena_scene_mark_dirty_all(scene);
    /* Screen shows the unchanged current page; the dirty above is ours. */
    scene->pager.snap_stale = false;
    return rc;
}

int arena_scene_handle_touch(gfx_display_t *disp, const gfx_touch_event_t *event)
{
    gfx_arena_scene_t *scene = arena_scene_from_disp(disp);
    if (scene == NULL || event == NULL) {
        return 0;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        if (scene->pager.animating) {
            return 1; /* ignore input until the snap animation settles */
        }
        if (scene->pager.enabled) {
            scene->pager.touch_active = true;
            scene->pager.track = event->track_id;
            scene->pager.start_x = (int16_t)event->x;
            scene->pager.start_y = (int16_t)event->y;
        }
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
        return scene->pager.enabled ? 1 : 0;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && scene->pager.enabled &&
            scene->pager.touch_active && !scene->pager.animating &&
            event->track_id == scene->pager.track) {
        const int32_t dx = (int32_t)event->x - scene->pager.start_x;
        const int32_t dy = (int32_t)event->y - scene->pager.start_y;
        const int32_t abs_dx = (dx < 0) ? -dx : dx;
        const int32_t abs_dy = (dy < 0) ? -dy : dy;

        if (!scene->pager.dragging &&
                abs_dx >= (int32_t)scene->pager.drag_threshold && abs_dx >= abs_dy) {
            /* Enter drag: release any pressed node, refresh a stale snapshot. */
            if (scene->pressed_off != ARENA_NO_NODE) {
                arena_node_t *n = arena_node(&scene->arena, scene->pressed_off);
                if (n != NULL) {
                    n->flags = (uint16_t)(n->flags & ~ARENA_F_PRESSED);
                    (void)arena_scene_mark_dirty(scene, scene->pressed_off);
                }
                scene->pressed_off = ARENA_NO_NODE;
            }
            arena_pager_refresh_stale(scene);
            arena_pager_ensure_neighbors(scene);
            scene->pager.dragging = true;
        }
        if (scene->pager.dragging) {
            scene->pager.drag_dx = arena_pager_clamp_dx(scene, dx);
            (void)arena_scene_mark_dirty_all(scene);
            return 1;
        }
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && scene->pager.dragging &&
            event->track_id == scene->pager.track) {
        scene->pager.touch_active = false;
        scene->pager.dragging = false;
        arena_pager_snap_start(scene);
        return 1;
    }
    if (event->type == GFX_TOUCH_EVENT_RELEASE && scene->pager.enabled) {
        scene->pager.touch_active = false;
    }

    if (scene->pressed_off == ARENA_NO_NODE) {
        return scene->pager.enabled ? 1 : 0;
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
