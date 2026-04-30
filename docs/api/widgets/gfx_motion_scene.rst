Motion Scene (gfx_motion_scene)
===============================

Types
-----

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
   } gfx_motion_action_step_t;

gfx_motion_action_t
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const gfx_motion_action_step_t *steps;
       uint8_t                  step_count;
       bool                     loop;
   } gfx_motion_action_t;

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

gfx_motion_point_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       int16_t x;
       int16_t y;
   } gfx_motion_point_t;

gfx_motion_scene_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const gfx_motion_asset_t *asset;
   
       gfx_motion_point_t  pose_cur[GFX_MOTION_SCENE_MAX_POINTS]; /**< Current (animated) positions */
       gfx_motion_point_t  pose_tgt[GFX_MOTION_SCENE_MAX_POINTS]; /**< Target positions              */
   
       uint16_t     active_action;
       uint8_t      active_step;
       uint16_t     step_ticks;
       bool         action_loop_override_en;
       bool         action_loop_override;
       bool         dirty;
   } gfx_motion_scene_t;

gfx_motion_player_t
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       gfx_motion_scene_t  scene;
       gfx_motion_t        motion;
       /* ── private ── */
       gfx_obj_t      *seg_objs[GFX_MOTION_PLAYER_MAX_SEGMENTS]; /**< One mesh_img per segment */
       uint8_t         seg_grid_cols[GFX_MOTION_PLAYER_MAX_SEGMENTS];
       uint8_t         seg_grid_rows[GFX_MOTION_PLAYER_MAX_SEGMENTS];
       uint8_t         seg_obj_count;
       gfx_color_t     stroke_color;
       uint32_t        layer_mask;
       uint16_t        solid_pixel;
       gfx_image_dsc_t solid_img;
       /** Per-palette-entry native pixels and their 1×1 image descriptors. */
       uint16_t        palette_pixels[GFX_MOTION_PALETTE_MAX];
       gfx_image_dsc_t palette_imgs[GFX_MOTION_PALETTE_MAX];
       gfx_coord_t     canvas_x;
       gfx_coord_t     canvas_y;
       uint16_t        canvas_w;
       uint16_t        canvas_h;
       bool            mesh_dirty;
       void           *scratch;
   } gfx_motion_player_t;

Functions
---------

gfx_motion_scene_tick()
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   bool gfx_motion_scene_tick(gfx_motion_scene_t *scene);

gfx_motion_scene_advance()
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void gfx_motion_scene_advance(gfx_motion_scene_t *scene);

gfx_motion_scene_log_active_step()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void gfx_motion_scene_log_active_step(const gfx_motion_scene_t *scene, const char *reason);

gfx_motion_player_init()
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_motion_player_init(gfx_motion_player_t *player,
                                 gfx_disp_t *disp,
                                 const gfx_motion_asset_t *asset);

gfx_motion_player_deinit()
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void gfx_motion_player_deinit(gfx_motion_player_t *player);

gfx_motion_player_set_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_motion_player_set_color(gfx_motion_player_t *player, gfx_color_t color);

gfx_motion_player_set_canvas()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_motion_player_set_canvas(gfx_motion_player_t *player,
                                       gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h);

gfx_motion_player_set_layer_mask()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_motion_player_set_layer_mask(gfx_motion_player_t *player, uint32_t layer_mask);

gfx_motion_player_sync()
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_motion_player_sync(gfx_motion_player_t *player);

gfx_motion_player_set_action()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_motion_player_set_action(gfx_motion_player_t *player, uint16_t action_idx, bool snap);
