Quick Start Guide
=================

This guide covers the minimum setup needed to initialize ESP Emote GFX, attach
a display, and create a first widget.

Installation
------------

Add ESP Emote GFX to your project as a component. The component is available
through the ESP Component Registry.

Basic Setup
-----------

1. Include the main header:

.. code-block:: c

   #include "gfx.h"

2. Initialize the graphics core (no display yet):

.. code-block:: c

   gfx_core_config_t gfx_cfg = {
       .fps = 30,
       .task = GFX_CORE_TASK_DEFAULT_CONFIG()
   };
   gfx_handle_t handle = gfx_core_init(&gfx_cfg);
   if (handle == NULL) {
       ESP_LOGE(TAG, "Failed to initialize GFX");
       return;
   }

3. Add a display with a flush callback:

.. code-block:: c

   void disp_flush_callback(gfx_display_t *disp, int x1, int y1, int x2, int y2, const void *data)
   {
       void *panel = gfx_display_get_user_data(disp);
       // Send RGB565 data (x1,y1)-(x2,y2) to your panel, e.g. esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
   }

   gfx_display_config_t disp_cfg = {
       .h_res = 320,
       .v_res = 240,
       .flush_cb = disp_flush_callback,
       .update_cb = NULL,
       .user_data = your_panel_handle,   // e.g. esp_lcd_panel_handle_t
       .flags = { .swap = true },
       .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = 320 * 16 },
   };
   gfx_display_t *disp = gfx_display_add(handle, &disp_cfg);
   if (disp == NULL) {
       ESP_LOGE(TAG, "Failed to add display");
       gfx_core_deinit(handle);
       return;
   }

4. (Optional) Register panel IO callback so the framework knows when flush is done:

.. code-block:: c

   static bool flush_io_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
   {
       gfx_display_t *disp = (gfx_display_t *)user_ctx;
       if (disp) {
           gfx_display_flush_ready(disp, true);
       }
       return true;
   }
   const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = flush_io_ready };
   esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, disp);

5. (Optional) Use a backend object instead of a raw flush callback:

.. code-block:: c

   gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
       .h_res = 320,
       .v_res = 240,
   });

   gfx_display_config_t sim_disp_cfg = {
       .h_res = 320,
       .v_res = 240,
       .backend = backend,
       .buffers = { .buf_pixels = 320 * 16 },
   };
   gfx_display_t *sim_disp = gfx_display_add(handle, &sim_disp_cfg);

After ``gfx_display_add()`` succeeds, the display owns the backend. Use
``gfx_memory_backend_delete()`` only if the backend was not added to a display.

6. (Optional) Add touch input:

.. code-block:: c

   void touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
   {
       // Handle PRESS / MOVE / RELEASE; event->x and event->y are display coordinates.
   }

   gfx_touch_config_t touch_cfg = {
       .driver_handle = esp_lcd_touch_handle,   // from your BSP or esp_lcd_touch_new
       .event_cb = touch_event_cb,
       .disp = disp,
       .poll_ms = 50,
       .user_data = NULL,
   };
   gfx_touch_t *touch = gfx_touch_add(handle, &touch_cfg);

Creating Your First Widget
--------------------------

Widgets are created on a **display** (``gfx_display_t *``), not on the handle.

Creating a Label
~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_object_t *label = gfx_label_create(disp);
   gfx_label_set_text(label, "Hello, World!");
   gfx_object_set_pos(label, 50, 50);
   gfx_label_set_color(label, GFX_COLOR_HEX(0xFF0000));  // Red

Creating an Image
~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_object_t *img = gfx_image_create(disp);
   extern const gfx_image_dsc_t my_image;
   gfx_image_set_source_desc(img, &(gfx_image_src_t) {
       .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
       .data = &my_image,
   });
   gfx_object_set_pos(img, 100, 100);

Creating an Animation
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_object_t *anim = gfx_anim_create(disp);
   gfx_anim_set_src_desc(anim, &(gfx_anim_src_t) {
       .type = GFX_ANIM_SRC_TYPE_MEMORY,
       .data = anim_data,
       .data_len = anim_size,
   });
   gfx_object_align(anim, GFX_ALIGN_CENTER, 0, 0);
   gfx_anim_set_segment(anim, 0, 0xFFFF, 15, true);
   gfx_anim_start(anim);

Touch Callback
~~~~~~~~~~~~~~

.. code-block:: c

   void my_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
   {
       if (event->type == GFX_TOUCH_EVENT_PRESS) { /* ... */ }
       if (event->type == GFX_TOUCH_EVENT_MOVE)  { gfx_object_set_pos(obj, event->x, event->y); }
   }
   gfx_object_set_touch_cb(label, my_touch_cb, NULL);

Thread Safety
-------------

When modifying objects from outside the graphics task, use the graphics lock:

.. code-block:: c

   gfx_core_lock(handle);
   gfx_label_set_text(label, "Updated text");
   gfx_object_set_pos(img, new_x, new_y);
   gfx_core_unlock(handle);

Complete Example
----------------

.. code-block:: c

   #include "gfx.h"
   #include "esp_log.h"

   static const char *TAG = "gfx_example";
   static gfx_handle_t gfx_handle = NULL;
   static gfx_display_t *gfx_disp = NULL;

   static void disp_flush_callback(gfx_display_t *disp, int x1, int y1, int x2, int y2, const void *data)
   {
       esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_display_get_user_data(disp);
       esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
   }

   void app_main(void)
   {
       gfx_core_config_t gfx_cfg = {
           .fps = 30,
           .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
       };
       gfx_handle = gfx_core_init(&gfx_cfg);
       if (gfx_handle == NULL) {
           ESP_LOGE(TAG, "Failed to initialize GFX");
           return;
       }

       gfx_display_config_t disp_cfg = {
           .h_res = 320,
           .v_res = 240,
           .flush_cb = disp_flush_callback,
           .update_cb = NULL,
           .user_data = panel_handle,   // your esp_lcd_panel_handle_t
           .flags = { .swap = true },
           .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = 320 * 16 },
       };
       gfx_disp = gfx_display_add(gfx_handle, &disp_cfg);
       if (gfx_disp == NULL) {
           ESP_LOGE(TAG, "Failed to add display");
           gfx_core_deinit(gfx_handle);
           return;
       }

       gfx_object_t *label = gfx_label_create(gfx_disp);
       gfx_label_set_text(label, "Hello, ESP Emote GFX!");
       gfx_object_set_pos(label, 50, 50);
       gfx_label_set_color(label, GFX_COLOR_HEX(0x00FF00));

       gfx_display_refresh_all(gfx_disp);
       ESP_LOGI(TAG, "GFX application started");
   }

Next Steps
----------

* Read :doc:`examples` for runnable test app scenarios.
* Read :doc:`motion_widget` for Motion scene playback.
* Check :doc:`api/core/index` and :doc:`api/widgets/index` for API details.
