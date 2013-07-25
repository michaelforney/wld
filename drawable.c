/* wld: drawable.c
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

#include "wld-private.h"

void default_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                            pixman_rectangle16_t * rectangle)
{
    wld_fill_rectangles(drawable, color, rectangle, 1);
}

void default_fill_rectangles(struct wld_drawable * drawable, uint32_t color,
                             pixman_rectangle16_t * rectangles,
                             uint32_t num_rectangles)
{
    while (num_rectangles--)
        wld_fill_rectangle(drawable, color, &rectangles[num_rectangles]);
}

void wld_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                        pixman_rectangle16_t * rectangle)
{
    drawable->interface->fill_rectangle(drawable, color, rectangle);
}

void wld_fill_rectangles(struct wld_drawable * drawable, uint32_t color,
                         pixman_rectangle16_t * rectangles,
                         uint32_t num_rectangles)
{
    drawable->interface->fill_rectangles(drawable, color, rectangles,
                                         num_rectangles);
}

void wld_draw_text_utf8_n(struct wld_drawable * drawable,
                          struct wld_font * font_base, uint32_t color,
                          int32_t x, int32_t y,
                          const char * text, int32_t length)
{
    struct font * font = (void *) font_base;

    drawable->interface->draw_text_utf8(drawable, font, color, x, y,
                                        text, length);
}

void wld_destroy_drawable(struct wld_drawable * drawable)
{
    drawable->interface->destroy(drawable);
}

void wld_flush(struct wld_drawable * drawable)
{
    drawable->interface->flush(drawable);
}

