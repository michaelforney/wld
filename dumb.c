/* wld: dumb.c
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

#include "pixman.h"
#include "pixman-private.h"
#include "drm-private.h"
#include "drm.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include <errno.h>

struct dumb_context
{
    struct wld_context base;
    int fd;
};

struct dumb_drawable
{
    struct pixman_drawable pixman;
    struct wld_exporter exporter;
    struct dumb_context * context;
    uint32_t handle;
};

#define DRM_DRIVER_NAME dumb
#include "interface/context.h"
#include "interface/exporter.h"
#include "interface/drm.h"
IMPL(dumb, context)

const struct wld_context_impl * dumb_context_impl = &context_impl;

bool driver_device_supported(uint32_t vendor_id, uint32_t device_id)
{
    return true;
}

struct wld_context * driver_create_context(int drm_fd)
{
    struct dumb_context * context;

    if (!(context = malloc(sizeof *context)))
        return NULL;

    context_initialize(&context->base, &context_impl);
    context->fd = drm_fd;

    return &context->base;
}

struct wld_renderer * context_create_renderer(struct wld_context * context)
{
    return wld_create_renderer(wld_pixman_context);
}

static struct wld_drawable * new_drawable(struct dumb_context * context,
                                          uint32_t width, uint32_t height,
                                          uint32_t format, uint32_t handle,
                                          unsigned long pitch)
{
    struct dumb_drawable * drawable;
    struct drm_mode_map_dumb map_dumb_arg = { .handle = handle };
    void * data;
    size_t size = pitch * height;
    int ret;

    if (!(drawable = malloc(sizeof *drawable)))
        goto error0;

    ret = drmIoctl(context->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_arg);

    if (ret != 0)
        goto error1;

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, context->fd,
                map_dumb_arg.offset);

    if (data == MAP_FAILED)
        goto error1;

    if (!pixman_initialize_drawable(wld_pixman_context, &drawable->pixman,
                                    width, height, data, pitch, format))
    {
        goto error2;
    }

    drawable->context = context;
    drawable->handle = handle;
    exporter_initialize(&drawable->exporter, &exporter_impl);
    drawable_add_exporter(&drawable->pixman.base, &drawable->exporter);

    return &drawable->pixman.base;

  error2:
    munmap(data, size);
  error1:
    free(drawable);
  error0:
    return NULL;
}

struct wld_drawable * context_create_drawable(struct  wld_context * base,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    struct dumb_context * context = dumb_context(base);
    struct wld_drawable * drawable;
    struct drm_mode_create_dumb create_dumb_arg = {
        .height = height, .width = width,
        .bpp = format_bytes_per_pixel(format) * 8,
    };
    int ret;

    ret = drmIoctl(context->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_arg);

    if (ret != 0)
        goto error0;

    drawable = new_drawable(context, width, height, format,
                            create_dumb_arg.handle, create_dumb_arg.pitch);

    if (!drawable)
        goto error1;

    return drawable;

  error1:
    {
        struct drm_mode_destroy_dumb destroy_dumb_arg = {
            .handle = create_dumb_arg.handle
        };

        drmIoctl(context->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb_arg);
    }
  error0:
    return NULL;
}

struct wld_drawable * context_import(struct wld_context * base,
                                     uint32_t type, union wld_object object,
                                     uint32_t width, uint32_t height,
                                     uint32_t format, uint32_t pitch)
{
    struct dumb_context * context = dumb_context(base);
    uint32_t handle;

    switch (type)
    {
        case WLD_DRM_OBJECT_PRIME_FD:
            if (drmPrimeFDToHandle(context->fd, object.i, &handle) != 0)
                return NULL;
            break;
        case WLD_DRM_OBJECT_GEM_NAME:
        {
            struct drm_gem_open gem_open = { .name = object.u32 };

            if (drmIoctl(context->fd, DRM_IOCTL_GEM_OPEN, &gem_open) != 0)
                return NULL;

            handle = gem_open.handle;
            break;
        }
        default: return NULL;
    }

    return new_drawable(context, width, height, format, handle, pitch);
}

void context_destroy(struct wld_context * base)
{
    struct dumb_context * context = dumb_context(base);

    close(context->fd);
    free(context);
}

/**** Exporter ****/

bool exporter_export(struct wld_exporter * exporter, struct wld_drawable * base,
                     uint32_t type, union wld_object * object)
{
    struct dumb_drawable * drawable = (void *) base;

    switch (type)
    {
        case WLD_DRM_OBJECT_HANDLE:
            object->u32 = drawable->handle;
            return true;
        case WLD_DRM_OBJECT_PRIME_FD:
            if (drmPrimeHandleToFD(drawable->context->fd, drawable->handle,
                                   DRM_CLOEXEC, &object->i) != 0)
            {
                return false;
            }

            return true;
        case WLD_DRM_OBJECT_GEM_NAME:
        {
            struct drm_gem_flink flink = { .handle = drawable->handle };

            if (drmIoctl(drawable->context->fd, DRM_IOCTL_GEM_FLINK,
                         &flink) != 0)
            {
                return false;
            }

            object->u32 = flink.name;
            return true;
        }
        default:
            return false;
    }
}

void exporter_destroy(struct wld_exporter * exporter)
{
}

