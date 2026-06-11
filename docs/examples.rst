Examples
========

The repository keeps executable examples in ``test_apps/``. This page is a
guide to those scenarios instead of a second copy of the Quick Start code.

Test App Menu
-------------

The test app currently exposes these cases:

* ``anim: emote generator`` - animation source and segment playback validation.
* ``image: source matrix`` - image source descriptors and RGB565/RGB565A8/RGB888/RGB888A8 input.
* ``label: bitmap font`` - bitmap/LVGL font label rendering.
* ``label: freetype font`` - FreeType font loading and text rendering.
* ``motion: rig preview`` - interactive Motion scene playback.
* ``display: multi routing`` - multiple display routing.
* ``object: multi scene`` - mixed object scene with image, label, and animation.
* ``qrcode: render preview`` - QR code generation and rendering.
* ``timer: api validation`` - graphics timer behavior.

Useful Files
------------

* ``test_apps/main/app_main.c`` - test menu registration.
* ``test_apps/main/common.c`` - display, touch, FPS label, and shared helpers.
* ``test_apps/main/test_image.c`` - image source examples.
* ``test_apps/main/test_label.c`` - label, wrapping, scrolling, and font examples.
* ``test_apps/main/test_anim.c`` - basic animation playback.
* ``test_apps/main/test_anim_emote_gen.c`` - animation segment plans and emote generator flow.
* ``test_apps/main/test_motion.c`` - Motion scene interaction and canvas movement.
* ``test_apps/main/test_qrcode.c`` - QR code setup.
* ``test_apps/main/test_timer.c`` - timer API validation.

Minimal Widget Snippets
-----------------------

Create a label:

.. code-block:: c

   gfx_obj_t *label = gfx_label_create(disp);
   gfx_label_set_text(label, "Hello");
   gfx_obj_align(label, GFX_ALIGN_CENTER, 0, 0);

Create an image:

.. code-block:: c

   extern const gfx_image_dsc_t icon;

   gfx_obj_t *img = gfx_img_create(disp);
   gfx_img_set_src(img, &icon);
   gfx_obj_align(img, GFX_ALIGN_CENTER, 0, 0);

Create a QR code:

.. code-block:: c

   gfx_obj_t *qrcode = gfx_qrcode_create(disp);
   gfx_qrcode_set_data(qrcode, "https://www.espressif.com");
   gfx_qrcode_set_size(qrcode, 160);
   gfx_obj_align(qrcode, GFX_ALIGN_CENTER, 0, 0);

For full initialization code, use :doc:`quickstart`.
