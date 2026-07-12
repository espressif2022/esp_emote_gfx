/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_arena_model_smoke — address-model only (no renderer).
 * Renderer / A-B: gfx_arena_draw_demo, gfx_arena_dirty_demo, gfx_arena_compare_demo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/scene/arena.h"

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
    printf("OK  %s\n", msg); \
} while (0)

static int check_corrupt_packages(const uint8_t *pkg, size_t pkg_size)
{
    uint8_t *bad = (uint8_t *)malloc(pkg_size);
    gfx_arena_t arena = {0};
    gfx_arena_hdr_t *hdr;
    gfx_arena_node_t *node;

    CHECK(bad != NULL, "allocate corrupt-package fixture");

    memcpy(bad, pkg, pkg_size);
    hdr = (gfx_arena_hdr_t *)bad;
    hdr->magic = 0U;
    CHECK(gfx_arena_load(bad, pkg_size, &arena) != 0, "reject bad magic");

    memcpy(bad, pkg, pkg_size);
    hdr = (gfx_arena_hdr_t *)bad;
    hdr->version++;
    CHECK(gfx_arena_load(bad, pkg_size, &arena) != 0, "reject unsupported version");

    memcpy(bad, pkg, pkg_size);
    hdr = (gfx_arena_hdr_t *)bad;
    hdr->total_size--;
    CHECK(gfx_arena_load(bad, pkg_size, &arena) != 0, "reject mismatched total size");

    memcpy(bad, pkg, pkg_size);
    hdr = (gfx_arena_hdr_t *)bad;
    hdr->root_off = hdr->nodes_off + 1U;
    CHECK(gfx_arena_load(bad, pkg_size, &arena) != 0, "reject unaligned root node offset");

    memcpy(bad, pkg, pkg_size);
    hdr = (gfx_arena_hdr_t *)bad;
    node = (gfx_arena_node_t *)(bad + hdr->nodes_off);
    node->first_child = (uint32_t)pkg_size;
    CHECK(gfx_arena_load(bad, pkg_size, &arena) != 0, "reject child node offset outside table");

    memcpy(bad, pkg, pkg_size);
    hdr = (gfx_arena_hdr_t *)bad;
    node = (gfx_arena_node_t *)(bad + hdr->nodes_off);
    node->name_off = (uint32_t)pkg_size;
    CHECK(gfx_arena_load(bad, pkg_size, &arena) != 0, "reject string offset outside table");

    free(bad);
    return 0;
}

int main(void)
{
    printf("arena_model_smoke: relative-offset isomorphic arena\n");
    printf("sizeof(gfx_arena_hdr_t)=%zu sizeof(gfx_arena_node_t)=%zu\n",
           sizeof(gfx_arena_hdr_t), sizeof(gfx_arena_node_t));

    const gfx_arena_desc_t descs[] = {
        {
            GFX_ARENA_NODE_CONTAINER, GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG, 0, 0, 800, 480,
            0x1c1f2e, "root", -1
        },
        {
            GFX_ARENA_NODE_LABEL, GFX_ARENA_F_VISIBLE, 16, 12, 200, 28,
            0, "title", 0
        },
        {
            GFX_ARENA_NODE_BUTTON, GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG, 680, 420, 96, 40,
            0x2f8cff, "ok", 0
        },
    };
    const uint16_t count = (uint16_t)(sizeof(descs) / sizeof(descs[0]));

    size_t pkg_size = 0;
    uint8_t *pkg = gfx_arena_pack(descs, count, &pkg_size);
    CHECK(pkg != NULL, "pack returns buffer");

    gfx_arena_t arena = {0};
    CHECK(gfx_arena_load(pkg, pkg_size, &arena) == 0, "load = memcpy + validate");
    CHECK(arena.base != pkg, "runtime arena is a writable copy");
    CHECK(check_corrupt_packages(pkg, pkg_size) == 0, "corrupt packages rejected");

    printf("--- tree after load ---\n");
    gfx_arena_dump_tree(&arena, gfx_arena_hdr(&arena)->root_off, 0);

    gfx_arena_node_t *root = gfx_arena_node(&arena, gfx_arena_hdr(&arena)->root_off);
    CHECK(root != NULL && root->first_child != GFX_ARENA_NO_NODE, "root has children");
    gfx_arena_node_t *title = gfx_arena_node(&arena, root->first_child);
    CHECK(title != NULL && title->next_sibling != GFX_ARENA_NO_NODE, "title has sibling");
    CHECK(strcmp(gfx_arena_str(&arena, title->name_off), "title") == 0, "first child is title");

    gfx_arena_node_t *ok = gfx_arena_find_by_name(&arena, "ok");
    CHECK(ok != NULL, "find button by name");
    CHECK(ok == gfx_arena_node(&arena, title->next_sibling), "ok is title's next_sibling");

    const uint32_t old_bg = ok->bg_rgb;
    ok->bg_rgb = 0xff6644;
    CHECK(ok->bg_rgb == 0xff6644 && old_bg != ok->bg_rgb, "in-place mutate button bg");

    const gfx_arena_hdr_t *rom_hdr = (const gfx_arena_hdr_t *)pkg;
    const gfx_arena_node_t *rom_ok =
        (const gfx_arena_node_t *)(pkg + rom_hdr->nodes_off + 2u * sizeof(gfx_arena_node_t));
    CHECK(rom_ok->bg_rgb == 0x2f8cff, "ROM package unchanged after mutate");

    printf("--- tree after in-place mutate ---\n");
    gfx_arena_dump_tree(&arena, gfx_arena_hdr(&arena)->root_off, 0);

    printf("\nModel checklist:\n");
    printf("  [x] isomorphic / fast / in-place / portable\n");
    printf("  [ ] renderer  -> see gfx_arena_draw_demo / gfx_arena_compare_demo\n");

    gfx_arena_free(&arena);
    free(pkg);
    printf("PASS\n");
    return 0;
}
