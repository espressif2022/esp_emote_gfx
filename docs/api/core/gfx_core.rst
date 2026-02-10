Core Graphics System (gfx_core)
===============================

Types
-----

gfx_core_config_t
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t fps;                               /**< Target FPS (frames per second) */
       struct {
           int task_priority;                       /**< Render task priority (1–20) */
           int task_stack;                         /**< Render task stack size (bytes) */
           int task_affinity;                       /**< CPU core (-1: any, 0/1: pinned) */
           unsigned task_stack_caps;                /**< Stack heap caps (see esp_heap_caps.h) */
       } task;
   } gfx_core_config_t;

Macros
------

GFX_EMOTE_INIT_CONFIG()
~~~~~~~~~~~~~~~~~~~~~~~

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
