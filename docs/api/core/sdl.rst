SDL Backend (sdl)
=================

Types
-----

gfx_backend_sdl_config_t
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t h_res;          /**< Simulated display width in pixels */
       uint32_t v_res;          /**< Simulated display height in pixels */
       uint32_t scale;          /**< Window scale factor; 0 means 1 */
       const char *title;       /**< Window title; NULL uses a default title */
   } gfx_backend_sdl_config_t;

Functions
---------

gfx_backend_sdl_create()
~~~~~~~~~~~~~~~~~~~~~~~~

Create an SDL display backend for host-side simulators.

.. code-block:: c

   gfx_backend_t * gfx_backend_sdl_create(const gfx_backend_sdl_config_t *cfg);

**Parameters:**

* ``cfg`` - SDL backend configuration.

**Returns:**

* Backend pointer on success, or NULL on failure.

gfx_backend_sdl_delete()
~~~~~~~~~~~~~~~~~~~~~~~~

Delete an SDL backend that is not owned by a display.

.. code-block:: c

   void gfx_backend_sdl_delete(gfx_backend_t *backend);

**Parameters:**

* ``backend`` - Backend returned by gfx_backend_sdl_create().

gfx_backend_sdl_poll()
~~~~~~~~~~~~~~~~~~~~~~

Poll SDL window events, dispatch mouse input, and tick the display.

.. code-block:: c

   bool gfx_backend_sdl_poll(gfx_display_t *display);

**Parameters:**

* ``display`` - Display that should receive simulated touch events, or NULL to only process window/keyboard events.

**Returns:**

* true when the user requested quit, false otherwise.

gfx_backend_sdl_poll_quit()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Poll SDL window events without a display.

.. code-block:: c

   bool gfx_backend_sdl_poll_quit(void);

**Returns:**

* true when the user requested quit, false otherwise.
