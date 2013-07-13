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

#include <stdbool.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#ifndef offsetof
#   define offsetof __builtin_offsetof
#endif

#define container_of(ptr, type, member) ({                                  \
    const typeof(((type *) 0)->member) *__mptr = (ptr);                     \
    ((type *) ((uintptr_t) __mptr - offsetof(type, member)));               \
})

#define ARRAY_LENGTH(array) (sizeof (array) / sizeof (array)[0])
#if ENABLE_DEBUG
#   define DEBUG(format, args...) \
        fprintf(stderr, "# %s: " format, __func__, ## args)
#else
#   define DEBUG(format, args...)
#endif

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

struct wld_draw_interface
{
    void (* fill_rectangle)(struct wld_drawable * drawable, uint32_t color,
                            pixman_rectangle16_t * rectangle);
    void (* fill_rectangles)(struct wld_drawable * drawable, uint32_t color,
                             pixman_rectangle16_t * rectangle,
                             uint32_t num_rectangles);
    void (* draw_text_utf8)(struct wld_drawable * drawable,
                            struct font * font, uint32_t color,
                            int32_t x, int32_t y,
                            const char * text, int32_t length);
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

/**
 * This default fill_rectangle method is implemented as fill_rectangles with a
 * length parameter of 1.
 */
void default_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                            pixman_rectangle16_t * rectangle);

/**
 * This default fill_rectangles method is implemented in terms of the singular
 * fill_rectangle.
 */
void default_fill_rectangles(struct wld_drawable * drawable, uint32_t color,
                             pixman_rectangle16_t * rectangles,
                             uint32_t num_rectangles);

/**
 * This is a NO-OP for implementations that do not require flushing.
 */
void default_flush(struct wld_drawable * drawable);

#endif

