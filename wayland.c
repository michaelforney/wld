/* wld: wayland.c
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

#include "wayland.h"
#include "wayland-private.h"
#include "wld-private.h"

#include <stdlib.h>
#include <wayland-client.h>

#define BACKBUF(drawable) (drawable)->buffers[(drawable)->front_buffer ^ 1]
#define FRONTBUF(drawable) (drawable)->buffers[(drawable)->front_buffer]

struct wld_wayland_context
{
    struct wl_display * display;
    struct wl_event_queue * queue;
    const struct wld_wayland_interface * interface;
    void * context;
};

struct wayland_drawable
{
    struct wld_drawable base;

    struct wld_wayland_context * context;
    struct wl_surface * surface;
    struct
    {
        struct wl_buffer * wl;
        struct wld_drawable * drawable;
        bool busy;
    } buffers[2];
    uint8_t front_buffer;
};

_Static_assert(offsetof(struct wayland_drawable, base) == 0,
               "Non-zero offset of base field");

static void sync_done(void * data, struct wl_callback * callback,
                      uint32_t msecs);

static void buffer_release(void * data, struct wl_buffer * buffer);

static void wayland_fill_rectangle(struct wld_drawable * drawable,
                                   uint32_t color, int32_t x, int32_t y,
                                   uint32_t width, uint32_t height);
static void wayland_fill_region(struct wld_drawable * drawable, uint32_t color,
                                pixman_region32_t * region);
static void wayland_copy_rectangle(struct wld_drawable * src,
                                   struct wld_drawable * dst,
                                   int32_t src_x, int32_t src_y,
                                   int32_t dst_x, int32_t dst_y,
                                   uint32_t width, uint32_t height);
static void wayland_copy_region(struct wld_drawable * src,
                                struct wld_drawable * dst,
                                pixman_region32_t * region,
                                int32_t dst_x, int32_t dst_y);
static void wayland_draw_text_utf8(struct wld_drawable * drawable,
                                   struct font * font, uint32_t color,
                                   int32_t x, int32_t y,
                                   const char * text, int32_t length,
                                   struct wld_extents * extents);
static void wayland_flush(struct wld_drawable * drawable);
static void wayland_destroy(struct wld_drawable * drawable);

const struct wl_callback_listener sync_listener = {
    .done = &sync_done
};

const struct wl_buffer_listener buffer_listener = {
    .release = &buffer_release
};

const struct wld_draw_interface wayland_draw = {
    .fill_rectangle = &wayland_fill_rectangle,
    .fill_region = &wayland_fill_region,
    .copy_rectangle = &wayland_copy_rectangle,
    .copy_region = &wayland_copy_region,
    .draw_text_utf8 = &wayland_draw_text_utf8,
    .flush = &wayland_flush,
    .destroy = &wayland_destroy
};

const static struct wld_wayland_interface * interfaces[] = {
#if ENABLE_WAYLAND_DRM
    [WLD_DRM] = &wayland_drm_interface,
#endif

#if ENABLE_WAYLAND_SHM
    [WLD_SHM] = &wayland_shm_interface
#endif
};

struct wld_wayland_context * wld_wayland_create_context
    (struct wl_display * display, enum wld_wayland_interface_id id, ...)
{
    struct wld_wayland_context * wayland;
    va_list requested_interfaces;
    bool interfaces_tried[ARRAY_LENGTH(interfaces)] = {0};

    wayland = malloc(sizeof *wayland);

    if (!wayland)
        goto error0;

    wayland->display = display;
    wayland->queue = wl_display_create_queue(display);
    wayland->context = NULL;
    wayland->interface = NULL;

    va_start(requested_interfaces, id);

    while (id >= 0)
    {
        if (interfaces_tried[id] || !interfaces[id])
            continue;

        wayland->context
            = interfaces[id]->create_context(display, wayland->queue);

        if (wayland->context)
        {
            wayland->interface = interfaces[id];
            break;
        }

        interfaces_tried[id] = true;
        id = va_arg(requested_interfaces, enum wld_wayland_interface_id);
    }

    va_end(requested_interfaces);

    /* If the user specified WLD_ANY, try any remaining interfaces. */
    if (!wayland->context && id == WLD_ANY)
    {
        for (id = 0; id < ARRAY_LENGTH(interfaces); ++id)
        {
            if (interfaces_tried[id] || !interfaces[id])
                continue;

            wayland->context
                = interfaces[id]->create_context(display, wayland->queue);

            if (wayland->context)
            {
                wayland->interface = interfaces[id];
                break;
            }
        }
    }

    if (!wayland->context)
    {
        DEBUG("Could not initialize any of the specified interfaces\n");
        goto error2;
    }

    return wayland;

  error2:
    wl_event_queue_destroy(wayland->queue);
  error1:
    free(wayland);
  error0:
    return NULL;
}

void wld_wayland_destroy_context(struct wld_wayland_context * wayland)
{
    wayland->interface->destroy_context(wayland->context);
    free(wayland);
}

struct wld_drawable * wld_wayland_create_drawable
    (struct wld_wayland_context * context, struct wl_surface * surface,
     uint32_t width, uint32_t height, enum wld_format format)
{
    struct wayland_drawable * wayland;

    wayland = malloc(sizeof *wayland);

    if (!wayland)
        goto error0;

    wayland->context = context;
    wayland->buffers[0].drawable = context->interface->create_drawable
        (context->context, width, height, format, &wayland->buffers[0].wl);

    if (!wayland->buffers[0].drawable)
        goto error1;

    wayland->buffers[1].drawable = context->interface->create_drawable
        (context->context, width, height, format, &wayland->buffers[1].wl);

    if (!wayland->buffers[1].drawable)
        goto error2;

    wayland->buffers[0].busy = false;
    wayland->buffers[1].busy = false;
    wayland->front_buffer = 0;
    wayland->surface = surface;

    wayland->base.interface = &wayland_draw;
    wayland->base.width = width;
    wayland->base.height = height;

    wl_buffer_add_listener(wayland->buffers[0].wl, &buffer_listener,
                           &wayland->buffers[0].busy);
    wl_buffer_add_listener(wayland->buffers[1].wl, &buffer_listener,
                           &wayland->buffers[1].busy);

    return &wayland->base;

  error2:
    wld_destroy_drawable(wayland->buffers[0].drawable);
  error1:
    free(wayland);
  error0:
    return NULL;
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

void sync_done(void * data, struct wl_callback * callback, uint32_t msecs)
{
    bool * done = data;

    *done = true;

    wl_callback_destroy(callback);
}

void buffer_release(void * data, struct wl_buffer * buffer)
{
    bool * busy = data;

    *busy = false;
}

static void wait_for_backbuf(struct wayland_drawable * wayland)
{
    if (BACKBUF(wayland).busy)
    {
        int ret, count = 0;

        do {
            ret = wl_display_dispatch_queue(wayland->context->display,
                                            wayland->context->queue);
        } while (BACKBUF(wayland).busy && ret != -1);
    }
}

static void begin(struct wayland_drawable * wayland)
{
    /* Wait for the back buffer to become available. */
    wait_for_backbuf(wayland);
}

static void wayland_fill_rectangle(struct wld_drawable * drawable,
                                   uint32_t color, int32_t x, int32_t y,
                                   uint32_t width, uint32_t height)
{
    struct wayland_drawable * wayland = (void *) drawable;

    begin(wayland);
    wld_fill_rectangle(BACKBUF(wayland).drawable, color, x, y, width, height);
}

static void wayland_fill_region(struct wld_drawable * drawable, uint32_t color,
                                pixman_region32_t * region)
{
    struct wayland_drawable * wayland = (void *) drawable;

    begin(wayland);
    wld_fill_region(BACKBUF(wayland).drawable, color, region);
}

static void wayland_copy_rectangle(struct wld_drawable * src_drawable,
                                   struct wld_drawable * dst_drawable,
                                   int32_t src_x, int32_t src_y,
                                   int32_t dst_x, int32_t dst_y,
                                   uint32_t width, uint32_t height)
{
    fprintf(stderr, "wayland: Copy rectangle is not implemented\n");
}

static void wayland_copy_region(struct wld_drawable * src_drawable,
                                struct wld_drawable * dst_drawable,
                                pixman_region32_t * region,
                                int32_t dst_x, int32_t dst_y)
{
    fprintf(stderr, "wayland: Copy region is not implemented\n");
}

static void wayland_draw_text_utf8(struct wld_drawable * drawable,
                                   struct font * font, uint32_t color,
                                   int32_t x, int32_t y,
                                   const char * text, int32_t length,
                                   struct wld_extents * extents)
{
    struct wayland_drawable * wayland = (void *) drawable;

    begin(wayland);
    wld_draw_text_utf8_n(BACKBUF(wayland).drawable, &font->base, color,
                         x, y, text, length, extents);
}

static void wayland_flush(struct wld_drawable * drawable)
{
    struct wayland_drawable * wayland = (void *) drawable;

    wait_for_backbuf(wayland);

    wld_flush(BACKBUF(wayland).drawable);
    wl_surface_attach(wayland->surface, BACKBUF(wayland).wl, 0, 0);
    wl_surface_commit(wayland->surface);

    BACKBUF(wayland).busy = true;

    wayland->front_buffer ^= 1;
}

static void wayland_destroy(struct wld_drawable * drawable)
{
    struct wayland_drawable * wayland = (void *) drawable;

    wl_buffer_destroy(wayland->buffers[0].wl);
    wl_buffer_destroy(wayland->buffers[1].wl);
    wld_destroy_drawable(wayland->buffers[0].drawable);
    wld_destroy_drawable(wayland->buffers[1].drawable);
}

