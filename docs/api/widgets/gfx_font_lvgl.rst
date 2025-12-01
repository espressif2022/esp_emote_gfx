LVGL Font Compatibility (gfx_font_lvgl)
=========================================

This module provides compatibility functions for loading and using LVGL fonts in the ESP Emote GFX framework.

Functions
---------

gfx_font_lv_load_from_binary()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Load an LVGL font from binary data.

.. code-block:: c

   lv_font_t *gfx_font_lv_load_from_binary(uint8_t *bin_addr);

**Parameters:**

* ``bin_addr`` - Pointer to binary data containing lv_font_t structure

**Returns:**

* Pointer to loaded lv_font_t on success, NULL on failure

**Note:**

This function is useful when loading LVGL fonts from binary files or memory-mapped regions.

gfx_font_lv_delete()
~~~~~~~~~~~~~~~~~~~~

Delete an LVGL font created from binary data.

.. code-block:: c

   void gfx_font_lv_delete(lv_font_t *font);

**Parameters:**

* ``font`` - Pointer to lv_font_t to delete

**Note:**

Use this function to free resources when you're done with a font loaded via gfx_font_lv_load_from_binary().

Usage with Label Widget
-----------------------

LVGL fonts can be used directly with the label widget:

.. code-block:: c

   // Using a compiled LVGL font
   extern const lv_font_t font_puhui_16_4;
   gfx_obj_t *label = gfx_label_create(handle);
   gfx_label_set_font(label, (gfx_font_t)&font_puhui_16_4);

   // Using a font loaded from binary
   uint8_t *font_binary = ...;  // Font binary data
   lv_font_t *font = gfx_font_lv_load_from_binary(font_binary);
   if (font) {
       gfx_label_set_font(label, (gfx_font_t)font);
       // ... use font ...
       gfx_font_lv_delete(font);
   }

Font Format
-----------

LVGL fonts are typically compiled into C arrays using the LVGL font converter tools. The fonts contain:

* Glyph bitmap data
* Character mapping information
* Font metrics (line height, baseline, etc.)

The ESP Emote GFX framework is compatible with standard LVGL font structures, allowing you to use fonts created with LVGL tools directly.

