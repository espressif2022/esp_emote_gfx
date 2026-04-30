/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DRAW_LABEL
#include "common/gfx_config_internal.h"
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "core/runtime/gfx_core_priv.h"

#include "widget/gfx_label.h"
#include "widget/font/gfx_font_priv.h"
#include "widget/label/gfx_draw_label_priv.h"

/*********************
 *      DEFINES
 *********************/

static const char *TAG = "draw_label";

#define CHECK_OBJ_TYPE_LABEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LABEL, TAG)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct gfx_glyph_atlas_page {
    uint8_t *buf;                       /* Backing storage for packed glyph alpha data. */
    size_t capacity;                    /* Total bytes available in this atlas page. */
    size_t used;                        /* Bytes already consumed by cached glyph bitmaps. */
    int ref_count;                      /* Number of glyph entries still pointing into this page. */
    struct gfx_glyph_atlas_page *next;  /* Next atlas page owned by the same font cache. */
} gfx_glyph_atlas_page_t;

/* Cached glyph record for a single Unicode code point. Bitmap data can live
 * either in a shared atlas page or in a dedicated fallback allocation. */
typedef struct gfx_label_glyph_cache_entry {
    uint32_t unicode;                           /* Unicode code point used as the cache key. */
    gfx_glyph_dsc_t glyph_dsc;                 /* Cached glyph metrics returned by the font adapter. */
    int advance_width;                          /* Precomputed advance width in pixels for layout reuse. */
    uint8_t *alpha_bitmap;                      /* Pointer to cached 8-bit alpha bitmap data. */
    size_t alpha_bitmap_size;                   /* Bitmap size in bytes for memory accounting. */
    bool alpha_in_atlas;                        /* True when alpha_bitmap points into a shared atlas page. */
    gfx_glyph_atlas_page_t *atlas_page;         /* Owning atlas page when alpha_in_atlas is true. */
    bool available;                             /* False when the glyph lookup failed and only a miss is cached. */
    struct gfx_label_glyph_cache_entry *next;   /* Next glyph entry in the per-font LRU-style list. */
} gfx_label_glyph_cache_entry_t;

/* Shared per-font cache. It owns the glyph entry list, atlas pages, memory
 * accounting, and lightweight hit/miss statistics for tuning. */
typedef struct gfx_font_glyph_cache {
    void *font_key;                             /* Stable identity of the font adapter backing this cache. */
    int ref_count;                              /* Number of labels currently sharing this font cache. */
    gfx_label_glyph_cache_entry_t *glyphs;      /* Head of the cached glyph entry list. */
    int glyph_count;                            /* Number of glyph entries currently retained. */
    size_t glyph_bitmap_bytes;                  /* Total bytes occupied by glyph alpha data only. */
    size_t allocated_bytes;                     /* Full memory tracked for atlas pages and fallback buffers. */
    gfx_glyph_atlas_page_t *atlas_pages;        /* Linked list of atlas pages owned by this cache. */
    uint32_t access_count;                      /* Total glyph lookup attempts. */
    uint32_t hit_count;                         /* Cache hits served without rebuilding glyph data. */
    uint32_t miss_count;                        /* Cache misses that required a font backend lookup. */
    uint32_t atlas_alloc_count;                 /* Number of atlas-backed bitmap placements. */
    uint32_t fallback_alloc_count;              /* Number of standalone bitmap allocations. */
    uint32_t evict_count;                       /* Number of glyph entries evicted for budget control. */
    struct gfx_font_glyph_cache *next;          /* Next shared font cache in the global cache list. */
} gfx_font_glyph_cache_t;

/* Streaming line iterator used during label layout/render so long text can be
 * processed incrementally without building a persistent line cache. */
typedef struct {
    const char *cursor;  /* Current scan position in the original UTF-8 text. */
    const char *start;   /* Start of the current logical line segment. */
    const char *end;     /* End of the current logical line segment. */
    int width;           /* Measured pixel width of the current line segment. */
} gfx_label_line_iter_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static gfx_font_glyph_cache_t *s_font_glyph_caches;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int gfx_utf8_to_unicode(const char **p, uint32_t *unicode);
static int32_t gfx_calculate_snap_offset(gfx_label_t *label, gfx_font_handle_t font,
        int32_t current_offset, int32_t target_width);
static void gfx_update_scroll_state(gfx_obj_t *obj);
static size_t gfx_get_mask_buffer_size(const gfx_obj_t *obj);
static int gfx_get_glyph_advance_width(gfx_font_handle_t font, uint32_t unicode);
static void gfx_glyph_cache_promote(gfx_font_glyph_cache_t *cache, gfx_label_glyph_cache_entry_t *prev,
                                    gfx_label_glyph_cache_entry_t *entry);
static gfx_font_glyph_cache_t *gfx_font_cache_acquire(gfx_font_handle_t font);
static void gfx_font_cache_release(gfx_font_handle_t font);
static void gfx_glyph_cache_clear(gfx_font_glyph_cache_t *cache);
static void gfx_glyph_cache_trim(gfx_font_glyph_cache_t *cache, size_t incoming_alloc_size);
static size_t gfx_glyph_cache_get_page_capacity(size_t bitmap_size);
static uint8_t *gfx_glyph_cache_alloc_atlas(gfx_font_glyph_cache_t *cache, size_t bitmap_size,
        gfx_glyph_atlas_page_t **out_page);
static void gfx_glyph_cache_log_stats(const gfx_font_glyph_cache_t *cache, const char *reason);
static esp_err_t gfx_get_cached_glyph(gfx_font_handle_t font, uint32_t unicode,
                                      gfx_label_glyph_cache_entry_t **out_entry);
static int gfx_calculate_text_width(gfx_font_handle_t font, const char *text, const char *text_end);
static bool gfx_label_line_iter_next(gfx_obj_t *obj, gfx_font_handle_t font, gfx_label_line_iter_t *iter);

static inline gfx_font_handle_t gfx_label_get_font_handle(const gfx_label_t *label)
{
    return (label != NULL) ? label->font.handle : NULL;
}
static esp_err_t gfx_render_text_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, int line_height, int total_line_height);

void gfx_label_clear_glyph_cache(gfx_label_t *label)
{
    gfx_font_handle_t font_handle = gfx_label_get_font_handle(label);
    if (label == NULL || font_handle == NULL) {
        return;
    }

    gfx_font_cache_release(font_handle);
}

/**
 * @brief Convert UTF-8 string to Unicode code point for LVGL font processing
 * @param p Pointer to the current position in the string (updated after conversion)
 * @param unicode Pointer to store the Unicode code point
 * @return Number of bytes consumed from the string, or 0 on error
 */
static int gfx_utf8_to_unicode(const char **p, uint32_t *unicode)
{
    const char *ptr = *p;
    uint8_t c = (uint8_t) * ptr;
    int bytes_in_char = 1;

    if (c < 0x80) {
        *unicode = c;
    } else if ((c & 0xE0) == 0xC0) {
        bytes_in_char = 2;
        if (*(ptr + 1) == 0) {
            return 0;
        }
        *unicode = ((c & 0x1F) << 6) | (*(ptr + 1) & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        bytes_in_char = 3;
        if (*(ptr + 1) == 0 || *(ptr + 2) == 0) {
            return 0;
        }
        *unicode = ((c & 0x0F) << 12) | ((*(ptr + 1) & 0x3F) << 6) | (*(ptr + 2) & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        bytes_in_char = 4;
        if (*(ptr + 1) == 0 || *(ptr + 2) == 0 || *(ptr + 3) == 0) {
            return 0;
        }
        *unicode = ((c & 0x07) << 18) | ((*(ptr + 1) & 0x3F) << 12) |
                   ((*(ptr + 2) & 0x3F) << 6) | (*(ptr + 3) & 0x3F);
    } else {
        *unicode = 0xFFFD;
        bytes_in_char = 1;
    }

    *p += bytes_in_char;
    return bytes_in_char;
}

/**
 * @brief Calculate snap offset aligned to character/word boundary
 * @param label Label context
 * @param font Font context
 * @param current_offset Current scroll offset
 * @param target_width Target width to display (usually obj->geometry.width)
 * @return Snap offset aligned to character/word boundary
 */
static int32_t gfx_calculate_snap_offset(gfx_label_t *label, gfx_font_handle_t font,
        int32_t current_offset, int32_t target_width)
{
    if (!label->text.text || !font) {
        return target_width;
    }

    const char *text = label->text.text;
    int accumulated_width = 0;
    const char *p = text;

    /* Skip characters until we reach current_offset */
    while (*p && accumulated_width < current_offset) {
        uint32_t unicode = 0;
        int bytes_in_char = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_in_char == 0) {
            p++;
            continue;
        }

        int char_width = gfx_get_glyph_advance_width(font, unicode);
        accumulated_width += char_width;
    }

    /* Reset for calculating snap offset from current position */
    int section_width = 0;
    int last_valid_width = 0;
    int last_space_width = 0;  /* Width at last space (word boundary) */

    /* Calculate how many complete characters fit in target_width */
    while (*p) {
        uint32_t unicode = 0;
        const char *p_before = p;
        int bytes_in_char = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_in_char == 0) {
            p++;
            continue;
        }

        if (*p_before == '\n') {
            break;
        }

        uint8_t c = (uint8_t) * p_before;
        int char_width = gfx_get_glyph_advance_width(font, unicode);

        /* Check if adding this character would exceed target_width */
        if (section_width + char_width > target_width) {
            /* Prefer to break at word boundary (space) if available */
            if (last_space_width > 0) {
                last_valid_width = last_space_width;
            }
            /* Otherwise use last complete character */
            break;
        }

        section_width += char_width;
        last_valid_width = section_width;

        /* Record position after space for word boundary */
        if (c == ' ') {
            last_space_width = section_width;
        }
    }

    /* Return the width of complete characters that fit */
    return last_valid_width > 0 ? last_valid_width : target_width;
}

void gfx_label_scroll_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    if (!obj || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (!label || !label->scroll.scrolling || label->text.long_mode != GFX_LABEL_LONG_SCROLL) {
        return;
    }

    // means don't fetch glphy dsc
    if (label->scroll.offset != label->render.offset) {
        return;
    }
    label->scroll.offset += label->scroll.step;

    if (label->scroll.loop) {
        if (label->scroll.offset > label->text.text_width) {
            label->scroll.offset = -obj->geometry.width;
        }
    } else {
        if (label->scroll.offset > label->text.text_width) {
            label->scroll.scrolling = false;
            gfx_timer_pause(label->scroll.timer);
            return;
        }
    }

    gfx_obj_invalidate(obj);
}

void gfx_label_snap_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    if (!obj || obj->type != GFX_OBJ_TYPE_LABEL) {
        return;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (!label || label->text.long_mode != GFX_LABEL_LONG_SCROLL_SNAP) {
        return;
    }

    gfx_font_handle_t font = gfx_label_get_font_handle(label);
    if (!font) {
        return;
    }

    /* Calculate snap offset aligned to character boundary */
    int32_t aligned_offset = gfx_calculate_snap_offset(label, font, label->snap.offset, obj->geometry.width);

    /* If no valid offset found, use default */
    if (aligned_offset == 0) {
        aligned_offset = obj->geometry.width;
    }

    /* Jump to next section */
    label->snap.offset += aligned_offset;
    GFX_LOGD(TAG, "aligned_offset: %" PRId32 ", text_width: %" PRId32 ", snap_offset: %" PRId32,
             label->snap.offset - aligned_offset, label->text.text_width, label->snap.offset);

    /* Handle looping */
    if (label->snap.loop) {
        if (label->snap.offset >= label->text.text_width) {
            label->snap.offset = 0;
        }
    } else {
        if (label->snap.offset >= label->text.text_width) {
            label->snap.offset = label->text.text_width - obj->geometry.width;
            if (label->snap.offset < 0) {
                label->snap.offset = 0;
            }
            gfx_timer_pause(label->snap.timer);
        }
    }

    /* Trigger redraw */
    gfx_obj_invalidate(obj);
}

static void gfx_update_scroll_state(gfx_obj_t *obj)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;

    /* Handle smooth scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL && label->text.text_width > obj->geometry.width) {
        if (!label->scroll.scrolling) {
            label->scroll.scrolling = true;
            if (label->scroll.timer) {
                gfx_timer_reset(label->scroll.timer);
                gfx_timer_resume(label->scroll.timer);
            }
        }
    } else if (label->scroll.scrolling) {
        label->scroll.scrolling = false;
        if (label->scroll.timer) {
            gfx_timer_pause(label->scroll.timer);
        }
        label->scroll.offset = 0;
    }

    /* Handle snap scroll mode */
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->text.text_width > obj->geometry.width) {
        /* snap_offset will be dynamically calculated in timer callback based on character boundaries */
        if (label->snap.timer) {
            gfx_timer_reset(label->snap.timer);
            gfx_timer_resume(label->snap.timer);
        }
    } else if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && label->snap.timer) {
        gfx_timer_pause(label->snap.timer);
        label->snap.offset = 0;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static size_t gfx_get_mask_buffer_size(const gfx_obj_t *obj)
{
    return (size_t)obj->geometry.width * (size_t)obj->geometry.height;
}

static void gfx_glyph_cache_promote(gfx_font_glyph_cache_t *cache, gfx_label_glyph_cache_entry_t *prev,
                                    gfx_label_glyph_cache_entry_t *entry)
{
    if (cache == NULL || prev == NULL || entry == NULL || cache->glyphs == entry) {
        return;
    }

    prev->next = entry->next;
    entry->next = cache->glyphs;
    cache->glyphs = entry;
}

static gfx_font_glyph_cache_t *gfx_font_cache_acquire(gfx_font_handle_t font)
{
    if (font == NULL || font->font == NULL) {
        return NULL;
    }

    if (font->glyph_cache != NULL) {
        gfx_font_glyph_cache_t *cache = font->glyph_cache;
        cache->ref_count++;
        return cache;
    }

    gfx_font_glyph_cache_t *cache = s_font_glyph_caches;
    while (cache != NULL) {
        if (cache->font_key == font->font) {
            cache->ref_count++;
            font->glyph_cache = cache;
            return cache;
        }
        cache = cache->next;
    }

    cache = calloc(1, sizeof(*cache));
    if (cache == NULL) {
        return NULL;
    }

    cache->font_key = font->font;
    cache->ref_count = 1;
    cache->next = s_font_glyph_caches;
    s_font_glyph_caches = cache;
    font->glyph_cache = cache;
    return cache;
}

static void gfx_font_cache_release(gfx_font_handle_t font)
{
    if (font == NULL || font->glyph_cache == NULL) {
        return;
    }

    gfx_font_glyph_cache_t *cache = font->glyph_cache;
    font->glyph_cache = NULL;

    if (--cache->ref_count > 0) {
        return;
    }

    gfx_font_glyph_cache_t *prev_cache = NULL;
    gfx_font_glyph_cache_t *iter = s_font_glyph_caches;
    while (iter != NULL && iter != cache) {
        prev_cache = iter;
        iter = iter->next;
    }

    if (iter == NULL) {
        return;
    }

    if (prev_cache != NULL) {
        prev_cache->next = cache->next;
    } else {
        s_font_glyph_caches = cache->next;
    }

    gfx_glyph_cache_log_stats(cache, "release");
    gfx_glyph_cache_clear(cache);
    free(cache);
}

static void gfx_glyph_cache_clear(gfx_font_glyph_cache_t *cache)
{
    if (cache == NULL) {
        return;
    }

    gfx_label_glyph_cache_entry_t *entry = cache->glyphs;
    while (entry != NULL) {
        gfx_label_glyph_cache_entry_t *next = entry->next;
        if (!entry->alpha_in_atlas) {
            free(entry->alpha_bitmap);
        }
        free(entry);
        entry = next;
    }

    gfx_glyph_atlas_page_t *page = cache->atlas_pages;
    while (page != NULL) {
        gfx_glyph_atlas_page_t *next_page = page->next;
        free(page->buf);
        free(page);
        page = next_page;
    }

    cache->glyphs = NULL;
    cache->glyph_count = 0;
    cache->glyph_bitmap_bytes = 0;
    cache->allocated_bytes = 0;
    cache->atlas_pages = NULL;
}

static void gfx_glyph_cache_log_stats(const gfx_font_glyph_cache_t *cache, const char *reason)
{
    if (cache == NULL) {
        return;
    }

    GFX_LOGD(TAG,
             "glyph-cache[%s] font=%p access=%lu hit=%lu miss=%lu hit_rate=%lu%% atlas_alloc=%lu fallback_alloc=%lu evict=%lu glyphs=%d bytes=%u",
             reason ? reason : "stats",
             cache->font_key,
             (unsigned long)cache->access_count,
             (unsigned long)cache->hit_count,
             (unsigned long)cache->miss_count,
             (unsigned long)((cache->access_count > 0) ? ((cache->hit_count * 100U) / cache->access_count) : 0U),
             (unsigned long)cache->atlas_alloc_count,
             (unsigned long)cache->fallback_alloc_count,
             (unsigned long)cache->evict_count,
             cache->glyph_count,
             (unsigned int)cache->allocated_bytes);
}

static size_t gfx_glyph_cache_get_page_capacity(size_t bitmap_size)
{
    size_t page_capacity = bitmap_size;

    if (page_capacity < GFX_LABEL_GLYPH_ATLAS_PAGE_BYTES) {
        page_capacity = GFX_LABEL_GLYPH_ATLAS_PAGE_BYTES;
    } else {
        page_capacity = (page_capacity + 255U) & ~((size_t)255U);
    }

    return page_capacity;
}

static void gfx_glyph_cache_trim(gfx_font_glyph_cache_t *cache, size_t incoming_alloc_size)
{
    if (cache == NULL) {
        return;
    }

    while (cache->glyphs != NULL &&
            (cache->glyph_count >= GFX_LABEL_GLYPH_CACHE_MAX_ENTRIES ||
             cache->allocated_bytes + incoming_alloc_size > GFX_LABEL_GLYPH_CACHE_MAX_BITMAP_BYTES)) {
        gfx_label_glyph_cache_entry_t *prev = NULL;
        gfx_label_glyph_cache_entry_t *entry = cache->glyphs;

        while (entry->next != NULL) {
            prev = entry;
            entry = entry->next;
        }

        if (prev != NULL) {
            prev->next = NULL;
        } else {
            cache->glyphs = NULL;
        }

        cache->glyph_bitmap_bytes -= entry->alpha_bitmap_size;
        if (entry->alpha_in_atlas) {
            gfx_glyph_atlas_page_t *page = entry->atlas_page;
            if (page != NULL && --page->ref_count == 0) {
                gfx_glyph_atlas_page_t *prev_page = NULL;
                gfx_glyph_atlas_page_t *iter_page = cache->atlas_pages;
                while (iter_page != NULL && iter_page != page) {
                    prev_page = iter_page;
                    iter_page = iter_page->next;
                }

                if (iter_page != NULL) {
                    if (prev_page != NULL) {
                        prev_page->next = page->next;
                    } else {
                        cache->atlas_pages = page->next;
                    }
                    cache->allocated_bytes -= page->capacity;
                    free(page->buf);
                    free(page);
                }
            }
        } else {
            cache->allocated_bytes -= entry->alpha_bitmap_size;
            free(entry->alpha_bitmap);
        }
        free(entry);
        cache->glyph_count--;
        cache->evict_count++;
    }
}

static uint8_t *gfx_glyph_cache_alloc_atlas(gfx_font_glyph_cache_t *cache, size_t bitmap_size,
        gfx_glyph_atlas_page_t **out_page)
{
    if (cache == NULL || out_page == NULL || bitmap_size == 0) {
        return NULL;
    }

    gfx_glyph_atlas_page_t *page = cache->atlas_pages;
    while (page != NULL) {
        if (page->capacity - page->used >= bitmap_size) {
            uint8_t *buf = page->buf + page->used;
            page->used += bitmap_size;
            page->ref_count++;
            *out_page = page;
            return buf;
        }
        page = page->next;
    }

    size_t page_capacity = gfx_glyph_cache_get_page_capacity(bitmap_size);
    if (page_capacity > GFX_LABEL_GLYPH_CACHE_MAX_BITMAP_BYTES) {
        return NULL;
    }

    gfx_glyph_cache_trim(cache, page_capacity);
    if (cache->allocated_bytes + page_capacity > GFX_LABEL_GLYPH_CACHE_MAX_BITMAP_BYTES) {
        return NULL;
    }

    page = calloc(1, sizeof(*page));
    if (page == NULL) {
        return NULL;
    }

    page->buf = malloc(page_capacity);
    if (page->buf == NULL) {
        free(page);
        return NULL;
    }

    page->capacity = page_capacity;
    page->used = bitmap_size;
    page->ref_count = 1;
    page->next = cache->atlas_pages;
    cache->atlas_pages = page;
    cache->allocated_bytes += page_capacity;
    *out_page = page;

    return page->buf;
}

static esp_err_t gfx_get_cached_glyph(gfx_font_handle_t font, uint32_t unicode,
                                      gfx_label_glyph_cache_entry_t **out_entry)
{
    gfx_font_glyph_cache_t *cache = font ? font->glyph_cache : NULL;
    if (cache == NULL) {
        cache = gfx_font_cache_acquire(font);
    }
    ESP_RETURN_ON_FALSE(cache != NULL, ESP_ERR_NO_MEM, TAG, "no mem for shared glyph cache");

    gfx_label_glyph_cache_entry_t *prev = NULL;
    gfx_label_glyph_cache_entry_t *entry = cache->glyphs;
    cache->access_count++;

    while (entry != NULL) {
        if (entry->unicode == unicode) {
            gfx_glyph_cache_promote(cache, prev, entry);
            cache->hit_count++;
            if ((cache->access_count % 64U) == 0U) {
                gfx_glyph_cache_log_stats(cache, "periodic");
            }
            *out_entry = (prev != NULL) ? cache->glyphs : entry;
            return ESP_OK;
        }
        prev = entry;
        entry = entry->next;
    }

    cache->miss_count++;

    entry = calloc(1, sizeof(*entry));
    ESP_RETURN_ON_FALSE(entry != NULL, ESP_ERR_NO_MEM, TAG, "no mem for glyph cache entry");

    entry->unicode = unicode;
    entry->available = font->get_glyph_dsc(font, &entry->glyph_dsc, unicode, 0);

    if (entry->available) {
        entry->advance_width = font->get_advance_width(font, &entry->glyph_dsc);
        const uint8_t *glyph_bitmap = font->get_glyph_bitmap(font, unicode, &entry->glyph_dsc);
        size_t bitmap_size = (size_t)entry->glyph_dsc.box_w * (size_t)entry->glyph_dsc.box_h;

        if (glyph_bitmap != NULL && bitmap_size > 0) {
            gfx_glyph_atlas_page_t *atlas_page = NULL;
            entry->alpha_bitmap = gfx_glyph_cache_alloc_atlas(cache, bitmap_size, &atlas_page);
            if (entry->alpha_bitmap != NULL) {
                entry->alpha_in_atlas = true;
                entry->atlas_page = atlas_page;
                cache->atlas_alloc_count++;
            } else {
                gfx_glyph_cache_trim(cache, bitmap_size);
                if (cache->allocated_bytes + bitmap_size > GFX_LABEL_GLYPH_CACHE_MAX_BITMAP_BYTES) {
                    free(entry);
                    return ESP_ERR_NO_MEM;
                }

                entry->alpha_bitmap = malloc(bitmap_size);
                if (entry->alpha_bitmap == NULL) {
                    free(entry);
                    return ESP_ERR_NO_MEM;
                }
                cache->allocated_bytes += bitmap_size;
                cache->fallback_alloc_count++;
            }

            for (uint16_t y = 0; y < entry->glyph_dsc.box_h; y++) {
                for (uint16_t x = 0; x < entry->glyph_dsc.box_w; x++) {
                    entry->alpha_bitmap[y * entry->glyph_dsc.box_w + x] =
                        font->get_pixel_value(font, glyph_bitmap, x, y, entry->glyph_dsc.box_w);
                }
            }

            entry->alpha_bitmap_size = bitmap_size;
            cache->glyph_bitmap_bytes += bitmap_size;
        }
    }

    entry->next = cache->glyphs;
    cache->glyphs = entry;
    cache->glyph_count++;
    if ((cache->access_count % 64U) == 0U || cache->miss_count <= 4U) {
        gfx_glyph_cache_log_stats(cache, "insert");
    }
    *out_entry = entry;
    return ESP_OK;
}

static int gfx_get_glyph_advance_width(gfx_font_handle_t font, uint32_t unicode)
{
    gfx_label_glyph_cache_entry_t *entry = NULL;

    if (gfx_get_cached_glyph(font, unicode, &entry) != ESP_OK || entry == NULL || !entry->available) {
        return 0;
    }

    return entry->advance_width;
}

static int gfx_calculate_text_width(gfx_font_handle_t font, const char *text, const char *text_end)
{
    int text_width = 0;
    const char *p = text;

    while (*p && (text_end == NULL || p < text_end)) {
        uint32_t unicode = 0;
        const char *char_start = p;
        int bytes_consumed = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_consumed == 0) {
            p++;
            continue;
        }

        if (*char_start == '\n') {
            break;
        }

        text_width += gfx_get_glyph_advance_width(font, unicode);
    }

    return text_width;
}

static bool gfx_label_line_iter_next(gfx_obj_t *obj, gfx_font_handle_t font, gfx_label_line_iter_t *iter)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    const char *start = iter->cursor;

    if (start == NULL || *start == '\0') {
        return false;
    }

    if (label->text.long_mode == GFX_LABEL_LONG_WRAP) {
        const char *cursor = start;
        const char *wrap_end = start;
        const char *last_space = NULL;
        int accumulated_width = 0;

        while (*cursor) {
            uint32_t unicode = 0;
            const char *char_start = cursor;
            int bytes_consumed = gfx_utf8_to_unicode(&cursor, &unicode);
            if (bytes_consumed == 0) {
                cursor++;
                continue;
            }

            if (*char_start == '\n') {
                break;
            }

            int char_width = gfx_get_glyph_advance_width(font, unicode);
            if (accumulated_width + char_width > obj->geometry.width) {
                if (last_space != NULL && last_space > start) {
                    wrap_end = last_space;
                }
                break;
            }

            accumulated_width += char_width;
            wrap_end = cursor;

            if (*char_start == ' ') {
                last_space = char_start;
            }
        }

        if (wrap_end == start && *cursor != '\0' && *cursor != '\n') {
            uint32_t unicode = 0;
            const char *forced_end = start;
            if (gfx_utf8_to_unicode(&forced_end, &unicode) > 0) {
                wrap_end = forced_end;
            }
        }

        iter->start = start;
        iter->end = wrap_end;
        iter->width = gfx_calculate_text_width(font, start, wrap_end);
        iter->cursor = wrap_end;
    } else {
        const char *cursor = start;

        while (*cursor && *cursor != '\n') {
            cursor++;
        }

        iter->start = start;
        iter->end = cursor;
        iter->width = gfx_calculate_text_width(font, start, cursor);
        iter->cursor = cursor;
    }

    while (*iter->cursor == ' ' || *iter->cursor == '\n') {
        iter->cursor++;
    }

    return (iter->end > start);
}

static int gfx_cal_text_start_x(gfx_text_align_t align, int obj_width, int line_width)
{
    int start_x = 0;

    switch (align) {
    case GFX_TEXT_ALIGN_LEFT:
    case GFX_TEXT_ALIGN_AUTO:
        start_x = 0;
        break;
    case GFX_TEXT_ALIGN_CENTER:
        start_x = (obj_width - line_width) / 2;
        break;
    case GFX_TEXT_ALIGN_RIGHT:
        start_x = obj_width - line_width;
        break;
    }

    return start_x < 0 ? 0 : start_x;
}

static void gfx_render_glyph_to_mask(gfx_opa_t *mask, int obj_width, int obj_height,
                                     gfx_font_handle_t font,
                                     const gfx_glyph_dsc_t *glyph_dsc,
                                     const uint8_t *glyph_bitmap, int x, int y)
{
    int ofs_x = glyph_dsc->ofs_x;
    int ofs_y = font->adjust_baseline_offset(font, (void *)glyph_dsc);

    for (int32_t iy = 0; iy < glyph_dsc->box_h; iy++) {
        for (int32_t ix = 0; ix < glyph_dsc->box_w; ix++) {
            int32_t pixel_x = ix + x + ofs_x;
            int32_t pixel_y = iy + y + ofs_y;

            if (pixel_x >= 0 && pixel_x < obj_width && pixel_y >= 0 && pixel_y < obj_height) {
                uint8_t pixel_value = glyph_bitmap[iy * glyph_dsc->box_w + ix];
                *(mask + pixel_y * obj_width + pixel_x) = pixel_value;
            }
        }
    }
}

static esp_err_t gfx_render_line_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, const gfx_label_line_iter_t *line, int y_pos)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    gfx_font_handle_t font = gfx_label_get_font_handle(label);

    int start_x = gfx_cal_text_start_x(label->style.text_align, obj->geometry.width, line->width);

    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL && label->scroll.scrolling) {
        start_x -= label->render.offset;

    } else if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        start_x -= label->render.offset;
    }

    /* For snap mode, find the last complete word that fits in viewport */
    const char *render_end = NULL;
    if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP) {
        int scan_x = start_x;
        const char *p_scan = line->start;
        const char *last_space_ptr = NULL;
        const char *last_valid_ptr = NULL;

        while (p_scan < line->end && *p_scan) {
            uint32_t unicode = 0;
            const char *p_before = p_scan;
            int bytes_consumed = gfx_utf8_to_unicode(&p_scan, &unicode);
            if (bytes_consumed == 0) {
                p_scan++;
                continue;
            }

            uint8_t c = (uint8_t) * p_before;
            gfx_label_glyph_cache_entry_t *glyph_entry = NULL;
            if (gfx_get_cached_glyph(font, unicode, &glyph_entry) == ESP_OK &&
                    glyph_entry != NULL && glyph_entry->available) {
                int char_width = glyph_entry->advance_width;

                /* Check if this character would go beyond viewport */
                if (scan_x + char_width > obj->geometry.width) {
                    /* Use last space position if available, otherwise last complete character */
                    render_end = last_space_ptr ? last_space_ptr : last_valid_ptr;
                    break;
                }

                scan_x += char_width;
                last_valid_ptr = p_scan;

                /* Track space positions for word boundary */
                if (c == ' ') {
                    last_space_ptr = p_scan;
                }
            }
        }

        /* If we scanned everything without exceeding, render all */
        if (!render_end) {
            render_end = p_scan;
        }
    }

    int x = start_x;
    const char *p = line->start;

    while (p < line->end && *p) {
        /* In snap mode, stop at calculated end position */
        if (label->text.long_mode == GFX_LABEL_LONG_SCROLL_SNAP && render_end && p >= render_end) {
            break;
        }

        uint32_t unicode = 0;
        int bytes_consumed = gfx_utf8_to_unicode(&p, &unicode);
        if (bytes_consumed == 0) {
            p++;
            continue;
        }

        gfx_label_glyph_cache_entry_t *glyph_entry = NULL;
        if (gfx_get_cached_glyph(font, unicode, &glyph_entry) != ESP_OK ||
                glyph_entry == NULL || !glyph_entry->available) {
            continue;
        }

        if (glyph_entry->glyph_dsc.box_w > 0 && glyph_entry->glyph_dsc.box_h > 0 &&
                glyph_entry->alpha_bitmap == NULL) {
            continue;
        }

        gfx_render_glyph_to_mask(mask, obj->geometry.width, obj->geometry.height, font,
                                 &glyph_entry->glyph_dsc, glyph_entry->alpha_bitmap, x, y_pos);

        x += glyph_entry->advance_width;

        if (x >= obj->geometry.width) {
            break;
        }
    }

    return ESP_OK;
}

static esp_err_t gfx_render_text_to_mask(gfx_obj_t *obj, gfx_opa_t *mask, int line_height, int total_line_height)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    gfx_font_handle_t font = gfx_label_get_font_handle(label);
    int current_y = 0;
    int max_lines = obj->geometry.height / total_line_height;
    gfx_label_line_iter_t line_iter = {
        .cursor = label->text.text,
        .start = NULL,
        .end = NULL,
        .width = 0,
    };

    if (max_lines <= 0) {
        max_lines = 1;
    }

    label->text.text_width = gfx_calculate_text_width(font, label->text.text, NULL);

    for (int line_idx = 0; line_idx < max_lines && line_iter.cursor != NULL && *line_iter.cursor != '\0'; line_idx++) {
        if (current_y + line_height > obj->geometry.height) {
            break;
        }

        if (!gfx_label_line_iter_next(obj, font, &line_iter)) {
            break;
        }

        gfx_render_line_to_mask(obj, mask, &line_iter, current_y);

        current_y += total_line_height;
    }

    return ESP_OK;
}

static gfx_opa_t *gfx_allocate_mask_buffer(gfx_obj_t *obj)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    size_t required_size = gfx_get_mask_buffer_size(obj);

    if (required_size == 0) {
        return NULL;
    }

    if (label->render.mask == NULL || label->render.mask_capacity < required_size) {
        gfx_opa_t *new_mask = (gfx_opa_t *)realloc(label->render.mask, required_size);
        if (new_mask == NULL) {
            GFX_LOGE(TAG, "Failed to allocate mask buffer");
            return NULL;
        }

        label->render.mask = new_mask;
        label->render.mask_capacity = required_size;
    }

    memset(label->render.mask, 0x00, required_size);
    return label->render.mask;
}

static esp_err_t gfx_render_parse(gfx_obj_t *obj, gfx_opa_t *mask)
{
    gfx_label_t *label = (gfx_label_t *)obj->src;
    gfx_font_handle_t font = gfx_label_get_font_handle(label);
    int line_height = font->get_line_height(font);
    int total_line_height = line_height + label->text.line_spacing;

    return gfx_render_text_to_mask(obj, mask, line_height, total_line_height);
}

esp_err_t gfx_get_glphy_dsc(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (!obj->state.dirty) {
        return ESP_OK;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    gfx_font_handle_t font = gfx_label_get_font_handle(label);
    if (font == NULL) {
        GFX_LOGD(TAG, "font adapter is NULL");
        return ESP_OK;
    }

    gfx_opa_t *mask_buf = gfx_allocate_mask_buffer(obj);
    ESP_RETURN_ON_FALSE(mask_buf, ESP_ERR_NO_MEM, TAG, "no mem for mask_buf");

    esp_err_t render_ret;
    render_ret = gfx_render_parse(obj, mask_buf);

    if (render_ret != ESP_OK) {
        return render_ret;
    }

    label->render.mask = mask_buf;
    obj->state.dirty = false;

    gfx_update_scroll_state(obj);

    return ESP_OK;
}

/**
 * @brief Blend label object to destination buffer
 *
 * @param obj Graphics object containing label data
 * @param x1 Left boundary of destination area
 * @param y1 Top boundary of destination area
 * @param x2 Right boundary of destination area
 * @param y2 Bottom boundary of destination area
 * @param dest_buf Destination buffer for blending
 */
esp_err_t gfx_draw_label(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    if (!obj || !ctx) {
        GFX_LOGE(TAG, "invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_label_t *label = (gfx_label_t *)obj->src;
    if (label->text.text == NULL) {
        GFX_LOGD(TAG, "text is NULL");
        return ESP_OK;
    }

    gfx_obj_calc_pos_in_parent(obj);

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + obj->geometry.width, obj->geometry.y + obj->geometry.height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect_exclusive(&clip_area, &render_area, &obj_area)) {
        return ESP_OK;
    }

    if (label->style.bg_enable) {
        gfx_color_t bg_color = {
            .full = gfx_color_to_native_u16(label->style.bg_color, ctx->swap),
        };
        for (int y = clip_area.y1; y < clip_area.y2; y++) {
            for (int x = clip_area.x1; x < clip_area.x2; x++) {
                gfx_color_t *dest_pixels = GFX_DRAW_CTX_DEST_PTR(ctx, x, y);
                *dest_pixels = bg_color;
            }
        }
    }

    if (!label->render.mask) {
        return ESP_OK;
    }

    gfx_color_t *dest_pixels = GFX_DRAW_CTX_DEST_PTR(ctx, clip_area.x1, clip_area.y1);
    gfx_coord_t mask_stride = obj->geometry.width;
    gfx_opa_t *mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(label->render.mask,
                      clip_area.y1 - obj->geometry.y,
                      mask_stride,
                      clip_area.x1 - obj->geometry.x);

    gfx_color_t color = label->style.color;

    gfx_sw_blend_draw(dest_pixels, ctx->stride, mask, mask_stride, &clip_area, color, label->style.opa, ctx->swap);
    return ESP_OK;
}
