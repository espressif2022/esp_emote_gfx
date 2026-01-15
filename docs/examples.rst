Examples
========

This section provides comprehensive examples demonstrating various features of ESP Emote GFX.

Initialization (core + display + optional touch)
------------------------------------------------

Initialize the graphics core, add a display with flush callback, and optionally add touch. Widgets are created on the display (``gfx_disp_t *disp``).

.. code-block:: c

   #include "gfx.h"
   #include "esp_check.h"
   #include "esp_log.h"

   static const char *TAG = "gfx_app";
   static gfx_handle_t gfx_handle = NULL;
   static gfx_disp_t *gfx_disp = NULL;

   static void disp_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
   {
       esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
       esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
   }

   static void touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
   {
       ESP_LOGD(TAG, "touch type %d at (%d, %d)", event->type, event->x, event->y);
   }

   esp_err_t init_gfx(esp_lcd_panel_handle_t panel_handle, esp_lcd_touch_handle_t touch_handle)
   {
       esp_err_t ret = ESP_OK;

       gfx_core_config_t gfx_cfg = {
           .fps = 30,
           .task = GFX_EMOTE_INIT_CONFIG(),
       };
       gfx_handle = gfx_emote_init(&gfx_cfg);
       ESP_GOTO_ON_FALSE(gfx_handle != NULL, ESP_FAIL, err_out, TAG, "Failed to init GFX");

       gfx_disp_config_t disp_cfg = {
           .h_res = 320,
           .v_res = 240,
           .flush_cb = disp_flush_callback,
           .update_cb = NULL,
           .user_data = (void *)panel_handle,
           .flags = { .swap = true },
           .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = 320 * 16 },
       };
       gfx_disp = gfx_disp_add(gfx_handle, &disp_cfg);
       ESP_GOTO_ON_FALSE(gfx_disp != NULL, ESP_FAIL, err_gfx, TAG, "Failed to add display");

       if (touch_handle) {
           gfx_touch_config_t touch_cfg = {
               .handle = touch_handle,
               .event_cb = touch_event_cb,
               .disp = gfx_disp,
               .poll_ms = 50,
               .user_data = NULL,
           };
           if (gfx_touch_add(gfx_handle, &touch_cfg) == NULL) {
               ESP_LOGW(TAG, "Touch add failed");
           }
       }
       return ESP_OK;
   err_gfx:
       gfx_emote_deinit(gfx_handle);
       gfx_handle = NULL;
   err_out:
       return ret;
   }

Basic Examples
--------------

Simple Label
~~~~~~~~~~~~

Create and display a simple text label on a display (``disp`` from ``gfx_disp_add``):

.. code-block:: c

   #include "gfx.h"

   void setup_label(gfx_disp_t *disp)
   {
       gfx_obj_t *label = gfx_label_create(disp);
       gfx_label_set_text(label, "Hello, World!");
       gfx_obj_set_pos(label, 50, 50);
       gfx_label_set_color(label, GFX_COLOR_HEX(0xFF0000));
       gfx_disp_refresh_all(disp);
   }

Image Display
~~~~~~~~~~~~~

Display an image:

.. code-block:: c

   #include "gfx.h"

   void setup_image(gfx_disp_t *disp)
   {
       gfx_obj_t *img = gfx_img_create(disp);
       extern const gfx_image_dsc_t my_image;
       gfx_img_set_src(img, (void *)&my_image);
       gfx_obj_align(img, GFX_ALIGN_CENTER, 0, 0);
   }

Advanced Examples
-----------------

Multiple Widgets
~~~~~~~~~~~~~~~~

Create and manage multiple widgets on the same display:

.. code-block:: c

   #include "gfx.h"

   void setup_widgets(gfx_disp_t *disp)
   {
       gfx_obj_t *label = gfx_label_create(disp);
       gfx_label_set_text(label, "Status: OK");
       gfx_obj_set_pos(label, 10, 10);

       gfx_obj_t *img = gfx_img_create(disp);
       extern const gfx_image_dsc_t icon;
       gfx_img_set_src(img, (void *)&icon);
       gfx_obj_set_pos(img, 10, 50);

       gfx_obj_t *anim = gfx_anim_create(disp);
       gfx_anim_set_src(anim, anim_data, anim_size);
       gfx_obj_set_size(anim, 100, 100);
       gfx_obj_set_pos(anim, 150, 50);
       gfx_anim_set_segment(anim, 0, 10, 30, true);
       gfx_anim_start(anim);
   }

Touch and object callback (e.g. drag)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Register a per-object touch callback so the object receives PRESS/MOVE/RELEASE (e.g. for dragging):

.. code-block:: c

   #include "gfx.h"

   static int32_t drag_off_x, drag_off_y;

   static void obj_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
   {
       gfx_coord_t ox, oy;
       gfx_obj_get_pos(obj, &ox, &oy);
       if (event->type == GFX_TOUCH_EVENT_PRESS) {
           drag_off_x = (int32_t)event->x - ox;
           drag_off_y = (int32_t)event->y - oy;
       }
       if (event->type == GFX_TOUCH_EVENT_MOVE) {
           gfx_obj_set_pos(obj, (int32_t)event->x - drag_off_x, (int32_t)event->y - drag_off_y);
       }
   }

   void make_draggable_label(gfx_disp_t *disp)
   {
       gfx_obj_t *label = gfx_label_create(disp);
       gfx_label_set_text(label, "Drag me");
       gfx_obj_set_pos(label, 50, 50);
       gfx_obj_set_touch_cb(label, obj_touch_cb, NULL);
   }

Text Scrolling
~~~~~~~~~~~~~~

Create a scrolling text label (see widget API for ``gfx_label_set_long_mode``, ``gfx_label_set_scroll_speed``, etc.):

.. code-block:: c

   #include "gfx.h"

   void setup_scroll_label(gfx_disp_t *disp)
   {
       gfx_obj_t *label = gfx_label_create(disp);
       gfx_label_set_text(label, "This is a very long text that will scroll horizontally");
       gfx_obj_set_size(label, 200, 30);
       gfx_obj_set_pos(label, 10, 100);
       gfx_label_set_long_mode(label, GFX_LABEL_LONG_SCROLL);
       gfx_label_set_scroll_speed(label, 30);
       gfx_label_set_scroll_loop(label, true);
   }

Timer-Based Updates
~~~~~~~~~~~~~~~~~~~

Use the graphics timer to update widgets periodically. Timers are created on the **handle**:

.. code-block:: c

   #include "gfx.h"

   static gfx_obj_t *label = NULL;
   static int counter = 0;

   static void timer_callback(void *user_data)
   {
       gfx_handle_t handle = (gfx_handle_t)user_data;
       gfx_emote_lock(handle);
       if (label) {
           gfx_label_set_text_fmt(label, "Counter: %d", counter++);
       }
       gfx_emote_unlock(handle);
   }

   void setup_timer_label(gfx_handle_t handle, gfx_disp_t *disp)
   {
       label = gfx_label_create(disp);
       gfx_obj_set_pos(label, 50, 50);
       gfx_timer_create(handle, timer_callback, 1000, handle);
   }

QR Code Generation
~~~~~~~~~~~~~~~~~~

Generate and display a QR code:

.. code-block:: c

   #include "gfx.h"

   void setup_qrcode(gfx_disp_t *disp)
   {
       gfx_obj_t *qrcode = gfx_qrcode_create(disp);
       gfx_qrcode_set_data(qrcode, "https://www.espressif.com");
       gfx_qrcode_set_size(qrcode, 200);
       gfx_qrcode_set_ecc(qrcode, GFX_QRCODE_ECC_MEDIUM);
       gfx_obj_align(qrcode, GFX_ALIGN_CENTER, 0, 0);
   }

Thread-Safe Operations
~~~~~~~~~~~~~~~~~~~~~~

When modifying widgets from another task, always use the graphics lock (on the **handle**):

.. code-block:: c

   #include "gfx.h"

   void update_widgets_from_task(gfx_handle_t handle)
   {
       gfx_emote_lock(handle);
       gfx_label_set_text(label, "Updated from task");
       gfx_obj_set_pos(img, new_x, new_y);
       gfx_emote_unlock(handle);
   }

Complete Application Example
-----------------------------

Initialization (core + one display), then create a label and refresh:

.. code-block:: c

   #include "gfx.h"
   #include "esp_log.h"

   static const char *TAG = "gfx_app";
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

       gfx_obj_t *title = gfx_label_create(gfx_disp);
       gfx_label_set_text(title, "ESP Emote GFX");
       gfx_obj_align(title, GFX_ALIGN_TOP_MID, 0, 10);
       gfx_label_set_color(title, GFX_COLOR_HEX(0x0000FF));

       gfx_disp_refresh_all(gfx_disp);
       ESP_LOGI(TAG, "GFX application started");
   }

For more examples, see the test applications in the ``test_apps/`` directory.
