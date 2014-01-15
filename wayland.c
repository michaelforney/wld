/* wld: wayland.c
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

#include "wayland.h"
#include "wayland-private.h"
#include "wld-private.h"

#include <stdlib.h>
#include <wayland-client.h>

struct wayland_exporter
{
    struct wld_exporter base;
    struct wl_buffer * buffer;
};

#include "interface/exporter.h"
IMPL(wayland, exporter)

static void sync_done(void * data, struct wl_callback * callback,
                      uint32_t msecs);

static const struct wl_callback_listener sync_listener = {
    .done = &sync_done
};

const static struct wld_wayland_interface * interfaces[] = {
#if WITH_WAYLAND_DRM
    [WLD_DRM] = &wayland_drm_interface,
#endif

#if WITH_WAYLAND_SHM
    [WLD_SHM] = &wayland_shm_interface
#endif
};

enum wld_wayland_interface_id interface_id(const char * string)
{
    if (strcmp(string, "drm") == 0)
        return WLD_DRM;
    if (strcmp(string, "shm") == 0)
        return WLD_SHM;

    fprintf(stderr, "Unknown Wayland interface specified: '%s'\n", string);

    return WLD_NONE;
}

EXPORT
struct wld_context * wld_wayland_create_context
    (struct wl_display * display, enum wld_wayland_interface_id id, ...)
{
    struct wld_context * context = NULL;
    struct wl_event_queue * queue;
    va_list requested_interfaces;
    bool interfaces_tried[ARRAY_LENGTH(interfaces)] = {0};
    const char * interface_string;

    if (!(queue = wl_display_create_queue(display)))
        return NULL;

    if ((interface_string = getenv("WLD_WAYLAND_INTERFACE")))
    {
        id = interface_id(interface_string);

        if ((context = interfaces[id]->create_context(display, queue)))
            return context;

        fprintf(stderr, "Could not create context for Wayland interface '%s'\n",
                interface_string);

        return NULL;
    }

    va_start(requested_interfaces, id);

    while (id >= 0)
    {
        if (interfaces_tried[id] || !interfaces[id])
            continue;

        if ((context = interfaces[id]->create_context(display, queue)))
            break;

        interfaces_tried[id] = true;
        id = va_arg(requested_interfaces, enum wld_wayland_interface_id);
    }

    va_end(requested_interfaces);

    /* If the user specified WLD_ANY, try any remaining interfaces. */
    if (!context && id == WLD_ANY)
    {
        for (id = 0; id < ARRAY_LENGTH(interfaces); ++id)
        {
            if (interfaces_tried[id] || !interfaces[id])
                continue;

            if ((context = interfaces[id]->create_context(display, queue)))
                break;
        }
    }

    if (!context)
    {
        DEBUG("Could not initialize any of the specified interfaces\n");
        return NULL;
    }

    return context;
}

int wayland_roundtrip(struct wl_display * display,
                      struct wl_event_queue * queue)
{
    struct wl_callback * callback;
    bool done = false;
    int ret = 0;

    callback = wl_display_sync(display);
    wl_callback_add_listener(callback, &sync_listener, &done);
    wl_proxy_set_queue((struct wl_proxy *) callback, queue);

    while (!done && ret >= 0)
        ret = wl_display_dispatch_queue(display, queue);

    if (ret == -1 && !done)
        wl_callback_destroy(callback);

    return ret;
}

struct wld_exporter * wayland_create_exporter(struct wl_buffer * buffer)
{
    struct wayland_exporter * exporter;

    if (!(exporter = malloc(sizeof *exporter)))
        return NULL;

    exporter_initialize(&exporter->base, &exporter_impl);
    exporter->buffer = buffer;

    return &exporter->base;
}

bool exporter_export(struct wld_exporter * base, struct wld_buffer * buffer,
                     uint32_t type, union wld_object * object)
{
    struct wayland_exporter * exporter = wayland_exporter(base);

    switch (type)
    {
        case WLD_WAYLAND_OBJECT_BUFFER:
            object->ptr = exporter->buffer;
            return true;
        default: return false;
    }
}

void exporter_destroy(struct wld_exporter * base)
{
    struct wayland_exporter * exporter = wayland_exporter(base);

    wl_buffer_destroy(exporter->buffer);
    free(exporter);
}

void sync_done(void * data, struct wl_callback * callback, uint32_t msecs)
{
    bool * done = data;

    *done = true;
    wl_callback_destroy(callback);
}

