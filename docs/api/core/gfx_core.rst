Core Graphics System (gfx_core)
===============================

Types
-----

gfx_handle_t
~~~~~~~~~~~~

.. code-block:: c

   typedef void *gfx_handle_t;

gfx_player_flush_cb_t
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef void (*gfx_player_flush_cb_t)(gfx_handle_t handle, int x1, int y1, int x2, int y2, const void *data);

gfx_player_update_cb_t
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef void (*gfx_player_update_cb_t)(gfx_handle_t handle, gfx_player_event_t event, const void *obj);

gfx_player_event_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_PLAYER_EVENT_IDLE = 0,
       GFX_PLAYER_EVENT_ONE_FRAME_DONE,
       GFX_PLAYER_EVENT_ALL_FRAME_DONE,
   } gfx_player_event_t;

gfx_core_config_t
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       gfx_player_flush_cb_t flush_cb;         ///< Callback function for flushing decoded data
       gfx_player_update_cb_t update_cb;       ///< Callback function for updating player
       void *user_data;             ///< User data
       struct {
           unsigned char swap: 1;
           unsigned char double_buffer: 1;
           unsigned char buff_dma: 1;
           unsigned char buff_spiram: 1;
       } flags;
   
       uint32_t h_res;        ///< Screen width in pixels
       uint32_t v_res;       ///< Screen height in pixels
       uint32_t fps;              ///< Target frame rate (frames per second)
   
       /* Buffer configuration */
       struct {
           void *buf1;                ///< Frame buffer 1 (NULL for internal allocation)
           void *buf2;                ///< Frame buffer 2 (NULL for internal allocation)
           size_t buf_pixels;         ///< Size of each buffer in pixels (0 for auto-calculation)
       } buffers;
   
       struct {
           int task_priority;      ///< Task priority (1-20)
           int task_stack;         ///< Task stack size in bytes
           int task_affinity;      ///< CPU core ID (-1: no affinity, 0: core 0, 1: core 1)
           unsigned task_stack_caps; /*!< LVGL task stack memory capabilities (see esp_heap_caps.h) */
       } task;
       gfx_touch_config_t touch;          ///< Optional touch configuration
   } gfx_core_config_t;

Macros
------

GFX_EMOTE_INIT_CONFIG()
~~~~~~~~~~~~~~~~~~~~~~~

LVGL port configuration structure

.. code-block:: c

   #define GFX_EMOTE_INIT_CONFIG()                   \

Functions
---------

gfx_emote_init()
~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg);

gfx_emote_deinit()
~~~~~~~~~~~~~~~~~~

Deinitialize graphics context

.. code-block:: c

   void gfx_emote_deinit(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

gfx_emote_flush_ready()
~~~~~~~~~~~~~~~~~~~~~~~

Check if flush is ready

.. code-block:: c

   bool gfx_emote_flush_ready(gfx_handle_t handle, bool swap_act_buf);

**Parameters:**

* ``handle`` - Graphics handle
* ``swap_act_buf`` - Whether to swap the active buffer

**Returns:**

* bool True if the flush is ready, false otherwise

gfx_emote_get_screen_size()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get the user data of the graphics context

.. code-block:: c

   esp_err_t gfx_emote_get_screen_size(gfx_handle_t handle, uint32_t *width, uint32_t *height);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* void* User data

gfx_emote_lock()
~~~~~~~~~~~~~~~~

Lock the recursive render mutex to prevent rendering during external operations

.. code-block:: c

   esp_err_t gfx_emote_lock(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* esp_err_t ESP_OK on success, otherwise an error code

gfx_emote_unlock()
~~~~~~~~~~~~~~~~~~

Unlock the recursive render mutex after external operations

.. code-block:: c

   esp_err_t gfx_emote_unlock(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* esp_err_t ESP_OK on success, otherwise an error code

gfx_emote_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~

Set the default background color for frame buffers

.. code-block:: c

   esp_err_t gfx_emote_set_bg_color(gfx_handle_t handle, gfx_color_t color);

**Parameters:**

* ``handle`` - Graphics handle
* ``color`` - Default background color in RGB565 format

**Returns:**

* esp_err_t ESP_OK on success, otherwise an error code

gfx_emote_is_flushing_last()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check if the system is currently flushing the last block

.. code-block:: c

   bool gfx_emote_is_flushing_last(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle

**Returns:**

* bool True if flushing the last block, false otherwise

gfx_emote_refresh_all()
~~~~~~~~~~~~~~~~~~~~~~~

Invalidate full screen to trigger initial refresh

.. code-block:: c

   void gfx_emote_refresh_all(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Graphics handle
