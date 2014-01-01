/* wld: wld-private.h
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WLD_PRIVATE_H
#define WLD_PRIVATE_H 1

#include "wld.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#ifndef offsetof
#   define offsetof __builtin_offsetof
#endif

#define ARRAY_LENGTH(array) (sizeof (array) / sizeof (array)[0])
#if ENABLE_DEBUG
#   define DEBUG(format, ...) \
        fprintf(stderr, "# %s: " format, __func__, ## __VA_ARGS__)
#else
#   define DEBUG(format, ...)
#endif

#define EXPORT __attribute__((visibility("default")))
#define IMPL(name, type)                                                    \
    static inline struct name ## _ ## type * name ## _ ## type              \
        (struct wld_ ## type * type)                                        \
    {                                                                       \
        assert(type->impl == &type ## _impl);                               \
        return (struct name ## _ ## type *) type;                           \
    }

struct wld_font_context
{
    FT_Library library;
};

struct glyph
{
    FT_Bitmap bitmap;

    /**
     * The offset from the origin to the top left corner of the bitmap.
     */
    int16_t x, y;

    /**
     * The width to advance to the origin of the next character.
     */
    uint16_t advance;
};

struct font
{
    struct wld_font base;

    struct wld_font_context * context;
    FT_Face face;
    struct glyph ** glyphs;
};

struct wld_context_impl
{
    struct wld_drawable * (* create_drawable)(struct wld_context * context,
                                              uint32_t width, uint32_t height,
                                              uint32_t format);
    struct wld_drawable * (* import)(struct wld_context * context,
                                     uint32_t type, union wld_object object,
                                     uint32_t width, uint32_t height,
                                     uint32_t format, uint32_t pitch);
    void (* destroy)(struct wld_context * context);
};

struct wld_draw_interface
{
    void (* fill_rectangle)(struct wld_drawable * drawable, uint32_t color,
                            int32_t x, int32_t y,
                            uint32_t width, uint32_t height);
    void (* fill_region)(struct wld_drawable * drawable, uint32_t color,
                         pixman_region32_t * region);
    void (* copy_rectangle)(struct wld_drawable * src,
                            struct wld_drawable * dst,
                            int32_t src_x, int32_t src_y,
                            int32_t dst_x, int32_t dst_y,
                            uint32_t width, uint32_t height);
    void (* copy_region)(struct wld_drawable * src, struct wld_drawable * dst,
                         pixman_region32_t * region,
                         int32_t dst_x, int32_t dst_y);
    void (* draw_text_utf8)(struct wld_drawable * drawable,
                            struct font * font, uint32_t color,
                            int32_t x, int32_t y,
                            const char * text, int32_t length,
                            struct wld_extents * extents);
    void (* write)(struct wld_drawable * drawable,
                   const void * data, size_t size);
    pixman_image_t * (* map)(struct wld_drawable * drawable);
    void (* flush)(struct wld_drawable * drawable);
    void (* destroy)(struct wld_drawable * drawable);
};

bool font_ensure_glyph(struct font * font, FT_UInt glyph_index);

/**
 * Returns the number of bytes per pixel for the given format.
 */
static inline uint8_t format_bytes_per_pixel(enum wld_format format)
{
    switch (format)
    {
        case WLD_FORMAT_ARGB8888:
        case WLD_FORMAT_XRGB8888:
            return 4;
        default:
            return 0;
    }
}

static inline pixman_format_code_t format_wld_to_pixman(uint32_t format)
{
    switch (format)
    {
        case WLD_FORMAT_ARGB8888:
            return PIXMAN_a8r8g8b8;
        case WLD_FORMAT_XRGB8888:
            return PIXMAN_x8r8g8b8;
        default:
            return 0;
    }
}

static inline uint32_t format_pixman_to_wld(pixman_format_code_t format)
{
    switch (format)
    {
        case PIXMAN_a8r8g8b8:
            return WLD_FORMAT_ARGB8888;
        case PIXMAN_x8r8g8b8:
            return WLD_FORMAT_XRGB8888;
        default:
            return 0;
    }
}

/**
 * This default fill_region method is implemented in terms of fill_rectangle.
 */
void default_fill_region(struct wld_drawable * drawable, uint32_t color,
                         pixman_region32_t * region);

/**
 * This default copy_region method is implemented in terms of copy_rectangle.
 */
void default_copy_region(struct wld_drawable * src, struct wld_drawable * dst,
                         pixman_region32_t * region,
                         int32_t dst_x, int32_t dst_y);

void context_initialize(struct wld_context * context,
                        const struct wld_context_impl * impl);

#endif

