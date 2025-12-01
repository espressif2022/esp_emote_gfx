Object System (gfx_obj)
========================

The object system provides the base functionality for all graphical elements in the framework.

Object Types
------------

.. code-block:: c

   #define GFX_OBJ_TYPE_IMAGE        0x01
   #define GFX_OBJ_TYPE_LABEL        0x02
   #define GFX_OBJ_TYPE_ANIMATION    0x03
   #define GFX_OBJ_TYPE_QRCODE       0x04

Alignment Constants
-------------------

The framework provides alignment constants similar to LVGL:

.. code-block:: c

   #define GFX_ALIGN_DEFAULT             0x00
   #define GFX_ALIGN_TOP_LEFT           0x00
   #define GFX_ALIGN_TOP_MID            0x01
   #define GFX_ALIGN_TOP_RIGHT          0x02
   #define GFX_ALIGN_LEFT_MID           0x03
   #define GFX_ALIGN_CENTER             0x04
   #define GFX_ALIGN_RIGHT_MID          0x05
   #define GFX_ALIGN_BOTTOM_LEFT        0x06
   #define GFX_ALIGN_BOTTOM_MID         0x07
   #define GFX_ALIGN_BOTTOM_RIGHT       0x08
   #define GFX_ALIGN_OUT_TOP_LEFT       0x09
   #define GFX_ALIGN_OUT_TOP_MID        0x0A
   #define GFX_ALIGN_OUT_TOP_RIGHT      0x0B
   #define GFX_ALIGN_OUT_LEFT_TOP       0x0C
   #define GFX_ALIGN_OUT_LEFT_MID       0x0D
   #define GFX_ALIGN_OUT_LEFT_BOTTOM    0x0E
   #define GFX_ALIGN_OUT_RIGHT_TOP      0x0F
   #define GFX_ALIGN_OUT_RIGHT_MID      0x10
   #define GFX_ALIGN_OUT_RIGHT_BOTTOM   0x11
   #define GFX_ALIGN_OUT_BOTTOM_LEFT    0x12
   #define GFX_ALIGN_OUT_BOTTOM_MID     0x13
   #define GFX_ALIGN_OUT_BOTTOM_RIGHT   0x14

Types
-----

gfx_obj_t
~~~~~~~~~

Opaque object type. All widgets inherit from this base type.

.. code-block:: c

   typedef struct gfx_obj gfx_obj_t;

Functions
---------

Position and Size
~~~~~~~~~~~~~~~~~

gfx_obj_set_pos()
~~~~~~~~~~~~~~~~~

Set the position of an object.

.. code-block:: c

   esp_err_t gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y);

**Parameters:**

* ``obj`` - Pointer to the object
* ``x`` - X coordinate
* ``y`` - Y coordinate

**Returns:**

* ESP_OK on success, error code otherwise

gfx_obj_get_pos()
~~~~~~~~~~~~~~~~~

Get the position of an object.

.. code-block:: c

   esp_err_t gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y);

**Parameters:**

* ``obj`` - Pointer to the object
* ``x`` - Pointer to store X coordinate
* ``y`` - Pointer to store Y coordinate

**Returns:**

* ESP_OK on success, error code otherwise

gfx_obj_set_size()
~~~~~~~~~~~~~~~~~~

Set the size of an object.

.. code-block:: c

   esp_err_t gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h);

**Parameters:**

* ``obj`` - Pointer to the object
* ``w`` - Width
* ``h`` - Height

**Returns:**

* ESP_OK on success, error code otherwise

gfx_obj_get_size()
~~~~~~~~~~~~~~~~~~

Get the size of an object.

.. code-block:: c

   esp_err_t gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h);

**Parameters:**

* ``obj`` - Pointer to the object
* ``w`` - Pointer to store width
* ``h`` - Pointer to store height

**Returns:**

* ESP_OK on success, error code otherwise

Alignment
~~~~~~~~~

gfx_obj_align()
~~~~~~~~~~~~~~~

Align an object relative to the screen or another object.

.. code-block:: c

   esp_err_t gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs);

**Parameters:**

* ``obj`` - Pointer to the object to align
* ``align`` - Alignment type (see GFX_ALIGN_* constants)
* ``x_ofs`` - X offset from the alignment position
* ``y_ofs`` - Y offset from the alignment position

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // Center an object on screen
   gfx_obj_align(obj, GFX_ALIGN_CENTER, 0, 0);

   // Align to top-middle with 10 pixel offset
   gfx_obj_align(obj, GFX_ALIGN_TOP_MID, 0, 10);

Visibility
~~~~~~~~~~

gfx_obj_set_visible()
~~~~~~~~~~~~~~~~~~~~~

Set object visibility.

.. code-block:: c

   esp_err_t gfx_obj_set_visible(gfx_obj_t *obj, bool visible);

**Parameters:**

* ``obj`` - Object to set visibility for
* ``visible`` - True to make object visible, false to hide

**Returns:**

* ESP_OK on success, error code otherwise

gfx_obj_get_visible()
~~~~~~~~~~~~~~~~~~~~~

Get object visibility.

.. code-block:: c

   bool gfx_obj_get_visible(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Object to check visibility for

**Returns:**

* True if object is visible, false if hidden

Layout Management
~~~~~~~~~~~~~~~~~

gfx_obj_update_layout()
~~~~~~~~~~~~~~~~~~~~~~~

Update object's layout (mark for recalculation before rendering).

.. code-block:: c

   void gfx_obj_update_layout(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Object to update layout

**Note:**

This is used when object properties that affect layout have changed, but the actual position calculation needs to be deferred until rendering.

Object Management
~~~~~~~~~~~~~~~~~

gfx_obj_delete()
~~~~~~~~~~~~~~~~

Delete an object.

.. code-block:: c

   esp_err_t gfx_obj_delete(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the object to delete

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

This function frees all resources associated with the object.

