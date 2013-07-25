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

void default_fill_region(struct wld_drawable * drawable, uint32_t color,
                         pixman_region32_t * region)
{
    pixman_box32_t * boxes;
    int num_boxes;
    uint32_t index;

    boxes = pixman_region32_rectangles(region, &num_boxes);

    for (index = 0; index < num_boxes; ++index)
    {
        drawable->interface->fill_rectangle
            (drawable, color, boxes[index].x1, boxes[index].y1,
             boxes[index].x2 - boxes[index].x1,
             boxes[index].y2 - boxes[index].y1);
    }
}

void wld_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                        int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    drawable->interface->fill_rectangle(drawable, color, x, y, width, height);
}

void wld_fill_region(struct wld_drawable * drawable, uint32_t color,
                     pixman_region32_t * region)
{
    drawable->interface->fill_region(drawable, color, region);
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

