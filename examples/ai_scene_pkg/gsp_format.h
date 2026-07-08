/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * GSP1 — 位置无关的 u32-offset scene 包（演示 / 概念验证）
 * ============================================================
 * 设计核心（对应 ai_designer/ 的选型结论）：
 *   包内不出现任何“原生指针”。所有引用只有两种：
 *     - u32 字节偏移（相对 buffer 起点），或
 *     - u16 对象索引（进 object table）。
 *   多字节字段一律固定宽度 + 小端 + 显式字节偏移编解码，
 *   不依赖任何 C struct 的内存布局 / padding / 指针宽度。
 *
 * 于是同一份字节：
 *   - 64 位 host（SDL 预览）与 32 位 device 解析结果完全一致；
 *   - 无需把 host 降成 32 位（不像 ITE 靠 win32 32 位仿真回避问题）；
 *   - 无需冻结任何 widget struct 布局（不像 ITU 指针内存镜像）。
 *
 * 加载走“工厂建树”：loader 按 type 调 gfx_*_create + setter，
 * 不做指针重定位。详见 gsp_load.c。
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/display.h"
#include "gfx/object.h"
#include "gfx/widgets/image.h"   /* gfx_image_dsc_t */
#include "gfx/widgets/label.h"   /* gfx_font_t */

#ifdef __cplusplus
extern "C" {
#endif

/* 'G''S''P''1' little-endian */
#define GSP_MAGIC   0x31505347u
#define GSP_VERSION 3u   /* v3：header 加 crc32；ObjEntry 扩到 64B（name/params/opacity/align）*/

/* 固定记录尺寸（字节）——与任何 C struct 布局无关 */
#define GSP_HEADER_SIZE 48u
#define GSP_OBJ_SIZE    64u
#define GSP_BLOB_SIZE   20u

/* blob（烘焙位图）压缩编码 */
enum {
    GSP_CODEC_STORE = 0,   /* 原样存储（未压缩）*/
    GSP_CODEC_RLE16 = 1,   /* 16bpp 像素游程编码：token = u16 count + u16 pixel */
};

/* 文字对齐（与 gfx_text_align_t 对应）*/
enum {
    GSP_ALIGN_AUTO = 0,
    GSP_ALIGN_LEFT = 1,
    GSP_ALIGN_CENTER = 2,
    GSP_ALIGN_RIGHT = 3,
};

/* object 类型 */
enum {
    GSP_OBJ_CONTAINER = 1,
    GSP_OBJ_LABEL     = 2,
    GSP_OBJ_BUTTON    = 3,
    GSP_OBJ_IMAGE     = 4,
};

/* ObjEntry.flags 位 */
#define GSP_F_TEXT      (1u << 0)   /* text_off 有效 */
#define GSP_F_FG_COLOR  (1u << 1)   /* fg_color 有效（label 文字色 / button 文字色）*/
#define GSP_F_BG_COLOR  (1u << 2)   /* bg_color 有效 */
#define GSP_F_BORDER    (1u << 3)   /* border_color + border_width 有效 */
#define GSP_F_RADIUS    (1u << 4)   /* radius 有效 */
#define GSP_F_CALLBACK  (1u << 5)   /* callback_off 有效 */
#define GSP_F_IMAGE     (1u << 6)   /* blob_idx 有效（烘焙进包的图片）*/
#define GSP_F_NAME      (1u << 7)   /* name_off 有效（控件名，供按名查找/绑定）*/
#define GSP_F_HIDDEN    (1u << 8)   /* 初始隐藏 */
#define GSP_F_OPACITY   (1u << 9)   /* opacity 有效（0..255）*/
#define GSP_F_ALIGN     (1u << 10)  /* text_align 有效 */
#define GSP_F_PARAMS    (1u << 11)  /* params_off + params_len 有效（每控件私有参数块）*/

/* parent_idx 哨兵：根对象（直接挂 display）*/
#define GSP_NO_PARENT   0xFFFFu

/*
 * Header 字节布局（GSP_HEADER_SIZE = 48）：
 *   off  0  u32 magic
 *   off  4  u32 version
 *   off  8  u16 screen_w
 *   off 10  u16 screen_h
 *   off 12  u32 screen_bg      RGB888
 *   off 16  u32 obj_count
 *   off 20  u32 obj_table_off
 *   off 24  u32 str_table_off
 *   off 28  u32 blob_count
 *   off 32  u32 blob_table_off
 *   off 36  u32 total_size
 *   off 40  u32 crc32          覆盖整包（计算时本字段视为 0）
 *   off 44  u32 reserved
 *
 * ObjEntry 字节布局（GSP_OBJ_SIZE = 64），先序排列、parent_idx < 自身索引：
 *   off  0  u16 type
 *   off  2  u16 parent_idx     GSP_NO_PARENT = 根
 *   off  4  i16 x
 *   off  6  i16 y
 *   off  8  u16 w
 *   off 10  u16 h
 *   off 12  u32 flags
 *   off 16  u32 fg_color       RGB888
 *   off 20  u32 bg_color       RGB888
 *   off 24  u32 border_color   RGB888
 *   off 28  u16 border_width
 *   off 30  u16 radius
 *   off 32  u32 text_off       -> string table 内 NUL 结尾字符串；0 = 无
 *   off 36  u32 callback_off   -> 回调名字符串；0 = 无
 *   off 40  u32 name_off       -> 控件名字符串；0 = 无
 *   off 44  u32 blob_idx       -> 图片 blob 索引（GSP_F_IMAGE 时有效）
 *   off 48  u32 params_off     -> 每控件私有参数块偏移（GSP_F_PARAMS 时有效）
 *   off 52  u16 params_len     私有参数块字节数
 *   off 54  u8  opacity        0..255（GSP_F_OPACITY 时有效）
 *   off 55  u8  text_align     GSP_ALIGN_*（GSP_F_ALIGN 时有效）
 *   off 56  u16 font_id        运行期字体表 id（gsp_font_binding_t.id）
 *   off 58  u16 bind_id        数据绑定 id（预留）
 *   off 60  u32 reserved0
 *
 * BlobEntry 字节布局（GSP_BLOB_SIZE = 20），烘焙进包的位图：
 *   off  0  u16 w
 *   off  2  u16 h
 *   off  4  u8  cf            gfx_color_format_t
 *   off  5  u8  codec         GSP_CODEC_*
 *   off  6  u16 stride        每行字节（0 = w*bpp）
 *   off  8  u32 raw_size      解压后字节数
 *   off 12  u32 comp_size     压缩后字节数（STORE 时 == raw_size）
 *   off 16  u32 data_off      -> 压缩字节在 buffer 内的绝对偏移
 *
 * String table：若干 NUL 结尾字符串，按“绝对字节偏移”引用。
 * Blob data：各 blob 的压缩像素，按 data_off 绝对偏移引用。
 *
 * 与 ITE 类比：ITE 把每个 surface 压缩（软件 UCL nrv2e / 硬件 DCPS，照片走
 * JPEG），头里存 compressedSize + 解压后 bufSize，加载期解到显存。这里同理
 * 把像素烘焙进包并压缩，头存 comp_size + raw_size，加载期解压后交给渲染器。
 * 区别：包内引用仍全是 u32 偏移 / 索引，无原生指针，32/64 位解析一致。
 */

/* ---------- 小端 / 固定宽度 编解码（与 struct 布局无关）---------- */

static inline void gsp_wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void gsp_wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline uint16_t gsp_rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline int16_t gsp_rd_i16(const uint8_t *p)
{
    return (int16_t)gsp_rd_u16(p);
}

static inline uint32_t gsp_rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 每像素字节数（仅覆盖本 demo 用到的格式；0 = 不支持）。*/
static inline uint8_t gsp_bpp(uint8_t cf)
{
    switch (cf) {
    case GFX_COLOR_FORMAT_RGB565:
    case GFX_COLOR_FORMAT_RGB565_SWAPPED:
        return 2;
    case GFX_COLOR_FORMAT_RGB888:
    case GFX_COLOR_FORMAT_BGR888:
        return 3;
    case GFX_COLOR_FORMAT_ARGB8888:
        return 4;
    default:
        return 0;
    }
}

/* ---------- CRC32（IEEE，table-less；两端一致，无依赖）---------- */

static inline uint32_t gsp_crc32_feed(uint32_t crc, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc;
}

/* 覆盖整包、但把 header 里的 crc32 字段（off 40..43）当作 0 计算。*/
static inline uint32_t gsp_crc32_scene(const uint8_t *buf, uint32_t total)
{
    static const uint8_t zero4[4] = { 0, 0, 0, 0 };
    uint32_t c = 0xFFFFFFFFu;
    c = gsp_crc32_feed(c, buf, 40);              /* [0, 40) */
    c = gsp_crc32_feed(c, zero4, 4);             /* [40, 44) 视为 0 */
    if (total > 44u) {
        c = gsp_crc32_feed(c, buf + 44, total - 44u);   /* [44, total) */
    }
    return c ^ 0xFFFFFFFFu;
}

/* 编译期锁死记录尺寸（字节偏移式无需 struct，但断言可防手滑改错）*/
_Static_assert(GSP_HEADER_SIZE == 48u, "GSP header size drift");
_Static_assert(GSP_OBJ_SIZE == 64u, "GSP object entry size drift");
_Static_assert(GSP_BLOB_SIZE == 20u, "GSP blob entry size drift");

/* ---------- 作者侧描述（打包输入）---------- */

typedef struct {
    uint16_t    id;            /* Object font_id references this id */
    const char *family;        /* Logical family name from the designer */
    const char *path;          /* Host path or device asset path */
    uint16_t    size_px;       /* Requested pixel size */
    uint16_t    weight;        /* 400=regular, 700=bold; advisory for now */
    uint8_t     style;         /* 0=normal, 1=italic; advisory for now */
    uint8_t     reserved;
} gsp_font_desc_t;

typedef struct {
    uint16_t    type;          /* GSP_OBJ_* */
    uint16_t    parent_idx;    /* GSP_NO_PARENT = 根 */
    int16_t     x, y;
    uint16_t    w, h;
    uint32_t    flags;         /* GSP_F_* */
    const char *text;          /* NULL 若无 */
    uint32_t    fg_color;      /* RGB888 */
    uint32_t    bg_color;      /* RGB888 */
    uint32_t    border_color;  /* RGB888 */
    uint16_t    border_width;
    uint16_t    radius;
    const char *callback;      /* NULL 若无 */
    const gfx_image_dsc_t *image_src;  /* 源位图；GSP_F_IMAGE 时打包器把像素烘焙进包 */
    const char *name;          /* 控件名（GSP_F_NAME）；NULL 若无 */
    uint8_t     opacity;       /* 0..255（GSP_F_OPACITY）*/
    uint8_t     text_align;    /* GSP_ALIGN_*（GSP_F_ALIGN）*/
    uint16_t    font_id;       /* References gsp_font_desc_t.id / gsp_font_binding_t.id */
    uint16_t    bind_id;       /* 预留 */
    const void *params;        /* 每控件私有参数块（GSP_F_PARAMS）；NULL 若无 */
    uint16_t    params_len;    /* params 字节数 */
} gsp_desc_t;

typedef struct {
    uint16_t          screen_w, screen_h;
    uint32_t          screen_bg;   /* RGB888 */
    const gsp_desc_t *objs;
    uint16_t          obj_count;
    const gsp_font_desc_t *fonts;
    uint16_t          font_count;
} gsp_scene_desc_t;

/* ---------- packer（host “编译器”）---------- */

/**
 * 把 scene 描述打包成一段位置无关的 u32-offset 字节。
 * 返回 malloc 的 buffer；*out_size 为字节数；失败返回 NULL。调用者 free()。
 */
uint8_t *gsp_pack(const gsp_scene_desc_t *scene, size_t *out_size);

/** 把包内容（header / object table / string table）打印到 stdout。*/
void gsp_dump(const uint8_t *buf, size_t size);

/* ---------- loader（host/device 共用）---------- */

typedef struct {
    const char           *name;
    gfx_object_touch_cb_t cb;
    void                 *user_data;
} gsp_cb_binding_t;

typedef struct {
    uint16_t   id;             /* Matches ObjEntry.font_id */
    gfx_font_t font;
} gsp_font_binding_t;

typedef struct {
    gfx_object_t  *root;       /* 第一个根对象 */
    gfx_object_t **objs;       /* 每个 entry 对应的 handle（malloc）*/
    uint16_t       obj_count;
    /* 加载期从 blob 解压出来的位图（随场景生命周期，gsp_scene_free 释放）*/
    gfx_image_dsc_t *img_dscs; /* [blob_count]，image 对象的 source 指向这里 */
    uint8_t        **img_bufs; /* [blob_count]，解压后的像素缓冲 */
    uint16_t         blob_count;
} gsp_scene_t;

/* gsp_load 返回码 */
enum {
    GSP_OK = 0,
    GSP_ERR_SIZE = -1,
    GSP_ERR_MAGIC = -2,
    GSP_ERR_VERSION = -3,
    GSP_ERR_BOUNDS = -4,
    GSP_ERR_COUNT = -5,
    GSP_ERR_PARENT = -6,
    GSP_ERR_STRING = -7,
    GSP_ERR_TYPE = -8,
    GSP_ERR_CREATE = -9,
    GSP_ERR_ALLOC = -10,
    GSP_ERR_IMAGE = -11,
    GSP_ERR_BLOB = -12,
    GSP_ERR_CODEC = -13,
    GSP_ERR_CRC = -14,
};

/**
 * 从 u32-offset 包加载成 gfx object 树（工厂建树，无指针重定位）。
 * 全程做边界/越界/parent 校验，坏包只返回错误码、绝不崩。
 *
 * @param buf/size 包字节（可来自文件读入，未来也可来自 mmap 只读区）
 * @param disp     目标 display
 * @param font     label/button 使用的字体
 * @param cbs/cb_count  回调名 -> 函数绑定表
 * @param out      输出场景句柄（需 gsp_scene_free 释放）
 * @return GSP_OK(0) 成功，否则 GSP_ERR_*
 */
int gsp_load(const uint8_t *buf, size_t size, gfx_display_t *disp, gfx_font_t font,
             const gsp_cb_binding_t *cbs, size_t cb_count, gsp_scene_t *out);

/**
 * Load a package with a runtime font binding table. If an object's font_id is
 * not found, default_font is used.
 */
int gsp_load_with_fonts(const uint8_t *buf, size_t size, gfx_display_t *disp,
                        const gsp_font_binding_t *fonts, size_t font_count,
                        gfx_font_t default_font,
                        const gsp_cb_binding_t *cbs, size_t cb_count,
                        gsp_scene_t *out);

/** 删除整棵已加载的对象树并释放句柄数组。*/
void gsp_scene_free(gsp_scene_t *scene);

#ifdef __cplusplus
}
#endif
