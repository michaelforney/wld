/* wld: wayland-shm.c
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

#define _GNU_SOURCE /* Required for mkostemp */

#include "wayland-shm.h"
#include "wayland.h"
#include "wayland-private.h"
#include "wld-private.h"
#include "pixman.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

struct shm_context
{
    struct wld_context base;

    struct wl_registry * registry;
    struct wl_shm * wl;
    struct wl_array formats;
};

#include "interface/context.h"
IMPL(shm, context)

static void registry_global(void * data, struct wl_registry * registry,
                            uint32_t name, const char * interface,
                            uint32_t version);
static void registry_global_remove(void * data, struct wl_registry * registry,
                                   uint32_t name);

static void shm_format(void * data, struct wl_shm * wl, uint32_t format);

const struct wld_wayland_interface wayland_shm_interface = {
    .create_context = &wld_shm_create_context,
};

const static struct wl_registry_listener registry_listener = {
    .global = &registry_global,
    .global_remove = &registry_global_remove
};

const static struct wl_shm_listener shm_listener = {
    .format = &shm_format,
};

static inline uint32_t wayland_format(uint32_t format)
{
    switch (format)
    {
        case WLD_FORMAT_ARGB8888:
            return WL_SHM_FORMAT_ARGB8888;
        case WLD_FORMAT_XRGB8888:
            return WL_SHM_FORMAT_XRGB8888;
        default:
            return 0;
    }
}

EXPORT
struct wld_context * wld_shm_create_context(struct wl_display * display,
                                            struct wl_event_queue * queue)
{
    struct shm_context * context;

    if (!(context = malloc(sizeof *context)))
        goto error0;

    context_initialize(&context->base, &context_impl);
    context->wl = NULL;
    wl_array_init(&context->formats);

    if (!(context->registry = wl_display_get_registry(display)))
    {
        DEBUG("Couldn't get registry\n");
        goto error1;
    }

    wl_registry_add_listener(context->registry, &registry_listener, context);
    wl_proxy_set_queue((struct wl_proxy *) context->registry, queue);

    /* Wait for wl_shm global. */
    wayland_roundtrip(display, queue);

    if (!context->wl)
    {
        DEBUG("No wl_shm global\n");
        goto error2;
    }

    wl_shm_add_listener(context->wl, &shm_listener, context);

    /* Wait for SHM formats. */
    wayland_roundtrip(display, queue);

    return &context->base;

  error2:
    wl_registry_destroy(context->registry);
  error1:
    wl_array_release(&context->formats);
    free(context);
  error0:
    return NULL;
}

EXPORT
bool wld_shm_has_format(struct wld_context * base, uint32_t format)
{
    struct shm_context * context = shm_context(base);
    uint32_t * supported_format;

    wl_array_for_each(supported_format, &context->formats)
    {
        if (*supported_format == format)
            return true;
    }

    return false;
}

struct wld_renderer * context_create_renderer(struct wld_context * context)
{
    return wld_create_renderer(wld_pixman_context);
}

struct wld_drawable * context_create_drawable(struct wld_context * base,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    struct shm_context * context = shm_context(base);
    struct wld_drawable * drawable;
    struct wld_exporter * exporter;
    char name[] = "/tmp/wld-XXXXXX";
    uint32_t pitch = width * format_bytes_per_pixel(format);
    uint32_t size = height * pitch;
    int fd;
    void * data;
    uint32_t shm_format = wayland_format(format);
    union wld_object object;
    struct wl_shm_pool * pool;
    struct wl_buffer * wl;

    fd = mkostemp(name, O_CLOEXEC);

    if (fd < 0)
        goto error0;

    unlink(name);

    if (ftruncate(fd, size) < 0)
        goto error1;

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED)
        goto error1;

    object.ptr = data;
    drawable = wld_import(wld_pixman_context, WLD_OBJECT_DATA, object,
                          width, height, format, pitch);

    if (!drawable)
        goto error2;

    if (!(pool = wl_shm_create_pool(context->wl, fd, size)))
        goto error3;

    wl = wl_shm_pool_create_buffer(pool, 0, width, height, pitch, shm_format);
    wl_shm_pool_destroy(pool);

    if (!wl)
        goto error3;

    if (!(exporter = wayland_create_exporter(wl)))
        goto error4;

    drawable_add_exporter(drawable, exporter);
    close(fd);

    return drawable;

  error4:
    wl_buffer_destroy(wl);
  error3:
    wld_destroy_drawable(drawable);
  error2:
    munmap(object.ptr, size);
  error1:
    close(fd);
  error0:
    return NULL;
}

struct wld_drawable * context_import(struct wld_context * context,
                                     uint32_t type, union wld_object object,
                                     uint32_t width, uint32_t height,
                                     uint32_t format, uint32_t pitch)
{
    return NULL;
}

void context_destroy(struct wld_context * base)
{
    struct shm_context * context = shm_context(base);

    wl_shm_destroy(context->wl);
    wl_registry_destroy(context->registry);
    wl_array_release(&context->formats);
    free(context);
}

void registry_global(void * data, struct wl_registry * registry, uint32_t name,
                     const char * interface, uint32_t version)
{
    struct shm_context * context = data;

    if (strcmp(interface, "wl_shm") == 0)
        context->wl = wl_registry_bind(registry, name, &wl_shm_interface, 1);
}

void registry_global_remove(void * data, struct wl_registry * registry,
                            uint32_t name)
{
}

void shm_format(void * data, struct wl_shm * wl, uint32_t format)
{
    struct shm_context * context = data;

    *((uint32_t *) wl_array_add(&context->formats, sizeof format)) = format;
}

