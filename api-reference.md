## API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg)` | Initialize graphics context |
| `void gfx_emote_deinit(gfx_handle_t handle)` | Deinitialize graphics context |
| `bool gfx_emote_flush_ready(gfx_handle_t handle, bool swap_act_buf)` | Check if flush is ready |
| `esp_err_t gfx_emote_get_screen_size(gfx_handle_t handle, uint32_t *width, uint32_t *height)` | Get screen dimensions |
| `esp_err_t gfx_emote_lock(gfx_handle_t handle)` | Lock render mutex |
| `esp_err_t gfx_emote_unlock(gfx_handle_t handle)` | Unlock render mutex |
| `esp_err_t gfx_emote_set_bg_color(gfx_handle_t handle, gfx_color_t color)` | Set default background color |
| `bool gfx_emote_is_flushing_last(gfx_handle_t handle)` | Check if flushing last block |

### Object Management

| Function | Description |
|----------|-------------|
| `void gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y)` | Set object position |
| `void gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h)` | Set object size |
| `void gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)` | Align object relative to screen |
| `void gfx_obj_set_visible(gfx_obj_t *obj, bool visible)` | Set object visibility |
| `bool gfx_obj_get_visible(gfx_obj_t *obj)` | Get object visibility |
| `void gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y)` | Get object position |
| `void gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h)` | Get object size |
| `void gfx_obj_delete(gfx_obj_t *obj)` | Delete object |

### Label Widget

| Function | Description |
|----------|-------------|
| `gfx_obj_t * gfx_label_create(gfx_handle_t handle)` | Create a label object |
| `esp_err_t gfx_label_new_font(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)` | Create new FreeType font |
| `esp_err_t gfx_label_delete_font(gfx_font_t font)` | Delete font and free resources |
| `esp_err_t gfx_label_set_text(gfx_obj_t *obj, const char *text)` | Set label text |
| `esp_err_t gfx_label_set_text_fmt(gfx_obj_t * obj, const char * fmt, ...)` | Set formatted text |
| `esp_err_t gfx_label_set_color(gfx_obj_t *obj, gfx_color_t color)` | Set text color |
| `esp_err_t gfx_label_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color)` | Set background color |
| `esp_err_t gfx_label_set_bg_enable(gfx_obj_t *obj, bool enable)` | Enable/disable background |
| `esp_err_t gfx_label_set_opa(gfx_obj_t *obj, gfx_opa_t opa)` | Set opacity (0-255) |
| `esp_err_t gfx_label_set_font(gfx_obj_t *obj, gfx_font_t font)` | Set font |
| `esp_err_t gfx_label_set_text_align(gfx_obj_t *obj, gfx_text_align_t align)` | Set text alignment |
| `esp_err_t gfx_label_set_long_mode(gfx_obj_t *obj, gfx_label_long_mode_t long_mode)` | Set long text handling mode |
| `esp_err_t gfx_label_set_line_spacing(gfx_obj_t *obj, uint16_t spacing)` | Set line spacing |
| `esp_err_t gfx_label_set_scroll_speed(gfx_obj_t *obj, uint32_t speed_ms)` | Set scrolling speed |
| `esp_err_t gfx_label_set_scroll_loop(gfx_obj_t *obj, bool loop)` | Set continuous scrolling |

### Image Widget

| Function | Description |
|----------|-------------|
| `gfx_obj_t * gfx_img_create(gfx_handle_t handle)` | Create an image object |
| `gfx_obj_t * gfx_img_set_src(gfx_obj_t *obj, void *src)` | Set image source data (RGB565A8 format) |

### Animation Widget

| Function | Description |
|----------|-------------|
| `gfx_obj_t * gfx_anim_create(gfx_handle_t handle)` | Create animation object |
| `esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len)` | Set animation source data |
| `esp_err_t gfx_anim_set_segment(gfx_obj_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat)` | Configure animation segment |
| `esp_err_t gfx_anim_start(gfx_obj_t *obj)` | Start animation playback |
| `esp_err_t gfx_anim_stop(gfx_obj_t *obj)` | Stop animation playback |
| `esp_err_t gfx_anim_set_mirror(gfx_obj_t *obj, bool enabled, int16_t offset)` | Set mirror display |
| `esp_err_t gfx_anim_set_auto_mirror(gfx_obj_t *obj, bool enabled)` | Set auto mirror alignment |

### Timer System

| Function | Description |
|----------|-------------|
| `gfx_timer_handle_t gfx_timer_create(void *handle, gfx_timer_cb_t timer_cb, uint32_t period, void *user_data)` | Create a new timer |
| `void gfx_timer_delete(void *handle, gfx_timer_handle_t timer)` | Delete a timer |
| `void gfx_timer_pause(gfx_timer_handle_t timer)` | Pause timer |
| `void gfx_timer_resume(gfx_timer_handle_t timer)` | Resume timer |
| `void gfx_timer_set_repeat_count(gfx_timer_handle_t timer, int32_t repeat_count)` | Set repeat count (-1 for infinite) |
| `void gfx_timer_set_period(gfx_timer_handle_t timer, uint32_t period)` | Set timer period (ms) |
| `void gfx_timer_reset(gfx_timer_handle_t timer)` | Reset timer |
| `uint32_t gfx_timer_tick_get(void)` | Get current system tick |
| `uint32_t gfx_timer_tick_elaps(uint32_t prev_tick)` | Calculate elapsed time |
| `uint32_t gfx_timer_get_actual_fps(void *handle)` | Get actual FPS |

### Utilities

| Function | Description |
|----------|-------------|
| `gfx_color_t gfx_color_hex(uint32_t c)` | Convert hex color to gfx_color_t |

