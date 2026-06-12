/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the built-in host simulator bitmap font.
 *
 * The font is intentionally tiny and ASCII-only. It is meant for simulator
 * smoke tests before a full host FreeType/SDL_ttf path is wired in.
 *
 * @return Font handle suitable for gfx_label_set_font(), gfx_button_set_font(),
 *         and gfx_list_set_font().
 */
gfx_font_t gfx_host_font_default(void);

#ifdef __cplusplus
}
#endif
