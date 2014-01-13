/* wld: wayland-drm.c
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

#include "wayland-drm.h"
#include "protocol/wayland-drm-client-protocol.h"
#include "wayland.h"
#include "drm.h"
#include "wayland-private.h"
#include "wld-private.h"
#include "drm-private.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <xf86drm.h>

#include <stdio.h>

struct drm_context
{
    struct wld_context base;
    struct wld_context * driver_context;
    struct wl_drm * wl;
    struct wl_registry * registry;
    struct wl_array formats;
    uint32_t capabilities;
    int fd;
    bool authenticated;
};

#include "interface/context.h"
IMPL(drm, context);

static void registry_global(void * data, struct wl_registry * registry,
                            uint32_t name, const char * interface,
                            uint32_t version);
static void registry_global_remove(void * data, struct wl_registry * registry,
                                   uint32_t name);

static void drm_device(void * data, struct wl_drm * wl, const char * name);
static void drm_format(void * data, struct wl_drm * wl, uint32_t format);
static void drm_authenticated(void * data, struct wl_drm * wl);
static void drm_capabilities(void * data, struct wl_drm * wl,
                             uint32_t capabilities);

const struct wld_wayland_interface wayland_drm_interface = {
    .create_context = &wld_wayland_drm_create_context,
};

const static struct wl_registry_listener registry_listener = {
    .global = &registry_global,
    .global_remove = &registry_global_remove
};

const static struct wl_drm_listener drm_listener = {
    .device = &drm_device,
    .format = &drm_format,
    .authenticated = &drm_authenticated,
    .capabilities = &drm_capabilities
};

EXPORT
struct wld_context * wld_wayland_drm_create_context(struct wl_display * display,
                                                    struct wl_event_queue * queue)
{
    struct drm_context * drm;

    drm = malloc(sizeof *drm);

    if (!drm)
        goto error0;

    context_initialize(&drm->base, &context_impl);
    drm->wl = NULL;
    drm->fd = -1;
    drm->capabilities = 0;
    wl_array_init(&drm->formats);

    drm->registry = wl_display_get_registry(display);

    if (!drm->registry)
        goto error1;

    wl_registry_add_listener(drm->registry, &registry_listener, drm);
    wl_proxy_set_queue((struct wl_proxy *) drm->registry, queue);

    /* Wait for wl_drm global. */
    wayland_roundtrip(display, queue);

    if (!drm->wl)
    {
        DEBUG("No wl_drm global\n");
        goto error2;
    }

    wl_drm_add_listener(drm->wl, &drm_listener, drm);

    /* Wait for DRM capabilities and device. */
    wayland_roundtrip(display, queue);

    if (!(drm->capabilities & WL_DRM_CAPABILITY_PRIME))
    {
        DEBUG("No PRIME support\n");
        goto error3;
    }

    if (drm->fd == -1)
    {
        DEBUG("No DRM device\n");
        goto error3;
    }

    /* Wait for DRM authentication. */
    wayland_roundtrip(display, queue);

    if (!drm->authenticated)
    {
        DEBUG("DRM authentication failed\n");
        goto error4;
    }

    if (!(drm->driver_context = wld_drm_create_context(drm->fd)))
    {
        DEBUG("Couldn't initialize context for DRM device\n");
        goto error4;
    }

    return &drm->base;

  error4:
    close(drm->fd);
  error3:
    wl_drm_destroy(drm->wl);
  error2:
    wl_registry_destroy(drm->registry);
  error1:
    wl_array_release(&drm->formats);
    free(drm);
  error0:
    return NULL;
}

EXPORT
bool wld_wayland_drm_has_format(struct wld_context * base, uint32_t format)
{
    struct drm_context * context = drm_context(base);
    uint32_t * supported_format;

    wl_array_for_each(supported_format, &context->formats)
    {
        if (*supported_format == format)
            return true;
    }

    return false;
}

EXPORT
int wld_wayland_drm_get_fd(struct wld_context * base)
{
    struct drm_context * context = drm_context(base);

    return context->authenticated ? context->fd : -1;
}

struct wld_drawable * context_create_drawable(struct wld_context * base,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    struct drm_context * context = drm_context(base);
    struct wld_drawable * drawable;
    struct wld_exporter * exporter;
    union wld_object object;
    struct wl_buffer * wl;

    if (!wld_wayland_drm_has_format(&context->base, format))
        goto error0;

    drawable = wld_create_drawable(context->driver_context, width, height, format);

    if (!drawable)
        goto error0;

    if (!wld_export(drawable, WLD_DRM_OBJECT_PRIME_FD, &object))
        goto error1;

    wl = wl_drm_create_prime_buffer(context->wl, object.i, width, height,
                                    format, 0, drawable->pitch, 0, 0, 0, 0);
    close(object.i);

    if (!wl)
        goto error1;

    if (!(exporter = wayland_create_exporter(wl)))
        goto error2;

    drawable_add_exporter(drawable, exporter);

    return drawable;

  error2:
    wl_buffer_destroy(wl);
  error1:
    wld_destroy_drawable(drawable);
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
    struct drm_context * context = drm_context(base);

    wld_destroy_context(context->driver_context);
    close(context->fd);
    wl_drm_destroy(context->wl);
    wl_registry_destroy(context->registry);
    wl_array_release(&context->formats);
    free(context);
}

void registry_global(void * data, struct wl_registry * registry, uint32_t name,
                     const char * interface, uint32_t version)
{
    struct drm_context * context = data;

    if (strcmp(interface, "wl_drm") == 0 && version >= 2)
        context->wl = wl_registry_bind(registry, name, &wl_drm_interface, 2);
}

void registry_global_remove(void * data, struct wl_registry * registry,
                            uint32_t name)
{
}

void drm_device(void * data, struct wl_drm * wl, const char * name)
{
    struct drm_context * context = data;
    drm_magic_t magic;

    context->fd = open(name, O_RDWR);

    if (context->fd == -1)
    {
        DEBUG("Couldn't open DRM device '%s'\n", name);
        return;
    }

    drmGetMagic(context->fd, &magic);
    wl_drm_authenticate(wl, magic);
}

void drm_format(void * data, struct wl_drm * wl, uint32_t format)
{
    struct drm_context * context = data;

    *((uint32_t *) wl_array_add(&context->formats, sizeof format)) = format;
}

void drm_authenticated(void * data, struct wl_drm * wl)
{
    struct drm_context * context = data;

    context->authenticated = true;
}

void drm_capabilities(void * data, struct wl_drm * wl, uint32_t capabilities)
{
    struct drm_context * context = data;

    context->capabilities = capabilities;
}

