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

#include <assert.h>

void default_fill_region(struct wld_drawable * drawable, uint32_t color,
                         pixman_region32_t * region)
{
    pixman_box32_t * box;
    int num_boxes;

    box = pixman_region32_rectangles(region, &num_boxes);

    while (num_boxes--)
    {
        drawable->impl->fill_rectangle(drawable, color, box->x1, box->y1,
                                       box->x2 - box->x1,
                                       box->y2 - box->y1);
        ++box;
    }
}

void default_copy_region(struct wld_drawable * src, struct wld_drawable * dst,
                         pixman_region32_t * region,
                         int32_t dst_x, int32_t dst_y)
{
    pixman_box32_t * box;
    int num_boxes;

    box = pixman_region32_rectangles(region, &num_boxes);

    while (num_boxes--)
    {
        dst->impl->copy_rectangle(src, dst, box->x1, box->y1,
                                  dst_x + box->x1, dst_y + box->y1,
                                  box->x2 - box->x1, box->y2 - box->y1);
        ++box;
    }
}

void drawable_initialize(struct wld_drawable * drawable,
                         const struct wld_drawable_impl * impl,
                         uint32_t width, uint32_t height,
                         uint32_t format, uint32_t pitch)
{
    *((const struct wld_drawable_impl **) &drawable->impl) = impl;
    drawable->width = width;
    drawable->height = height;
    drawable->format = format;
    drawable->pitch = pitch;
    drawable->exporters = NULL;
}

void drawable_add_exporter(struct wld_drawable * drawable,
                           struct wld_exporter * exporter)
{
    exporter->next = drawable->exporters;
    drawable->exporters = exporter;
}

void exporter_initialize(struct wld_exporter * exporter,
                         const struct wld_exporter_impl * impl)
{
    *((const struct wld_exporter_impl **) &exporter->impl) = impl;
    exporter->next = NULL;
}

EXPORT
void wld_fill_rectangle(struct wld_drawable * drawable, uint32_t color,
                        int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    drawable->impl->fill_rectangle(drawable, color, x, y, width, height);
}

EXPORT
void wld_fill_region(struct wld_drawable * drawable, uint32_t color,
                     pixman_region32_t * region)
{
    drawable->impl->fill_region(drawable, color, region);
}

EXPORT
void wld_copy_rectangle(struct wld_drawable * src, struct wld_drawable * dst,
                        int32_t src_x, int32_t src_y,
                        int32_t dst_x, int32_t dst_y,
                        uint32_t width, uint32_t height)
{
    assert(src->impl == dst->impl);
    dst->impl->copy_rectangle(src, dst, src_x, src_y, dst_x, dst_y,
                              width, height);
}

EXPORT
void wld_copy_region(struct wld_drawable * src, struct wld_drawable * dst,
                     pixman_region32_t * region, int32_t dst_x, int32_t dst_y)
{
    assert(src->impl == dst->impl);
    dst->impl->copy_region(src, dst, region, dst_x, dst_y);
}

EXPORT
void wld_draw_text_n(struct wld_drawable * drawable,
                          struct wld_font * font_base, uint32_t color,
                          int32_t x, int32_t y,
                          const char * text, int32_t length,
                          struct wld_extents * extents)
{
    struct font * font = (void *) font_base;

    drawable->impl->draw_text(drawable, font, color, x, y,
                              text, length, extents);
}

EXPORT
void wld_destroy_drawable(struct wld_drawable * drawable)
{
    drawable->impl->destroy(drawable);
}

EXPORT
void wld_write(struct wld_drawable * drawable, const void * data, size_t size)
{
    drawable->impl->write(drawable, data, size);
}

EXPORT
pixman_image_t * wld_map(struct wld_drawable * drawable)
{
    return drawable->impl->map(drawable);
}

EXPORT
bool wld_export(struct wld_drawable * drawable,
                uint32_t type, union wld_object * object)
{
    struct wld_exporter * exporter;

    for (exporter = drawable->exporters; exporter; exporter = exporter->next)
    {
        if (exporter->impl->export(exporter, drawable, type, object))
            return true;
    }

    return false;
}

EXPORT
void wld_flush(struct wld_drawable * drawable)
{
    drawable->impl->flush(drawable);
}

