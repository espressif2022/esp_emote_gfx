Scene Asset and Runtime (gfx_motion_scene)
======================================

Overview
--------

``gfx_motion_scene.h`` defines both the ROM-side scene asset format and the runtime used to play it back on a display.

The asset model is built around:

* joints
* segments
* poses
* actions
* layout metadata

Important Types
---------------

gfx_motion_segment_kind_t
~~~~~~~~~~~~~~~~~

Segment primitive kind.

.. code-block:: c

   typedef enum {
       GFX_MOTION_SEG_CAPSULE      = 0,
       GFX_MOTION_SEG_RING         = 1,
       GFX_MOTION_SEG_BEZIER_STRIP = 2,
       GFX_MOTION_SEG_BEZIER_LOOP  = 3,
       GFX_MOTION_SEG_BEZIER_FILL  = 4,
   } gfx_motion_segment_kind_t;

gfx_motion_segment_t
~~~~~~~~~~~~~~~~

One visual primitive wired to joints and optional style/resource metadata.

gfx_motion_pose_t
~~~~~~~~~~~~~

One pose containing a flat coordinate array for all joints.

gfx_motion_action_step_t and gfx_motion_action_t
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These describe action playback: pose target, hold time, interpolation mode, facing, and loop behavior.

gfx_motion_asset_t
~~~~~~~~~~~~~~

Top-level scene asset bundle exported into ROM and consumed by the runtime.

gfx_motion_player_t
~~~~~~~~~~~~~~~~

Unified display runtime that owns:

* one ``gfx_motion_scene_t`` parser state
* one ``gfx_motion_t`` timer/runtime driver
* one ``gfx_mesh_img`` object per segment

Scene Functions
---------------

gfx_motion_scene_init()
~~~~~~~~~~~~~~~~~~~

Validate and initialize a parser scene state.

.. code-block:: c

   esp_err_t gfx_motion_scene_init(gfx_motion_scene_t *scene, const gfx_motion_asset_t *asset);

gfx_motion_scene_set_action()
~~~~~~~~~~~~~~~~~~~~~~~

Switch the active action by index.

.. code-block:: c

   esp_err_t gfx_motion_scene_set_action(gfx_motion_scene_t *scene, uint16_t action_index, bool snap_now);

gfx_motion_scene_set_action_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Override the current action loop behavior.

.. code-block:: c

   esp_err_t gfx_motion_scene_set_action_loop(gfx_motion_scene_t *scene, bool loop);

gfx_motion_scene_clear_action_loop_override()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Clear the loop override and restore the asset-defined loop flag.

.. code-block:: c

   esp_err_t gfx_motion_scene_clear_action_loop_override(gfx_motion_scene_t *scene);

gfx_motion_scene_tick()
~~~~~~~~~~~~~~~~~~~

Advance the current pose toward its target pose.

.. code-block:: c

   bool gfx_motion_scene_tick(gfx_motion_scene_t *scene);

gfx_motion_scene_advance()
~~~~~~~~~~~~~~~~~~~~~~

Advance the action timeline.

.. code-block:: c

   void gfx_motion_scene_advance(gfx_motion_scene_t *scene);

Runtime Functions
-----------------

gfx_motion_player_init()
~~~~~~~~~~~~~~~~~~~~~

Create display objects for all scene segments and start the motion timer.

.. code-block:: c

   esp_err_t gfx_motion_player_init(gfx_motion_player_t *player,
                                 gfx_disp_t *disp,
                                 const gfx_motion_asset_t *asset);

gfx_motion_player_deinit()
~~~~~~~~~~~~~~~~~~~~~~~

Destroy all segment objects and stop the runtime.

.. code-block:: c

   void gfx_motion_player_deinit(gfx_motion_player_t *player);

gfx_motion_player_set_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the default runtime color used by non-palette, non-textured segments.

.. code-block:: c

   esp_err_t gfx_motion_player_set_color(gfx_motion_player_t *player, gfx_color_t color);

gfx_motion_player_set_canvas()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the destination canvas rectangle the scene is scaled into.

.. code-block:: c

   esp_err_t gfx_motion_player_set_canvas(gfx_motion_player_t *player,
                                       gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h);

gfx_motion_player_sync()
~~~~~~~~~~~~~~~~~~~~~~~~

Force the current player state to be pushed to display objects immediately without advancing the action timeline.

.. code-block:: c

   esp_err_t gfx_motion_player_sync(gfx_motion_player_t *player);

gfx_motion_player_set_action()
~~~~~~~~~~~~~~~~~~~~~~~~~

Switch the active runtime action.

.. code-block:: c

   esp_err_t gfx_motion_player_set_action(gfx_motion_player_t *player, uint16_t action_idx, bool snap);

gfx_motion_player_set_action_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Override action loop behavior at runtime.

.. code-block:: c

   esp_err_t gfx_motion_player_set_action_loop(gfx_motion_player_t *player, bool loop);

gfx_motion_player_clear_action_loop_override()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Clear the runtime loop override.

.. code-block:: c

   esp_err_t gfx_motion_player_clear_action_loop_override(gfx_motion_player_t *player);
