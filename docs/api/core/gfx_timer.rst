Timer System (gfx_timer)
========================

Types
-----

gfx_timer_handle_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef void *gfx_timer_handle_t;

gfx_timer_cb_t
~~~~~~~~~~~~~~

.. code-block:: c

   typedef void (*gfx_timer_cb_t)(void *);

Functions
---------

gfx_timer_create()
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_timer_handle_t gfx_timer_create(void *handle, gfx_timer_cb_t timer_cb, uint32_t period, void *user_data);

gfx_timer_delete()
~~~~~~~~~~~~~~~~~~

Delete a timer

.. code-block:: c

   void gfx_timer_delete(void *handle, gfx_timer_handle_t timer);

**Parameters:**

* ``handle`` - Player handle
* ``timer`` - Timer handle to delete

gfx_timer_pause()
~~~~~~~~~~~~~~~~~

Pause a timer

.. code-block:: c

   void gfx_timer_pause(gfx_timer_handle_t timer);

**Parameters:**

* ``timer`` - Timer handle to pause

gfx_timer_resume()
~~~~~~~~~~~~~~~~~~

Resume a timer

.. code-block:: c

   void gfx_timer_resume(gfx_timer_handle_t timer);

**Parameters:**

* ``timer`` - Timer handle to resume

gfx_timer_is_running()
~~~~~~~~~~~~~~~~~~~~~~

Check if a timer is running

.. code-block:: c

   bool gfx_timer_is_running(gfx_timer_handle_t timer_handle);

**Parameters:**

* ``timer_handle`` - Timer handle to check

**Returns:**

* true if timer is running, false otherwise

gfx_timer_set_repeat_count()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set timer repeat count

.. code-block:: c

   void gfx_timer_set_repeat_count(gfx_timer_handle_t timer, int32_t repeat_count);

**Parameters:**

* ``timer`` - Timer handle to modify
* ``repeat_count`` - Number of times to repeat (-1 for infinite)

gfx_timer_set_period()
~~~~~~~~~~~~~~~~~~~~~~

Set timer period

.. code-block:: c

   void gfx_timer_set_period(gfx_timer_handle_t timer, uint32_t period);

**Parameters:**

* ``timer`` - Timer handle to modify
* ``period`` - New period in milliseconds

gfx_timer_reset()
~~~~~~~~~~~~~~~~~

Reset a timer

.. code-block:: c

   void gfx_timer_reset(gfx_timer_handle_t timer);

**Parameters:**

* ``timer`` - Timer handle to reset

gfx_timer_tick_get()
~~~~~~~~~~~~~~~~~~~~

Get current system tick

.. code-block:: c

   uint32_t gfx_timer_tick_get(void);

**Returns:**

* Current tick value in milliseconds

gfx_timer_tick_elaps()
~~~~~~~~~~~~~~~~~~~~~~

Calculate elapsed time since previous tick

.. code-block:: c

   uint32_t gfx_timer_tick_elaps(uint32_t prev_tick);

**Parameters:**

* ``prev_tick`` - Previous tick value

**Returns:**

* Elapsed time in milliseconds

gfx_timer_get_actual_fps()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Get actual FPS from timer manager

.. code-block:: c

   uint32_t gfx_timer_get_actual_fps(void *handle);

**Parameters:**

* ``handle`` - Player handle

**Returns:**

* Actual FPS value, 0 if handle is invalid
