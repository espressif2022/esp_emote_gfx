/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Host builds intentionally keep ESP-IDF-only codecs disabled. */
#define CONFIG_GFX_EAF_HEATSHRINK_SUPPORT 1
#define CONFIG_HEATSHRINK_DYNAMIC_ALLOC 1
#define CONFIG_HEATSHRINK_USE_INDEX 1
