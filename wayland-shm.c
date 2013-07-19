/* wld: wayland-shm.c
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

struct wld_shm_context
{
    struct wl_registry * registry;
    struct wl_shm * wl;
    struct wl_array formats;

    struct wld_pixman_context * pixman_context;
};

static void registry_global(void * data, struct wl_registry * registry,
                            uint32_t name, const char * interface,
                            uint32_t version);

static void shm_format(void * data, struct wl_shm * wl, uint32_t format);

const struct wld_wayland_interface wayland_shm_interface = {
    .create_context = (wayland_create_context_func_t) &wld_shm_create_context,
    .destroy_context
        = (wayland_destroy_context_func_t) &wld_shm_destroy_context,
    .create_drawable = (wayland_create_drawable_func_t) &wld_shm_create_drawable
};

const static struct wl_registry_listener registry_listener = {
    .global = &registry_global
};

const static struct wl_shm_listener shm_listener = {
    .format = &shm_format,
};

static inline uint32_t wayland_format(enum wld_format format)
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

struct wld_shm_context * wld_shm_create_context(struct wl_display * display,
                                                struct wl_event_queue * queue)
{
    struct wld_shm_context * shm;

    shm = malloc(sizeof *shm);

    if (!shm)
        goto error0;

    shm->wl = NULL;
    wl_array_init(&shm->formats);

    shm->registry = wl_display_get_registry(display);

    if (!shm->registry)
    {
        DEBUG("Couldn't get registry\n");
        goto error1;
    }

    wl_registry_add_listener(shm->registry, &registry_listener, shm);
    wl_proxy_set_queue((struct wl_proxy *) shm->registry, queue);

    shm->pixman_context = wld_pixman_create_context();

    /* Wait for wl_shm global. */
    wayland_roundtrip(display, queue);

    if (!shm->wl)
    {
        DEBUG("No wl_shm global\n");
        goto error2;
    }

    wl_shm_add_listener(shm->wl, &shm_listener, shm);

    /* Wait for SHM formats. */
    wayland_roundtrip(display, queue);

    return shm;

  error2:
    wl_registry_destroy(shm->registry);
  error1:
    wl_array_release(&shm->formats);
    free(shm);
  error0:
    return NULL;
}

void wld_shm_destroy_context(struct wld_shm_context * shm)
{
    wld_pixman_destroy_context(shm->pixman_context);
    wl_shm_destroy(shm->wl);
    wl_registry_destroy(shm->registry);
    wl_array_release(&shm->formats);

    free(shm);
}

bool wld_shm_has_format(struct wld_shm_context * shm, uint32_t format)
{
    uint32_t * supported_format;

    wl_array_for_each(supported_format, &shm->formats)
    {
        if (*supported_format == format)
            return true;
    }

    return false;
}

struct wld_drawable * wld_shm_create_drawable(struct wld_shm_context * shm,
                                              struct wl_surface * surface,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    char name[] = "/tmp/wld-XXXXXX";
    uint32_t pitch = width * format_bytes_per_pixel(format);
    uint32_t size = height * pitch;
    int fd;
    void * data;
    struct wl_shm_pool * pool;
    struct wl_buffer * buffer0, * buffer1;
    struct wld_drawable * drawable0, * drawable1;
    uint32_t shm_format = wayland_format(format);

    fd = mkostemp(name, O_CLOEXEC);

    if (fd < 0)
        goto error0;

    unlink(name);

    if (ftruncate(fd, size * 2) < 0)
        goto error1;

    data = mmap(NULL, size * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED)
        goto error1;

    pool = wl_shm_create_pool(shm->wl, fd, size * 2);
    buffer0 = wl_shm_pool_create_buffer(pool, 0, width, height, pitch,
                                        shm_format);
    buffer1 = wl_shm_pool_create_buffer(pool, size, width, height, pitch,
                                        shm_format);
    wl_shm_pool_destroy(pool);
    close(fd);

    drawable0 = wld_pixman_create_drawable(shm->pixman_context, width, height,
                                           data, pitch, format);
    drawable1 = wld_pixman_create_drawable(shm->pixman_context, width, height,
                                           data + size, pitch, format);

    return wld_wayland_create_drawable_from_buffers(surface,
                                                    buffer0, drawable0,
                                                    buffer1, drawable1);

  error1:
    close(fd);
  error0:
    return NULL;
}

void registry_global(void * data, struct wl_registry * registry, uint32_t name,
                     const char * interface, uint32_t version)
{
    struct wld_shm_context * shm = data;

    if (strcmp(interface, "wl_shm") == 0)
        shm->wl = wl_registry_bind(registry, name, &wl_shm_interface, 1);
}

void shm_format(void * data, struct wl_shm * wl, uint32_t format)
{
    struct wld_shm_context * shm = data;

    *((uint32_t *) wl_array_add(&shm->formats, sizeof format)) = format;
}

