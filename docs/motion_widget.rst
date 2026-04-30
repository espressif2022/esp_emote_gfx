Motion Scene Widget
===================

The motion scene widget is the path-driven character and emote runtime in ESP Emote GFX. It is designed for assets exported as a ``gfx_motion_asset_t`` bundle and rendered through ``gfx_motion_player_t``.

When to Use It
--------------

Use the motion scene path when you need:

* Character or emote playback built from vector-like paths instead of bitmap frames
* Per-part styling with solid colors, palette colors, opacity, or texture binding
* Small action sets such as idle, move, happy, thinking, or touch-reactive actions
* Canvas-level movement where the whole character can swim, drift, or follow touch input

Core Model
----------

The motion scene asset has four main layers:

* ``joint_names`` and joint coordinates: named control points in design space
* ``segments``: visual primitives built from joints
* ``poses``: complete joint-coordinate snapshots
* ``actions``: sequences of pose steps with hold time, interpolation, and facing

The runtime owns a parser plus renderer:

* ``gfx_motion_scene_t`` manages pose interpolation and action state
* ``gfx_motion_player_t`` creates one ``gfx_mesh_img`` object per segment and applies the current pose to screen space

Segment Types
-------------

The current scene format supports these segment kinds:

* ``GFX_MOTION_SEG_CAPSULE``: thick limb/body stroke between two joints
* ``GFX_MOTION_SEG_RING``: circular outline around a center joint
* ``GFX_MOTION_SEG_BEZIER_STRIP``: open Bezier stroke
* ``GFX_MOTION_SEG_BEZIER_LOOP``: closed Bezier stroke
* ``GFX_MOTION_SEG_BEZIER_FILL``: closed filled Bezier region

Each segment can also carry:

* ``stroke_width`` override
* ``resource_idx`` for texture/image binding
* ``color_idx`` for palette-bound solid color
* ``opacity`` for per-part alpha

Playback Flow
-------------

Typical runtime usage:

1. Create or include a generated scene asset (for example from a designer/export pipeline).
2. Call ``gfx_motion_player_init()`` with a display and the asset.
3. Set the target canvas using ``gfx_motion_player_set_canvas()``.
4. Optionally set the runtime color with ``gfx_motion_player_set_color()``.
5. Select an initial action using ``gfx_motion_player_set_action()``.
6. When finished, call ``gfx_motion_player_deinit()``.

Example
-------

.. code-block:: c

   #include "gfx.h"
   #include "rig_active.inc"

   static gfx_motion_player_t motion_player;

   void motion_scene_start(gfx_disp_t *disp)
   {
       gfx_motion_player_init(&motion_player, disp, &s_motion_scene_asset);
       gfx_motion_player_set_canvas(&motion_player, 0, 0, 360, 360);
       gfx_motion_player_set_color(&motion_player, GFX_COLOR_HEX(0xFF7A00));
       gfx_motion_player_set_action(&motion_player, 0, true);
   }

   void motion_scene_stop(void)
   {
       gfx_motion_player_deinit(&motion_player);
   }

Interactive Example
-------------------

An end-to-end example is available in ``test_apps/main/test_motion.c``. It demonstrates:

* full-screen motion scene preview
* tap-to-switch action
* touch-guided movement by changing the runtime canvas
* timer-driven autonomous movement between touch interactions

Current Notes
-------------

The current implementation intentionally keeps the dependency chain small:

* no NanoVG dependency
* no libtess2 dependency
* filled polygon rendering uses the internal software path

This makes the widget easier to release and integrate into ESP-IDF projects, while keeping the scene model stable for designer/export tooling.
