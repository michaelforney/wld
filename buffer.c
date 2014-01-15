/* wld: buffer.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

void buffer_initialize(struct wld_buffer * buffer,
                       const struct wld_buffer_impl * impl,
                       uint32_t width, uint32_t height,
                       uint32_t format, uint32_t pitch)
{
    *((const struct wld_buffer_impl **) &buffer->impl) = impl;
    buffer->width = width;
    buffer->height = height;
    buffer->format = format;
    buffer->pitch = pitch;
    buffer->map.data = NULL;
    buffer->map.count = 0;
    buffer->exporters = NULL;
}

void buffer_add_exporter(struct wld_buffer * buffer,
                         struct wld_exporter * exporter)
{
    exporter->next = buffer->exporters;
    buffer->exporters = exporter;
}

void exporter_initialize(struct wld_exporter * exporter,
                         const struct wld_exporter_impl * impl)
{
    *((const struct wld_exporter_impl **) &exporter->impl) = impl;
    exporter->next = NULL;
}

EXPORT
bool wld_map(struct wld_buffer * buffer)
{
    if (buffer->map.count == 0 && !buffer->impl->map(buffer))
        return false;

    ++buffer->map.count;
    return true;
}

EXPORT
bool wld_unmap(struct wld_buffer * buffer)
{
    if (buffer->map.count == 0
        || (buffer->map.count == 1 && !buffer->impl->unmap(buffer)))
    {
        return false;
    }

    --buffer->map.count;
    return true;
}

EXPORT
bool wld_export(struct wld_buffer * buffer,
                uint32_t type, union wld_object * object)
{
    struct wld_exporter * exporter;

    for (exporter = buffer->exporters; exporter; exporter = exporter->next)
    {
        if (exporter->impl->export(exporter, buffer, type, object))
            return true;
    }

    return false;
}

EXPORT
void wld_destroy_buffer(struct wld_buffer * buffer)
{
    struct wld_exporter * exporter, * next;

    if (buffer->map.count > 0)
        wld_unmap(buffer);

    for (exporter = buffer->exporters, next = exporter ? exporter->next : NULL;
         exporter; exporter = next, next = exporter ? exporter->next : NULL)
    {
        exporter->impl->destroy(exporter);
    }

    buffer->impl->destroy(buffer);
}

