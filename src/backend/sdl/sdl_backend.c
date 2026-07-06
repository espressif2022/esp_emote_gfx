/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"
#include "gfx/backends/sdl.h"
#include "gfx/input.h"
#include "common/gfx_types_priv.h"
#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/runtime/gfx_core_priv.h"

#if defined(GFX_SDL_USE_SDL3)
#include <SDL3/SDL.h>
#elif defined(GFX_SDL_USE_SDL2)
#include <SDL2/SDL.h>
#else
#error "Define GFX_SDL_USE_SDL3 or GFX_SDL_USE_SDL2 when building the SDL backend"
#endif

typedef struct {
    gfx_backend_t base;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t *pixels;
    uint32_t h_res;
    uint32_t v_res;
    uint32_t scale;
    bool mouse_pressed;
    uint16_t last_x;
    uint16_t last_y;
} gfx_backend_sdl_t;

static const char *const TAG = "sdl_backend";

static uint32_t s_rgb565_to_xrgb8888(uint16_t rgb565)
{
    uint32_t r = (rgb565 >> 11) & 0x1f;
    uint32_t g = (rgb565 >> 5) & 0x3f;
    uint32_t b = rgb565 & 0x1f;

    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    return 0xff000000U | (r << 16) | (g << 8) | b;
}

static uint32_t s_rgb888_to_xrgb8888(const uint8_t *rgb888)
{
    return 0xff000000U | ((uint32_t)rgb888[0] << 16) | ((uint32_t)rgb888[1] << 8) | rgb888[2];
}

static uint32_t s_bgr888_to_xrgb8888(const uint8_t *bgr888)
{
    return 0xff000000U | ((uint32_t)bgr888[2] << 16) | ((uint32_t)bgr888[1] << 8) | bgr888[0];
}

static uint32_t s_xrgb8888_to_xrgb8888(const uint8_t *xrgb8888)
{
    uint32_t px;
    memcpy(&px, xrgb8888, sizeof(px));
    return 0xff000000U | (px & 0x00ffffffU);
}

static gfx_backend_sdl_t *s_backend_from_display(gfx_display_t *disp)
{
    if (disp == NULL || disp->backend == NULL) {
        return NULL;
    }

    return (gfx_backend_sdl_t *)disp->backend;
}

static uint16_t s_clamp_coord(int32_t value, uint32_t limit)
{
    if (value < 0) {
        return 0;
    }
    if (limit == 0) {
        return 0;
    }
    if ((uint32_t)value >= limit) {
        return (uint16_t)(limit - 1U);
    }

    return (uint16_t)value;
}

static uint16_t s_window_to_screen_coord(int32_t value, uint32_t scale, uint32_t limit)
{
    uint32_t safe_scale = scale > 0 ? scale : 1;
    return s_clamp_coord(value / (int32_t)safe_scale, limit);
}

static void s_set_texture_nearest(SDL_Texture *texture)
{
    if (texture == NULL) {
        return;
    }

#if defined(GFX_SDL_USE_SDL3)
    if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
        GFX_LOGW(TAG, "create: set nearest texture scale failed: %s", SDL_GetError());
    }
#else
    if (SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) != 0) {
        GFX_LOGW(TAG, "create: set nearest texture scale failed: %s", SDL_GetError());
    }
#endif
}

static void s_dispatch_touch(gfx_display_t *disp, gfx_touch_event_type_t type, uint16_t x, uint16_t y)
{
    gfx_backend_sdl_t *sdl = s_backend_from_display(disp);

    if (sdl == NULL || disp == NULL) {
        return;
    }

    gfx_touch_event_t event = {
        .type = type,
        .x = x,
        .y = y,
        .strength = type == GFX_TOUCH_EVENT_RELEASE ? 0 : 1,
        .track_id = 0,
        .timestamp_ms = (uint32_t)(SDL_GetTicks()),
    };

    if (type == GFX_TOUCH_EVENT_PRESS) {
        sdl->mouse_pressed = true;
    } else if (type == GFX_TOUCH_EVENT_RELEASE) {
        sdl->mouse_pressed = false;
    }

    sdl->last_x = x;
    sdl->last_y = y;

    (void)gfx_touch_inject(disp, &event);
}

static void s_handle_mouse_button(gfx_display_t *disp, int32_t x, int32_t y, bool pressed)
{
    gfx_backend_sdl_t *sdl = s_backend_from_display(disp);
    if (sdl == NULL) {
        return;
    }

#if defined(GFX_SDL_USE_SDL3)
    SDL_CaptureMouse(pressed);
#else
    SDL_CaptureMouse(pressed ? SDL_TRUE : SDL_FALSE);
#endif

    uint16_t sx = s_window_to_screen_coord(x, sdl->scale, sdl->h_res);
    uint16_t sy = s_window_to_screen_coord(y, sdl->scale, sdl->v_res);
    s_dispatch_touch(disp, pressed ? GFX_TOUCH_EVENT_PRESS : GFX_TOUCH_EVENT_RELEASE, sx, sy);
}

static void s_handle_mouse_motion(gfx_display_t *disp, int32_t x, int32_t y)
{
    gfx_backend_sdl_t *sdl = s_backend_from_display(disp);
    if (sdl == NULL || !sdl->mouse_pressed) {
        return;
    }

    uint16_t sx = s_window_to_screen_coord(x, sdl->scale, sdl->h_res);
    uint16_t sy = s_window_to_screen_coord(y, sdl->scale, sdl->v_res);
    if (sx == sdl->last_x && sy == sdl->last_y) {
        return;
    }
    s_dispatch_touch(disp, GFX_TOUCH_EVENT_MOVE, sx, sy);
}

static gfx_err_t s_flush(gfx_backend_t *backend, gfx_display_t *disp,
                         gfx_coord_t x1, gfx_coord_t y1,
                         gfx_coord_t x2, gfx_coord_t y2,
                         const void *pixels, gfx_coord_t stride)
{
    gfx_backend_sdl_t *sdl = (gfx_backend_sdl_t *)backend;
    const uint8_t *src = (const uint8_t *)pixels;
    gfx_coord_t clip_x1;
    gfx_coord_t clip_y1;
    gfx_coord_t clip_x2;
    gfx_coord_t clip_y2;
    uint32_t original_w;
    uint32_t w;
    uint32_t h;
    uint32_t src_stride;
    size_t src_offset;

    GFX_RETURN_ON_FALSE(sdl != NULL && src != NULL, GFX_ERR_INVALID_ARG, TAG, "flush: invalid args");
    GFX_RETURN_ON_FALSE(x2 >= x1 && y2 >= y1, GFX_ERR_INVALID_ARG, TAG, "flush: invalid area");

    gfx_color_format_t src_format = disp != NULL ? disp->format.output_format : GFX_COLOR_FORMAT_RGB565;
    uint8_t src_pixel_size = gfx_color_format_get_size(src_format);
    GFX_RETURN_ON_FALSE(src_format == GFX_COLOR_FORMAT_RGB565 ||
                        src_format == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
                        src_format == GFX_COLOR_FORMAT_RGB888 ||
                        src_format == GFX_COLOR_FORMAT_BGR888 ||
                        src_format == GFX_COLOR_FORMAT_XRGB8888 ||
                        src_format == GFX_COLOR_FORMAT_ARGB8888,
                        GFX_ERR_NOT_SUPPORTED, TAG, "flush: unsupported source format %u", (unsigned)src_format);
    GFX_RETURN_ON_FALSE(src_pixel_size > 0U, GFX_ERR_NOT_SUPPORTED, TAG, "flush: invalid source pixel size");
    original_w = (uint32_t)(x2 - x1);

    clip_x1 = x1 < 0 ? 0 : x1;
    clip_y1 = y1 < 0 ? 0 : y1;
    clip_x2 = ((uint32_t)x2 > sdl->h_res) ? (gfx_coord_t)sdl->h_res : x2;
    clip_y2 = ((uint32_t)y2 > sdl->v_res) ? (gfx_coord_t)sdl->v_res : y2;

    if (clip_x1 >= clip_x2 || clip_y1 >= clip_y2) {
        return GFX_OK;
    }

    w = (uint32_t)(clip_x2 - clip_x1);
    h = (uint32_t)(clip_y2 - clip_y1);
    src_stride = stride > 0 ? (uint32_t)stride : original_w;
    src_offset = (size_t)(clip_y1 - y1) * src_stride + (size_t)(clip_x1 - x1);

    if (disp != NULL && gfx_display_has_full_frame_buf(disp)) {
        src_stride = disp->res.h_res;
        src_offset = (size_t)clip_y1 * src_stride + (size_t)clip_x1;
    }

    for (uint32_t y = 0; y < h; y++) {
        uint32_t *dst_row = sdl->pixels + ((uint32_t)clip_y1 + y) * sdl->h_res + (uint32_t)clip_x1;
        const uint8_t *src_row = src + (src_offset + (size_t)y * src_stride) * src_pixel_size;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *src_px = src_row + (size_t)x * src_pixel_size;
            if (src_format == GFX_COLOR_FORMAT_RGB888) {
                dst_row[x] = s_rgb888_to_xrgb8888(src_px);
            } else if (src_format == GFX_COLOR_FORMAT_BGR888) {
                dst_row[x] = s_bgr888_to_xrgb8888(src_px);
            } else if (src_format == GFX_COLOR_FORMAT_XRGB8888 || src_format == GFX_COLOR_FORMAT_ARGB8888) {
                dst_row[x] = s_xrgb8888_to_xrgb8888(src_px);
            } else {
                uint16_t rgb565 = gfx_color_read_rgb565_bytes(src_px, src_format);
                dst_row[x] = s_rgb565_to_xrgb8888(rgb565);
            }
        }
    }

    return GFX_OK;
}

static gfx_err_t s_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    gfx_backend_sdl_t *sdl = (gfx_backend_sdl_t *)backend;
    (void)disp;

    GFX_RETURN_ON_FALSE(sdl != NULL, GFX_ERR_INVALID_ARG, TAG, "wait: invalid backend");

#if defined(GFX_SDL_USE_SDL3)
    SDL_UpdateTexture(sdl->texture, NULL, sdl->pixels, (int)(sdl->h_res * sizeof(uint32_t)));
    if (disp != NULL && !disp->render.flushing_last) {
        return GFX_OK;
    }
    SDL_RenderClear(sdl->renderer);
    SDL_RenderTexture(sdl->renderer, sdl->texture, NULL, NULL);
    SDL_RenderPresent(sdl->renderer);
#else
    SDL_UpdateTexture(sdl->texture, NULL, sdl->pixels, (int)(sdl->h_res * sizeof(uint32_t)));
    if (disp != NULL && !disp->render.flushing_last) {
        return GFX_OK;
    }
    SDL_RenderClear(sdl->renderer);
    SDL_RenderCopy(sdl->renderer, sdl->texture, NULL, NULL);
    SDL_RenderPresent(sdl->renderer);
#endif

    return GFX_OK;
}

static void s_destroy(gfx_backend_t *backend)
{
    gfx_backend_sdl_t *sdl = (gfx_backend_sdl_t *)backend;
    if (sdl == NULL) {
        return;
    }

    free(sdl->pixels);
    if (sdl->texture != NULL) {
        SDL_DestroyTexture(sdl->texture);
    }
    if (sdl->renderer != NULL) {
        SDL_DestroyRenderer(sdl->renderer);
    }
    if (sdl->window != NULL) {
        SDL_DestroyWindow(sdl->window);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(sdl);
}

static const gfx_backend_ops_t s_sdl_backend_ops = {
    .flush = s_flush,
    .wait_flush = s_wait_flush,
    .destroy = s_destroy,
};

gfx_backend_t *gfx_backend_sdl_create(const gfx_backend_sdl_config_t *cfg)
{
    GFX_RETURN_ON_FALSE(cfg != NULL && cfg->h_res > 0 && cfg->v_res > 0,
                        NULL, TAG, "create: invalid config");

    gfx_backend_sdl_t *sdl = calloc(1, sizeof(*sdl));
    GFX_RETURN_ON_FALSE(sdl != NULL, NULL, TAG, "create: no mem for backend");

    sdl->h_res = cfg->h_res;
    sdl->v_res = cfg->v_res;
    sdl->scale = cfg->scale > 0 ? cfg->scale : 1;
    sdl->pixels = calloc((size_t)sdl->h_res * sdl->v_res, sizeof(uint32_t));
    if (sdl->pixels == NULL) {
        free(sdl);
        return NULL;
    }

#if defined(GFX_SDL_USE_SDL3)
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        GFX_LOGE(TAG, "create: SDL_InitSubSystem video failed: %s", SDL_GetError());
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        GFX_LOGE(TAG, "create: SDL_InitSubSystem video failed: %s", SDL_GetError());
#endif
        free(sdl->pixels);
        free(sdl);
        return NULL;
    }

#if !defined(GFX_SDL_USE_SDL3)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#endif

#if defined(GFX_SDL_USE_SDL3)
    sdl->window = SDL_CreateWindow(cfg->title ? cfg->title : "ESP Emote GFX",
                                   (int)(sdl->h_res * sdl->scale),
                                   (int)(sdl->v_res * sdl->scale),
                                   0);
    if (sdl->window != NULL) {
        sdl->renderer = SDL_CreateRenderer(sdl->window, NULL);
        if (sdl->renderer == NULL) {
            GFX_LOGE(TAG, "create: SDL_CreateRenderer failed: %s", SDL_GetError());
        }
    } else {
        GFX_LOGE(TAG, "create: SDL_CreateWindow failed: %s", SDL_GetError());
    }
#else
    sdl->window = SDL_CreateWindow(cfg->title ? cfg->title : "ESP Emote GFX",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   (int)(sdl->h_res * sdl->scale),
                                   (int)(sdl->v_res * sdl->scale),
                                   0);
    if (sdl->window != NULL) {
        sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
        if (sdl->renderer == NULL) {
            GFX_LOGW(TAG, "create: accelerated renderer failed: %s", SDL_GetError());
            sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (sdl->renderer == NULL) {
            GFX_LOGE(TAG, "create: software renderer failed: %s", SDL_GetError());
        }
    } else {
        GFX_LOGE(TAG, "create: SDL_CreateWindow failed: %s", SDL_GetError());
    }
#endif

    if (sdl->renderer != NULL) {
        sdl->texture = SDL_CreateTexture(sdl->renderer,
                                         SDL_PIXELFORMAT_XRGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         (int)sdl->h_res,
                                         (int)sdl->v_res);
        if (sdl->texture == NULL) {
            GFX_LOGE(TAG, "create: SDL_CreateTexture failed: %s", SDL_GetError());
        } else {
            s_set_texture_nearest(sdl->texture);
        }
    }

    if (sdl->window == NULL || sdl->renderer == NULL || sdl->texture == NULL) {
        s_destroy(&sdl->base);
        return NULL;
    }

    sdl->base.ops = &s_sdl_backend_ops;
    sdl->base.caps = GFX_BACKEND_CAP_FLUSH | GFX_BACKEND_CAP_PRESENT;
    sdl->base.alignment = gfx_backend_get_alignment(NULL);
    return &sdl->base;
}

void gfx_backend_sdl_delete(gfx_backend_t *backend)
{
    gfx_backend_destroy(backend);
}

bool gfx_backend_sdl_poll_events(gfx_display_t *disp)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
#if defined(GFX_SDL_USE_SDL3)
        if (event.type == SDL_EVENT_QUIT) {
            return true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            s_handle_mouse_button(disp, (int32_t)event.button.x, (int32_t)event.button.y, true);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            s_handle_mouse_button(disp, (int32_t)event.button.x, (int32_t)event.button.y, false);
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            s_handle_mouse_motion(disp, (int32_t)event.motion.x, (int32_t)event.motion.y);
        }
#else
        if (event.type == SDL_QUIT) {
            return true;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            return true;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            s_handle_mouse_button(disp, event.button.x, event.button.y, true);
        }
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            s_handle_mouse_button(disp, event.button.x, event.button.y, false);
        }
        if (event.type == SDL_MOUSEMOTION) {
            s_handle_mouse_motion(disp, event.motion.x, event.motion.y);
        }
#endif
    }

    return false;
}

bool gfx_backend_sdl_poll(gfx_display_t *disp)
{
    bool quit = gfx_backend_sdl_poll_events(disp);
    if (quit) {
        return true;
    }

    if (disp != NULL && disp->ctx != NULL && ((gfx_core_context_t *)disp->ctx)->manual_tick) {
        (void)gfx_core_tick((gfx_handle_t)disp->ctx);
    }

    return false;
}

bool gfx_backend_sdl_poll_quit(void)
{
    return gfx_backend_sdl_poll(NULL);
}
