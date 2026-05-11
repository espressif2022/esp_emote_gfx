/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Lightweight public core API.
 *
 * Include this when a module only needs the runtime/display/object/timer/touch
 * layer and does not need widget APIs. Application code normally includes
 * "gfx.h" instead.
 */

#include "core/gfx_types.h"
#include "core/gfx_log.h"
#include "core/gfx_core.h"
#include "core/gfx_disp.h"
#include "core/gfx_obj.h"
#include "core/gfx_timer.h"
#include "core/gfx_touch.h"
