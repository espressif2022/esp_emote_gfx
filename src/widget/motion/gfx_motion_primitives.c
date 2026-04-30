/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdint.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_MOTION
#include "common/gfx_log_priv.h"
#include "widget/motion/gfx_motion_player_priv.h"

static const char *TAG = "gfx_motion_prim";

uint8_t gfx_motion_player_ring_segs(float radius)
{
    int32_t s = (int32_t)lroundf(radius * 2.0f);
    if (s < (int32_t)GFX_MOTION_RING_SEGS_MIN) {
        s = (int32_t)GFX_MOTION_RING_SEGS_MIN;
    }
    if (s > (int32_t)GFX_MOTION_RING_SEGS_MAX) {
        s = (int32_t)GFX_MOTION_RING_SEGS_MAX;
    }
    return (uint8_t)s;
}

esp_err_t gfx_motion_player_apply_capsule(gfx_obj_t *obj,
        const gfx_motion_player_screen_point_t *a,
        const gfx_motion_player_screen_point_t *b,
        int32_t thick)
{
    gfx_mesh_img_point_q8_t pts[4];
    float ax = (float)a->x, ay = (float)a->y;
    float bx = (float)b->x, by = (float)b->y;
    float dx = bx - ax, dy = by - ay;
    float len  = sqrtf(dx * dx + dy * dy);
    float half = (float)thick * 0.5f;
    int32_t px[4], py[4];
    int32_t min_x, max_x, min_y, max_y;

    if (len <= 0.001f) {
        px[0] = (int32_t)(ax - half); py[0] = (int32_t)(ay - half);
        px[1] = (int32_t)(ax + half); py[1] = (int32_t)(ay - half);
        px[2] = (int32_t)(ax - half); py[2] = (int32_t)(ay + half);
        px[3] = (int32_t)(ax + half); py[3] = (int32_t)(ay + half);
    } else {
        float tx = dx / len * half, ty = dy / len * half;
        float nx = -dy / len * half, ny = dx / len * half;
        px[0] = (int32_t)lroundf(ax - tx + nx); py[0] = (int32_t)lroundf(ay - ty + ny);
        px[1] = (int32_t)lroundf(bx + tx + nx); py[1] = (int32_t)lroundf(by + ty + ny);
        px[2] = (int32_t)lroundf(ax - tx - nx); py[2] = (int32_t)lroundf(ay - ty - ny);
        px[3] = (int32_t)lroundf(bx + tx - nx); py[3] = (int32_t)lroundf(by + ty - ny);
    }

    min_x = max_x = px[0]; min_y = max_y = py[0];
    for (int i = 1; i < 4; i++) {
        if (px[i] < min_x) {
            min_x = px[i];
        } else if (px[i] > max_x) {
            max_x = px[i];
        }
        if (py[i] < min_y) {
            min_y = py[i];
        } else if (py[i] > max_y) {
            max_y = py[i];
        }
    }
    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    for (int i = 0; i < 4; i++) {
        pts[i].x_q8 = (px[i] - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[i].y_q8 = (py[i] - min_y) << GFX_MESH_FRAC_SHIFT;
    }
    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "capsule align");
    return gfx_mesh_img_set_points_q8(obj, pts, 4U);
}

esp_err_t gfx_motion_player_apply_ring(gfx_obj_t *obj,
                                       gfx_motion_player_runtime_scratch_t *scratch,
                                       const gfx_motion_player_screen_point_t *c,
                                       int32_t radius, int32_t thick, uint8_t segs)
{
    gfx_mesh_img_point_q8_t *pts = scratch->ring_pts;
    float outer_r = (float)radius + (float)thick * 0.5f;
    float inner_r = (float)radius - (float)thick * 0.5f;
    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;

    if (inner_r < 1.0f) {
        inner_r = 1.0f;
    }

    for (uint8_t i = 0; i <= segs; i++) {
        float ang = (float)i * 2.0f * (float)M_PI / (float)segs;
        int32_t ox = (int32_t)lroundf((float)c->x + cosf(ang) * outer_r);
        int32_t oy = (int32_t)lroundf((float)c->y + sinf(ang) * outer_r);
        int32_t ix = (int32_t)lroundf((float)c->x + cosf(ang) * inner_r);
        int32_t iy = (int32_t)lroundf((float)c->y + sinf(ang) * inner_r);

        if (ox < min_x) {
            min_x = ox;
        } if (ix < min_x) {
            min_x = ix;
        }
        if (ox > max_x) {
            max_x = ox;
        } if (ix > max_x) {
            max_x = ix;
        }
        if (oy < min_y) {
            min_y = oy;
        } if (iy < min_y) {
            min_y = iy;
        }
        if (oy > max_y) {
            max_y = oy;
        } if (iy > max_y) {
            max_y = iy;
        }

        pts[i].x_q8 = ox << GFX_MESH_FRAC_SHIFT;
        pts[i].y_q8 = oy << GFX_MESH_FRAC_SHIFT;
        pts[(size_t)segs + 1 + i].x_q8 = ix << GFX_MESH_FRAC_SHIFT;
        pts[(size_t)segs + 1 + i].y_q8 = iy << GFX_MESH_FRAC_SHIFT;
    }
    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    for (size_t i = 0; i < ((size_t)segs + 1U) * 2U; i++) {
        pts[i].x_q8 -= (min_x << GFX_MESH_FRAC_SHIFT);
        pts[i].y_q8 -= (min_y << GFX_MESH_FRAC_SHIFT);
    }
    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "ring align");
    return gfx_mesh_img_set_points_q8(obj, pts, ((size_t)segs + 1U) * 2U);
}

static void gfx_motion_prim_cubic_bezier(const gfx_motion_player_screen_point_t *p0,
        const gfx_motion_player_screen_point_t *p1,
        const gfx_motion_player_screen_point_t *p2,
        const gfx_motion_player_screen_point_t *p3,
        float t, gfx_motion_player_screen_point_t *out)
{
    float u  = 1.0f - t;
    float u2 = u * u, u3 = u2 * u;
    float t2 = t * t, t3 = t2 * t;
    out->x = (int32_t)lroundf(u3 * (float)p0->x + 3.0f * u2 * t * (float)p1->x
                              + 3.0f * u * t2 * (float)p2->x + t3 * (float)p3->x);
    out->y = (int32_t)lroundf(u3 * (float)p0->y + 3.0f * u2 * t * (float)p1->y
                              + 3.0f * u * t2 * (float)p2->y + t3 * (float)p3->y);
}

static inline void gfx_motion_prim_cubic_pos_tan(const gfx_motion_player_screen_point_t *p0,
        const gfx_motion_player_screen_point_t *p1,
        const gfx_motion_player_screen_point_t *p2,
        const gfx_motion_player_screen_point_t *p3,
        float t,
        float *px, float *py,
        float *tx, float *ty)
{
    float u  = 1.0f - t;
    float u2 = u * u, t2 = t * t;
    *px = u2 * u * (float)p0->x + 3.0f * u2 * t * (float)p1->x
          + 3.0f * u * t2 * (float)p2->x + t2 * t * (float)p3->x;
    *py = u2 * u * (float)p0->y + 3.0f * u2 * t * (float)p1->y
          + 3.0f * u * t2 * (float)p2->y + t2 * t * (float)p3->y;
    *tx = 3.0f * u2 * (float)(p1->x - p0->x) + 6.0f * u * t * (float)(p2->x - p1->x)
          + 3.0f * t2 * (float)(p3->x - p2->x);
    *ty = 3.0f * u2 * (float)(p1->y - p0->y) + 6.0f * u * t * (float)(p2->y - p1->y)
          + 3.0f * t2 * (float)(p3->y - p2->y);

    if (fabsf(*tx) + fabsf(*ty) < 1e-6f) {
        *tx = (float)(p2->x - p0->x); *ty = (float)(p2->y - p0->y);
        if (fabsf(*tx) + fabsf(*ty) < 1e-6f) {
            *tx = (float)(p3->x - p0->x); *ty = (float)(p3->y - p0->y);
            if (fabsf(*tx) + fabsf(*ty) < 1e-6f) {
                *tx = 1.0f; *ty = 0.0f;
            }
        }
    }
}

esp_err_t gfx_motion_player_apply_bezier(gfx_obj_t *obj,
        gfx_motion_player_runtime_scratch_t *scratch,
        const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n, int32_t thick, bool loop)
{
    gfx_mesh_img_point_q8_t *pts = scratch->bezier_pts;
    int32_t *ox = scratch->bezier_ox;
    int32_t *oy = scratch->bezier_oy;
    int32_t *ix_arr = scratch->bezier_ix;
    int32_t *iy_arr = scratch->bezier_iy;
    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;
    float half = (float)thick * 0.5f;
    uint16_t M = 0;

    if (n < 4U) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t k = (uint8_t)((n - 1U) / 3U);

    {
        const float min_step2 = 1.0f;
        float last_px = 0.0f, last_py = 0.0f;
        float last_nx = 0.0f, last_ny = 0.0f;
        bool have_last = false;

        for (uint8_t seg = 0U; seg < k; seg++) {
            const gfx_motion_player_screen_point_t *p0 = &ctrl[seg * 3U];
            const gfx_motion_player_screen_point_t *p1 = &ctrl[seg * 3U + 1U];
            const gfx_motion_player_screen_point_t *p2 = &ctrl[seg * 3U + 2U];
            const gfx_motion_player_screen_point_t *p3 = loop
                    ? &ctrl[((seg + 1U) * 3U) % (n - 1U)]
                    : &ctrl[(seg + 1U) * 3U];
            for (uint8_t sub = 0U; sub < MOTION_BEZIER_SEGS_PER_SEG; sub++) {
                float t = (float)sub / (float)MOTION_BEZIER_SEGS_PER_SEG;
                float px, py, tx, ty;
                gfx_motion_prim_cubic_pos_tan(p0, p1, p2, p3, t, &px, &py, &tx, &ty);
                float len = sqrtf(tx * tx + ty * ty);
                if (len < 1e-6f) {
                    tx = 1.0f;
                    ty = 0.0f;
                } else             {
                    tx /= len;
                    ty /= len;
                }
                float nx = -ty, ny = tx;
                if (have_last) {
                    float dx = px - last_px, dy = py - last_py;
                    if (dx * dx + dy * dy < min_step2) {
                        px = last_px; py = last_py;
                        nx = last_nx; ny = last_ny;
                    } else {
                        last_px = px; last_py = py;
                        last_nx = nx; last_ny = ny;
                    }
                } else {
                    last_px = px; last_py = py;
                    last_nx = nx; last_ny = ny;
                    have_last = true;
                }
                ox[M]     = (int32_t)lroundf(px + nx * half);
                oy[M]     = (int32_t)lroundf(py + ny * half);
                ix_arr[M] = (int32_t)lroundf(px - nx * half);
                iy_arr[M] = (int32_t)lroundf(py - ny * half);
                M++;
            }
        }
        if (!loop) {
            const gfx_motion_player_screen_point_t *p0 = &ctrl[(k - 1U) * 3U];
            const gfx_motion_player_screen_point_t *p1 = &ctrl[(k - 1U) * 3U + 1U];
            const gfx_motion_player_screen_point_t *p2 = &ctrl[(k - 1U) * 3U + 2U];
            const gfx_motion_player_screen_point_t *p3 = &ctrl[k * 3U];
            float px, py, tx, ty;
            gfx_motion_prim_cubic_pos_tan(p0, p1, p2, p3, 1.0f, &px, &py, &tx, &ty);
            float len = sqrtf(tx * tx + ty * ty);
            if (len < 1e-6f) {
                tx = 1.0f;
                ty = 0.0f;
            } else             {
                tx /= len;
                ty /= len;
            }
            float nx = -ty, ny = tx;
            if (have_last) {
                float dx = px - last_px, dy = py - last_py;
                if (dx * dx + dy * dy < min_step2) {
                    px = last_px; py = last_py;
                    nx = last_nx; ny = last_ny;
                }
            }
            ox[M]     = (int32_t)lroundf(px + nx * half);
            oy[M]     = (int32_t)lroundf(py + ny * half);
            ix_arr[M] = (int32_t)lroundf(px - nx * half);
            iy_arr[M] = (int32_t)lroundf(py - ny * half);
            M++;
        }
    }

    uint16_t cols;
    if (loop) {
        ox[M] = ox[0]; oy[M] = oy[0];
        ix_arr[M] = ix_arr[0]; iy_arr[M] = iy_arr[0];
        cols = (uint16_t)(M + 1U);
    } else {
        cols = M;
    }

    for (uint16_t i = 0; i < cols; i++) {
        if (ox[i] < min_x) {
            min_x = ox[i];
        } if (ox[i] > max_x) {
            max_x = ox[i];
        }
        if (oy[i] < min_y) {
            min_y = oy[i];
        } if (oy[i] > max_y) {
            max_y = oy[i];
        }
        if (ix_arr[i] < min_x) {
            min_x = ix_arr[i];
        } if (ix_arr[i] > max_x) {
            max_x = ix_arr[i];
        }
        if (iy_arr[i] < min_y) {
            min_y = iy_arr[i];
        } if (iy_arr[i] > max_y) {
            max_y = iy_arr[i];
        }
    }
    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    for (uint16_t i = 0; i < cols; i++) {
        pts[i].x_q8 = (ox[i] - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[i].y_q8 = (oy[i] - min_y) << GFX_MESH_FRAC_SHIFT;
        pts[cols + i].x_q8 = (ix_arr[i] - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[cols + i].y_q8 = (iy_arr[i] - min_y) << GFX_MESH_FRAC_SHIFT;
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "bezier align");
    return gfx_mesh_img_set_points_q8(obj, pts, (size_t)cols * 2U);
}

static uint16_t gfx_motion_prim_closed_loop_centerline_fixed(const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n, uint8_t samples_per_seg,
        float *ox, float *oy, uint16_t max_o)
{
    const uint8_t k = (uint8_t)((n - 1U) / 3U);
    uint16_t M = 0U;

    for (uint8_t seg = 0U; seg < k; seg++) {
        const gfx_motion_player_screen_point_t *p0 = &ctrl[seg * 3U];
        const gfx_motion_player_screen_point_t *p1 = &ctrl[seg * 3U + 1U];
        const gfx_motion_player_screen_point_t *p2 = &ctrl[seg * 3U + 2U];
        const gfx_motion_player_screen_point_t *p3 = &ctrl[((seg + 1U) * 3U) % (n - 1U)];

        for (uint8_t sub = 0U; sub < samples_per_seg; sub++) {
            float t = (float)sub / (float)samples_per_seg;
            float px, py, tx, ty;
            if (M >= max_o) {
                return M;
            }
            gfx_motion_prim_cubic_pos_tan(p0, p1, p2, p3, t, &px, &py, &tx, &ty);
            ox[M] = px;
            oy[M] = py;
            M++;
        }
    }
    return M;
}

static esp_err_t gfx_motion_prim_apply_bezier_fill_hub(gfx_obj_t *obj,
        gfx_motion_player_runtime_scratch_t *scratch,
        const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n)
{
    const uint8_t k = (uint8_t)((n - 1U) / 3U);
    const uint16_t expect = (uint16_t)k * (uint16_t)MOTION_BEZIER_FILL_LOOP_SEGS_PER_SEG;
    float *ox = scratch->hub_ox;
    float *oy = scratch->hub_oy;
    uint16_t M = gfx_motion_prim_closed_loop_centerline_fixed(
                     ctrl, n, MOTION_BEZIER_FILL_LOOP_SEGS_PER_SEG, ox, oy, MOTION_BEZIER_MAX_TESS + 1U);

    if (M != expect) {
        return ESP_ERR_INVALID_STATE;
    }

    double cx = 0.0, cy = 0.0;
    for (uint16_t i = 0; i < M; i++) {
        cx += (double)ox[i];
        cy += (double)oy[i];
    }
    cx /= (double)M;
    cy /= (double)M;

    const uint16_t gcols = expect;
    const size_t pc = (size_t)(gcols + 1U) * 2U;
    gfx_mesh_img_point_q8_t *pts = scratch->hub_pts;

    if (pc > MOTION_HUB_FILL_MAX_PTS) {
        return ESP_ERR_INVALID_STATE;
    }

    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;
    int32_t hub_x = (int32_t)lroundf((float)cx);
    int32_t hub_y = (int32_t)lroundf((float)cy);

    for (uint16_t col = 0; col <= gcols; col++) {
        int32_t rx = (col < M) ? (int32_t)lroundf(ox[col]) : (int32_t)lroundf(ox[0]);
        int32_t ry = (col < M) ? (int32_t)lroundf(oy[col]) : (int32_t)lroundf(oy[0]);

        if (hub_x < min_x) {
            min_x = hub_x;
        } if (hub_x > max_x) {
            max_x = hub_x;
        }
        if (hub_y < min_y) {
            min_y = hub_y;
        } if (hub_y > max_y) {
            max_y = hub_y;
        }
        if (rx < min_x) {
            min_x = rx;
        } if (rx > max_x) {
            max_x = rx;
        }
        if (ry < min_y) {
            min_y = ry;
        } if (ry > max_y) {
            max_y = ry;
        }
    }

    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    for (uint16_t col = 0; col <= gcols; col++) {
        size_t i0 = (size_t)col;
        size_t i1 = (size_t)(gcols + 1U) + (size_t)col;
        int32_t rx = (col < M) ? (int32_t)lroundf(ox[col]) : (int32_t)lroundf(ox[0]);
        int32_t ry = (col < M) ? (int32_t)lroundf(oy[col]) : (int32_t)lroundf(oy[0]);

        pts[i0].x_q8 = (hub_x - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[i0].y_q8 = (hub_y - min_y) << GFX_MESH_FRAC_SHIFT;
        pts[i1].x_q8 = (rx - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[i1].y_q8 = (ry - min_y) << GFX_MESH_FRAC_SHIFT;
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "bezier_fill_hub align");
    return gfx_mesh_img_set_points_q8(obj, pts, pc);
}

esp_err_t gfx_motion_player_apply_bezier_fill(gfx_obj_t *obj,
        gfx_motion_player_runtime_scratch_t *scratch,
        const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n)
{
    const uint8_t segs = MOTION_BEZIER_FILL_SEGS;
    const uint16_t cols = (uint16_t)segs + 1U;
    gfx_mesh_img_point_q8_t *pts = scratch->fill_pts;
    gfx_motion_player_screen_point_t *upper = scratch->fill_upper;
    gfx_motion_player_screen_point_t *lower = scratch->fill_lower;
    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;

    if (n != 7U && n != 13U) {
        if (((n - 1U) % 3U) != 0U || n < 4U) {
            return ESP_ERR_INVALID_ARG;
        }
        return gfx_motion_prim_apply_bezier_fill_hub(obj, scratch, ctrl, n);
    }

    if (n == 7U) {
        for (uint8_t i = 0; i <= segs; i++) {
            float t = (float)i / (float)segs;
            gfx_motion_prim_cubic_bezier(&ctrl[0], &ctrl[1], &ctrl[2], &ctrl[3], t, &upper[i]);
        }
    } else {
        const uint8_t h = segs / 2U;
        for (uint8_t i = 0; i <= h; i++) {
            gfx_motion_prim_cubic_bezier(&ctrl[9], &ctrl[10], &ctrl[11], &ctrl[12],
                                         (float)i / (float)h, &upper[i]);
        }
        for (uint8_t i = 1; i <= segs - h; i++) {
            gfx_motion_prim_cubic_bezier(&ctrl[0], &ctrl[1], &ctrl[2], &ctrl[3],
                                         (float)i / (float)(segs - h), &upper[h + i]);
        }
    }

    if (n == 7U) {
        for (uint8_t i = 0; i <= segs; i++) {
            float t = (float)(segs - i) / (float)segs;
            gfx_motion_prim_cubic_bezier(&ctrl[3], &ctrl[4], &ctrl[5], &ctrl[6], t, &lower[i]);
        }
    } else {
        const uint8_t h = segs / 2U;
        for (uint8_t i = 0; i <= h; i++) {
            gfx_motion_prim_cubic_bezier(&ctrl[9], &ctrl[8], &ctrl[7], &ctrl[6],
                                         (float)i / (float)h, &lower[i]);
        }
        for (uint8_t i = 1; i <= segs - h; i++) {
            gfx_motion_prim_cubic_bezier(&ctrl[6], &ctrl[5], &ctrl[4], &ctrl[3],
                                         (float)i / (float)(segs - h), &lower[h + i]);
        }
    }

    for (uint16_t i = 0; i < cols; i++) {
        if (upper[i].x < min_x) {
            min_x = upper[i].x;
        } if (upper[i].x > max_x) {
            max_x = upper[i].x;
        }
        if (upper[i].y < min_y) {
            min_y = upper[i].y;
        } if (upper[i].y > max_y) {
            max_y = upper[i].y;
        }
        if (lower[i].x < min_x) {
            min_x = lower[i].x;
        } if (lower[i].x > max_x) {
            max_x = lower[i].x;
        }
        if (lower[i].y < min_y) {
            min_y = lower[i].y;
        } if (lower[i].y > max_y) {
            max_y = lower[i].y;
        }
    }
    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    for (uint16_t i = 0; i < cols; i++) {
        pts[i].x_q8 = (upper[i].x - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[i].y_q8 = (upper[i].y - min_y) << GFX_MESH_FRAC_SHIFT;
    }
    for (uint16_t i = 0; i < cols; i++) {
        pts[cols + i].x_q8 = (lower[i].x - min_x) << GFX_MESH_FRAC_SHIFT;
        pts[cols + i].y_q8 = (lower[i].y - min_y) << GFX_MESH_FRAC_SHIFT;
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "bezier_fill align");
    return gfx_mesh_img_set_points_q8(obj, pts, (size_t)cols * 2U);
}
