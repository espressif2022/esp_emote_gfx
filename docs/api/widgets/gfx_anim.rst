Animation Widget (gfx_anim)
=============================

The animation widget provides playback of EAF (ESP Animation Format) files with frame-by-frame control.

Functions
---------

Object Creation
~~~~~~~~~~~~~~~

gfx_anim_create()
~~~~~~~~~~~~~~~~~

Create an animation object.

.. code-block:: c

   gfx_obj_t *gfx_anim_create(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Animation player handle

**Returns:**

* Pointer to the created animation object, NULL on error

Animation Operations
~~~~~~~~~~~~~~~~~~~~

gfx_anim_set_src()
~~~~~~~~~~~~~~~~~~~

Set the source data for an animation object.

.. code-block:: c

   esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``src_data`` - Source data (EAF format)
* ``src_len`` - Source data length in bytes

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // Load animation from file or memory
   const uint8_t *anim_data = ...;
   size_t anim_size = ...;
   
   gfx_obj_t *anim = gfx_anim_create(handle);
   gfx_anim_set_src(anim, anim_data, anim_size);
   gfx_obj_set_size(anim, 200, 150);

gfx_anim_set_segment()
~~~~~~~~~~~~~~~~~~~~~~

Set the segment for an animation object.

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

**Example:**

.. code-block:: c

   // Play frames 0-10 at 30 FPS, looping
   gfx_anim_set_segment(anim, 0, 10, 30, true);

   // Play frames 5-15 at 24 FPS, once
   gfx_anim_set_segment(anim, 5, 15, 24, false);

Playback Control
~~~~~~~~~~~~~~~~

gfx_anim_start()
~~~~~~~~~~~~~~~~~

Start the animation.

.. code-block:: c

   esp_err_t gfx_anim_start(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_stop()
~~~~~~~~~~~~~~~~

Stop the animation.

.. code-block:: c

   esp_err_t gfx_anim_stop(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, error code otherwise

Visual Effects
~~~~~~~~~~~~~~

gfx_anim_set_mirror()
~~~~~~~~~~~~~~~~~~~~~

Set mirror display for an animation object.

.. code-block:: c

   esp_err_t gfx_anim_set_mirror(gfx_obj_t *obj, bool enabled, int16_t offset);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``enabled`` - Whether to enable mirror display
* ``offset`` - Mirror offset in pixels

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

This provides manual mirror control. For automatic horizontal mirroring, use gfx_anim_set_auto_mirror().

gfx_anim_set_auto_mirror()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set auto mirror alignment for animation object.

.. code-block:: c

   esp_err_t gfx_anim_set_auto_mirror(gfx_obj_t *obj, bool enabled);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``enabled`` - Whether to enable auto mirror alignment

**Returns:**

* ESP_OK on success, ESP_ERR_* otherwise

**Note:**

When enabled, the animation will be automatically mirrored horizontally. This is useful for creating symmetric animations.

Animation Format
----------------

EAF (ESP Animation Format)
~~~~~~~~~~~~~~~~~~~~~~~~~~

EAF files are created using the ESP32 GIF animation tools available at:
https://esp32-gif.espressif.com/

The format supports:
* Multiple color depths (4-bit, 8-bit, 24-bit)
* Compression (Huffman encoding)
* Frame-by-frame playback
* Efficient memory usage

Example Usage
-------------

Complete animation setup:

.. code-block:: c

   // Create animation object
   gfx_obj_t *anim = gfx_anim_create(handle);

   // Load animation data
   gfx_anim_set_src(anim, anim_data, anim_size);

   // Set display size
   gfx_obj_set_size(anim, 200, 150);

   // Configure playback segment
   gfx_anim_set_segment(anim, 0, 20, 30, true);  // Frames 0-20, 30 FPS, loop

   // Enable auto mirror
   gfx_anim_set_auto_mirror(anim, true);

   // Start animation
   gfx_anim_start(anim);

   // Later, stop the animation
   gfx_anim_stop(anim);

