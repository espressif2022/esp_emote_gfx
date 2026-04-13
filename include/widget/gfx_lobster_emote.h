/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/gfx_obj.h"
#include "widget/gfx_mesh_img.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t x; /* Local translation X in export-space pixels. */
    int16_t y; /* Local translation Y in export-space pixels. */
    int16_t r; /* Local rotation in degrees. */
    int16_t s; /* Local scale in percent, where 100 means 1.0x. */
} gfx_lobster_transform_t;

/* Pupil shape selected per exported state. */
typedef enum {
    GFX_LOBSTER_PUPIL_SHAPE_AUTO = 0, /* Resolve shape from emotion weights. */
    GFX_LOBSTER_PUPIL_SHAPE_O,        /* Neutral round pupil. */
    GFX_LOBSTER_PUPIL_SHAPE_U,        /* Upturned smiling pupil. */
    GFX_LOBSTER_PUPIL_SHAPE_N,        /* Downturned sad pupil. */
    GFX_LOBSTER_PUPIL_SHAPE_LINE,     /* Narrow angry line pupil. */
} gfx_lobster_pupil_shape_t;

/* One exported expression state in the playback sequence. */
typedef struct {
    const char *name;     /* Stable state id used by runtime lookup. */
    const char *name_cn;  /* Human-readable Chinese label for tools/UI. */
    int16_t w_smile;      /* Smile axis weight. */
    int16_t w_happy;      /* Happy axis weight. */
    int16_t w_sad;        /* Sad axis weight. */
    int16_t w_surprise;   /* Surprise axis weight. */
    int16_t w_angry;      /* Angry axis weight. */
    int16_t w_look_x;     /* Manual look X override. */
    int16_t w_look_y;     /* Manual look Y override. */
    gfx_lobster_pupil_shape_t pupil_shape; /* Pupil shape for this state. */
    uint32_t hold_ticks;  /* Duration in animation timer ticks. */
} gfx_lobster_emote_state_t;

#define GFX_LOBSTER_EMOTE_EXPORT_VERSION 2

/* Describes the original design viewbox and exported canvas. */
typedef struct {
    uint32_t version;         /* Export schema version. */
    int32_t design_viewbox_x; /* Source design-space origin X. */
    int32_t design_viewbox_y; /* Source design-space origin Y. */
    int32_t design_viewbox_w; /* Source design-space width. */
    int32_t design_viewbox_h; /* Source design-space height. */
    int32_t export_width;     /* Intended exported canvas width. */
    int32_t export_height;    /* Intended exported canvas height. */
    float export_scale;       /* Design-to-export scale factor. */
    int32_t export_offset_x;  /* Canvas offset X after fitting. */
    int32_t export_offset_y;  /* Canvas offset Y after fitting. */
} gfx_lobster_emote_export_meta_t;

/* Contribution of one emotion axis to the derived pose. */
typedef struct {
    float eye_open;      /* Opens or closes the eye contour. */
    float eye_focus;     /* Adds sharper focused eye rotation. */
    float eye_soft;      /* Adds softer eye rotation. */
    float pupil_x;       /* Local pupil X shift. */
    float pupil_y;       /* Local pupil Y shift. */
    float pupil_scale;   /* Pupil size multiplier. */
    float droop;         /* Overall droop/downcast amount. */
    float alert;         /* Alert/uplift amount. */
    float antenna_lift;  /* Antenna lift amount. */
    float antenna_open;  /* Antenna spread amount. */
    float antenna_curl;  /* Antenna curl amount. */
    float look_bias_x;   /* Semantic look bias on X. */
    float look_bias_y;   /* Semantic look bias on Y. */
} gfx_lobster_emote_axis_t;

/*
 * Runtime semantic contract exported with the asset pack.
 * These coefficients control how emotion weights map to eye, pupil,
 * mouth and antenna motion, plus timing and mesh sampling defaults.
 */
typedef struct {
    float look_scale_x;             /* Scales semantic look bias on X. */
    float look_scale_y;             /* Scales semantic look bias on Y. */
    float eye_x_from_look;          /* Eye X translation derived from look. */
    float eye_y_from_alert;         /* Eye Y translation derived from alert. */
    float eye_y_from_droop;         /* Eye Y translation derived from droop. */
    float eye_scale_base;           /* Base eye scale before axis offsets. */
    float eye_scale_from_eye_open;  /* Eye scale contribution from eye_open. */
    float eye_scale_from_droop;     /* Eye scale contribution from droop. */
    float eye_rot_from_focus;       /* Eye rotation contribution from focus. */
    float eye_rot_from_soft;        /* Eye rotation contribution from soft. */
    float pupil_x_from_look;        /* Pupil X offset derived from look. */
    float pupil_y_from_look;        /* Pupil Y offset derived from look. */
    float mouth_x_from_look;        /* Mouth X offset derived from look. */
    float mouth_y_from_look;        /* Mouth Y offset derived from look. */
    float antenna_x_from_look;      /* Antenna X offset derived from look. */
    float antenna_y_from_lift;      /* Antenna Y offset derived from lift. */
    float antenna_y_from_droop;     /* Antenna Y offset derived from droop. */
    float antenna_rot_from_open;    /* Antenna rotation from spread. */
    float antenna_rot_from_curl;    /* Antenna rotation from curl. */
    float antenna_rot_from_droop;   /* Antenna rotation from droop. */
    float antenna_scale_base;       /* Base antenna scale. */
    float antenna_scale_from_alert; /* Antenna scale contribution from alert. */
    float antenna_scale_from_lift;  /* Antenna scale contribution from lift. */
    float look_x_min;               /* Lower clamp for final look X. */
    float look_x_max;               /* Upper clamp for final look X. */
    float look_y_min;               /* Lower clamp for final look Y. */
    float look_y_max;               /* Upper clamp for final look Y. */
    float pupil_x_min;              /* Lower clamp for final pupil X. */
    float pupil_x_max;              /* Upper clamp for final pupil X. */
    float pupil_y_min;              /* Lower clamp for final pupil Y. */
    float pupil_y_max;              /* Upper clamp for final pupil Y. */
    float eye_scale_multiplier;     /* Final eye render scale multiplier. */
    float antenna_thickness_base;   /* Base stroke thickness for antenna. */
    uint16_t timer_period_ms;       /* Default animation timer period. */
    int16_t damping_div;            /* Easing divisor used by runtime. */
    uint8_t eye_segs;               /* Preferred eye bezier sampling segments. */
    uint8_t pupil_segs;             /* Preferred pupil bezier sampling segments. */
    uint8_t antenna_segs;           /* Preferred antenna bezier sampling segments. */
    uint8_t reserved;               /* Reserved for future export fields. */
    gfx_lobster_emote_axis_t smile;     /* Smile axis coefficients. */
    gfx_lobster_emote_axis_t happy;     /* Happy axis coefficients. */
    gfx_lobster_emote_axis_t sad;       /* Sad axis coefficients. */
    gfx_lobster_emote_axis_t surprise;  /* Surprise axis coefficients. */
    gfx_lobster_emote_axis_t angry;     /* Angry axis coefficients. */
} gfx_lobster_emote_semantics_t;

/* Part anchors in exported pixel space. */
typedef struct {
    int32_t eye_left_cx;       /* Left eye anchor center X. */
    int32_t eye_left_cy;       /* Left eye anchor center Y. */
    int32_t eye_right_cx;      /* Right eye anchor center X. */
    int32_t eye_right_cy;      /* Right eye anchor center Y. */
    int32_t pupil_left_cx;     /* Left pupil anchor center X. */
    int32_t pupil_left_cy;     /* Left pupil anchor center Y. */
    int32_t pupil_right_cx;    /* Right pupil anchor center X. */
    int32_t pupil_right_cy;    /* Right pupil anchor center Y. */
    int32_t mouth_cx;          /* Mouth anchor center X. */
    int32_t mouth_cy;          /* Mouth anchor center Y. */
    int32_t antenna_left_cx;   /* Left antenna anchor center X. */
    int32_t antenna_left_cy;   /* Left antenna anchor center Y. */
    int32_t antenna_right_cx;  /* Right antenna anchor center X. */
    int32_t antenna_right_cy;  /* Right antenna anchor center Y. */
} gfx_lobster_emote_layout_t;

/*
 * Self-contained lobster asset package.
 * Geometry, curve bases, semantic mapping and playback sequence are all
 * grouped here so the runtime does not need hardcoded fallback data.
 */
typedef struct {
    const gfx_mesh_img_point_t *pts_head;     /* Head mesh outline points. */
    const gfx_mesh_img_point_t *pts_body;     /* Body mesh outline points. */
    const gfx_mesh_img_point_t *pts_tail;     /* Tail mesh outline points. */
    const gfx_mesh_img_point_t *pts_claw_l;   /* Left claw mesh outline points. */
    const gfx_mesh_img_point_t *pts_claw_r;   /* Right claw mesh outline points. */
    const gfx_mesh_img_point_t *pts_eye_l;    /* Left eye mesh outline points. */
    const gfx_mesh_img_point_t *pts_eye_r;    /* Right eye mesh outline points. */
    const gfx_mesh_img_point_t *pts_dots;     /* Optional decorative dots points. */
    size_t count_head;        /* Number of points in pts_head. */
    size_t count_body;        /* Number of points in pts_body. */
    size_t count_tail;        /* Number of points in pts_tail. */
    size_t count_claw_l;      /* Number of points in pts_claw_l. */
    size_t count_claw_r;      /* Number of points in pts_claw_r. */
    size_t count_eye_l;       /* Number of points in pts_eye_l. */
    size_t count_eye_r;       /* Number of points in pts_eye_r. */
    size_t count_dots;        /* Number of points in pts_dots. */
    const int16_t (*eye_white_base)[14];      /* Eye white bezier bases per emotion. */
    const int16_t (*pupil_base)[14];          /* Optional pupil bezier bases per emotion. */
    const int16_t (*mouth_base)[14];          /* Mouth bezier bases per emotion. */
    const int16_t (*antenna_left_base)[8];    /* Left antenna bezier bases per emotion. */
    const int16_t (*antenna_right_base)[8];   /* Right antenna bezier bases per emotion. */
    const gfx_lobster_emote_export_meta_t *export_meta; /* Export metadata block. */
    const gfx_lobster_emote_layout_t *layout;           /* Anchor layout block. */
    const gfx_lobster_emote_semantics_t *semantics;     /* Semantic coefficients block. */
    const gfx_lobster_emote_state_t *sequence;          /* Playback sequence array. */
    size_t sequence_count;       /* Number of entries in sequence. */
} gfx_lobster_emote_assets_t;

/* Runtime widget configuration derived from display size and semantics. */
typedef struct {
    uint16_t display_w;       /* Target widget width in pixels. */
    uint16_t display_h;       /* Target widget height in pixels. */
    uint16_t timer_period_ms; /* Active animation timer period. */
    int16_t damping_div;      /* Active easing divisor. */
} gfx_lobster_emote_cfg_t;

#define GFX_LOBSTER_EMOTE_LAYER_ANTENNA (1U << 0)
#define GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE (1U << 1)
#define GFX_LOBSTER_EMOTE_LAYER_PUPIL (1U << 2)
#define GFX_LOBSTER_EMOTE_LAYER_MOUTH (1U << 3)
#define GFX_LOBSTER_EMOTE_LAYER_ALL (GFX_LOBSTER_EMOTE_LAYER_ANTENNA | GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE | GFX_LOBSTER_EMOTE_LAYER_PUPIL | GFX_LOBSTER_EMOTE_LAYER_MOUTH)

/* Creates a lobster_emote widget sized for the target display region. */
gfx_obj_t *gfx_lobster_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h);
/* Binds a validated exported asset package to the widget. */
esp_err_t gfx_lobster_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_emote_assets_t *assets);
/* Changes the accent color used by the lobster body and antenna. */
esp_err_t gfx_lobster_emote_set_color(gfx_obj_t *obj, gfx_color_t color);
/* Switches to a named exported state, optionally snapping instead of easing. */
esp_err_t gfx_lobster_emote_set_state_name(gfx_obj_t *obj, const char *name, bool snap_now);
/* Overrides look direction without changing the active emotion state. */
esp_err_t gfx_lobster_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y);
/* Shows or hides dynamic layers such as antenna, eye white, pupil and mouth. */
esp_err_t gfx_lobster_emote_set_layer_mask(gfx_obj_t *obj, uint32_t mask);

#ifdef __cplusplus
}
#endif
