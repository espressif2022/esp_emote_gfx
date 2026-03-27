/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "core/gfx_obj.h"
#include "widget/gfx_mesh_img.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t pts[14];
} gfx_face_emote_eye_shape_t;

typedef struct {
    int16_t pts[8];
} gfx_face_emote_brow_shape_t;

typedef gfx_face_emote_eye_shape_t gfx_face_emote_mouth_shape_t;

/**
 * @brief One named facial expression preset.
 *
 * Each expression stores blend weights for the reference eye/brow/mouth
 * shapes, plus an optional look direction and hold duration.
 */
typedef struct {
    const char *name;
    const char *name_cn;
    int16_t w_smile;
    int16_t w_happy;
    int16_t w_sad;
    int16_t w_surprise;
    int16_t w_angry;
    int16_t w_look_x;
    int16_t w_look_y;
    uint32_t hold_ticks;
} gfx_face_emote_expr_t;

/**
 * @brief Continuous facial blend state.
 *
 * Compared with `gfx_face_emote_expr_t`, this type has no naming metadata and
 * is intended for runtime control or external interpolation.
 */
typedef struct {
    int16_t w_smile;
    int16_t w_happy;
    int16_t w_sad;
    int16_t w_surprise;
    int16_t w_angry;
    int16_t look_x;
    int16_t look_y;
} gfx_face_emote_mix_t;

/**
 * @brief Reference asset bundle for one face-emote widget.
 *
 * `ref_eye`, `ref_brow`, and `ref_mouth` provide the base shape sets used by
 * `gfx_face_emote_set_mix()` and `gfx_face_emote_set_expression_name()`.
 * `sequence` is optional and provides named presets built on top of those
 * reference shapes.
 */
typedef struct {
    const gfx_face_emote_eye_shape_t *ref_eye;
    size_t ref_eye_count;
    const gfx_face_emote_brow_shape_t *ref_brow;
    size_t ref_brow_count;
    const gfx_face_emote_mouth_shape_t *ref_mouth;
    size_t ref_mouth_count;
    const gfx_face_emote_expr_t *sequence;
    size_t sequence_count;
} gfx_face_emote_assets_t;

/**
 * @brief Layout and mesh-density configuration for the face-emote widget.
 *
 * These fields control:
 * - overall logical display size
 * - relative anchor offsets
 * - mesh segment density
 * - scale and stroke thickness
 */
typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    int16_t mouth_x_ofs;
    int16_t mouth_y_ofs;
    int16_t eye_x_half_gap;
    int16_t eye_y_ofs;
    int16_t brow_y_ofs_extra;
    uint16_t timer_period_ms;
    uint8_t eye_segs;
    uint8_t brow_segs;
    uint8_t mouth_segs;
    int16_t eye_scale_percent;
    int16_t brow_scale_percent;
    int16_t mouth_scale_percent;
    int16_t brow_thickness;
    int16_t mouth_thickness;
} gfx_face_emote_cfg_t;

/**
 * @brief Fill configuration from the widget (or screen) pixel size.
 *
 * This helper computes a proportional default layout from a reference size.
 * Callers may then modify individual fields before passing the config to
 * `gfx_face_emote_set_config()`.
 *
 * @param cfg                 Output configuration
 * @param display_w           Width in pixels (0 is clamped to 1)
 * @param display_h           Height in pixels (0 is clamped to 1)
 * @param layout_ref_x  Horizontal layout reference (e.g. BSP_LCD_H_RES × ratio). Must be > 0.
 * @param layout_ref_y  Vertical layout reference (e.g. BSP_LCD_V_RES × ratio). Must be > 0.
 */
void gfx_face_emote_cfg_init(gfx_face_emote_cfg_t *cfg, uint16_t display_w, uint16_t display_h,
                             uint16_t layout_ref_x, uint16_t layout_ref_y);

/**
 * @brief Create a face-emote widget.
 *
 * The widget internally composes several mesh-image children for mouth, eyes,
 * and brows. `layout_ref_x/y` should match the reference size used to design
 * the facial proportions.
 *
 * @param layout_ref_x  Same as gfx_face_emote_cfg_init()
 * @param layout_ref_y  Same as gfx_face_emote_cfg_init()
 * @return Created object, or NULL on failure
 */
gfx_obj_t *gfx_face_emote_create(gfx_disp_t *disp, uint16_t layout_ref_x, uint16_t layout_ref_y);

/**
 * @brief Apply widget layout and mesh-density configuration.
 *
 * Compared with `gfx_face_emote_cfg_init()`, this applies an already prepared
 * configuration to a live widget instance.
 *
 * @param obj Face-emote widget
 * @param cfg Configuration to apply
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_face_emote_set_config(gfx_obj_t *obj, const gfx_face_emote_cfg_t *cfg);

/**
 * @brief Set the reference shape assets and optional expression sequence.
 *
 * This must be called before expression-name or runtime-mix control can work.
 *
 * @param obj Face-emote widget
 * @param assets Reference shape and expression assets
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_face_emote_set_assets(gfx_obj_t *obj, const gfx_face_emote_assets_t *assets);

/**
 * @brief Set the rendered face color.
 *
 * This updates the solid-color source used by the internal mesh-image parts.
 *
 * @param obj Face-emote widget
 * @param color New face color
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_face_emote_set_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Apply one named expression from the current asset sequence.
 *
 * Compared with `gfx_face_emote_set_mix()`, this selects a named preset
 * instead of supplying weights directly.
 *
 * @param obj Face-emote widget
 * @param name Expression name
 * @param snap_now Whether to jump directly to the target pose
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_face_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now);

/**
 * @brief Apply a runtime facial blend mix.
 *
 * Compared with `gfx_face_emote_set_expression_name()`, this directly uses
 * caller-provided weights and look direction instead of a named preset.
 *
 * @param obj Face-emote widget
 * @param mix Runtime blend weights
 * @param snap_now Whether to jump directly to the target pose
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_face_emote_set_mix(gfx_obj_t *obj, const gfx_face_emote_mix_t *mix, bool snap_now);

/**
 * @brief Override or release automatic look direction.
 *
 * When enabled, the widget uses the supplied look vector directly.
 * When disabled, look direction follows the current expression or mix target.
 *
 * @param obj Face-emote widget
 * @param enabled Whether manual look is enabled
 * @param look_x Manual look x
 * @param look_y Manual look y
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_face_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y);

#ifdef __cplusplus
}
#endif
