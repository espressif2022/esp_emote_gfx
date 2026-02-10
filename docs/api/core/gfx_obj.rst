Object (gfx_obj)
================

Types
-----

gfx_obj_touch_cb_t
~~~~~~~~~~~~~~~~~~

Application-level touch callback (register with gfx_obj_set_touch_cb)

.. code-block:: c

   typedef void (*gfx_obj_touch_cb_t)(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data);

Functions
---------

gfx_obj_set_pos()
~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y);

gfx_obj_set_size()
~~~~~~~~~~~~~~~~~~

Set the size of an object

.. code-block:: c

   esp_err_t gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h);

**Parameters:**

* ``obj`` - Pointer to the object
* ``w`` - Width
* ``h`` - Height

gfx_obj_align()
~~~~~~~~~~~~~~~

Align an object relative to the screen or another object

.. code-block:: c

   esp_err_t gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs);

**Parameters:**

* ``obj`` - Pointer to the object to align
* ``align`` - Alignment type (see GFX_ALIGN_* constants)
* ``x_ofs`` - X offset from the alignment position
* ``y_ofs`` - Y offset from the alignment position

gfx_obj_set_visible()
~~~~~~~~~~~~~~~~~~~~~

Set object visibility

.. code-block:: c

   esp_err_t gfx_obj_set_visible(gfx_obj_t *obj, bool visible);

**Parameters:**

* ``obj`` - Object to set visibility for
* ``visible`` - True to make object visible, false to hide

gfx_obj_get_visible()
~~~~~~~~~~~~~~~~~~~~~

Get object visibility

.. code-block:: c

   bool gfx_obj_get_visible(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Object to check visibility for

**Returns:**

* True if object is visible, false if hidden

gfx_obj_update_layout()
~~~~~~~~~~~~~~~~~~~~~~~

Update object's layout (mark for recalculation before rendering)

.. code-block:: c

   void gfx_obj_update_layout(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Object to update layout

**Note:**

This is used when object properties that affect layout have changed, but the actual position calculation needs to be deferred until rendering

gfx_obj_get_pos()
~~~~~~~~~~~~~~~~~

Get the position of an object

.. code-block:: c

   esp_err_t gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y);

**Parameters:**

* ``obj`` - Pointer to the object
* ``x`` - Pointer to store X coordinate
* ``y`` - Pointer to store Y coordinate

gfx_obj_get_size()
~~~~~~~~~~~~~~~~~~

Get the size of an object

.. code-block:: c

   esp_err_t gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h);

**Parameters:**

* ``obj`` - Pointer to the object
* ``w`` - Pointer to store width
* ``h`` - Pointer to store height

gfx_obj_delete()
~~~~~~~~~~~~~~~~

Delete an object

.. code-block:: c

   esp_err_t gfx_obj_delete(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the object to delete

gfx_obj_set_touch_cb()
~~~~~~~~~~~~~~~~~~~~~~

Register application touch callback for an object

.. code-block:: c

   esp_err_t gfx_obj_set_touch_cb(gfx_obj_t *obj, gfx_obj_touch_cb_t cb, void *user_data);

**Parameters:**

* ``obj`` - Object to listen on
* ``cb`` - Callback (NULL to clear)
* ``user_data`` - Passed to cb

**Returns:**

* ESP_OK on success
