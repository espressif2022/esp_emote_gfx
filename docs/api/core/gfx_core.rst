Core Graphics System (gfx_core)
================================

The core graphics system manages the graphics context, buffers, and rendering pipeline.

Types
-----

gfx_handle_t
~~~~~~~~~~~~

Opaque handle type for the graphics context.

.. code-block:: c

   typedef void *gfx_handle_t;

gfx_core_config_t
~~~~~~~~~~~~~~~~~

Configuration structure for initializing the graphics system.

.. code-block:: c

   typedef struct {
       gfx_player_flush_cb_t flush_cb;         ///< Callback for flushing decoded data
       gfx_player_update_cb_t update_cb;        ///< Callback for updating player
       void *user_data;                        ///< User data
       struct {
           unsigned char swap: 1;
           unsigned char double_buffer: 1;
           unsigned char buff_dma: 1;
           unsigned char buff_spiram: 1;
       } flags;
       uint32_t h_res;                        ///< Screen width in pixels
       uint32_t v_res;                        ///< Screen height in pixels
       uint32_t fps;                          ///< Target frame rate
       struct {
           void *buf1;                         ///< Frame buffer 1 (NULL for internal)
           void *buf2;                         ///< Frame buffer 2 (NULL for internal)
           size_t buf_pixels;                  ///< Size of each buffer in pixels
       } buffers;
       struct {
           int task_priority;                  ///< Task priority (1-20)
           int task_stack;                     ///< Task stack size in bytes
           int task_affinity;                  ///< CPU core ID (-1: no affinity)
           unsigned task_stack_caps;          ///< Stack memory capabilities
       } task;
   } gfx_core_config_t;

gfx_player_event_t
~~~~~~~~~~~~~~~~~~

Player event types.

.. code-block:: c

   typedef enum {
       GFX_PLAYER_EVENT_IDLE = 0,
       GFX_PLAYER_EVENT_ONE_FRAME_DONE,
       GFX_PLAYER_EVENT_ALL_FRAME_DONE,
   } gfx_player_event_t;

gfx_player_flush_cb_t
~~~~~~~~~~~~~~~~~~~~~

Callback function type for flushing display data.

.. code-block:: c

   typedef void (*gfx_player_flush_cb_t)(gfx_handle_t handle, int x1, int y1, int x2, int y2, const void *data);

gfx_player_update_cb_t
~~~~~~~~~~~~~~~~~~~~~~~

Callback function type for player updates.

.. code-block:: c

   typedef void (*gfx_player_update_cb_t)(gfx_handle_t handle, gfx_player_event_t event, const void *obj);

Macros
------

GFX_EMOTE_INIT_CONFIG()
~~~~~~~~~~~~~~~~~~~~~~~

Default configuration macro for task settings.

.. code-block:: c

   #define GFX_EMOTE_INIT_CONFIG() \
       { \
           .task_priority = 4, \
           .task_stack = 7168, \
           .task_affinity = -1, \
           .task_stack_caps = MALLOC_CAP_DEFAULT, \
       }

Functions
---------

gfx_emote_init()
~~~~~~~~~~~~~~~~

Initialize the graphics context.

.. code-block:: c

   gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg);

**Parameters:**

* ``cfg`` - Graphics configuration structure

**Returns:**

* Graphics handle on success, NULL on error

**Example:**

.. code-block:: c

   gfx_core_config_t cfg = {
       .flush_cb = my_flush_callback,
       .h_res = 320,
       .v_res = 240,
       .fps = 30,
       .buffers = { NULL, NULL, 0 },
       .task = GFX_EMOTE_INIT_CONFIG(),
   };
   gfx_handle_t handle = gfx_emote_init(&cfg);

gfx_emote_deinit()
~~~~~~~~~~~~~~~~~~

Deinitialize the graphics context.

.. code-block:: c

   void gfx_emote_deinit(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

gfx_emote_flush_ready()
~~~~~~~~~~~~~~~~~~~~~~~

Check if flush is ready.

.. code-block:: c

   bool gfx_emote_flush_ready(gfx_handle_t handle, bool swap_act_buf);

**Parameters:**

* ``handle`` - Graphics handle
* ``swap_act_buf`` - Whether to swap the active buffer

**Returns:**

* True if flush is ready, false otherwise

gfx_emote_get_user_data()
~~~~~~~~~~~~~~~~~~~~~~~~~

Get the user data of the graphics context.

.. code-block:: c

   void *gfx_emote_get_user_data(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* User data pointer

gfx_emote_get_screen_size()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get screen dimensions.

.. code-block:: c

   esp_err_t gfx_emote_get_screen_size(gfx_handle_t handle, uint32_t *width, uint32_t *height);

**Parameters:**

* ``handle`` - Graphics handle
* ``width`` - Pointer to store screen width
* ``height`` - Pointer to store screen height

**Returns:**

* ESP_OK on success, error code otherwise

gfx_emote_lock()
~~~~~~~~~~~~~~~~

Lock the recursive render mutex.

.. code-block:: c

   esp_err_t gfx_emote_lock(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

Use this before performing widget operations from outside the graphics task.

gfx_emote_unlock()
~~~~~~~~~~~~~~~~~~

Unlock the recursive render mutex.

.. code-block:: c

   esp_err_t gfx_emote_unlock(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* ESP_OK on success, error code otherwise

gfx_emote_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~

Set the default background color for frame buffers.

.. code-block:: c

   esp_err_t gfx_emote_set_bg_color(gfx_handle_t handle, gfx_color_t color);

**Parameters:**

* ``handle`` - Graphics handle
* ``color`` - Background color in RGB565 format

**Returns:**

* ESP_OK on success, error code otherwise

gfx_emote_is_flushing_last()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check if the system is currently flushing the last block.

.. code-block:: c

   bool gfx_emote_is_flushing_last(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* True if flushing the last block, false otherwise

gfx_emote_refresh_all()
~~~~~~~~~~~~~~~~~~~~~~~

Invalidate full screen to trigger initial refresh.

.. code-block:: c

   void gfx_emote_refresh_all(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

