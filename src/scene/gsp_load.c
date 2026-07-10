/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_gsp_load — host/device 共用 loader：u32-offset 字节包 -> gfx object 树。
 *
 * 关键：全程只把包字段当“u32 偏移 / u16 索引”解释，不做指针重定位，
 * 用 gfx_*_create + setter 工厂建树。因此不依赖 struct 布局、不区分
 * 32/64 位，同一份 buf 在两端解析一致。全程边界校验，坏包只返回错误码。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/scene/gsp.h"

#include "gfx/input.h"
#include "gfx/object.h"
#include "gfx/types.h"
#include "gfx/widgets/button.h"
#include "gfx/widgets/container.h"
#include "gfx/widgets/image.h"
#include "gfx/widgets/label.h"
#include "gfx/widgets/list.h"
#include "gfx/widgets/wheel.h"

/* 校验 off 指向的字符串在 [off,size) 内 NUL 结尾，返回指针或 NULL。*/
static const char *safe_str(const uint8_t *buf, size_t size, uint32_t off)
{
    if (off == 0 || (size_t)off >= size) {
        return NULL;
    }
    for (size_t i = off; i < size; i++) {
        if (buf[i] == 0) {
            return (const char *)(buf + off);
        }
    }
    return NULL;
}

static gfx_font_t resolve_font(uint16_t font_id, const gfx_gsp_font_binding_t *fonts,
                               size_t font_count, gfx_font_t default_font)
{
    for (size_t i = 0; i < font_count; i++) {
        if (fonts != NULL && fonts[i].id == font_id && fonts[i].font != NULL) {
            return fonts[i].font;
        }
    }
    return default_font;
}

/* 安全销毁：先把所有节点从父节点脱开（消除级联关系），再逐个删除。*/
static void destroy_all(gfx_object_t **objs, uint32_t count)
{
    if (objs == NULL) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        gfx_object_t *o = objs[i];
        if (o != NULL) {
            gfx_object_t *p = gfx_object_get_parent(o);
            if (p != NULL) {
                (void)gfx_object_remove_child(p, o);
            }
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        if (objs[i] != NULL) {
            (void)gfx_object_delete(objs[i]);
            objs[i] = NULL;
        }
    }
}

/*
 * 解压 blob[idx] 到 out->img_bufs[idx] 并填 out->img_dscs[idx]（幂等缓存）。
 * 成功返回指向 img_dscs[idx] 的指针，失败返回 NULL 并置 *err。
 */
static const gfx_image_dsc_t *blob_get(const uint8_t *buf, size_t size, uint32_t blob_off,
                                       uint32_t blob_count, uint32_t idx,
                                       gfx_gsp_scene_t *out, int *err)
{
    if (idx >= blob_count) {
        *err = GFX_GSP_ERR_BLOB;
        return NULL;
    }
    if (out->img_bufs[idx] != NULL) {
        return &out->img_dscs[idx];   /* 已解压过，直接复用 */
    }

    const uint8_t *b = buf + blob_off + (size_t)idx * GFX_GSP_BLOB_SIZE;
    const uint16_t w = gfx_gsp_rd_u16(b + 0);
    const uint16_t h = gfx_gsp_rd_u16(b + 2);
    const uint8_t  cf = b[4];
    const uint8_t  codec = b[5];
    const uint16_t stride = gfx_gsp_rd_u16(b + 6);
    const uint32_t raw_size = gfx_gsp_rd_u32(b + 8);
    const uint32_t comp_size = gfx_gsp_rd_u32(b + 12);
    const uint32_t data_off = gfx_gsp_rd_u32(b + 16);

    if ((uint64_t)data_off + comp_size > size || raw_size == 0) {
        *err = GFX_GSP_ERR_BOUNDS;
        return NULL;
    }
    const uint8_t *comp = buf + data_off;
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (raw == NULL) {
        *err = GFX_GSP_ERR_ALLOC;
        return NULL;
    }

    if (codec == GFX_GSP_CODEC_STORE) {
        if (comp_size != raw_size) {
            free(raw);
            *err = GFX_GSP_ERR_CODEC;
            return NULL;
        }
        memcpy(raw, comp, raw_size);
    } else if (codec == GFX_GSP_CODEC_RLE16) {
        uint32_t ro = 0, ci = 0;
        while (ci + 4u <= comp_size && ro + 2u <= raw_size) {
            uint32_t run = gfx_gsp_rd_u16(comp + ci);
            const uint16_t px = gfx_gsp_rd_u16(comp + ci + 2);
            ci += 4;
            while (run-- > 0 && ro + 2u <= raw_size) {
                gfx_gsp_wr_u16(raw + ro, px);
                ro += 2;
            }
        }
        if (ro != raw_size) {   /* 解出的字节数必须恰好填满 */
            free(raw);
            *err = GFX_GSP_ERR_CODEC;
            return NULL;
        }
    } else {
        free(raw);
        *err = GFX_GSP_ERR_CODEC;
        return NULL;
    }

    out->img_bufs[idx] = raw;
    gfx_image_dsc_t *dsc = &out->img_dscs[idx];
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic = GFX_IMAGE_HEADER_MAGIC;
    dsc->header.cf = cf;
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.stride = stride;
    dsc->data_size = raw_size;
    dsc->data = raw;
    return dsc;
}

static int apply_item_params(const uint8_t *buf, size_t size, const uint8_t *params,
                             uint16_t params_len, gfx_object_t *obj, bool wheel)
{
    (void)buf;
    (void)size;

    if (params == NULL || params_len == 0) {
        return GFX_GSP_OK;
    }
    if (params_len < 12u) {
        return GFX_GSP_ERR_BOUNDS;
    }

    const uint16_t item_count = gfx_gsp_rd_u16(params + 0);
    const uint16_t selected = gfx_gsp_rd_u16(params + 2);
    const uint16_t item_height = gfx_gsp_rd_u16(params + 4);
    const uint16_t rows_or_page = gfx_gsp_rd_u16(params + 6);
    const uint16_t flags = gfx_gsp_rd_u16(params + 8);
    uint32_t cursor = 12u;

    if (wheel) {
        (void)gfx_wheel_clear(obj);
        if (item_height > 0) {
            (void)gfx_wheel_set_item_height(obj, item_height);
        }
        if (rows_or_page > 0) {
            (void)gfx_wheel_set_visible_rows(obj, rows_or_page > 255u ? 255u : (uint8_t)rows_or_page);
        }
        (void)gfx_wheel_set_cyclic(obj, (flags & GFX_GSP_ITEM_PARAMS_F_CYCLIC) != 0);
    } else {
        (void)gfx_list_clear(obj);
        if (item_height > 0) {
            (void)gfx_list_set_item_height(obj, item_height);
        }
        if (rows_or_page > 0) {
            (void)gfx_list_set_items_per_page(obj, rows_or_page);
        }
        (void)gfx_list_set_snap_to_item(obj, (flags & GFX_GSP_ITEM_PARAMS_F_SNAP_TO_ITEM) != 0);
    }

    for (uint16_t i = 0; i < item_count; i++) {
        if (cursor + 2u > params_len) {
            return GFX_GSP_ERR_BOUNDS;
        }
        const uint16_t len = gfx_gsp_rd_u16(params + cursor);
        cursor += 2u;
        if ((uint32_t)cursor + len > params_len) {
            return GFX_GSP_ERR_BOUNDS;
        }
        char *item = (char *)malloc((size_t)len + 1u);
        if (item == NULL) {
            return GFX_GSP_ERR_ALLOC;
        }
        memcpy(item, params + cursor, len);
        item[len] = '\0';
        if (wheel) {
            (void)gfx_wheel_add_item(obj, item);
        } else {
            (void)gfx_list_add_item(obj, item);
        }
        free(item);
        cursor += len;
    }

    if (selected != GFX_GSP_ITEM_PARAMS_SELECTED_NONE) {
        if (wheel) {
            (void)gfx_wheel_set_selected(obj, selected);
        } else {
            (void)gfx_list_set_selected(obj, selected);
            (void)gfx_list_set_focus(obj, selected);
        }
    }
    return GFX_GSP_OK;
}

/* ------------------------------------------------------------------ */
/* v4 动作表：运行期分发（源对象 touch 回调 -> 目标对象操作）           */
/* ------------------------------------------------------------------ */

static const char *trace_type_name(uint16_t type)
{
    switch (type) {
    case GFX_GSP_OBJ_CONTAINER: return "container";
    case GFX_GSP_OBJ_LABEL: return "label";
    case GFX_GSP_OBJ_BUTTON: return "button";
    case GFX_GSP_OBJ_IMAGE: return "image";
    case GFX_GSP_OBJ_LIST: return "list";
    case GFX_GSP_OBJ_WHEEL: return "wheel";
    case GFX_GSP_OBJ_LAYER: return "layer";
    default: return "?";
    }
}

static const gfx_gsp_object_ref_t *trace_ref_from_obj(const gfx_gsp_scene_t *s, const gfx_object_t *obj)
{
    if (s == NULL || s->refs == NULL || obj == NULL) {
        return NULL;
    }
    for (uint16_t i = 0; i < s->ref_count; i++) {
        if (s->refs[i].obj == obj) {
            return &s->refs[i];
        }
    }
    return NULL;
}

static void trace_obj_event(gfx_gsp_scene_t *s, gfx_object_t *obj,
                            const char *event, const char *detail)
{
    const gfx_gsp_object_ref_t *ref = trace_ref_from_obj(s, obj);
    const char *name = (ref != NULL && ref->name != NULL) ? ref->name : "-";
    uint16_t idx = ref != NULL ? ref->obj_idx : GFX_GSP_ACT_NO_TARGET;
    uint16_t type = ref != NULL ? ref->type : 0;

    printf("[gsp-event] obj[%u] %s name=\"%s\" event=%s%s%s\n",
           idx, trace_type_name(type), name, event,
           detail != NULL && detail[0] != '\0' ? " " : "",
           detail != NULL ? detail : "");
}

static void trace_list_focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    char detail[48];

    snprintf(detail, sizeof(detail), "focused=%ld", (long)focused_index);
    trace_obj_event((gfx_gsp_scene_t *)user_data, obj, "list_focus", detail);
}

static void trace_list_select_cb(gfx_object_t *obj, int32_t selected_index,
                                 bool confirmed, void *user_data)
{
    char detail[72];

    snprintf(detail, sizeof(detail), "selected=%ld confirmed=%s",
             (long)selected_index, confirmed ? "true" : "false");
    trace_obj_event((gfx_gsp_scene_t *)user_data, obj, "list_select", detail);
}

static void trace_list_page_cb(gfx_object_t *obj, uint16_t page_index,
                               uint16_t items_per_page, void *user_data)
{
    char detail[72];

    snprintf(detail, sizeof(detail), "page=%u items_per_page=%u",
             page_index, items_per_page);
    trace_obj_event((gfx_gsp_scene_t *)user_data, obj, "list_page", detail);
}

static void trace_wheel_value_cb(gfx_object_t *obj, int32_t selected_index, void *user_data)
{
    char detail[48];

    snprintf(detail, sizeof(detail), "selected=%ld", (long)selected_index);
    trace_obj_event((gfx_gsp_scene_t *)user_data, obj, "wheel_value", detail);
}

static void trace_wheel_confirm_cb(gfx_object_t *obj, int32_t selected_index, void *user_data)
{
    char detail[48];

    snprintf(detail, sizeof(detail), "selected=%ld", (long)selected_index);
    trace_obj_event((gfx_gsp_scene_t *)user_data, obj, "wheel_confirm", detail);
}

static gfx_object_t *action_target(const gfx_gsp_scene_t *s, const gfx_gsp_action_rt_t *a)
{
    if (a->target_name != NULL) {
        return gfx_gsp_scene_find_by_name(s, a->target_name);
    }
    if (a->target_idx != GFX_GSP_ACT_NO_TARGET) {
        return gfx_gsp_scene_get_obj(s, a->target_idx);
    }
    return NULL;
}

static int show_layer_object(gfx_gsp_scene_t *s, gfx_object_t *layer)
{
    if (s == NULL || layer == NULL || s->refs == NULL) {
        return GFX_GSP_ERR_ACTION;
    }

    const gfx_gsp_object_ref_t *target = NULL;
    for (uint16_t i = 0; i < s->ref_count; i++) {
        if (s->refs[i].obj == layer) {
            target = &s->refs[i];
            break;
        }
    }
    if (target == NULL || target->type != GFX_GSP_OBJ_LAYER) {
        return GFX_GSP_ERR_TYPE;
    }

    for (uint16_t i = 0; i < s->ref_count; i++) {
        if (s->refs[i].type == GFX_GSP_OBJ_LAYER && s->refs[i].parent_idx == target->parent_idx) {
            bool next_visible = s->refs[i].obj == layer;
            bool was_visible = gfx_object_get_visible(s->refs[i].obj);

            if (was_visible != next_visible) {
                trace_obj_event(s, s->refs[i].obj,
                                next_visible ? "layer_enter" : "layer_exit", NULL);
            }
            (void)gfx_object_set_visible(s->refs[i].obj, next_visible);
        }
    }
    return GFX_GSP_OK;
}

static void action_exec(gfx_gsp_scene_t *s, const gfx_gsp_action_rt_t *a,
                        gfx_object_t *src, const gfx_touch_event_t *ev)
{
    gfx_object_t *tgt = action_target(s, a);
    const char *cn = (tgt != NULL) ? gfx_object_get_class_name(tgt) : NULL;

    switch (a->action) {
    case GFX_GSP_ACT_SHOW:
        if (tgt != NULL) {
            (void)gfx_object_set_visible(tgt, true);
        }
        break;
    case GFX_GSP_ACT_HIDE:
        if (tgt != NULL) {
            (void)gfx_object_set_visible(tgt, false);
        }
        break;
    case GFX_GSP_ACT_SET_TEXT:
        if (tgt != NULL && a->param != NULL && cn != NULL) {
            if (strcmp(cn, "label") == 0) {
                (void)gfx_label_set_text(tgt, a->param);
            } else if (strcmp(cn, "button") == 0) {
                (void)gfx_button_set_text(tgt, a->param);
            }
        }
        break;
    case GFX_GSP_ACT_SET_BG_COLOR:
        if (tgt != NULL && cn != NULL) {
            if (strcmp(cn, "container") == 0) {
                (void)gfx_container_set_bg_color(tgt, GFX_COLOR_HEX(a->arg));
            } else if (strcmp(cn, "button") == 0) {
                (void)gfx_button_set_bg_color(tgt, GFX_COLOR_HEX(a->arg));
            }
        }
        break;
    case GFX_GSP_ACT_CALL:
        if (a->target_name != NULL) {
            for (size_t k = 0; k < s->cb_count; k++) {
                if (s->cbs != NULL && s->cbs[k].name != NULL &&
                        strcmp(s->cbs[k].name, a->target_name) == 0) {
                    if (s->cbs[k].cb != NULL) {
                        s->cbs[k].cb(src, ev, s->cbs[k].user_data);
                    }
                    break;
                }
            }
        }
        break;
    case GFX_GSP_ACT_GOTO:
        if (tgt != NULL) {
            (void)show_layer_object(s, tgt);
        }
        break;
    default:
        /* TOGGLE/OPACITY/BACK 预留：运行期先忽略（已在加载期做过合法性校验）*/
        break;
    }
}

/* 把 touch 事件类型映射到 GFX_GSP_EV_*，命中返回 true。*/
static bool action_event_match(uint16_t ev_kind, const gfx_touch_event_t *ev)
{
    if (ev == NULL) {
        return false;
    }
    switch (ev_kind) {
    case GFX_GSP_EV_CLICK:
    case GFX_GSP_EV_RELEASE:
        return ev->type == GFX_TOUCH_EVENT_RELEASE;
    case GFX_GSP_EV_PRESS:
        return ev->type == GFX_TOUCH_EVENT_PRESS;
    default:
        return false;
    }
}

/* 统一 touch trampoline：先跑 GFX_GSP_F_CALLBACK 用户回调，再跑动作表。*/
static void gfx_gsp_touch_trampoline(gfx_object_t *obj, const gfx_touch_event_t *ev, void *ud)
{
    gfx_gsp_scene_t *s = (gfx_gsp_scene_t *)ud;
    int idx = -1;

    if (s == NULL || s->objs == NULL) {
        return;
    }
    for (uint16_t i = 0; i < s->obj_count; i++) {
        if (s->objs[i] == obj) {
            idx = (int)i;
            break;
        }
    }
    if (idx < 0) {
        return;
    }
    if (s->refs != NULL && s->refs[idx].user_cb != NULL) {
        s->refs[idx].user_cb(obj, ev, s->refs[idx].user_cb_data);
    }
    for (uint16_t i = 0; i < s->action_count; i++) {
        const gfx_gsp_action_rt_t *a = &s->actions[i];
        if (a->src_idx == (uint16_t)idx && action_event_match(a->event, ev)) {
            action_exec(s, a, obj, ev);
        }
    }
}

/* 解析动作表到 out->actions[]（全程边界校验）。返回 GFX_GSP_OK 或错误码。*/
static int parse_actions(const uint8_t *buf, size_t size, uint32_t total,
                         uint32_t action_count, uint32_t action_off,
                         uint32_t obj_count, gfx_gsp_scene_t *out)
{
    if (action_count == 0) {
        return GFX_GSP_OK;
    }
    if (action_count > 0xFFFFu) {
        return GFX_GSP_ERR_COUNT;
    }
    const uint64_t end = (uint64_t)action_off + (uint64_t)action_count * GFX_GSP_ACTION_SIZE;
    if (action_off < GFX_GSP_HEADER_SIZE || end > total) {
        return GFX_GSP_ERR_BOUNDS;
    }

    out->actions = (gfx_gsp_action_rt_t *)calloc(action_count, sizeof(*out->actions));
    if (out->actions == NULL) {
        return GFX_GSP_ERR_ALLOC;
    }

    for (uint32_t i = 0; i < action_count; i++) {
        const uint8_t *e = buf + action_off + (size_t)i * GFX_GSP_ACTION_SIZE;
        gfx_gsp_action_rt_t *a = &out->actions[i];

        a->src_idx     = gfx_gsp_rd_u16(e + 0);
        a->event       = gfx_gsp_rd_u16(e + 2);
        a->action      = gfx_gsp_rd_u16(e + 4);
        a->target_idx  = gfx_gsp_rd_u16(e + 6);
        const uint32_t name_off  = gfx_gsp_rd_u32(e + 8);
        const uint32_t param_off = gfx_gsp_rd_u32(e + 12);
        a->param_len   = gfx_gsp_rd_u16(e + 16);
        a->arg         = gfx_gsp_rd_u32(e + 20);

        if (a->src_idx >= obj_count) {
            return GFX_GSP_ERR_ACTION;
        }
        if (a->target_idx != GFX_GSP_ACT_NO_TARGET && a->target_idx >= obj_count) {
            return GFX_GSP_ERR_ACTION;
        }
        if (name_off != 0) {
            a->target_name = safe_str(buf, size, name_off);
            if (a->target_name == NULL) {
                return GFX_GSP_ERR_STRING;
            }
        }
        if (param_off != 0) {
            a->param = safe_str(buf, size, param_off);
            if (a->param == NULL) {
                return GFX_GSP_ERR_STRING;
            }
        }
    }
    out->action_count = (uint16_t)action_count;
    return GFX_GSP_OK;
}

int gfx_gsp_load_with_fonts(const uint8_t *buf, size_t size, gfx_display_t *disp,
                        const gfx_gsp_font_binding_t *fonts, size_t font_count,
                        gfx_font_t default_font,
                        const gfx_gsp_cb_binding_t *cbs, size_t cb_count,
                        gfx_gsp_scene_t *out)
{
    if (buf == NULL || out == NULL || disp == NULL) {
        return GFX_GSP_ERR_BOUNDS;
    }
    memset(out, 0, sizeof(*out));

    if (size < GFX_GSP_HEADER_SIZE) {
        return GFX_GSP_ERR_SIZE;
    }
    if (gfx_gsp_rd_u32(buf + 0) != GFX_GSP_MAGIC) {
        return GFX_GSP_ERR_MAGIC;
    }
    if (gfx_gsp_rd_u32(buf + 4) != GFX_GSP_VERSION) {
        return GFX_GSP_ERR_VERSION;
    }

    const uint32_t obj_count = gfx_gsp_rd_u32(buf + 16);
    const uint32_t obj_off = gfx_gsp_rd_u32(buf + 20);
    const uint32_t blob_count = gfx_gsp_rd_u32(buf + 28);
    const uint32_t blob_off = gfx_gsp_rd_u32(buf + 32);
    const uint32_t total = gfx_gsp_rd_u32(buf + 36);
    const uint32_t crc = gfx_gsp_rd_u32(buf + 40);
    const uint32_t action_count = gfx_gsp_rd_u32(buf + 44);
    const uint32_t action_off = gfx_gsp_rd_u32(buf + 48);

    out->cbs = cbs;
    out->cb_count = cb_count;

    if (total > size || total < GFX_GSP_HEADER_SIZE) {
        return GFX_GSP_ERR_BOUNDS;
    }
    if (gfx_gsp_crc32_scene(buf, total) != crc) {
        return GFX_GSP_ERR_CRC;
    }
    if (obj_count == 0 || obj_count > 0xFFFFu) {
        return GFX_GSP_ERR_COUNT;
    }
    const uint64_t obj_end = (uint64_t)obj_off + (uint64_t)obj_count * GFX_GSP_OBJ_SIZE;
    if (obj_off < GFX_GSP_HEADER_SIZE || obj_end > total) {
        return GFX_GSP_ERR_BOUNDS;
    }
    if (blob_count > 0xFFFFu) {
        return GFX_GSP_ERR_COUNT;
    }
    if (blob_count > 0) {
        const uint64_t blob_end = (uint64_t)blob_off + (uint64_t)blob_count * GFX_GSP_BLOB_SIZE;
        if (blob_off < obj_end || blob_end > total) {
            return GFX_GSP_ERR_BOUNDS;
        }
    }

    gfx_object_t **objs = (gfx_object_t **)calloc(obj_count, sizeof(*objs));
    gfx_gsp_object_ref_t *refs = (gfx_gsp_object_ref_t *)calloc(obj_count, sizeof(*refs));
    if (objs == NULL || refs == NULL) {
        free(objs);
        free(refs);
        return GFX_GSP_ERR_ALLOC;
    }
    /* blob 解压缓存（可能为空）*/
    if (blob_count > 0) {
        out->img_dscs = (gfx_image_dsc_t *)calloc(blob_count, sizeof(*out->img_dscs));
        out->img_bufs = (uint8_t **)calloc(blob_count, sizeof(*out->img_bufs));
        if (out->img_dscs == NULL || out->img_bufs == NULL) {
            free(objs);
            free(refs);
            free(out->img_dscs);
            free(out->img_bufs);
            memset(out, 0, sizeof(*out));
            return GFX_GSP_ERR_ALLOC;
        }
    }
    out->blob_count = (uint16_t)blob_count;

    int rc = GFX_GSP_OK;
    for (uint32_t i = 0; i < obj_count; i++) {
        const uint8_t *e = buf + obj_off + (size_t)i * GFX_GSP_OBJ_SIZE;

        const uint16_t type = gfx_gsp_rd_u16(e + 0);
        const uint16_t parent = gfx_gsp_rd_u16(e + 2);
        const int16_t x = gfx_gsp_rd_i16(e + 4);
        const int16_t y = gfx_gsp_rd_i16(e + 6);
        const uint16_t w = gfx_gsp_rd_u16(e + 8);
        const uint16_t h = gfx_gsp_rd_u16(e + 10);
        const uint32_t flags = gfx_gsp_rd_u32(e + 12);
        const uint32_t fg = gfx_gsp_rd_u32(e + 16);
        const uint32_t bg = gfx_gsp_rd_u32(e + 20);
        const uint32_t bc = gfx_gsp_rd_u32(e + 24);
        const uint16_t bw = gfx_gsp_rd_u16(e + 28);
        const uint16_t radius = gfx_gsp_rd_u16(e + 30);
        const uint32_t text_off = gfx_gsp_rd_u32(e + 32);
        const uint32_t cb_off = gfx_gsp_rd_u32(e + 36);
        const uint32_t name_off = gfx_gsp_rd_u32(e + 40);
        const uint32_t blob_idx = gfx_gsp_rd_u32(e + 44);
        const uint32_t params_off = gfx_gsp_rd_u32(e + 48);
        const uint16_t params_len = gfx_gsp_rd_u16(e + 52);
        const uint8_t  text_align = e[55];
        const uint16_t font_id = gfx_gsp_rd_u16(e + 56);
        const uint16_t bind_id = gfx_gsp_rd_u16(e + 58);

        /* 先序约束：父必须是更早的对象 */
        if (parent != GFX_GSP_NO_PARENT && parent >= i) {
            rc = GFX_GSP_ERR_PARENT;
            break;
        }

        const char *text = NULL;
        if (flags & GFX_GSP_F_TEXT) {
            text = safe_str(buf, size, text_off);
            if (text == NULL) {
                rc = GFX_GSP_ERR_STRING;
                break;
            }
        }
        const char *cbname = NULL;
        if (flags & GFX_GSP_F_CALLBACK) {
            cbname = safe_str(buf, size, cb_off);
            if (cbname == NULL) {
                rc = GFX_GSP_ERR_STRING;
                break;
            }
        }
        const char *name = NULL;
        if (flags & GFX_GSP_F_NAME) {
            name = safe_str(buf, size, name_off);
            if (name == NULL) {
                rc = GFX_GSP_ERR_STRING;
                break;
            }
        }
        const uint8_t *params = NULL;
        if (flags & GFX_GSP_F_PARAMS) {
            /* 私有参数块只做边界校验，具体解析交给各 widget（扩展点）*/
            if ((uint64_t)params_off + params_len > total || params_off < GFX_GSP_HEADER_SIZE) {
                rc = GFX_GSP_ERR_BOUNDS;
                break;
            }
            params = buf + params_off;
        }

        gfx_object_t *o = NULL;
        gfx_object_touch_cb_t user_cb = NULL;   /* GFX_GSP_F_CALLBACK 解析出的用户回调 */
        void *user_cb_data = NULL;
        gfx_font_t obj_font = resolve_font(font_id, fonts, font_count, default_font);
        switch (type) {
        case GFX_GSP_OBJ_CONTAINER:
        case GFX_GSP_OBJ_LAYER:
            o = gfx_container_create(disp);
            if (o != NULL) {
                if (flags & GFX_GSP_F_BG_COLOR) {
                    (void)gfx_container_set_bg_color(o, GFX_COLOR_HEX(bg));
                }
                if (flags & GFX_GSP_F_BORDER) {
                    (void)gfx_container_set_border_color(o, GFX_COLOR_HEX(bc));
                    (void)gfx_container_set_border_width(o, bw);
                }
                if (flags & GFX_GSP_F_RADIUS) {
                    (void)gfx_container_set_radius(o, radius);
                }
            }
            break;

        case GFX_GSP_OBJ_LIST:
            o = gfx_list_create(disp);
            if (o != NULL) {
                if (obj_font != NULL) {
                    (void)gfx_list_set_font(o, obj_font);
                }
                if (flags & GFX_GSP_F_BG_COLOR) {
                    (void)gfx_list_set_bg_color(o, GFX_COLOR_HEX(bg));
                }
                if (flags & GFX_GSP_F_FG_COLOR) {
                    (void)gfx_list_set_text_color(o, GFX_COLOR_HEX(fg));
                }
                if (flags & GFX_GSP_F_BORDER) {
                    (void)gfx_list_set_border_color(o, GFX_COLOR_HEX(bc));
                    (void)gfx_list_set_border_width(o, bw);
                }
                rc = apply_item_params(buf, size, params, params_len, o, false);
            }
            break;

        case GFX_GSP_OBJ_WHEEL:
            o = gfx_wheel_create(disp);
            if (o != NULL) {
                if (obj_font != NULL) {
                    (void)gfx_wheel_set_font(o, obj_font);
                }
                if (flags & GFX_GSP_F_BG_COLOR) {
                    (void)gfx_wheel_set_bg_color(o, GFX_COLOR_HEX(bg));
                }
                if (flags & GFX_GSP_F_FG_COLOR) {
                    (void)gfx_wheel_set_text_color(o, GFX_COLOR_HEX(fg));
                }
                if (flags & GFX_GSP_F_BORDER) {
                    (void)gfx_wheel_set_border_color(o, GFX_COLOR_HEX(bc));
                    (void)gfx_wheel_set_border_width(o, bw);
                }
                rc = apply_item_params(buf, size, params, params_len, o, true);
            }
            break;

        case GFX_GSP_OBJ_LABEL:
            o = gfx_label_create(disp);
            if (o != NULL) {
                if (obj_font != NULL) {
                    (void)gfx_label_set_font(o, obj_font);
                }
                if (text != NULL) {
                    (void)gfx_label_set_text(o, text);
                }
                if (flags & GFX_GSP_F_FG_COLOR) {
                    (void)gfx_label_set_color(o, GFX_COLOR_HEX(fg));
                }
                if (flags & GFX_GSP_F_ALIGN) {
                    (void)gfx_label_set_text_align(o, (gfx_text_align_t)text_align);
                }
            }
            break;

        case GFX_GSP_OBJ_BUTTON:
            o = gfx_button_create(disp);
            if (o != NULL) {
                if (obj_font != NULL) {
                    (void)gfx_button_set_font(o, obj_font);
                }
                if (text != NULL) {
                    (void)gfx_button_set_text(o, text);
                }
                if (flags & GFX_GSP_F_FG_COLOR) {
                    (void)gfx_button_set_text_color(o, GFX_COLOR_HEX(fg));
                }
                if (flags & GFX_GSP_F_BG_COLOR) {
                    (void)gfx_button_set_bg_color(o, GFX_COLOR_HEX(bg));
                }
                if (flags & GFX_GSP_F_BORDER) {
                    (void)gfx_button_set_border_color(o, GFX_COLOR_HEX(bc));
                    (void)gfx_button_set_border_width(o, bw);
                }
                if (flags & GFX_GSP_F_RADIUS) {
                    (void)gfx_button_set_radius(o, radius);
                }
                if (cbname != NULL) {
                    /* 解析出用户回调，但不直接绑定：统一交给 trampoline，
                     * 使 GFX_GSP_F_CALLBACK 与 v4 动作表能在同一控件上共存。*/
                    for (size_t k = 0; k < cb_count; k++) {
                        if (cbs != NULL && cbs[k].name != NULL &&
                                strcmp(cbs[k].name, cbname) == 0) {
                            user_cb = cbs[k].cb;
                            user_cb_data = cbs[k].user_data;
                            break;
                        }
                    }
                }
            }
            break;

        case GFX_GSP_OBJ_IMAGE:
            o = gfx_image_create(disp);
            if (o != NULL && (flags & GFX_GSP_F_IMAGE)) {
                int berr = GFX_GSP_OK;
                const gfx_image_dsc_t *dsc =
                    blob_get(buf, size, blob_off, blob_count, blob_idx, out, &berr);
                if (dsc == NULL) {
                    (void)gfx_object_delete(o);   /* 尚未入 objs[]，先自行销毁避免泄漏 */
                    o = NULL;
                    rc = berr;                     /* blob 越界 / 坏 codec */
                    break;
                }
                gfx_image_src_t src = {
                    .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
                    .data = dsc,
                };
                (void)gfx_image_set_source_desc(o, &src);
            }
            break;

        default:
            rc = GFX_GSP_ERR_TYPE;
            break;
        }

        if (rc != GFX_GSP_OK) {
            if (o != NULL) {
                (void)gfx_object_delete(o);
            }
            break;
        }
        if (o == NULL) {
            rc = GFX_GSP_ERR_CREATE;
            break;
        }

        (void)gfx_object_set_pos(o, x, y);
        (void)gfx_object_set_size(o, w, h);
        if (flags & GFX_GSP_F_HIDDEN) {
            (void)gfx_object_set_visible(o, false);
        }
        if (parent != GFX_GSP_NO_PARENT) {
            (void)gfx_object_add_child(objs[parent], o);
        }
        objs[i] = o;
        refs[i] = (gfx_gsp_object_ref_t) {
            .obj_idx = (uint16_t)i,
            .type = type,
            .parent_idx = parent,
            .bind_id = bind_id,
            .name = name,
            .obj = o,
            .user_cb = user_cb,
            .user_cb_data = user_cb_data,
        };
    }

    if (rc != GFX_GSP_OK) {
        destroy_all(objs, obj_count);
        free(objs);
        free(refs);
        for (uint32_t k = 0; k < blob_count; k++) {
            free(out->img_bufs ? out->img_bufs[k] : NULL);
        }
        free(out->img_bufs);
        free(out->img_dscs);
        memset(out, 0, sizeof(*out));
        return rc;
    }

    out->objs = objs;
    out->obj_count = (uint16_t)obj_count;
    out->refs = refs;
    out->ref_count = (uint16_t)obj_count;
    out->root = objs[0];

    for (uint16_t i = 0; i < out->ref_count; i++) {
        if (out->refs[i].type == GFX_GSP_OBJ_LIST) {
            (void)gfx_list_set_focus_cb(out->refs[i].obj, trace_list_focus_cb, out);
            (void)gfx_list_set_select_cb(out->refs[i].obj, trace_list_select_cb, out);
            (void)gfx_list_set_page_load_cb(out->refs[i].obj, trace_list_page_cb, out);
        } else if (out->refs[i].type == GFX_GSP_OBJ_WHEEL) {
            (void)gfx_wheel_set_value_cb(out->refs[i].obj, trace_wheel_value_cb, out);
            (void)gfx_wheel_set_confirm_cb(out->refs[i].obj, trace_wheel_confirm_cb, out);
        }
    }

    /* v4：解析动作表（坏包只返回错误码）*/
    rc = parse_actions(buf, size, total, action_count, action_off, obj_count, out);
    if (rc != GFX_GSP_OK) {
        gfx_gsp_scene_free(out);   /* 释放已建好的树/refs/blobs/actions */
        return rc;
    }

    /* 安装统一 touch trampoline：凡有用户回调或作为动作源的对象都挂上，
     * 由 trampoline 依次分发用户回调 + 动作表，二者共存。*/
    for (uint16_t i = 0; i < out->obj_count; i++) {
        bool needs = (out->refs[i].user_cb != NULL);
        for (uint16_t k = 0; !needs && k < out->action_count; k++) {
            if (out->actions[k].src_idx == i) {
                needs = true;
            }
        }
        if (needs) {
            (void)gfx_object_set_touch_cb(out->objs[i], gfx_gsp_touch_trampoline, out);
        }
    }
    return GFX_GSP_OK;
}

int gfx_gsp_load(const uint8_t *buf, size_t size, gfx_display_t *disp, gfx_font_t font,
             const gfx_gsp_cb_binding_t *cbs, size_t cb_count, gfx_gsp_scene_t *out)
{
    const gfx_gsp_font_binding_t default_binding = {
        .id = 0,
        .font = font,
    };

    return gfx_gsp_load_with_fonts(buf, size, disp, &default_binding, font != NULL ? 1U : 0U,
                               font, cbs, cb_count, out);
}

void gfx_gsp_scene_free(gfx_gsp_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->objs != NULL) {
        destroy_all(scene->objs, scene->obj_count);
        free(scene->objs);
    }
    free(scene->refs);
    free(scene->actions);
    for (uint32_t k = 0; k < scene->blob_count; k++) {
        free(scene->img_bufs ? scene->img_bufs[k] : NULL);
    }
    free(scene->img_bufs);
    free(scene->img_dscs);
    memset(scene, 0, sizeof(*scene));
}

gfx_object_t *gfx_gsp_scene_get_obj(const gfx_gsp_scene_t *scene, uint16_t index)
{
    if (scene == NULL || scene->objs == NULL || index >= scene->obj_count) {
        return NULL;
    }
    return scene->objs[index];
}

gfx_object_t *gfx_gsp_scene_find_by_name(const gfx_gsp_scene_t *scene, const char *name)
{
    if (scene == NULL || scene->refs == NULL || name == NULL) {
        return NULL;
    }
    for (uint16_t i = 0; i < scene->ref_count; i++) {
        if (scene->refs[i].name != NULL && strcmp(scene->refs[i].name, name) == 0) {
            return scene->refs[i].obj;
        }
    }
    return NULL;
}

gfx_object_t *gfx_gsp_scene_find_by_bind_id(const gfx_gsp_scene_t *scene, uint16_t bind_id)
{
    if (scene == NULL || scene->refs == NULL || bind_id == 0) {
        return NULL;
    }
    for (uint16_t i = 0; i < scene->ref_count; i++) {
        if (scene->refs[i].bind_id == bind_id) {
            return scene->refs[i].obj;
        }
    }
    return NULL;
}

int gfx_gsp_scene_show_layer(gfx_gsp_scene_t *scene, const char *name)
{
    gfx_object_t *layer = gfx_gsp_scene_find_by_name(scene, name);

    if (layer == NULL) {
        return GFX_GSP_ERR_ACTION;
    }
    return show_layer_object(scene, layer);
}

/* ------------------------------------------------------------------ */
/* gfx_gsp_dump：把包内容打印到 stdout，直观展示“里面含啥 + 全是 offset”。 */
/* ------------------------------------------------------------------ */

static const char *type_name(uint16_t t)
{
    switch (t) {
    case GFX_GSP_OBJ_CONTAINER: return "container";
    case GFX_GSP_OBJ_LABEL:     return "label";
    case GFX_GSP_OBJ_BUTTON:    return "button";
    case GFX_GSP_OBJ_IMAGE:     return "image";
    case GFX_GSP_OBJ_LIST:      return "list";
    case GFX_GSP_OBJ_WHEEL:     return "wheel";
    case GFX_GSP_OBJ_LAYER:     return "layer";
    default:                return "?";
    }
}

void gfx_gsp_dump(const uint8_t *buf, size_t size)
{
    if (buf == NULL || size < GFX_GSP_HEADER_SIZE) {
        printf("gsp_dump: invalid buffer\n");
        return;
    }

    const uint32_t magic = gfx_gsp_rd_u32(buf + 0);
    const uint32_t version = gfx_gsp_rd_u32(buf + 4);
    const uint16_t sw = gfx_gsp_rd_u16(buf + 8);
    const uint16_t sh = gfx_gsp_rd_u16(buf + 10);
    const uint32_t sbg = gfx_gsp_rd_u32(buf + 12);
    const uint32_t n = gfx_gsp_rd_u32(buf + 16);
    const uint32_t obj_off = gfx_gsp_rd_u32(buf + 20);
    const uint32_t str_off = gfx_gsp_rd_u32(buf + 24);
    const uint32_t blob_count = gfx_gsp_rd_u32(buf + 28);
    const uint32_t blob_off = gfx_gsp_rd_u32(buf + 32);
    const uint32_t total = gfx_gsp_rd_u32(buf + 36);
    const uint32_t crc = gfx_gsp_rd_u32(buf + 40);
    const uint32_t action_count = gfx_gsp_rd_u32(buf + 44);
    const uint32_t action_off = gfx_gsp_rd_u32(buf + 48);
    const uint32_t crc_calc = (total <= size) ? gfx_gsp_crc32_scene(buf, total) : 0u;

    printf("==== GSP package (%zu bytes) ====\n", size);
    printf("header: magic=%c%c%c%c version=%u screen=%ux%u bg=#%06X\n",
           (char)(magic & 0xFF), (char)((magic >> 8) & 0xFF),
           (char)((magic >> 16) & 0xFF), (char)((magic >> 24) & 0xFF),
           (unsigned)version, (unsigned)sw, (unsigned)sh, (unsigned)(sbg & 0xFFFFFFu));
    printf("        obj_count=%u obj_table_off=%u str_table_off=%u\n",
           (unsigned)n, (unsigned)obj_off, (unsigned)str_off);
    printf("        blob_count=%u blob_table_off=%u total=%u\n",
           (unsigned)blob_count, (unsigned)blob_off, (unsigned)total);
    printf("        action_count=%u action_table_off=%u\n",
           (unsigned)action_count, (unsigned)action_off);
    printf("        crc32=0x%08X (%s)\n", (unsigned)crc, crc == crc_calc ? "ok" : "MISMATCH");

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *e = buf + obj_off + (size_t)i * GFX_GSP_OBJ_SIZE;
        const uint16_t type = gfx_gsp_rd_u16(e + 0);
        const uint16_t parent = gfx_gsp_rd_u16(e + 2);
        const int16_t x = gfx_gsp_rd_i16(e + 4);
        const int16_t y = gfx_gsp_rd_i16(e + 6);
        const uint16_t w = gfx_gsp_rd_u16(e + 8);
        const uint16_t h = gfx_gsp_rd_u16(e + 10);
        const uint32_t flags = gfx_gsp_rd_u32(e + 12);
        const uint32_t text_off = gfx_gsp_rd_u32(e + 32);
        const uint32_t cb_off = gfx_gsp_rd_u32(e + 36);
        const uint32_t name_off = gfx_gsp_rd_u32(e + 40);
        const uint32_t blob_idx = gfx_gsp_rd_u32(e + 44);
        const uint16_t font_id = gfx_gsp_rd_u16(e + 56);

        char parent_buf[8];
        if (parent == GFX_GSP_NO_PARENT) {
            snprintf(parent_buf, sizeof(parent_buf), "root");
        } else {
            snprintf(parent_buf, sizeof(parent_buf), "%u", parent);
        }
        printf("obj[%u] %-9s parent=%-4s rect=(%d,%d %ux%u) flags=0x%03X",
               (unsigned)i, type_name(type), parent_buf, x, y,
               (unsigned)w, (unsigned)h, (unsigned)flags);
        if ((flags & GFX_GSP_F_TEXT) && text_off < size) {
            printf(" text@%u=\"%s\"", (unsigned)text_off, (const char *)(buf + text_off));
        }
        if ((flags & GFX_GSP_F_CALLBACK) && cb_off < size) {
            printf(" cb@%u=\"%s\"", (unsigned)cb_off, (const char *)(buf + cb_off));
        }
        if ((flags & GFX_GSP_F_NAME) && name_off < size) {
            printf(" name@%u=\"%s\"", (unsigned)name_off, (const char *)(buf + name_off));
        }
        if (flags & GFX_GSP_F_IMAGE) {
            printf(" blob=%u", (unsigned)blob_idx);
        }
        if (type == GFX_GSP_OBJ_LABEL || type == GFX_GSP_OBJ_BUTTON) {
            printf(" font=%u", font_id);
        }
        if (flags & GFX_GSP_F_HIDDEN) {
            printf(" hidden");
        }
        printf("\n");
    }

    for (uint32_t k = 0; k < blob_count; k++) {
        const uint8_t *b = buf + blob_off + (size_t)k * GFX_GSP_BLOB_SIZE;
        const uint16_t w = gfx_gsp_rd_u16(b + 0);
        const uint16_t h = gfx_gsp_rd_u16(b + 2);
        const uint8_t  cf = b[4];
        const uint8_t  codec = b[5];
        const uint32_t raw_size = gfx_gsp_rd_u32(b + 8);
        const uint32_t comp_size = gfx_gsp_rd_u32(b + 12);
        const uint32_t data_off = gfx_gsp_rd_u32(b + 16);
        const char *codec_name = (codec == GFX_GSP_CODEC_RLE16) ? "rle16" :
                                 (codec == GFX_GSP_CODEC_STORE) ? "store" : "?";
        double ratio = raw_size ? (100.0 * comp_size / raw_size) : 0.0;
        printf("blob[%u] %ux%u cf=0x%02X codec=%s raw=%uB comp=%uB (%.1f%%) data@%u\n",
               (unsigned)k, (unsigned)w, (unsigned)h, (unsigned)cf, codec_name,
               (unsigned)raw_size, (unsigned)comp_size, ratio, (unsigned)data_off);
    }

    for (uint32_t k = 0; k < action_count && action_off != 0; k++) {
        const uint8_t *e = buf + action_off + (size_t)k * GFX_GSP_ACTION_SIZE;
        const uint16_t src = gfx_gsp_rd_u16(e + 0);
        const uint16_t ev = gfx_gsp_rd_u16(e + 2);
        const uint16_t act = gfx_gsp_rd_u16(e + 4);
        const uint16_t tgt = gfx_gsp_rd_u16(e + 6);
        const uint32_t name_off = gfx_gsp_rd_u32(e + 8);
        const uint32_t param_off = gfx_gsp_rd_u32(e + 12);
        const uint32_t arg = gfx_gsp_rd_u32(e + 20);
        static const char *ev_names[] = { "none", "click", "press", "release", "long", "value" };
        static const char *act_names[] = { "none", "show", "hide", "toggle", "set_text",
                                           "set_bg_color", "set_opacity", "call", "goto", "back"
                                         };
        const char *evn = (ev < sizeof(ev_names) / sizeof(ev_names[0])) ? ev_names[ev] : "?";
        const char *actn = (act < sizeof(act_names) / sizeof(act_names[0])) ? act_names[act] : "?";
        printf("action[%u] src=%u on=%-7s do=%-12s",
               (unsigned)k, (unsigned)src, evn, actn);
        if (name_off != 0 && name_off < size) {
            printf(" target=\"%s\"", (const char *)(buf + name_off));
        } else if (tgt != GFX_GSP_ACT_NO_TARGET) {
            printf(" target=obj%u", tgt);
        }
        if (param_off != 0 && param_off < size) {
            printf(" param=\"%s\"", (const char *)(buf + param_off));
        }
        if (arg != 0) {
            printf(" arg=0x%06X", (unsigned)(arg & 0xFFFFFFu));
        }
        printf("\n");
    }

    printf("note: 结构引用全是 u32 偏移 / u16 索引；图片像素已烘焙进包并压缩，\n");
    printf("      带 codec+raw/comp 头，加载期解压 —— 同一份字节 32/64 位解析一致。\n");
    printf("=================================\n");
}
