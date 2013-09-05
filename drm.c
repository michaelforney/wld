/* wld: drm.c
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

#include "drm.h"
#include "drm-private.h"

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

bool drm_initialize_context(struct wld_drm_context * drm, int fd)
{
    if (!(drm->interface = find_drm_interface(fd)))
        return false;

    if (!(drm->context = drm->interface->create_context(fd)))
        return false;

    return true;
}

void drm_finalize_context(struct wld_drm_context * drm)
{
    drm->interface->destroy_context(drm->context);
}

struct wld_drm_context * wld_drm_create_context(int fd)
{
    struct wld_drm_context * drm;

    if (!(drm = malloc(sizeof *drm)))
        goto error0;

    if (!drm_initialize_context(drm, fd))
        goto error1;

    return drm;

  error1:
    free(drm);
  error0:
    return NULL;
}

void wld_drm_destroy_context(struct wld_drm_context * drm)
{
    drm_finalize_context(drm);
    free(drm);
}

static struct wld_drawable * finish_drawable(struct wld_drm_context * drm,
                                             struct drm_drawable * drawable)
{
    if (!drawable)
        return NULL;

    drawable->drm = drm;

    return &drawable->base;
}

struct wld_drawable * wld_drm_create_drawable(struct wld_drm_context * drm,
                                              uint32_t width, uint32_t height,
                                              uint32_t format)
{
    struct drm_drawable * drawable;

    drawable = drm->interface->create_drawable(drm->context,
                                               width, height, format);

    return finish_drawable(drm, drawable);
}

