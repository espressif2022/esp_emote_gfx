Quick Start Guide
=================

This guide will help you get started with ESP Emote GFX in just a few steps.

Installation
------------

Add ESP Emote GFX to your ESP-IDF project by including it as a component. The component is available through the ESP Component Registry.

Basic Setup
-----------

1. Include the main header:

.. code-block:: c

   #include "gfx.h"

2. Initialize the graphics core (no display yet):

.. code-block:: c

   gfx_core_config_t gfx_cfg = {
       .fps = 30,
       .task = GFX_EMOTE_INIT_CONFIG()
   };
   gfx_handle_t handle = gfx_emote_init(&gfx_cfg);
   if (handle == NULL) {
       ESP_LOGE(TAG, "Failed to initialize GFX");
       return;
   }

3. Add a display with a flush callback:

.. code-block:: c

   void disp_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
   {
       void *panel = gfx_disp_get_user_data(disp);
       // Send RGB565 data (x1,y1)-(x2,y2) to your panel, e.g. esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
   }

   gfx_disp_config_t disp_cfg = {
       .h_res = 320,
       .v_res = 240,
       .flush_cb = disp_flush_callback,
       .update_cb = NULL,
       .user_data = your_panel_handle,   // e.g. esp_lcd_panel_handle_t
       .flags = { .swap = true },
       .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = 320 * 16 },
   };
   gfx_disp_t *disp = gfx_disp_add(handle, &disp_cfg);
   if (disp == NULL) {
       ESP_LOGE(TAG, "Failed to add display");
       gfx_emote_deinit(handle);
       return;
   }

4. (Optional) Register panel IO callback so the framework knows when flush is done:

.. code-block:: c

   static bool flush_io_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
   {
       gfx_disp_t *disp = (gfx_disp_t *)user_ctx;
       if (disp) {
           gfx_disp_flush_ready(disp, true);
       }
       return true;
   }
   const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = flush_io_ready };
   esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, disp);

5. (Optional) Add touch input:

.. code-block:: c

   void touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
   {
       // Handle PRESS / MOVE / RELEASE; event->x, event->y, event->hit_obj
   }

   gfx_touch_config_t touch_cfg = {
       .handle = esp_lcd_touch_handle,   // from your BSP or esp_lcd_touch_new
       .event_cb = touch_event_cb,
       .disp = disp,
       .poll_ms = 50,
       .user_data = NULL,
   };
   gfx_touch_t *touch = gfx_touch_add(handle, &touch_cfg);

Creating Your First Widget
--------------------------

Widgets are created on a **display** (``gfx_disp_t *``), not on the handle.

Creating a Label
~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_obj_t *label = gfx_label_create(disp);
   gfx_label_set_text(label, "Hello, World!");
   gfx_obj_set_pos(label, 50, 50);
   gfx_label_set_color(label, GFX_COLOR_HEX(0xFF0000));  // Red

Creating an Image
~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_obj_t *img = gfx_img_create(disp);
   extern const gfx_image_dsc_t my_image;
   gfx_img_set_src(img, (void *)&my_image);
   gfx_obj_set_pos(img, 100, 100);

Creating an Animation
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_obj_t *anim = gfx_anim_create(disp);
   gfx_anim_set_src(anim, anim_data, anim_size);
   gfx_obj_align(anim, GFX_ALIGN_CENTER, 0, 0);
   gfx_anim_set_segment(anim, 0, 0xFFFF, 15, true);
   gfx_anim_start(anim);

Object touch callback (e.g. drag)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void my_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
   {
       if (event->type == GFX_TOUCH_EVENT_PRESS) { /* ... */ }
       if (event->type == GFX_TOUCH_EVENT_MOVE)  { gfx_obj_set_pos(obj, event->x, event->y); }
   }
   gfx_obj_set_touch_cb(label, my_touch_cb, NULL);

Thread Safety
-------------

When modifying objects from outside the graphics task, use the graphics lock:

.. code-block:: c

   gfx_emote_lock(handle);
   gfx_label_set_text(label, "Updated text");
   gfx_obj_set_pos(img, new_x, new_y);
   gfx_emote_unlock(handle);

Complete Example
----------------

.. code-block:: c

   #include "gfx.h"
   #include "esp_log.h"

   static const char *TAG = "gfx_example";
   static gfx_handle_t gfx_handle = NULL;
   static gfx_disp_t *gfx_disp = NULL;

   static void disp_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
   {
       esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
       esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
   }

   void app_main(void)
   {
       gfx_core_config_t gfx_cfg = {
           .fps = 30,
           .task = GFX_EMOTE_INIT_CONFIG(),
       };
       gfx_handle = gfx_emote_init(&gfx_cfg);
       if (gfx_handle == NULL) {
           ESP_LOGE(TAG, "Failed to initialize GFX");
           return;
       }

       gfx_disp_config_t disp_cfg = {
           .h_res = 320,
           .v_res = 240,
           .flush_cb = disp_flush_callback,
           .update_cb = NULL,
           .user_data = panel_handle,   // your esp_lcd_panel_handle_t
           .flags = { .swap = true },
           .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = 320 * 16 },
       };
       gfx_disp = gfx_disp_add(gfx_handle, &disp_cfg);
       if (gfx_disp == NULL) {
           ESP_LOGE(TAG, "Failed to add display");
           gfx_emote_deinit(gfx_handle);
           return;
       }

       gfx_obj_t *label = gfx_label_create(gfx_disp);
       gfx_label_set_text(label, "Hello, ESP Emote GFX!");
       gfx_obj_set_pos(label, 50, 50);
       gfx_label_set_color(label, GFX_COLOR_HEX(0x00FF00));

       gfx_disp_refresh_all(gfx_disp);
       ESP_LOGI(TAG, "GFX application started");
   }

Next Steps
----------

* Read the :doc:`Core API Reference <api/core/index>` for detailed API documentation
* Check out the :doc:`Widget API Reference <api/widgets/index>` for widget-specific functions
* See :doc:`Examples <examples>` for more complex usage patterns
