Motion Scene (motion)
=====================

Types
-----

gfx_motion_player_action_end_cb_t
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Motion action finished callback.

.. code-block:: c

   typedef void (*gfx_motion_player_action_end_cb_t)(gfx_motion_player_t *player,
           uint16_t action_idx,
           void *user_data);

gfx_motion_segment_kind_t
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_MOTION_SEG_CAPSULE      = 0, /**< Thick capsule between joint_a → joint_b        */
       GFX_MOTION_SEG_RING         = 1, /**< Hollow ring centred at joint_a                  */
       GFX_MOTION_SEG_BEZIER_STRIP = 2, /**< Open thick Bézier curve  (e.g. brow)            */
       GFX_MOTION_SEG_BEZIER_LOOP  = 3, /**< Closed thick Bézier loop (e.g. mouth outline)   */
       GFX_MOTION_SEG_BEZIER_FILL  = 4, /**< Closed filled Bézier shape (e.g. eye sclera)    */
   } gfx_motion_segment_kind_t;

gfx_motion_interp_t
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_MOTION_INTERP_HOLD   = 0, /**< Snap immediately to target pose */
       GFX_MOTION_INTERP_DAMPED = 1, /**< Exponential ease (damping_div)  */
   } gfx_motion_interp_t;

gfx_motion_resource_t
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const gfx_image_dsc_t *image;  /**< Pointer to the image descriptor (ROM .inc array)         */
       uint16_t uv_x;                 /**< Source crop origin X  (0 = full image)                   */
       uint16_t uv_y;                 /**< Source crop origin Y  (0 = full image)                   */
       uint16_t uv_w;                 /**< Source crop width     (0 = image width from uv_x)         */
       uint16_t uv_h;                 /**< Source crop height    (0 = image height from uv_y)        */
   } gfx_motion_resource_t;

gfx_motion_segment_t
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       gfx_motion_segment_kind_t kind;
       uint16_t          joint_a;       /**< CAPSULE: start; RING: centre; BEZIER: first ctrl pt     */
       uint16_t          joint_b;       /**< CAPSULE: end  ; unused for RING/BEZIER                  */
       uint16_t          joint_count;   /**< BEZIER_*: number of consecutive control points (n=3k+1) */
       uint8_t           stroke_width;  /**< Design-space override; 0 = use layout->stroke_width     */
       uint8_t           layer_bit;     /**< Visibility layer mask bit (0 = always shown)            */
       int16_t           radius_hint;   /**< RING: design-space radius                               */
       /**
        * Texture / resource binding.
        * 0   = solid colour (driven by gfx_motion_player_set_color).
        * N>0 = use asset->resources[N-1] as the mesh_img image source.
        */
       uint8_t           resource_idx;
       /**
        * Palette colour index.
        * 0   = use runtime colour (gfx_motion_player_set_color), not affected by set_color.
        * N>0 = use asset->color_palette[N-1] (0xRRGGBB) as the fixed segment colour.
        *        set_color() skips palette-coloured segments.
        */
       uint8_t           color_idx;
       /**
        * Segment opacity 0-255.
        * 0 is treated as 255 (fully opaque) for zero-init compatibility.
        */
       uint8_t           opacity;
   } gfx_motion_segment_t;

gfx_motion_pose_t
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const int16_t *coords;
   } gfx_motion_pose_t;

gfx_motion_action_step_t
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint16_t         pose_index;  /**< Index into gfx_motion_asset_t.poses[]     */
       uint16_t         hold_ticks;  /**< Timer ticks to hold before advancing      */
       gfx_motion_interp_t  interp;  /**< Transition style into this step           */
       int8_t           facing;      /**< 1=right  -1=left (mirrors X)              */
       uint8_t          icon_enabled;   /**< 1 = render a step-local icon overlay       */
       uint16_t         icon_index;     /**< Index into gfx_motion_asset_t.icons[]       */
       int16_t          icon_x;         /**< Icon centre X in design-space coordinates   */
       int16_t          icon_y;         /**< Icon centre Y in design-space coordinates   */
       uint16_t         icon_scale_q8;  /**< Icon scale in Q8.8 (256 = 1.0x)             */
   } gfx_motion_action_step_t;

gfx_motion_action_t
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const gfx_motion_action_step_t *steps;
       uint8_t                  step_count;
       bool                     loop;
   } gfx_motion_action_t;

gfx_motion_icon_t
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const char *name;
       const gfx_motion_segment_t *segments;
       uint8_t segment_count;
       const int16_t *coords;   /**< Flat [x0,y0, x1,y1, ...] local control-point array */
       uint16_t joint_count;
   } gfx_motion_icon_t;

gfx_motion_meta_t
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t version;    /**< Must equal GFX_MOTION_SCENE_SCHEMA_VERSION */
       int32_t  viewbox_x;
       int32_t  viewbox_y;
       int32_t  viewbox_w;
       int32_t  viewbox_h;
   } gfx_motion_meta_t;

gfx_motion_layout_t
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       int16_t  stroke_width;    /**< Default capsule / Bézier stroke thickness (design units) */
       int16_t  mirror_x;        /**< X axis for facing=-1 horizontal mirroring               */
       int16_t  ground_y;        /**< Informational floor position                             */
       uint16_t timer_period_ms; /**< Action-advance timer period                              */
       int16_t  damping_div;     /**< Divisor for INTERP_DAMPED easing (1 = snap)             */
   } gfx_motion_layout_t;

gfx_motion_asset_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const gfx_motion_meta_t    *meta;
   
       /** Control point name table (joint_count entries; field name kept for ABI). */
       const char *const       *joint_names;
       uint16_t                 joint_count;
   
       /** Segment wiring (segment_count entries; 0 is valid). */
       const gfx_motion_segment_t *segments;
       uint8_t                  segment_count;
   
       /** Pose library. */
       const gfx_motion_pose_t    *poses;
       uint16_t                 pose_count;
   
       /** Action library. */
       const gfx_motion_action_t    *actions;
       uint16_t                 action_count;
   
       /** Default playback sequence (action indices). */
       const uint16_t          *sequence;
       uint16_t                 sequence_count;
   
       /** Rendering hints. */
       const gfx_motion_layout_t  *layout;
   
       /** Optional static icon overlays. */
       const gfx_motion_icon_t *icons;
       uint16_t icon_count;
   
       /**
        * Optional texture/image resource table.
        * Segments reference entries here via segment.resource_idx (1-based).
        * NULL and resource_count=0 are valid (all segments use solid colour).
        */
       const gfx_motion_resource_t *resources;
       uint8_t                   resource_count;
   
       /**
        * Optional per-segment colour palette.
        * Stored as 0xRRGGBB 24-bit values; converted to native pixel at runtime init.
        * Segments reference entries via segment.color_idx (1-based).
        * NULL and color_palette_count=0 are valid (all non-resource segments use
        * the runtime colour set by gfx_motion_player_set_color).
        */
       const uint32_t *color_palette;
       uint8_t         color_palette_count;
   } gfx_motion_asset_t;

Functions
---------

void()
~~~~~~

Motion action finished callback.

.. code-block:: c

   typedef void(*gfx_motion_player_action_end_cb_t)(gfx_motion_player_t *player,
        uint16_t action_idx,
        void *user_data);

**Parameters:**

* ``player`` - Motion player that finished an action.
* ``action_idx`` - Finished action index.
* ``user_data`` - User data passed to gfx_motion_player_set_action_end_cb().

gfx_motion_player_create()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a motion player for a display.

.. code-block:: c

   gfx_motion_player_t * gfx_motion_player_create(gfx_display_t *disp, const gfx_motion_asset_t *asset);

**Parameters:**

* ``disp`` - Display that owns the generated segment objects.
* ``asset`` - Motion scene asset descriptor.

**Returns:**

* Motion player handle on success, or NULL on failure.

gfx_motion_player_delete()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Delete a motion player.

.. code-block:: c

   void gfx_motion_player_delete(gfx_motion_player_t *player);

**Parameters:**

* ``player`` - Motion player handle returned from gfx_motion_player_create().

gfx_motion_player_set_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the runtime color used by solid-color segments.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_color(gfx_motion_player_t *player, gfx_color_t color);

**Parameters:**

* ``player`` - Motion player handle.
* ``color`` - Runtime segment color.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_set_canvas()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the canvas region used to scale and place the scene.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_canvas(gfx_motion_player_t *player,
                                       gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h);

**Parameters:**

* ``player`` - Motion player handle.
* ``x`` - Canvas origin X in screen coordinates.
* ``y`` - Canvas origin Y in screen coordinates.
* ``w`` - Canvas width in pixels; must be greater than 0.
* ``h`` - Canvas height in pixels; must be greater than 0.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_set_layer_mask()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the visible segment layer mask.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_layer_mask(gfx_motion_player_t *player, uint32_t layer_mask);

**Parameters:**

* ``player`` - Motion player handle.
* ``layer_mask`` - Visibility mask for segment layers.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_set_visible()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set whole motion player visibility.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_visible(gfx_motion_player_t *player, bool visible);

**Parameters:**

* ``player`` - Motion player handle.
* ``visible`` - true to show the player, false to hide it.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_sync()
~~~~~~~~~~~~~~~~~~~~~~~~

Apply the current player state immediately without advancing time.

.. code-block:: c

   gfx_err_t gfx_motion_player_sync(gfx_motion_player_t *player);

**Parameters:**

* ``player`` - Motion player handle.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_reset_timer()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reset the internal motion timer.

.. code-block:: c

   gfx_err_t gfx_motion_player_reset_timer(gfx_motion_player_t *player);

**Parameters:**

* ``player`` - Motion player handle.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_set_action()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Switch to an action by index.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_action(gfx_motion_player_t *player, uint16_t action_idx, bool snap);

**Parameters:**

* ``player`` - Motion player handle.
* ``action_idx`` - Action index in gfx_motion_asset_t.actions.
* ``snap`` - Whether to snap directly to the first target pose.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_set_action_end_cb()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the callback invoked when a non-looping action finishes.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_action_end_cb(gfx_motion_player_t *player,
        gfx_motion_player_action_end_cb_t cb,
        void *user_data);

**Parameters:**

* ``player`` - Motion player handle.
* ``cb`` - Callback to invoke from the motion timer context.
* ``user_data`` - User data passed to the callback.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_set_action_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Override the loop setting of the active action.

.. code-block:: c

   gfx_err_t gfx_motion_player_set_action_loop(gfx_motion_player_t *player, bool loop);

**Parameters:**

* ``player`` - Motion player handle.
* ``loop`` - true to force looping, false to force one-shot playback.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.

gfx_motion_player_clear_action_loop_override()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Clear the action loop override.

.. code-block:: c

   gfx_err_t gfx_motion_player_clear_action_loop_override(gfx_motion_player_t *player);

**Parameters:**

* ``player`` - Motion player handle.

**Returns:**

* GFX_OK on success, or an GFX_ERR_* code on failure.
