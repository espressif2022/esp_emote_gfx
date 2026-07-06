/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Full public bundle: core + fs + default backends + all built-in widgets.
 *
 * Prefer narrower includes in new code:
 *   #include "gfx/base.h"              core runtime only
 *   #include "gfx/widgets/image.h"     one widget at a time
 *   #include "gfx/backends/sdl.h"      host backend
 */

#include "gfx/base.h"
#include "gfx/fs.h"
#include "gfx/backends/custom.h"
#include "gfx/backends/memory.h"
#include "gfx/tween.h"
#include "gfx/widgets/anim.h"
#include "gfx/widgets/button.h"
#include "gfx/widgets/container.h"
#include "gfx/widgets/coverflow.h"
#include "gfx/widgets/font_lvgl.h"
#include "gfx/widgets/image.h"
#include "gfx/widgets/image_button.h"
#include "gfx/widgets/label.h"
#include "gfx/widgets/list.h"
#include "gfx/widgets/mesh_image.h"
#include "gfx/widgets/motion.h"
#include "gfx/widgets/pageflow.h"
#include "gfx/widgets/progress_bar.h"
#include "gfx/widgets/qrcode.h"
#include "gfx/widgets/wheel.h"
