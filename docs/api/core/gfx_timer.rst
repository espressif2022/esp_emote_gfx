Timer System (gfx_timer)
==========================

The timer system provides high-resolution timers for animations and timed callbacks.

Types
-----

gfx_timer_handle_t
~~~~~~~~~~~~~~~~~~~

Timer handle type for external use.

.. code-block:: c

   typedef void *gfx_timer_handle_t;

gfx_timer_cb_t
~~~~~~~~~~~~~~~

Timer callback function type.

.. code-block:: c

   typedef void (*gfx_timer_cb_t)(void *user_data);

Functions
---------

Timer Creation and Deletion
~~~~~~~~~~~~~~~~~~~~~~~~~~~

gfx_timer_create()
~~~~~~~~~~~~~~~~~~

Create a new timer.

.. code-block:: c

   gfx_timer_handle_t gfx_timer_create(void *handle, gfx_timer_cb_t timer_cb, uint32_t period, void *user_data);

**Parameters:**

* ``handle`` - Player handle
* ``timer_cb`` - Timer callback function
* ``period`` - Timer period in milliseconds
* ``user_data`` - User data passed to callback

**Returns:**

* Timer handle on success, NULL on error

**Example:**

.. code-block:: c

   void my_timer_callback(void *user_data) {
       ESP_LOGI(TAG, "Timer fired!");
   }

   gfx_timer_handle_t timer = gfx_timer_create(handle, my_timer_callback, 1000, NULL);

gfx_timer_delete()
~~~~~~~~~~~~~~~~~~

Delete a timer.

.. code-block:: c

   void gfx_timer_delete(void *handle, gfx_timer_handle_t timer);

**Parameters:**

* ``handle`` - Player handle
* ``timer`` - Timer handle to delete

Timer Control
~~~~~~~~~~~~~

gfx_timer_pause()
~~~~~~~~~~~~~~~~~

Pause a timer.

.. code-block:: c

   void gfx_timer_pause(gfx_timer_handle_t timer);

**Parameters:**

* ``timer`` - Timer handle to pause

gfx_timer_resume()
~~~~~~~~~~~~~~~~~~

Resume a paused timer.

.. code-block:: c

   void gfx_timer_resume(gfx_timer_handle_t timer);

**Parameters:**

* ``timer`` - Timer handle to resume

gfx_timer_reset()
~~~~~~~~~~~~~~~~~

Reset a timer (restart from beginning).

.. code-block:: c

   void gfx_timer_reset(gfx_timer_handle_t timer);

**Parameters:**

* ``timer`` - Timer handle to reset

gfx_timer_is_running()
~~~~~~~~~~~~~~~~~~~~~~

Check if a timer is running.

.. code-block:: c

   bool gfx_timer_is_running(gfx_timer_handle_t timer_handle);

**Parameters:**

* ``timer_handle`` - Timer handle to check

**Returns:**

* True if timer is running, false otherwise

Timer Configuration
~~~~~~~~~~~~~~~~~~~~

gfx_timer_set_period()
~~~~~~~~~~~~~~~~~~~~~~

Set timer period.

.. code-block:: c

   void gfx_timer_set_period(gfx_timer_handle_t timer, uint32_t period);

**Parameters:**

* ``timer`` - Timer handle to modify
* ``period`` - New period in milliseconds

gfx_timer_set_repeat_count()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set timer repeat count.

.. code-block:: c

   void gfx_timer_set_repeat_count(gfx_timer_handle_t timer, int32_t repeat_count);

**Parameters:**

* ``timer`` - Timer handle to modify
* ``repeat_count`` - Number of times to repeat (-1 for infinite)

**Example:**

.. code-block:: c

   // Run timer 5 times
   gfx_timer_set_repeat_count(timer, 5);

   // Run timer infinitely
   gfx_timer_set_repeat_count(timer, -1);

System Tick Functions
~~~~~~~~~~~~~~~~~~~~~

gfx_timer_tick_get()
~~~~~~~~~~~~~~~~~~~~

Get current system tick.

.. code-block:: c

   uint32_t gfx_timer_tick_get(void);

**Returns:**

* Current tick value in milliseconds

gfx_timer_tick_elaps()
~~~~~~~~~~~~~~~~~~~~~~

Calculate elapsed time since previous tick.

.. code-block:: c

   uint32_t gfx_timer_tick_elaps(uint32_t prev_tick);

**Parameters:**

* ``prev_tick`` - Previous tick value

**Returns:**

* Elapsed time in milliseconds

**Example:**

.. code-block:: c

   uint32_t start = gfx_timer_tick_get();
   // ... do some work ...
   uint32_t elapsed = gfx_timer_tick_elaps(start);
   ESP_LOGI(TAG, "Work took %lu ms", elapsed);

gfx_timer_get_actual_fps()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get actual FPS from timer manager.

.. code-block:: c

   uint32_t gfx_timer_get_actual_fps(void *handle);

**Parameters:**

* ``handle`` - Player handle

**Returns:**

* Actual FPS value, 0 if handle is invalid

