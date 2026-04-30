Motion Driver (gfx_motion)
==========================

Types
-----

gfx_motion_cfg_t
~~~~~~~~~~~~~

.. code-block:: c

   typedef struct gfx_motion_cfg_t {
       uint16_t timer_period_ms;
       int16_t damping_div;
   } gfx_motion_cfg_t;

gfx_motion_tick_cb_t
~~~~~~~~~~~~~~~~~

Callback executed on each motion timer tick. Return ``true`` when state changed and an apply pass is needed.

.. code-block:: c

   typedef bool (*gfx_motion_tick_cb_t)(gfx_motion_t *motion, void *user_data);

gfx_motion_apply_cb_t
~~~~~~~~~~~~~~~~~~

Callback that pushes the current motion state into display objects.

.. code-block:: c

   typedef esp_err_t (*gfx_motion_apply_cb_t)(gfx_motion_t *motion, void *user_data, bool force_apply);

Functions
---------

gfx_motion_cfg_init()
~~~~~~~~~~~~~~~~~~

Initialize a motion config with timer period and damping divisor.

.. code-block:: c

   void gfx_motion_cfg_init(gfx_motion_cfg_t *cfg, uint16_t timer_period_ms, int16_t damping_div);

gfx_motion_init()
~~~~~~~~~~~~~~

Create and start a motion driver bound to a display and anchor object.

.. code-block:: c

   esp_err_t gfx_motion_init(gfx_motion_t *motion,
                          gfx_disp_t *disp,
                          gfx_obj_t *anchor,
                          const gfx_motion_cfg_t *cfg,
                          gfx_motion_tick_cb_t tick_cb,
                          gfx_motion_apply_cb_t apply_cb,
                          void *user_data);

gfx_motion_deinit()
~~~~~~~~~~~~~~~~

Stop and destroy the motion driver.

.. code-block:: c

   void gfx_motion_deinit(gfx_motion_t *motion);

gfx_motion_set_period()
~~~~~~~~~~~~~~~~~~~~

Change the timer period of a running motion driver.

.. code-block:: c

   esp_err_t gfx_motion_set_period(gfx_motion_t *motion, uint16_t period_ms);

gfx_motion_step()
~~~~~~~~~~~~~~

Run one motion tick immediately.

.. code-block:: c

   esp_err_t gfx_motion_step(gfx_motion_t *motion, bool force_apply);

gfx_motion_ease_i16()
~~~~~~~~~~~~~~~~~~

Utility helper for damped integer interpolation.

.. code-block:: c

   int16_t gfx_motion_ease_i16(int16_t cur, int16_t tgt, int16_t div);
