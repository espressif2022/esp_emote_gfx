List (list)
===========

Types
-----

gfx_list_focus_cb_t
~~~~~~~~~~~~~~~~~~~

Focus-change callback for a list widget.

.. code-block:: c

   typedef void (*gfx_list_focus_cb_t)(gfx_object_t *obj, int32_t focused_index, void *user_data);

Functions
---------

void()
~~~~~~

Focus-change callback for a list widget.

.. code-block:: c

   typedef void(*gfx_list_focus_cb_t)(gfx_object_t *obj, int32_t focused_index, void *user_data);

**Parameters:**

* ``obj`` - List object.
* ``focused_index`` - New focused item index, or -1 when no item is focused.
* ``user_data`` - User data passed to gfx_list_set_focus_cb().

gfx_list_create()
~~~~~~~~~~~~~~~~~

Create a list object on a display.

.. code-block:: c

   gfx_object_t * gfx_list_create(gfx_display_t *disp);

**Parameters:**

* ``disp`` - Display that owns the list.

**Returns:**

* Created list object, or NULL on failure.

gfx_list_clear()
~~~~~~~~~~~~~~~~

Remove all items from a list.

.. code-block:: c

   gfx_err_t gfx_list_clear(gfx_object_t *obj);

**Parameters:**

* ``obj`` - List object.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_add_item()
~~~~~~~~~~~~~~~~~~~

Append one text item to a list.

.. code-block:: c

   gfx_err_t gfx_list_add_item(gfx_object_t *obj, const char *text);

**Parameters:**

* ``obj`` - List object.
* ``text`` - Item text. The string is copied by the list.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_items()
~~~~~~~~~~~~~~~~~~~~

Replace all list items with a text array.

.. code-block:: c

   gfx_err_t gfx_list_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count);

**Parameters:**

* ``obj`` - List object.
* ``items`` - Array of item text pointers.
* ``item_count`` - Number of entries in items.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_focus()
~~~~~~~~~~~~~~~~~~~~

Set the focused item index.

.. code-block:: c

   gfx_err_t gfx_list_set_focus(gfx_object_t *obj, int32_t index);

**Parameters:**

* ``obj`` - List object.
* ``index`` - Item index to focus, or -1 to clear focus.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_get_focus()
~~~~~~~~~~~~~~~~~~~~

Get the focused item index.

.. code-block:: c

   int32_t gfx_list_get_focus(gfx_object_t *obj);

**Parameters:**

* ``obj`` - List object.

**Returns:**

* Focused item index, or -1 when no item is focused or obj is invalid.

gfx_list_set_top_index()
~~~~~~~~~~~~~~~~~~~~~~~~

Set the first visible item index.

.. code-block:: c

   gfx_err_t gfx_list_set_top_index(gfx_object_t *obj, uint16_t index);

**Parameters:**

* ``obj`` - List object.
* ``index`` - Item index to show at the top.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_get_top_index()
~~~~~~~~~~~~~~~~~~~~~~~~

Get the first visible item index.

.. code-block:: c

   uint16_t gfx_list_get_top_index(gfx_object_t *obj);

**Parameters:**

* ``obj`` - List object.

**Returns:**

* Top item index, or 0 when obj is invalid.

gfx_list_get_item_count()
~~~~~~~~~~~~~~~~~~~~~~~~~

Get the number of items in a list.

.. code-block:: c

   uint16_t gfx_list_get_item_count(gfx_object_t *obj);

**Parameters:**

* ``obj`` - List object.

**Returns:**

* Item count, or 0 when obj is invalid.

gfx_list_get_item_text()
~~~~~~~~~~~~~~~~~~~~~~~~

Get item text by index.

.. code-block:: c

   const char * gfx_list_get_item_text(gfx_object_t *obj, uint16_t index);

**Parameters:**

* ``obj`` - List object.
* ``index`` - Item index.

**Returns:**

* Item text pointer owned by the list, or NULL if index is invalid.

gfx_list_set_font()
~~~~~~~~~~~~~~~~~~~

Set the font used to draw list items.

.. code-block:: c

   gfx_err_t gfx_list_set_font(gfx_object_t *obj, gfx_font_t font);

**Parameters:**

* ``obj`` - List object.
* ``font`` - Font handle created by gfx_label_font_create(), or an LVGL font pointer.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_item_height()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set fixed item height.

.. code-block:: c

   gfx_err_t gfx_list_set_item_height(gfx_object_t *obj, uint16_t height);

**Parameters:**

* ``obj`` - List object.
* ``height`` - Item height in pixels.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_text_pad()
~~~~~~~~~~~~~~~~~~~~~~~

Set item text padding.

.. code-block:: c

   gfx_err_t gfx_list_set_text_pad(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y);

**Parameters:**

* ``obj`` - List object.
* ``pad_x`` - Horizontal text padding in pixels.
* ``pad_y`` - Vertical text padding in pixels.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_focus_cb()
~~~~~~~~~~~~~~~~~~~~~~~

Set the focus-change callback.

.. code-block:: c

   gfx_err_t gfx_list_set_focus_cb(gfx_object_t *obj, gfx_list_focus_cb_t cb, void *user_data);

**Parameters:**

* ``obj`` - List object.
* ``cb`` - Callback to invoke on focus change, or NULL to clear it.
* ``user_data`` - User data passed to cb.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~

Set the normal item background color.

.. code-block:: c

   gfx_err_t gfx_list_set_bg_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - List object.
* ``color`` - Background color.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_focus_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the focused item background color.

.. code-block:: c

   gfx_err_t gfx_list_set_focus_bg_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - List object.
* ``color`` - Focused background color.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_text_color()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the normal item text color.

.. code-block:: c

   gfx_err_t gfx_list_set_text_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - List object.
* ``color`` - Text color.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_focus_text_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the focused item text color.

.. code-block:: c

   gfx_err_t gfx_list_set_focus_text_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - List object.
* ``color`` - Focused text color.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_border_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the list border color.

.. code-block:: c

   gfx_err_t gfx_list_set_border_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - List object.
* ``color`` - Border color.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.

gfx_list_set_border_width()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the list border width.

.. code-block:: c

   gfx_err_t gfx_list_set_border_width(gfx_object_t *obj, uint16_t width);

**Parameters:**

* ``obj`` - List object.
* ``width`` - Border width in pixels; 0 disables the border.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise.
