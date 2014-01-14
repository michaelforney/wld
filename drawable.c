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
void wld_destroy_drawable(struct wld_drawable * drawable)
{
    drawable->impl->destroy(drawable);
}

