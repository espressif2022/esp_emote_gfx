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
       GFX_DISP_EVENT_ALL_FRAME_DONE,
   } gfx_disp_event_t;

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
       } flags;
       struct {
           void *buf1;                          /**< Frame buffer 1 (NULL = internal alloc) */
           void *buf2;                          /**< Frame buffer 2 (NULL = internal alloc) */
           size_t buf_pixels;                   /**< Size per buffer in pixels (0 = auto) */
       } buffers;
   } gfx_disp_config_t;

Functions
---------

gfx_disp_del()
~~~~~~~~~~~~~~

.. code-block:: c

   void gfx_disp_del(gfx_disp_t *disp);

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

gfx_disp_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~

Get user data for a display

.. code-block:: c

   esp_err_t gfx_disp_set_bg_color(gfx_disp_t *disp, gfx_color_t color);

**Parameters:**

* ``disp`` - Display from gfx_disp_add

**Returns:**

* void* User data, or NULL
