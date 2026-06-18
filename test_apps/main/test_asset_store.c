/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "unity.h"

#include "core/gfx_asset.h"
#include "mmap_generate_assets_test.h"

TEST_CASE("asset: mmap-assets store returns direct views", "[asset]")
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_view_t view;
    gfx_asset_store_caps_t caps;
    const gfx_asset_mmap_config_t mmap_config = {
        .partition_label = "assets_test",
        .full_check = true,
    };

    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_open_mmap(&mmap_config, &store));
    TEST_ASSERT_NOT_NULL(store);
    TEST_ASSERT_EQUAL(GFX_ASSET_BACKEND_MMAP, gfx_asset_store_get_backend(store));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_get_caps(store, &caps));
    TEST_ASSERT_TRUE(caps.open_by_id);
    TEST_ASSERT_TRUE(caps.open_by_name);
    TEST_ASSERT_TRUE(caps.can_direct_addr);
    TEST_ASSERT_FALSE(caps.can_owned_copy);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_open_by_id(store, MMAP_ASSETS_TEST_ICON_RGB565_BIN, &view));
    TEST_ASSERT_NOT_NULL(view.data);
    TEST_ASSERT_GREATER_THAN_UINT32(0, view.size);
    TEST_ASSERT_EQUAL(MMAP_ASSETS_TEST_ICON_RGB565_BIN, view.id);
    TEST_ASSERT_TRUE((view.flags & GFX_ASSET_VIEW_FLAG_DIRECT_ADDR) != 0U);
    TEST_ASSERT_TRUE((view.flags & GFX_ASSET_VIEW_FLAG_MAPPED) != 0U);
    TEST_ASSERT_TRUE((view.flags & GFX_ASSET_VIEW_FLAG_PERSISTENT) != 0U);
    TEST_ASSERT_TRUE(view.is_mapped);
    gfx_asset_view_close(&view);
    TEST_ASSERT_NULL(view.data);
    TEST_ASSERT_EQUAL(-1, view.id);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_open_by_name(store, "icon_rgb565.bin", &view));
    TEST_ASSERT_NOT_NULL(view.data);
    TEST_ASSERT_TRUE((view.flags & GFX_ASSET_VIEW_FLAG_DIRECT_ADDR) != 0U);
    gfx_asset_view_close(&view);
    gfx_asset_store_close(store);
}

TEST_CASE("asset: VFS dir store exposes copy-backed capabilities", "[asset]")
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_view_t view;
    gfx_asset_store_caps_t caps;
    const gfx_asset_store_config_t config = {
        .type = GFX_ASSET_STORE_TYPE_DIR,
        .load_mode = GFX_ASSET_LOAD_FORCE_COPY,
        .root_dir = "/",
    };

    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_open(&config, &store));
    TEST_ASSERT_NOT_NULL(store);
    TEST_ASSERT_EQUAL(GFX_ASSET_BACKEND_DIR, gfx_asset_store_get_backend(store));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_get_caps(store, &caps));
    TEST_ASSERT_TRUE(caps.open_by_name);
    TEST_ASSERT_FALSE(caps.open_by_id);
    TEST_ASSERT_FALSE(caps.can_direct_addr);
    TEST_ASSERT_TRUE(caps.can_owned_copy);
    TEST_ASSERT_EQUAL(GFX_ERR_NOT_FOUND, gfx_asset_open_by_name(store, "__gfx_missing_asset__.bin", &view));
    TEST_ASSERT_NULL(view.data);
    TEST_ASSERT_EQUAL(-1, view.id);
    gfx_asset_store_close(store);
}

TEST_CASE("asset: raw partition region supports copy and preferred-direct views", "[asset]")
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_view_t view;
    gfx_asset_store_caps_t caps;
    const gfx_asset_region_t region = {
        .offset = 0,
        .size = 16,
        .name = "assets_test_header",
        .id = 7,
    };
    const gfx_asset_store_config_t copy_config = {
        .type = GFX_ASSET_STORE_TYPE_PARTITION,
        .load_mode = GFX_ASSET_LOAD_FORCE_COPY,
        .partition_label = "assets_test",
    };
    const gfx_asset_store_config_t direct_config = {
        .type = GFX_ASSET_STORE_TYPE_PARTITION,
        .load_mode = GFX_ASSET_LOAD_PREFER_DIRECT,
        .partition_label = "assets_test",
    };

    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_open(&copy_config, &store));
    TEST_ASSERT_EQUAL(GFX_ASSET_BACKEND_PARTITION, gfx_asset_store_get_backend(store));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_get_caps(store, &caps));
    TEST_ASSERT_TRUE(caps.open_region);
    TEST_ASSERT_FALSE(caps.can_direct_addr);
    TEST_ASSERT_TRUE(caps.can_owned_copy);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_open_region(store, &region, &view));
    TEST_ASSERT_NOT_NULL(view.data);
    TEST_ASSERT_EQUAL(region.size, view.size);
    TEST_ASSERT_EQUAL(region.id, view.id);
    TEST_ASSERT_TRUE((view.flags & GFX_ASSET_VIEW_FLAG_OWNED) != 0U);
    TEST_ASSERT_FALSE(view.is_mapped);
    gfx_asset_view_close(&view);
    gfx_asset_store_close(store);

    store = NULL;
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_open(&direct_config, &store));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_store_get_caps(store, &caps));
    TEST_ASSERT_TRUE(caps.can_direct_addr);
    TEST_ASSERT_TRUE(caps.can_owned_copy);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_asset_open_region(store, &region, &view));
    TEST_ASSERT_NOT_NULL(view.data);
    TEST_ASSERT_EQUAL(region.size, view.size);
    TEST_ASSERT_TRUE((view.flags & (GFX_ASSET_VIEW_FLAG_DIRECT_ADDR | GFX_ASSET_VIEW_FLAG_OWNED)) != 0U);
    gfx_asset_view_close(&view);
    gfx_asset_store_close(store);
}
