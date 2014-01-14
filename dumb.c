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

#include "drm-private.h"
#include "drm.h"
#include "pixman.h"

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
    struct wld_drawable base;
    struct wld_exporter exporter;
    struct dumb_context * context;
    uint32_t handle;
};

#include "interface/context.h"
#include "interface/drawable.h"
#include "interface/exporter.h"
#define DRM_DRIVER_NAME dumb
#include "interface/drm.h"
IMPL(dumb, context)
IMPL(dumb, drawable)

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

    if (!(drawable = malloc(sizeof *drawable)))
        return NULL;

    drawable_initialize(&drawable->base, &drawable_impl,
                        width, height, format, pitch);
    drawable->context = context;
    drawable->handle = handle;
    exporter_initialize(&drawable->exporter, &exporter_impl);
    drawable_add_exporter(&drawable->base, &drawable->exporter);

    return &drawable->base;
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

/**** Drawable ****/

bool drawable_map(struct wld_drawable * drawable)
{
    struct dumb_drawable * dumb = dumb_drawable(drawable);
    struct drm_mode_map_dumb map_dumb = { .handle = dumb->handle };
    void * data;

    if (drmIoctl(dumb->context->fd, DRM_IOCTL_MODE_MAP_DUMB,
                 &map_dumb) != 0)
    {
        return false;
    }

    data = mmap(NULL, drawable->pitch * drawable->height, PROT_READ | PROT_WRITE,
                MAP_SHARED, dumb->context->fd, map_dumb.offset);

    if (data == MAP_FAILED)
        return false;

    drawable->map.data = data;

    return true;
}

bool drawable_unmap(struct wld_drawable * drawable)
{
    if (munmap(drawable->map.data, drawable->pitch * drawable->height) == -1)
        return false;

    drawable->map.data = NULL;

    return true;
}

void drawable_destroy(struct wld_drawable * drawable_base)
{
    struct dumb_drawable * drawable = dumb_drawable(drawable_base);
    struct drm_mode_destroy_dumb destroy_dumb = {
        .handle = drawable->handle
    };

    drmIoctl(drawable->context->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
    free(drawable);
}

/**** Exporter ****/

bool exporter_export(struct wld_exporter * exporter, struct wld_drawable * base,
                     uint32_t type, union wld_object * object)
{
    struct dumb_drawable * drawable = dumb_drawable(base);

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

