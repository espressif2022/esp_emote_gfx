LVGL Font Compatibility (gfx_font_lvgl)
=======================================

Functions
---------

gfx_font_lv_load_from_binary()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   lv_font_t * gfx_font_lv_load_from_binary(uint8_t *bin_addr);

gfx_font_lv_delete()
~~~~~~~~~~~~~~~~~~~~

Delete an LVGL font created from binary data

.. code-block:: c

   void gfx_font_lv_delete(lv_font_t *font);

**Parameters:**

* ``font`` - Pointer to lv_font_t to delete
