/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Main public umbrella header.
 *
 * Applications should usually include only this file. It exposes the public
 * core API plus the built-in widgets. Private implementation headers stay
 * under src/ and are intentionally not reachable from here.
 */

#include "gfx_base.h"
#include "widget/gfx_anim.h"
#include "widget/gfx_button.h"
#include "widget/gfx_font_lvgl.h"
#include "widget/gfx_img.h"
#include "widget/gfx_label.h"
#include "widget/gfx_list.h"
#include "widget/gfx_mesh_img.h"
#include "widget/gfx_motion_scene.h"
#include "widget/gfx_qrcode.h"
