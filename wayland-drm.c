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
#include "wayland-drm-client-protocol.h"
#include "wayland.h"
#include "wayland-private.h"
#include "wld-private.h"
#include "drm-private.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <xf86drm.h>

#include <stdio.h>

struct wld_drm_context
{
    struct wl_drm * wl;
    struct wl_registry * registry;
    struct wl_array formats;
    int fd;
    bool authenticated;

    const struct wld_drm_interface * interface;
    void * context;
};

static void registry_global(void * data, struct wl_registry * registry,
                            uint32_t name, const char * interface,
                            uint32_t version);

static void drm_device(void * data, struct wl_drm * wl, const char * name);
static void drm_format(void * data, struct wl_drm * wl, uint32_t format);
static void drm_authenticated(void * data, struct wl_drm * wl);

const struct wld_wayland_interface wayland_drm_interface = {
    .create_context = (wayland_create_context_func_t) &wld_drm_create_context,
    .destroy_context
        = (wayland_destroy_context_func_t) &wld_drm_destroy_context,
    .create_drawable = (wayland_create_drawable_func_t) &wld_drm_create_drawable
};

const static struct wl_registry_listener registry_listener = {
    .global = &registry_global
};

const static struct wl_drm_listener drm_listener = {
    .device = &drm_device,
    .format = &drm_format,
    .authenticated = &drm_authenticated
};

const static struct wld_drm_interface * drm_interfaces[] = {
#if ENABLE_INTEL
    &intel_drm
#endif
};

static const struct wld_drm_interface * find_drm_interface(int fd)
{
    char path[64], id[32];
    uint32_t vendor_id, device_id;
    char * path_part;
    struct stat st;
    FILE * file;
    uint32_t index;

    if (fstat(fd, &st) == -1)
        return NULL;

    snprintf(path, sizeof path, "/sys/dev/char/%u:%u/",
             major(st.st_rdev), minor(st.st_rdev));
    path_part = path + strlen(path);

    strcpy(path_part, "device/vendor");
    file = fopen(path, "r");
    fgets(id, sizeof id, file);
    fclose(file);
    vendor_id = strtoul(id, NULL, 0);

    strcpy(path_part, "device/device");
    file = fopen(path, "r");
    fgets(id, sizeof id, file);
    fclose(file);
    device_id = strtoul(id, NULL, 0);

    for (index = 0; index < ARRAY_LENGTH(drm_interfaces); ++index)
    {
        if (drm_interfaces[index]->device_supported(vendor_id, device_id))
            return drm_interfaces[index];
    }

    return NULL;
}

struct wld_drm_context * wld_drm_create_context(struct wl_display * display,
                                                struct wl_event_queue * queue)
{
    struct wld_drm_context * drm;

    drm = malloc(sizeof *drm);

    if (!drm)
        goto error0;

    drm->wl = NULL;
    drm->fd = -1;
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

    /* Wait for DRM device. */
    wayland_roundtrip(display, queue);

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

    drm->interface = find_drm_interface(drm->fd);

    if (!drm->interface)
    {
        DEBUG("Couldn't find drawable implementation for DRM device\n");
        goto error4;
    }

    drm->context = drm->interface->create_context(drm->fd);

    if (!drm->context)
    {
        DEBUG("Couldn't create context for DRM drawable implementation\n");
        goto error4;
    }

    return drm;

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

void wld_drm_destroy_context(struct wld_drm_context * drm)
{
    drm->interface->destroy_context(drm->context);
    close(drm->fd);
    wl_drm_destroy(drm->wl);
    wl_registry_destroy(drm->registry);
    wl_array_release(&drm->formats);

    free(drm);
}

bool wld_drm_has_format(struct wld_drm_context * drm, uint32_t format)
{
    uint32_t * supported_format;

    wl_array_for_each(supported_format, &drm->formats)
    {
        if (*supported_format == format)
            return true;
    }

    return false;
}

int wld_drm_get_fd(struct wld_drm_context * drm)
{
    return drm->authenticated ? drm->fd : -1;
}

struct wld_drawable * wld_drm_create_drawable(struct wld_drm_context * drm,
                                              struct wl_surface * surface,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    struct wld_drawable * drawable0, * drawable1;
    uint32_t name0, name1, pitch0, pitch1;
    struct wl_buffer * buffer0, * buffer1;

    if (!wld_drm_has_format(drm, format))
        return NULL;

    drawable0 = drm->interface->create_drawable(drm->context, width, height,
                                                format);
    drawable1 = drm->interface->create_drawable(drm->context, width, height,
                                                format);

    drm->interface->get_drawable_info(drawable0, &name0, &pitch0);
    drm->interface->get_drawable_info(drawable1, &name1, &pitch1);

    buffer0 = wl_drm_create_buffer(drm->wl, name0, width, height,
                                   pitch0, format);
    buffer1 = wl_drm_create_buffer(drm->wl, name1, width, height,
                                   pitch1, format);

    return wld_wayland_create_drawable_from_buffers(surface,
                                                    buffer0, drawable0,
                                                    buffer1, drawable1);
}

void registry_global(void * data, struct wl_registry * registry, uint32_t name,
                     const char * interface, uint32_t version)
{
    struct wld_drm_context * drm = data;

    if (strcmp(interface, "wl_drm") == 0)
        drm->wl = wl_registry_bind(registry, name, &wl_drm_interface, 1);
}

void drm_device(void * data, struct wl_drm * wl, const char * name)
{
    struct wld_drm_context * drm = data;
    drm_magic_t magic;

    drm->fd = open(name, O_RDWR);

    if (drm->fd == -1)
    {
        DEBUG("Couldn't open DRM device '%s'\n", name);
        return;
    }

    drmGetMagic(drm->fd, &magic);
    wl_drm_authenticate(wl, magic);
}

void drm_format(void * data, struct wl_drm * wl, uint32_t format)
{
    struct wld_drm_context * drm = data;

    *((uint32_t *) wl_array_add(&drm->formats, sizeof format)) = format;
}

void drm_authenticated(void * data, struct wl_drm * wl)
{
    struct wld_drm_context * drm = data;

    drm->authenticated = true;
}

