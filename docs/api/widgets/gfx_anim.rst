Animation (gfx_anim)
====================

Functions
---------

gfx_anim_set_src()
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len);

gfx_anim_set_segment()
~~~~~~~~~~~~~~~~~~~~~~

Set the segment for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_segment(gfx_obj_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``start`` - Start frame index
* ``end`` - End frame index
* ``fps`` - Frames per second
* ``repeat`` - Whether to repeat the animation

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_start()
~~~~~~~~~~~~~~~~

Start the animation

.. code-block:: c

   esp_err_t gfx_anim_start(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_stop()
~~~~~~~~~~~~~~~

Stop the animation

.. code-block:: c

   esp_err_t gfx_anim_stop(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_mirror()
~~~~~~~~~~~~~~~~~~~~~

Set mirror display for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_mirror(gfx_obj_t *obj, bool enabled, int16_t offset);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``enabled`` - Whether to enable mirror display
* ``offset`` - Mirror offset in pixels

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_auto_mirror()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set auto mirror alignment for animation object

.. code-block:: c

   esp_err_t gfx_anim_set_auto_mirror(gfx_obj_t *obj, bool enabled);

**Parameters:**

* ``obj`` - Animation object
* ``enabled`` - Whether to enable auto mirror alignment

**Returns:**

* ESP_OK on success, ESP_ERR_* otherwise
