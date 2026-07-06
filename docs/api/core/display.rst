Display (display)
=================

Functions
---------

gfx_display_add()
~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_display_t * gfx_display_add(gfx_handle_t handle, const gfx_display_config_t *cfg);

gfx_display_delete()
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void gfx_display_delete(gfx_display_t *display);

gfx_display_refresh_all()
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void gfx_display_refresh_all(gfx_display_t *display);

gfx_display_flush_ready()
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   bool gfx_display_flush_ready(gfx_display_t *display);

gfx_display_get_user_data()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void * gfx_display_get_user_data(gfx_display_t *display);

gfx_display_get_h_res()
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   uint32_t gfx_display_get_h_res(gfx_display_t *display);

gfx_display_get_v_res()
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   uint32_t gfx_display_get_v_res(gfx_display_t *display);

gfx_display_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_display_set_bg_color(gfx_display_t *display, gfx_color_t color);

gfx_display_set_bg_enable()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_display_set_bg_enable(gfx_display_t *display, bool enable);

gfx_display_is_flushing_last()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   bool gfx_display_is_flushing_last(gfx_display_t *display);

gfx_display_get_perf_stats()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_err_t gfx_display_get_perf_stats(gfx_display_t *display, gfx_display_perf_stats_t *out_stats);
