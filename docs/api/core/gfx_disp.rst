Display (gfx_disp)
==================

Types
-----

gfx_disp_flush_cb_t
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef void (*gfx_disp_flush_cb_t)(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data);

gfx_disp_update_cb_t
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef void (*gfx_disp_update_cb_t)(gfx_disp_t *disp, gfx_disp_event_t event, const void *obj);

gfx_disp_event_t
~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_DISP_EVENT_IDLE = 0,
       GFX_DISP_EVENT_ONE_FRAME_DONE,
       GFX_DISP_EVENT_PART_FRAME_DONE,
       GFX_DISP_EVENT_ALL_FRAME_DONE,
   } gfx_disp_event_t;

gfx_perf_counter_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint64_t calls;           /**< Number of API calls */
       uint64_t pixels;          /**< Processed pixels */
       uint64_t time_us;         /**< Elapsed time in microseconds */
   } gfx_perf_counter_t;

gfx_blend_perf_stats_t
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       gfx_perf_counter_t fill;          /**< gfx_sw_blend_fill_area */
       gfx_perf_counter_t color_draw;    /**< gfx_sw_blend_draw */
       gfx_perf_counter_t image_draw;    /**< gfx_sw_blend_img_draw */
       gfx_perf_counter_t triangle_draw; /**< gfx_sw_blend_img_triangle_draw */
       uint64_t triangle_covered_pixels; /**< Triangle pixels blended (inside + AA) */
       uint64_t triangle_aa_pixels;      /**< Triangle edge-AA blended pixels */
   } gfx_blend_perf_stats_t;

gfx_disp_perf_stats_t
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t dirty_pixels;            /**< Dirty pixels in the latest rendered frame */
       uint64_t frame_time_us;           /**< Total frame time */
       uint64_t render_time_us;          /**< Time spent in render phase */
       uint64_t flush_time_us;           /**< Time spent in flush callbacks */
       uint32_t flush_count;             /**< Number of flush calls */
       gfx_blend_perf_stats_t blend;     /**< Blend-stage details */
   } gfx_disp_perf_stats_t;

gfx_disp_config_t
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t h_res;                          /**< Screen width in pixels */
       uint32_t v_res;                          /**< Screen height in pixels */
       gfx_disp_flush_cb_t flush_cb;          /**< Flush callback for this display */
       gfx_disp_update_cb_t update_cb;       /**< Update callback (frame/playback events) */
       void *user_data;                         /**< User data for this display */
       struct {
           unsigned char swap : 1;              /**< Color swap flag */
           unsigned char buff_dma : 1;          /**< Alloc buffer with MALLOC_CAP_DMA (internal alloc only) */
           unsigned char buff_spiram : 1;       /**< Alloc buffer in PSRAM (internal alloc only) */
           unsigned char double_buffer : 1;     /**< Alloc second buffer for double buffering (internal alloc only) */
           unsigned char full_frame : 1;    /**< 1 = buf1/buf2 are full-screen framebuffers (e.g. RGB); draw at chunk region. 0 = partition buffer; draw from start. */
       } flags;
       struct {
           void *buf1;                          /**< Frame buffer 1 (NULL = internal alloc) */
           void *buf2;                          /**< Frame buffer 2 (NULL = internal alloc) */
           size_t buf_pixels;                   /**< Size per buffer in pixels (0 = auto) */
       } buffers;
   } gfx_disp_config_t;

Functions
---------

gfx_disp_add()
~~~~~~~~~~~~~~

.. code-block:: c

   gfx_disp_t * gfx_disp_add(gfx_handle_t handle, const gfx_disp_config_t *cfg);

gfx_disp_del()
~~~~~~~~~~~~~~

Remove a display from the list and release its resources (child list nodes, event group, buffers). Does not free the gfx_disp_t; caller must free(disp) after.

.. code-block:: c

   void gfx_disp_del(gfx_disp_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_disp_add; safe to pass NULL

gfx_disp_refresh_all()
~~~~~~~~~~~~~~~~~~~~~~

Invalidate full screen of a display to trigger refresh

.. code-block:: c

   void gfx_disp_refresh_all(gfx_disp_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_disp_add

gfx_disp_flush_ready()
~~~~~~~~~~~~~~~~~~~~~~

Notify that flush is done (e.g. from panel IO callback)

.. code-block:: c

   bool gfx_disp_flush_ready(gfx_disp_t *disp, bool swap_act_buf);

**Parameters:**

* ``disp`` - Display from gfx_disp_add
* ``swap_act_buf`` - Whether to swap the active buffer

**Returns:**

* bool True on success

gfx_disp_get_user_data()
~~~~~~~~~~~~~~~~~~~~~~~~

Get user data for a display

.. code-block:: c

   void * gfx_disp_get_user_data(gfx_disp_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_disp_add

**Returns:**

* void* User data, or NULL

gfx_disp_get_hor_res()
~~~~~~~~~~~~~~~~~~~~~~

Get display horizontal resolution in pixels

.. code-block:: c

   uint32_t gfx_disp_get_hor_res(gfx_disp_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_disp_add (NULL allowed; returns default width)

**Returns:**

* uint32_t Width in pixels

gfx_disp_get_ver_res()
~~~~~~~~~~~~~~~~~~~~~~

Get display vertical resolution in pixels

.. code-block:: c

   uint32_t gfx_disp_get_ver_res(gfx_disp_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_disp_add (NULL allowed; returns default height)

**Returns:**

* uint32_t Height in pixels

gfx_disp_is_flushing_last()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check if display is currently flushing the last block

.. code-block:: c

   bool gfx_disp_is_flushing_last(gfx_disp_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_disp_add

**Returns:**

* true if flushing last block, false otherwise

gfx_disp_get_perf_stats()
~~~~~~~~~~~~~~~~~~~~~~~~~

Get latest per-display performance statistics

.. code-block:: c

   esp_err_t gfx_disp_get_perf_stats(gfx_disp_t *disp, gfx_disp_perf_stats_t *out_stats);

**Parameters:**

* ``disp`` - Display handle
* ``out_stats`` - Output stats structure

**Returns:**

* ESP_OK on success

gfx_disp_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~

Set default background color for a display

.. code-block:: c

   esp_err_t gfx_disp_set_bg_color(gfx_disp_t *disp, gfx_color_t color);

**Parameters:**

* ``disp`` - Display from gfx_disp_add
* ``color`` - Background color (e.g. RGB565)

**Returns:**

* esp_err_t ESP_OK on success

gfx_disp_set_bg_enable()
~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable drawing the background (fill with bg_color before widgets)

.. code-block:: c

   esp_err_t gfx_disp_set_bg_enable(gfx_disp_t *disp, bool enable);

**Parameters:**

* ``disp`` - Display from gfx_disp_add
* ``enable`` - true to enable background (default), false to disable background

**Returns:**

* ESP_OK on success
