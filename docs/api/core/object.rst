Object (object)
===============

Functions
---------

gfx_object_set_pos()
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_set_pos(gfx_object_t *obj, gfx_coord_t x, gfx_coord_t y);

gfx_object_set_size()
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_set_size(gfx_object_t *obj, uint16_t w, uint16_t h);

gfx_object_align()
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_align(gfx_object_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs);

gfx_object_align_to()
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_align_to(gfx_object_t *obj, gfx_object_t *base, uint8_t align,
                              gfx_coord_t x_ofs, gfx_coord_t y_ofs);

gfx_object_set_visible()
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_set_visible(gfx_object_t *obj, bool visible);

gfx_object_get_visible()
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   bool gfx_object_get_visible(gfx_object_t *obj);

gfx_object_get_pos()
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_get_pos(gfx_object_t *obj, gfx_coord_t *x, gfx_coord_t *y);

gfx_object_get_size()
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_get_size(gfx_object_t *obj, uint16_t *w, uint16_t *h);

gfx_object_set_touch_cb()
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_set_touch_cb(gfx_object_t *obj, gfx_object_touch_cb_t cb, void *user_data);

gfx_object_get_trace_id()
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   uint32_t gfx_object_get_trace_id(gfx_object_t *obj);

gfx_object_get_class_name()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   const char * gfx_object_get_class_name(gfx_object_t *obj);

gfx_object_get_trace_tag()
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   const char * gfx_object_get_trace_tag(gfx_object_t *obj);

gfx_object_delete()
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_object_delete(gfx_object_t *obj);
