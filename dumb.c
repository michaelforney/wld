/* wld: dumb.c
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

#include "pixman.h"
#include "pixman-private.h"
#include "drm-private.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include <errno.h>

struct dumb_context
{
    struct wld_pixman_context * pixman;
    int fd;
};

struct dumb_drawable
{
    struct pixman_drawable pixman;
    struct dumb_context * context;
    uint32_t handle;
};

/* DRM implementation */
static bool dumb_device_supported(uint32_t vendor_id, uint32_t device_id);
static struct dumb_context * dumb_create_context(int drm_fd);
static void dumb_destroy_context(struct dumb_context * context);
static struct wld_drawable * dumb_create_drawable
    (struct dumb_context * context, uint32_t width, uint32_t height,
     uint32_t format);
static struct wld_drawable * dumb_import
    (struct dumb_context * context, uint32_t width, uint32_t height,
     uint32_t format, int prime_fd, unsigned long pitch);
static struct wld_drawable * dumb_import_gem
    (struct dumb_context * context, uint32_t width, uint32_t height,
     uint32_t format, uint32_t gem_name, unsigned long pitch);

/* DRM drawable */
static int dumb_export(struct wld_drawable * drawable);
static uint32_t dumb_get_handle(struct wld_drawable * drawable);

static struct drm_draw_interface dumb_draw = {
    .export = &dumb_export,
    .get_handle = &dumb_get_handle
};
static bool dumb_draw_initialized;

const struct wld_drm_interface dumb_drm = {
    .device_supported = &dumb_device_supported,
    .create_context = (drm_create_context_func_t) &dumb_create_context,
    .destroy_context = (drm_destroy_context_func_t) &dumb_destroy_context,
    .create_drawable = (drm_create_drawable_func_t) &dumb_create_drawable,
    .import = (drm_import_func_t) &dumb_import,
    .import_gem = (drm_import_gem_func_t) &dumb_import_gem,
};

bool dumb_device_supported(uint32_t vendor_id, uint32_t device_id)
{
    return true;
}

struct dumb_context * dumb_create_context(int drm_fd)
{
    struct dumb_context * context;

    if (!(context = malloc(sizeof *context)))
        goto error0;

    if (!(context->pixman = wld_pixman_create_context()))
        goto error1;

    context->fd = drm_fd;

    if (!dumb_draw_initialized)
    {
        dumb_draw.base = pixman_draw;
        dumb_draw_initialized = true;
    }

    return context;

  error1:
    free(context);
  error0:
    return NULL;
}

void dumb_destroy_context(struct dumb_context * context)
{
    wld_pixman_destroy_context(context->pixman);
    free(context);
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

    if (!pixman_initialize_drawable(context->pixman, &drawable->pixman,
                                    width, height, data, pitch, format))
    {
        goto error2;
    }

    drawable->pixman.base.interface = (struct wld_draw_interface *) &dumb_draw;
    drawable->context = context;
    drawable->handle = handle;

    return &drawable->pixman.base;

  error2:
    munmap(data, size);
  error1:
    free(drawable);
  error0:
    return NULL;
}

struct wld_drawable * dumb_create_drawable
    (struct dumb_context * context, uint32_t width, uint32_t height,
     uint32_t format)
{
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

struct wld_drawable * dumb_import(struct dumb_context * context,
                                  uint32_t width, uint32_t height,
                                  uint32_t format,
                                  int prime_fd, unsigned long pitch)
{
    int ret;
    uint32_t handle;

    ret = drmPrimeFDToHandle(context->fd, prime_fd, &handle);

    if (ret != 0)
        goto error0;

    return new_drawable(context, width, height, format, handle, pitch);

  error0:
    return NULL;
}

struct wld_drawable * dumb_import_gem(struct dumb_context * context,
                                      uint32_t width, uint32_t height,
                                      uint32_t format,
                                      uint32_t gem_name, unsigned long pitch)
{
    DEBUG("dumb: import_gem is not supported\n");

    return NULL;
}

static int dumb_export(struct wld_drawable * drawable)
{
    struct dumb_drawable * dumb = (void *) drawable;
    int prime_fd, ret;

    ret = drmPrimeHandleToFD(dumb->context->fd, dumb->handle,
                             DRM_CLOEXEC, &prime_fd);

    if (ret != 0)
        return -1;

    return prime_fd;
}

static uint32_t dumb_get_handle(struct wld_drawable * drawable)
{
    struct dumb_drawable * dumb = (void *) drawable;

    return dumb->handle;
}

