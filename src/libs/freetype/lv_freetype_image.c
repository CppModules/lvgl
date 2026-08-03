/**
 * @file lv_freetype_image.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl.h"
#include "lv_freetype_private.h"

#if LV_USE_FREETYPE

#include "../../core/lv_global.h"

#define font_draw_buf_handlers &(LV_GLOBAL_DEFAULT()->font_draw_buf_handlers)

/*********************
 *      DEFINES
 *********************/

#define CACHE_NAME "FREETYPE_IMAGE"

/**********************
 *      TYPEDEFS
 **********************/

typedef struct _lv_freetype_image_cache_data_t {
    FT_UInt glyph_index;
    uint32_t size;

    lv_draw_buf_t * draw_buf;
} lv_freetype_image_cache_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static const void * freetype_get_glyph_bitmap_cb(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf);

static bool freetype_image_create_cb(lv_freetype_image_cache_data_t * data, void * user_data);
static void freetype_image_free_cb(lv_freetype_image_cache_data_t * node, void * user_data);
static lv_cache_compare_res_t freetype_image_compare_cb(const lv_freetype_image_cache_data_t * lhs,
                                                        const lv_freetype_image_cache_data_t * rhs);

static void freetype_image_release_cb(const lv_font_t * font, lv_font_glyph_dsc_t * g_dsc);
/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_cache_t * lv_freetype_create_draw_data_image(uint32_t cache_size)
{
    lv_cache_ops_t ops = {
        .compare_cb = (lv_cache_compare_cb_t)freetype_image_compare_cb,
        .create_cb = (lv_cache_create_cb_t)freetype_image_create_cb,
        .free_cb = (lv_cache_free_cb_t)freetype_image_free_cb,
    };

    lv_cache_t * draw_data_cache = lv_cache_create(&lv_cache_class_lru_rb_count, sizeof(lv_freetype_image_cache_data_t),
                                                   cache_size, ops);
    lv_cache_set_name(draw_data_cache, CACHE_NAME);

    return draw_data_cache;
}

void lv_freetype_set_cbs_image_font(lv_freetype_font_dsc_t * dsc)
{
    LV_ASSERT_FREETYPE_FONT_DSC(dsc);
    dsc->font.get_glyph_bitmap = freetype_get_glyph_bitmap_cb;
    dsc->font.release_glyph = freetype_image_release_cb;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static const void * freetype_get_glyph_bitmap_cb(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf)
{
    LV_UNUSED(draw_buf);
    LV_PROFILER_FONT_BEGIN;
    const lv_font_t * font = g_dsc->resolved_font;
    lv_freetype_font_dsc_t * dsc = (lv_freetype_font_dsc_t *)font->dsc;
    LV_ASSERT_FREETYPE_FONT_DSC(dsc);

    FT_UInt glyph_index = (FT_UInt)g_dsc->gid.index;

    lv_cache_t * cache = dsc->cache_node->draw_data_cache;

    lv_freetype_image_cache_data_t search_key = {
        .glyph_index = glyph_index,
        .size = dsc->size,
    };

    lv_cache_entry_t * entry = lv_cache_acquire_or_create(cache, &search_key, dsc);
    if(entry == NULL) {
        LV_LOG_ERROR("glyph bitmap lookup failed for glyph_index = 0x%" LV_PRIx32, (uint32_t)glyph_index);
        LV_PROFILER_FONT_END;
        return NULL;
    }

    g_dsc->entry = entry;
    lv_freetype_image_cache_data_t * cache_node = lv_cache_entry_get_data(entry);

    LV_PROFILER_FONT_END;
    return cache_node->draw_buf;
}

static void freetype_image_release_cb(const lv_font_t * font, lv_font_glyph_dsc_t * g_dsc)
{
    LV_ASSERT_NULL(font);
    lv_freetype_font_dsc_t * dsc = (lv_freetype_font_dsc_t *)font->dsc;
    lv_cache_release(dsc->cache_node->draw_data_cache, g_dsc->entry, NULL);
    g_dsc->entry = NULL;
}

/*-----------------
 * Cache Callbacks
 *----------------*/

static void freetype_downscale_bgra(const FT_Bitmap * src, uint16_t dst_w, uint16_t dst_h,
                                    uint8_t * dst, uint32_t dst_stride)
{
    uint32_t sw = src->width;
    uint32_t sh = src->rows;
    int32_t spitch = src->pitch;
    const uint8_t * sb = src->buffer;
    for(uint32_t dy = 0; dy < dst_h; ++dy) {
        uint32_t y0 = dy * sh / dst_h;
        uint32_t y1 = (dy + 1) * sh / dst_h;
        if(y1 <= y0) y1 = y0 + 1;
        for(uint32_t dx = 0; dx < dst_w; ++dx) {
            uint32_t x0 = dx * sw / dst_w;
            uint32_t x1 = (dx + 1) * sw / dst_w;
            if(x1 <= x0) x1 = x0 + 1;
            uint32_t b = 0, g = 0, r = 0, a = 0, cnt = 0;
            for(uint32_t y = y0; y < y1; ++y) {
                const uint8_t * row = sb + (int32_t)y * spitch;
                for(uint32_t x = x0; x < x1; ++x) {
                    const uint8_t * px = row + x * 4;   /* BGRA */
                    b += px[0]; g += px[1]; r += px[2]; a += px[3];
                    cnt++;
                }
            }
            uint8_t * dp = dst + dy * dst_stride + dx * 4;
            dp[0] = (uint8_t)(b / cnt);
            dp[1] = (uint8_t)(g / cnt);
            dp[2] = (uint8_t)(r / cnt);
            dp[3] = (uint8_t)(a / cnt);
        }
    }
}

static bool freetype_image_create_cb(lv_freetype_image_cache_data_t * data, void * user_data)
{
    LV_PROFILER_FONT_BEGIN;

    lv_freetype_font_dsc_t * dsc = (lv_freetype_font_dsc_t *)user_data;

    FT_Error error;

    lv_mutex_lock(&dsc->cache_node->face_lock);

    FT_Face face = dsc->cache_node->face;
    if(FT_IS_SCALABLE(face)) {
        error = FT_Set_Pixel_Sizes(face, 0, dsc->size);
    }
    else {
        error = FT_Select_Size(face, 0);
    }
    if(error) {
        FT_ERROR_MSG("FT_Set_Pixel_Sizes", error);
        lv_mutex_unlock(&dsc->cache_node->face_lock);
        return false;
    }
    error = FT_Load_Glyph(face, data->glyph_index,
                          FT_LOAD_COLOR | FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_AUTOHINT);
    if(error) {
        FT_ERROR_MSG("FT_Load_Glyph", error);
        lv_mutex_unlock(&dsc->cache_node->face_lock);
        LV_PROFILER_FONT_END;
        return false;
    }
    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if(error) {
        FT_ERROR_MSG("FT_Render_Glyph", error);
        lv_mutex_unlock(&dsc->cache_node->face_lock);
        LV_PROFILER_FONT_END;
        return false;
    }

    FT_Glyph glyph;
    error = FT_Get_Glyph(face->glyph, &glyph);
    if(error) {
        FT_ERROR_MSG("FT_Get_Glyph", error);
        lv_mutex_unlock(&dsc->cache_node->face_lock);
        LV_PROFILER_FONT_END;
        return false;
    }

    FT_BitmapGlyph glyph_bitmap = (FT_BitmapGlyph)glyph;

    uint16_t box_h = glyph_bitmap->bitmap.rows;         /*Height of the bitmap in [px]*/
    uint16_t box_w = glyph_bitmap->bitmap.width;        /*Width of the bitmap in [px]*/

    uint16_t dst_w = box_w;
    uint16_t dst_h = box_h;
    int scale_down = 0;
    if(!FT_IS_SCALABLE(face) && glyph_bitmap->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        FT_Pos ppem = face->size->metrics.x_ppem;
        if(ppem > 0 && (uint32_t)ppem != dsc->size) {
            int32_t sz = (int32_t)dsc->size;
            int32_t p = (int32_t)ppem;
            dst_w = (box_w * sz + p / 2) / p;
            dst_h = (box_h * sz + p / 2) / p;
            if(dst_w < 1) dst_w = 1;
            if(dst_h < 1) dst_h = 1;
            scale_down = 1;
        }
    }

    lv_color_format_t col_format;
    if(glyph_bitmap->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        col_format = LV_COLOR_FORMAT_ARGB8888;
    }
    else {
        col_format = LV_COLOR_FORMAT_A8;
    }
    uint32_t stride = lv_draw_buf_width_to_stride(dst_w, col_format);
    data->draw_buf = lv_draw_buf_create_ex(font_draw_buf_handlers, dst_w, dst_h, col_format, stride);
    if(!data->draw_buf) {
        LV_LOG_WARN("Could not create draw buffer");
        FT_Done_Glyph(glyph);
        LV_PROFILER_FONT_END;
        return false;
    }
    lv_draw_buf_clear(data->draw_buf, NULL);

    if(scale_down) {
        freetype_downscale_bgra(&glyph_bitmap->bitmap, dst_w, dst_h,
                                (uint8_t *)data->draw_buf->data, stride);
    }
    else {
        uint32_t pitch = glyph_bitmap->bitmap.pitch;
        for(int y = 0; y < box_h; ++y) {
            lv_memcpy((uint8_t *)(data->draw_buf->data) + y * stride, glyph_bitmap->bitmap.buffer + y * pitch,
                      pitch);
        }
    }

    lv_draw_buf_flush_cache(data->draw_buf, NULL);
    FT_Done_Glyph(glyph);
    lv_mutex_unlock(&dsc->cache_node->face_lock);
    LV_PROFILER_FONT_END;
    return true;
}
static void freetype_image_free_cb(lv_freetype_image_cache_data_t * data, void * user_data)
{
    LV_UNUSED(user_data);
    lv_draw_buf_destroy(data->draw_buf);
}
static lv_cache_compare_res_t freetype_image_compare_cb(const lv_freetype_image_cache_data_t * lhs,
                                                        const lv_freetype_image_cache_data_t * rhs)
{
    if(lhs->glyph_index != rhs->glyph_index) {
        return lhs->glyph_index > rhs->glyph_index ? 1 : -1;
    }
    if(lhs->size != rhs->size) {
        return lhs->size > rhs->size ? 1 : -1;
    }
    return 0;
}

#endif /*LV_USE_FREETYPE*/
