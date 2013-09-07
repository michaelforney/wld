/* wld: wld.h
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

#ifndef WLD_H
#define WLD_H 1

#include <stdbool.h>
#include <stdint.h>
#include <pixman.h>
#include <fontconfig/fontconfig.h>

#define __WLD_FOURCC(a, b, c, d) ( (a)          \
                                 | ((b) << 8)   \
                                 | ((c) << 16)  \
                                 | ((d) << 24) )

/**
 * Supported pixel formats.
 *
 * These formats can safely be interchanged with GBM and wl_drm formats.
 */
enum wld_format
{
    WLD_FORMAT_XRGB8888 = __WLD_FOURCC('X', 'R', '2', '4'),
    WLD_FORMAT_ARGB8888 = __WLD_FOURCC('A', 'R', '2', '4')
};

bool wld_lookup_named_color(const char * name, uint32_t * color);

/* Font Handling */
struct wld_extents
{
    uint32_t advance;
};

struct wld_font
{
    uint32_t ascent, descent;
    uint32_t height;
    uint32_t max_advance;
};

/**
 * Create a new font context.
 *
 * This sets up the underlying FreeType library.
 */
struct wld_font_context * wld_font_create_context();

/**
 * Destroy a font context.
 */
void wld_font_destroy_context(struct wld_font_context * context);

/**
 * Open a new font from the given fontconfig match.
 */
struct wld_font * wld_font_open_pattern(struct wld_font_context * context,
                                        FcPattern * match);

/**
 * Open a new font from a fontconfig pattern string.
 */
struct wld_font * wld_font_open_name(struct wld_font_context * context,
                                     const char * name);

/**
 * Close a font.
 */
void wld_font_close(struct wld_font * font);

/**
 * Check if the given font has a particular character (in UTF-32), and if so,
 * load the glyph.
 */
bool wld_font_ensure_char(struct wld_font * font, uint32_t character);

/**
 * Calculate the text extents of the given UTF-8 string.
 *
 * @param length The maximum number of bytes in the string to process
 */
void wld_font_text_extents_utf8_n(struct wld_font * font,
                                  const char * text, int32_t length,
                                  struct wld_extents * extents);

static inline void wld_font_text_extents_utf8(struct wld_font * font,
                                              const char * text,
                                              struct wld_extents * extents)
{
    wld_font_text_extents_utf8_n(font, text, INT32_MAX, extents);
}

/* Drawables */
struct wld_drawable
{
    uint32_t width, height;
    unsigned long pitch;

    const struct wld_draw_interface * interface;
};

/**
 * Destroy a drawable (created with any context).
 */
void wld_destroy_drawable(struct wld_drawable * drawable);

void wld_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                        int32_t x, int32_t y, uint32_t width, uint32_t height);

void wld_fill_region(struct wld_drawable * drawable, uint32_t color,
                     pixman_region32_t * region);

void wld_copy_rectangle(struct wld_drawable * src, struct wld_drawable * dst,
                        int32_t src_x, int32_t src_y,
                        int32_t dst_x, int32_t dst_y,
                        uint32_t width, uint32_t height);

void wld_copy_region(struct wld_drawable * src, struct wld_drawable * dst,
                     pixman_region32_t * region, int32_t dst_x, int32_t dst_y);

/**
 * Draw a UTF-8 text string to the given drawable.
 *
 * @param length    The maximum number of bytes in the string to process
 * @param extents   If not NULL, will be initialized to the extents of the
 *                  drawn text
 */
void wld_draw_text_utf8_n(struct wld_drawable * drawable,
                          struct wld_font * font, uint32_t color,
                          int32_t x, int32_t y,
                          const char * text, int32_t length,
                          struct wld_extents * extents);

static inline void wld_draw_text_utf8(struct wld_drawable * drawable,
                        struct wld_font * font, uint32_t color,
                        int32_t x, int32_t y,
                        const char * text,
                        struct wld_extents * extents)
{
    wld_draw_text_utf8_n(drawable, font, color, x, y, text, INT32_MAX, extents);
}

void wld_write(struct wld_drawable * drawable, const void * data, size_t size);

void wld_flush(struct wld_drawable * drawable);

#endif

