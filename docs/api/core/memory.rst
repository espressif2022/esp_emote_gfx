Memory Backend (memory)
=======================

Types
-----

gfx_memory_backend_config_t
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t h_res;          /**< Framebuffer width in pixels */
       uint32_t v_res;          /**< Framebuffer height in pixels */
       gfx_color_format_t color_format; /**< Backend framebuffer format (0 = RGB565) */
       void *buffer;            /**< Optional external RGB565 framebuffer */
       size_t buffer_pixels;    /**< External framebuffer size in pixels */
   } gfx_memory_backend_config_t;

Functions
---------

gfx_memory_backend_create()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a memory display backend.

.. code-block:: c

   gfx_backend_t * gfx_memory_backend_create(const gfx_memory_backend_config_t *cfg);

**Parameters:**

* ``cfg`` - Memory backend configuration.

**Returns:**

* Backend pointer on success, or NULL on failure.

gfx_memory_backend_delete()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Delete a memory backend that is not owned by a display.

.. code-block:: c

   void gfx_memory_backend_delete(gfx_backend_t *backend);

**Parameters:**

* ``backend`` - Backend returned by gfx_memory_backend_create().

gfx_memory_backend_get_buffer()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get the semantic RGB565 framebuffer.

.. code-block:: c

   const uint16_t * gfx_memory_backend_get_buffer(const gfx_backend_t *backend);

**Parameters:**

* ``backend`` - Memory backend.

**Returns:**

* Framebuffer pointer, or NULL if backend is invalid.

gfx_memory_backend_get_buffer_pixels()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get framebuffer size in pixels.

.. code-block:: c

   size_t gfx_memory_backend_get_buffer_pixels(const gfx_backend_t *backend);

**Parameters:**

* ``backend`` - Memory backend.

**Returns:**

* Number of pixels, or 0 if backend is invalid.

gfx_memory_backend_clear()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Fill the memory backend framebuffer with a semantic RGB565 color.

.. code-block:: c

   gfx_err_t gfx_memory_backend_clear(gfx_backend_t *backend, gfx_color_t color);

**Parameters:**

* ``backend`` - Memory backend.
* ``color`` - Semantic RGB565 color.

**Returns:**

* GFX_OK on success, error code otherwise.
