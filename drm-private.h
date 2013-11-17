/* wld: drm-private.h
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

#ifndef WLD_DRM_PRIVATE_H
#define WLD_DRM_PRIVATE_H 1

#include "wld-private.h"

typedef bool (* drm_device_supported_func_t)(uint32_t vendor_id,
                                             uint32_t device_id);
typedef void * (* drm_create_context_func_t)(int drm_fd);
typedef void (* drm_destroy_context_func_t)(void * context);
typedef struct wld_drawable * (* drm_create_drawable_func_t)
    (void * context, uint32_t width, uint32_t height, uint32_t format);
typedef struct wld_drawable * (* drm_import_func_t)
    (void * context, uint32_t width, uint32_t height, uint32_t format,
     int prime_fd, unsigned long pitch);
typedef struct wld_drawable * (* drm_import_gem_func_t)
    (void * context, uint32_t width, uint32_t height, uint32_t format,
     uint32_t gem_name, unsigned long pitch);

typedef int (* drm_export_func_t)(struct wld_drawable * drawable);
typedef uint32_t (* drm_get_handle_func_t)(struct wld_drawable * drawable);

struct wld_drm_interface
{
    drm_device_supported_func_t device_supported;
    drm_create_context_func_t create_context;
    drm_destroy_context_func_t destroy_context;
    drm_create_drawable_func_t create_drawable;
    drm_import_func_t import;
    drm_import_gem_func_t import_gem;
};

struct drm_draw_interface
{
    struct wld_draw_interface base;
    drm_export_func_t export;
    drm_get_handle_func_t get_handle;
};

struct wld_drm_context
{
    const struct wld_drm_interface * interface;
    void * context;
};

#if WITH_DRM_INTEL
extern const struct wld_drm_interface intel_drm;
#endif

bool drm_initialize_context(struct wld_drm_context * context, int fd);
void drm_finalize_context(struct wld_drm_context * context);

#endif

